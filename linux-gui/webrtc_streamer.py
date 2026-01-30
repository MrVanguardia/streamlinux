#!/usr/bin/env python3
"""
WebRTC Streamer - Screen streaming with Wayland/X11 support
Uses xdg-desktop-portal for Wayland, ximagesrc for X11
"""

import gi
gi.require_version('Gst', '1.0')

# Check for GstWebRTC before importing
try:
    gi.require_version('GstWebRTC', '1.0')
    gi.require_version('GstSdp', '1.0')
except ValueError as e:
    import sys
    print("=" * 60)
    print("ERROR: GStreamer WebRTC support not found!")
    print("=" * 60)
    print()
    print("This is required for streaming to work.")
    print()
    print("To fix on Ubuntu/Debian/Mint:")
    print("  sudo apt install gir1.2-gst-plugins-bad-1.0")
    print("  sudo apt install gstreamer1.0-plugins-bad gstreamer1.0-nice")
    print()
    print("To fix on Fedora:")
    print("  sudo dnf install gstreamer1-plugins-bad-free")
    print()
    print("To fix on Arch:")
    print("  sudo pacman -S gst-plugins-bad")
    print()
    print("=" * 60)
    raise ImportError(f"GstWebRTC not available: {e}")

from gi.repository import Gst, GstWebRTC, GstSdp, GLib
import json
import threading
import websocket
import ssl
import os
import subprocess
import random
from dataclasses import dataclass
from typing import Callable, Optional, Dict, Any
from enum import Enum
import logging

# Setup logging
logging.basicConfig(level=logging.DEBUG, format='%(levelname)s:%(name)s:%(message)s')
logger = logging.getLogger('WebRTCStreamer')

# Initialize GStreamer
Gst.init(None)

# Try to import portal screencast for Wayland
try:
    from portal_screencast import PortalScreencast, is_wayland
    PORTAL_AVAILABLE = True
except ImportError:
    PORTAL_AVAILABLE = False
    def is_wayland():
        return os.environ.get('XDG_SESSION_TYPE', '').lower() == 'wayland'
    logger.warning("portal_screencast not available - Wayland capture limited")


class StreamerState(Enum):
    STOPPED = "stopped"
    CONNECTING = "connecting"
    WAITING = "waiting"
    STREAMING = "streaming"
    ERROR = "error"


@dataclass
class StreamConfig:
    """Streaming configuration"""
    width: int = 1280
    height: int = 720
    fps: int = 30
    bitrate: int = 2000  # kbps
    audio: bool = True  # Enable audio
    audio_source: str = "system"  # system, microphone, both, none
    audio_bitrate: int = 128  # kbps for audio
    hw_encoding: bool = False
    capture_method: str = "auto"
    stun_server: str = "stun://stun.l.google.com:19302"
    capture_screen: int = 0  # 0=primary, -1=all, -2=specific window
    window_xid: int = 0  # X11 window ID for specific window capture


# Validación de servidor STUN/TURN
import re
import ipaddress

def validate_stun_server(stun_server: str) -> str:
    """
    Valida y sanitiza URL de servidor STUN/TURN.
    Previene inyección de pipeline GStreamer y SSRF.
    """
    if not stun_server:
        return "stun://stun.l.google.com:19302"  # Default seguro
    
    # Rechazar caracteres de inyección GStreamer
    forbidden_chars = ['!', '|', '&', ';', '$', '`', '(', ')', '{', '}', '<', '>', '\n', '\r']
    for char in forbidden_chars:
        if char in stun_server:
            raise ValueError(f"STUN server contains forbidden character: '{char}'")
    
    # Validar formato: stun://host:port o turn://host:port
    pattern = r'^(stun|stuns|turn|turns)://[a-zA-Z0-9][a-zA-Z0-9.-]*:[0-9]{1,5}$'
    if not re.match(pattern, stun_server):
        raise ValueError(f"Invalid STUN server format: {stun_server}")
    
    # Extraer host y validar que no sea IP privada (SSRF protection)
    try:
        host_part = stun_server.split('://')[1].split(':')[0]
        try:
            ip = ipaddress.ip_address(host_part)
            if ip.is_private or ip.is_loopback or ip.is_link_local:
                raise ValueError(f"STUN server cannot use private/local IP: {host_part}")
        except ValueError:
            pass  # Es un hostname, no una IP - OK
    except Exception:
        pass
    
    return stun_server


class WebRTCStreamer:
    """
    WebRTC streamer using GStreamer webrtcbin.
    Supports both Wayland (via xdg-desktop-portal) and X11 (via ximagesrc).
    """
    
    def __init__(self, config: StreamConfig = None):
        self.config = config or StreamConfig()
        self.pipeline = None
        self.webrtc = None
        self.signaling_ws = None
        self.ws_thread = None
        self.state = StreamerState.STOPPED
        self.peer_id = None
        self.my_id = None
        
        # Wayland portal
        self._portal = None
        self._pipewire_node_id = None
        
        # Connection management - only ONE active stream at a time
        self._pipeline_lock = threading.Lock()
        self._active_peer_id = None  # Currently streaming to this peer
        self._ice_connected = False  # Track if ICE has connected successfully
        
        # Callbacks
        self.on_state_changed: Optional[Callable[[StreamerState], None]] = None
        self.on_stats_update: Optional[Callable[[Dict], None]] = None
        self.on_client_connected: Optional[Callable[[str], None]] = None
        self.on_client_disconnected: Optional[Callable[[str], None]] = None
        self.on_error: Optional[Callable[[str], None]] = None
        
        # Stats
        self.stats_timer_id = None
    
    def _is_local_connection(self, url: str) -> bool:
        """Check if URL points to a local/LAN address"""
        try:
            from urllib.parse import urlparse
            parsed = urlparse(url)
            host = parsed.hostname or ''
            
            # Check for localhost
            if host in ('localhost', '127.0.0.1', '::1'):
                return True
            
            # Check for private IP ranges
            try:
                ip = ipaddress.ip_address(host)
                return ip.is_private or ip.is_loopback or ip.is_link_local
            except ValueError:
                # Not an IP, check if it's a .local domain
                return host.endswith('.local')
        except Exception:
            return False
    
    def setup_wayland_capture(self) -> bool:
        """
        Initialize Wayland screen capture via portal.
        Must be called before starting the stream on Wayland.
        Returns True if successful.
        """
        if not is_wayland():
            logger.info("Not running on Wayland, skipping portal setup")
            return True
        
        if not PORTAL_AVAILABLE:
            logger.error("Portal screencast module not available")
            return False
        
        logger.info("Setting up Wayland screen capture via portal...")
        
        try:
            self._portal = PortalScreencast()
            if self._portal.start_capture():
                self._pipewire_node_id = self._portal.pipewire_node_id
                logger.info(f"✓ Wayland capture ready, PipeWire node: {self._pipewire_node_id}")
                return True
            else:
                logger.error("Failed to start portal screencast")
                return False
        except Exception as e:
            logger.error(f"Portal setup failed: {e}")
            return False
        
    def _set_state(self, state: StreamerState):
        """Update state and notify"""
        old_state = self.state
        self.state = state
        logger.info(f"State changed: {old_state.value} -> {state.value}")
        if self.on_state_changed:
            GLib.idle_add(self.on_state_changed, state)

    def start(self, signaling_url: str):
        """Start the WebRTC streamer"""
        logger.info(f"Starting WebRTC streamer, connecting to: {signaling_url}")
        self._set_state(StreamerState.CONNECTING)
        
        try:
            # Connect to signaling server with host authentication headers
            self.signaling_ws = websocket.WebSocketApp(
                signaling_url,
                on_open=self._on_ws_open,
                on_message=self._on_ws_message,
                on_error=self._on_ws_error,
                on_close=self._on_ws_close,
                header=["X-Client-Type: host"]  # Identify as host for token registration
            )
            
            # Configuración SSL segura - validar certificados en producción
            # Para LAN con certificados auto-firmados, usar CA bundle local
            ssl_opts = {
                "cert_reqs": ssl.CERT_REQUIRED,
                "check_hostname": True,
            }
            # Intentar usar certifi si está disponible
            try:
                import certifi
                ssl_opts["ca_certs"] = certifi.where()
            except ImportError:
                # Fallback: usar bundle del sistema
                ssl_opts["ca_certs"] = "/etc/ssl/certs/ca-certificates.crt"
            
            # Para conexiones locales (LAN), permitir self-signed con advertencia
            if self._is_local_connection(signaling_url):
                logger.warning("Local connection detected - using relaxed SSL for self-signed certs")
                ssl_opts = {"cert_reqs": ssl.CERT_NONE}
            
            self.ws_thread = threading.Thread(
                target=lambda: self.signaling_ws.run_forever(sslopt=ssl_opts),
                daemon=True
            )
            self.ws_thread.start()
            
        except Exception as e:
            logger.error(f"Failed to start: {e}")
            self._set_state(StreamerState.ERROR)
            if self.on_error:
                self.on_error(str(e))

    def stop(self):
        """Stop streaming and cleanup all resources"""
        logger.info("Stopping streamer...")
        
        # Remove stats timer first
        if self.stats_timer_id:
            GLib.source_remove(self.stats_timer_id)
            self.stats_timer_id = None
        
        # Stop GStreamer pipeline
        if self.pipeline:
            logger.debug("Stopping GStreamer pipeline...")
            self.pipeline.set_state(Gst.State.NULL)
            # Wait for state change to complete
            self.pipeline.get_state(Gst.CLOCK_TIME_NONE)
            self.pipeline = None
            self.webrtc = None
        
        # Stop Wayland portal capture
        if self._portal:
            logger.debug("Stopping portal screencast...")
            try:
                self._portal.stop()
            except Exception as e:
                logger.debug(f"Error stopping portal: {e}")
            self._portal = None
        self._pipewire_node_id = None
        
        # Close WebSocket connection
        if self.signaling_ws:
            logger.debug("Closing WebSocket connection...")
            try:
                self.signaling_ws.close()
            except Exception as e:
                logger.debug(f"Error closing WebSocket: {e}")
            self.signaling_ws = None
        
        # Wait for WebSocket thread to finish
        if self.ws_thread and self.ws_thread.is_alive():
            logger.debug("Waiting for WebSocket thread to finish...")
            self.ws_thread.join(timeout=2.0)
        self.ws_thread = None
        
        # Reset all connection and identity state
        self._active_peer_id = None
        self._ice_connected = False
        self.peer_id = None
        self.my_id = None
        
        logger.info("Streamer stopped and cleaned up")
        self._set_state(StreamerState.STOPPED)

    def _on_ws_open(self, ws):
        """WebSocket connected"""
        logger.info("WebSocket connected to signaling server")
        # Register as host
        self._send_signaling({
            'type': 'register',
            'role': 'host',
            'name': os.uname().nodename
        })
        self._set_state(StreamerState.WAITING)

    def _on_ws_message(self, ws, message):
        """Handle signaling message"""
        try:
            data = json.loads(message)
            msg_type = data.get('type', '')
            
            logger.debug(f"Received signaling message: {msg_type} - {data}")
            
            if msg_type == 'registered':
                self.my_id = data.get('peerId') or data.get('id')
                logger.info(f"Registered with ID: {self.my_id}")
                
            elif msg_type == 'peer-joined':
                # A peer joined - if it's a client/viewer, start streaming to them
                peer_id = data.get('peerId')
                peer_name = data.get('name', 'Unknown')
                peer_role = data.get('role', 'client')
                
                # Stream to clients and viewers, not other hosts
                if peer_role in ('client', 'viewer'):
                    # Check if we already have an active streaming session
                    if self._active_peer_id and self._ice_connected:
                        logger.info(f"Ignoring new viewer {peer_name} ({peer_id}) - already streaming to {self._active_peer_id}")
                        return
                    
                    logger.info(f"Client/Viewer connected: {peer_name} ({peer_id})")
                    GLib.idle_add(self._start_pipeline_for_peer, peer_id)
                else:
                    logger.info(f"Peer joined (role={peer_role}): {peer_name}")
                
            elif msg_type == 'viewer-connected':
                # Legacy format
                peer_id = data.get('viewerId')
                peer_name = data.get('viewerName', 'Unknown')
                logger.info(f"Viewer connected: {peer_name} ({peer_id})")
                GLib.idle_add(self._start_pipeline_for_peer, peer_id)
                
            elif msg_type == 'answer':
                sdp = data.get('sdp')
                peer_id = data.get('from')
                logger.info(f"Received answer from: {peer_id}")
                GLib.idle_add(self._handle_answer, sdp)
                
            elif msg_type == 'ice-candidate':
                candidate = data.get('candidate')
                sdp_mid = data.get('sdpMid', '0')
                sdp_mline_index = data.get('sdpMLineIndex', 0)
                logger.debug(f"Received ICE candidate")
                GLib.idle_add(self._handle_ice_candidate, candidate, sdp_mid, sdp_mline_index)
                
            elif msg_type == 'peer-left' or msg_type == 'viewer-disconnected':
                peer_id = data.get('peerId') or data.get('viewerId')
                logger.info(f"Peer disconnected: {peer_id}")
                
                # Reset active peer if this is our streaming target
                if peer_id == self._active_peer_id:
                    logger.info(f"Active streaming peer {peer_id} disconnected, ready for new connections")
                    self._active_peer_id = None
                    self._ice_connected = False
                    
                if self.on_client_disconnected:
                    GLib.idle_add(self.on_client_disconnected, peer_id)
                    
        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON message: {e}")
        except Exception as e:
            logger.error(f"Error handling message: {e}")
            import traceback
            traceback.print_exc()

    def _on_ws_error(self, ws, error):
        """WebSocket error"""
        logger.error(f"WebSocket error: {error}")
        if self.on_error:
            GLib.idle_add(self.on_error, str(error))

    def _on_ws_close(self, ws, close_status, close_msg):
        """WebSocket closed"""
        logger.info(f"WebSocket closed: {close_status} - {close_msg}")
        self._set_state(StreamerState.STOPPED)

    def _build_capture_source(self, display: str) -> str:
        """Build the GStreamer capture source element based on configuration"""
        screen = self.config.capture_screen
        
        # Check if we have Wayland portal capture ready
        if self._pipewire_node_id:
            logger.info(f"Using Wayland portal capture with PipeWire node: {self._pipewire_node_id}")
            return f'pipewiresrc path={self._pipewire_node_id} do-timestamp=true keepalive-time=1000 resend-last=true'
        
        # Check if running on Wayland without portal setup
        if is_wayland():
            logger.warning("=" * 60)
            logger.warning("WAYLAND SESSION - Portal not initialized")
            logger.warning("Call setup_wayland_capture() before starting stream")
            logger.warning("Falling back to XWayland capture (limited)")
            logger.warning("=" * 60)
        
        # X11 or XWayland fallback
        if screen == -2 and self.config.window_xid > 0:
            logger.info(f"Capturing specific window XID: {self.config.window_xid}")
            return f'ximagesrc display-name={display} xid={self.config.window_xid} use-damage=0 show-pointer=true'
        elif screen == -1:
            logger.info("Capturing all screens")
            return f'ximagesrc display-name={display} use-damage=0 show-pointer=true'
        else:
            logger.info(f"Capturing primary screen (display={display})")
            return f'ximagesrc display-name={display} use-damage=0 show-pointer=true'
    
    def _build_audio_source(self) -> Optional[str]:
        """Build the GStreamer audio source element based on configuration"""
        audio_source = self.config.audio_source
        
        if audio_source == 'none':
            return None
        
        # Check if PulseAudio/PipeWire is available
        if not self._check_pulseaudio():
            logger.warning("PulseAudio not available - audio disabled")
            return None
        
        if audio_source == 'system':
            # Capture system audio (what you hear) - use PulseAudio monitor
            monitor_source = self._get_default_monitor()
            if monitor_source:
                logger.info(f"Using PulseAudio monitor: {monitor_source}")
                # Use buffer-time and latency-time for better sync
                return f'pulsesrc device="{monitor_source}" buffer-time=20000 latency-time=10000'
            else:
                # Fallback: try default pulsesrc (PipeWire may handle this)
                logger.warning("No monitor found - trying default audio source")
                return 'pulsesrc buffer-time=20000 latency-time=10000'
                
        elif audio_source == 'microphone':
            # Capture microphone input - default source
            logger.info("Using default microphone input")
            return 'pulsesrc buffer-time=20000 latency-time=10000'
                
        elif audio_source == 'both':
            # For now, just use system audio (mixing would require audiomixer)
            logger.info("Both audio sources requested - using system audio")
            monitor_source = self._get_default_monitor()
            if monitor_source:
                return f'pulsesrc device="{monitor_source}" buffer-time=20000 latency-time=10000'
            return 'pulsesrc buffer-time=20000 latency-time=10000'
        
        return None
    
    def _get_default_monitor(self) -> Optional[str]:
        """Get the monitor source for the default audio sink"""
        # Try multiple methods for different distros/setups
        
        # Method 1: pactl get-default-sink (newer pactl)
        try:
            result = subprocess.run(
                ['pactl', 'get-default-sink'],
                capture_output=True, text=True, timeout=2
            )
            if result.returncode == 0:
                default_sink = result.stdout.strip()
                if default_sink:
                    monitor = f"{default_sink}.monitor"
                    logger.info(f"Default audio monitor (method 1): {monitor}")
                    return monitor
        except Exception as e:
            logger.debug(f"Method 1 failed: {e}")
        
        # Method 2: Parse pactl info (older systems)
        try:
            result = subprocess.run(
                ['pactl', 'info'],
                capture_output=True, text=True, timeout=2
            )
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    if 'Default Sink:' in line:
                        default_sink = line.split(':', 1)[1].strip()
                        if default_sink:
                            monitor = f"{default_sink}.monitor"
                            logger.info(f"Default audio monitor (method 2): {monitor}")
                            return monitor
        except Exception as e:
            logger.debug(f"Method 2 failed: {e}")
        
        # Method 3: List sources and find a monitor
        try:
            result = subprocess.run(
                ['pactl', 'list', 'sources', 'short'],
                capture_output=True, text=True, timeout=2
            )
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    parts = line.split('\t')
                    if len(parts) >= 2:
                        source_name = parts[1]
                        if '.monitor' in source_name:
                            logger.info(f"Default audio monitor (method 3): {source_name}")
                            return source_name
        except Exception as e:
            logger.debug(f"Method 3 failed: {e}")
        
        # Method 4: PipeWire - try to get default through pw-cli
        try:
            result = subprocess.run(
                ['pw-cli', 'info', 'default.audio.sink'],
                capture_output=True, text=True, timeout=2
            )
            if result.returncode == 0:
                # PipeWire uses node names - just use pulsesrc without device
                logger.info("PipeWire detected - using default audio source")
                return None  # Will use default pulsesrc
        except Exception as e:
            logger.debug(f"PipeWire method failed: {e}")
        
        logger.warning("Could not find default audio monitor - audio may not work")
        return None
    
    def _build_audio_source_for_type(self, source_type: str) -> Optional[str]:
        """Helper to build audio source for a specific type"""
        if source_type == 'system':
            monitor = self._get_default_monitor()
            if monitor:
                return f'pulsesrc device="{monitor}"'
        elif source_type == 'microphone':
            return 'pulsesrc'
        return None
    
    def _check_pipewire_audio(self) -> bool:
        """Check if PipeWire audio is available"""
        try:
            result = subprocess.run(['pw-cli', 'info', '0'], 
                                    capture_output=True, text=True, timeout=2)
            return result.returncode == 0
        except:
            return False
    
    def _check_pulseaudio(self) -> bool:
        """Check if PulseAudio is available"""
        try:
            result = subprocess.run(['pactl', 'info'], 
                                    capture_output=True, text=True, timeout=2)
            return result.returncode == 0
        except:
            return False

    def _send_signaling(self, data: dict):
        """Send message to signaling server"""
        if self.signaling_ws:
            try:
                msg = json.dumps(data)
                self.signaling_ws.send(msg)
                logger.debug(f"Sent signaling message: {data.get('type')}")
            except Exception as e:
                logger.error(f"Failed to send signaling message: {e}")

    def _start_pipeline_for_peer(self, peer_id: str):
        """Create and start the GStreamer pipeline for a peer"""
        
        # Use lock to prevent race conditions
        with self._pipeline_lock:
            # Double-check we're not already streaming
            if self._active_peer_id and self._ice_connected:
                logger.info(f"Already streaming to {self._active_peer_id}, ignoring {peer_id}")
                return False
            
            self.peer_id = peer_id
            self._active_peer_id = peer_id
            self._ice_connected = False
            logger.info(f"Starting pipeline for peer: {peer_id}")
        
        try:
            # Stop existing pipeline if any
            if self.pipeline:
                logger.info("Stopping existing pipeline...")
                self.pipeline.set_state(Gst.State.NULL)
                self.pipeline = None
                
            # Get display
            display = os.environ.get('DISPLAY', ':0')
            
            # Build capture source based on settings
            capture_src = self._build_capture_source(display)
            
            # Security: Validate STUN server to prevent injection (vuln-0011)
            try:
                validated_stun = validate_stun_server(self.config.stun_server)
            except ValueError as e:
                logger.error(f"Invalid STUN server: {e}")
                validated_stun = "stun://stun.l.google.com:19302"
            
            # Use VP8 by default - it's more reliable and works well with Android
            logger.info("Using VP8 encoder (reliable for WebRTC)")
            video_enc = f'vp8enc deadline=1 target-bitrate={self.config.bitrate * 1000} keyframe-max-dist=30 cpu-used=4 threads=4 ! rtpvp8pay pt=96 picture-id-mode=1'
            
            # Build audio pipeline if enabled
            audio_pipeline = ""
            if self.config.audio and self.config.audio_source != 'none':
                audio_src = self._build_audio_source()
                if audio_src:
                    # Use error-tolerant audio pipeline
                    audio_pipeline = f'''
                {audio_src}
                ! audioconvert
                ! audioresample
                ! audio/x-raw,rate=48000,channels=2
                ! queue max-size-buffers=2 leaky=downstream
                ! opusenc bitrate={self.config.audio_bitrate * 1000} audio-type=generic
                ! rtpopuspay pt=97
                ! queue max-size-time=100000000 leaky=downstream
                ! webrtc.
            '''
                    logger.info(f"Audio enabled: {self.config.audio_source}")
                else:
                    logger.warning("Audio source not available - continuing without audio")
            
            # Pipeline with webrtcbin defined first, then linked
            # Using leaky queues to prevent buffer overflows
            # Security: Using validated STUN server (vuln-0011)
            pipeline_str = f'''
                webrtcbin name=webrtc bundle-policy=max-bundle stun-server={validated_stun}
                {capture_src}
                ! videoconvert
                ! videoscale
                ! videorate
                ! video/x-raw,width={self.config.width},height={self.config.height},framerate={self.config.fps}/1
                ! queue max-size-buffers=2 leaky=downstream
                ! {video_enc}
                ! queue max-size-time=100000000 leaky=downstream
                ! webrtc.
                {audio_pipeline}
            '''
            
            logger.info("Creating pipeline...")
            logger.debug(f"Pipeline: {pipeline_str}")
            
            try:
                self.pipeline = Gst.parse_launch(pipeline_str)
            except GLib.Error as e:
                # If audio fails, try without audio
                if audio_pipeline and 'pulsesrc' in str(e):
                    logger.warning(f"Audio pipeline failed: {e}, trying without audio")
                    pipeline_str = f'''
                        webrtcbin name=webrtc bundle-policy=max-bundle stun-server={validated_stun}
                        {capture_src}
                        ! videoconvert
                        ! videoscale
                        ! videorate
                        ! video/x-raw,width={self.config.width},height={self.config.height},framerate={self.config.fps}/1
                        ! queue max-size-buffers=2 leaky=downstream
                        ! {video_enc}
                        ! queue max-size-time=100000000 leaky=downstream
                        ! webrtc.
                    '''
                    self.pipeline = Gst.parse_launch(pipeline_str)
                else:
                    raise
            
            # Get webrtcbin element
            self.webrtc = self.pipeline.get_by_name('webrtc')
            if not self.webrtc:
                raise RuntimeError("Failed to get webrtcbin element")
            
            # Connect webrtcbin signals
            self.webrtc.connect('on-negotiation-needed', self._on_negotiation_needed)
            self.webrtc.connect('on-ice-candidate', self._on_ice_candidate)
            self.webrtc.connect('notify::ice-connection-state', self._on_ice_connection_state)
            self.webrtc.connect('notify::ice-gathering-state', self._on_ice_gathering_state)
            
            # Setup bus for messages
            bus = self.pipeline.get_bus()
            bus.add_signal_watch()
            bus.connect('message::error', self._on_bus_error)
            bus.connect('message::eos', self._on_bus_eos)
            bus.connect('message::state-changed', self._on_bus_state_changed)
            
            # Start pipeline
            ret = self.pipeline.set_state(Gst.State.PLAYING)
            if ret == Gst.StateChangeReturn.FAILURE:
                raise RuntimeError("Failed to set pipeline to PLAYING")
                
            logger.info(f"Pipeline started, state change returned: {ret}")
            
            self._set_state(StreamerState.STREAMING)
            
            if self.on_client_connected:
                self.on_client_connected(peer_id)
                
            # Start stats timer
            self.stats_timer_id = GLib.timeout_add(1000, self._update_stats)
            
        except Exception as e:
            logger.error(f"Pipeline creation failed: {e}")
            import traceback
            traceback.print_exc()
            self._set_state(StreamerState.ERROR)
            if self.on_error:
                self.on_error(str(e))

    def _on_negotiation_needed(self, webrtc):
        """Called when negotiation is needed - create offer"""
        logger.info("Negotiation needed, creating offer...")
        promise = Gst.Promise.new_with_change_func(self._on_offer_created, None)
        self.webrtc.emit('create-offer', None, promise)

    def _on_offer_created(self, promise, user_data):
        """Offer created, set local description and send to peer"""
        logger.info("Offer created")
        
        reply = promise.get_reply()
        if not reply:
            logger.error("No reply from create-offer promise")
            return
            
        offer = reply.get_value('offer')
        if not offer:
            logger.error("No offer in reply")
            return
            
        # Set local description
        promise2 = Gst.Promise.new()
        self.webrtc.emit('set-local-description', offer, promise2)
        promise2.interrupt()
        
        # Get SDP text and send to peer
        sdp_text = offer.sdp.as_text()
        logger.info(f"Sending offer to peer {self.peer_id}")
        logger.debug(f"SDP Offer:\n{sdp_text[:500]}...")
        
        self._send_signaling({
            'type': 'offer',
            'to': self.peer_id,
            'sdp': sdp_text
        })

    def _on_ice_candidate(self, webrtc, mline_index, candidate):
        """ICE candidate gathered, send to peer"""
        logger.debug(f"ICE candidate gathered: mline={mline_index}")
        
        self._send_signaling({
            'type': 'ice-candidate',
            'to': self.peer_id,
            'candidate': candidate,
            'sdpMLineIndex': mline_index,
            'sdpMid': str(mline_index)
        })

    def _on_ice_connection_state(self, webrtc, pspec):
        """ICE connection state changed"""
        state = webrtc.get_property('ice-connection-state')
        state_name = GstWebRTC.WebRTCICEConnectionState(state).value_nick
        logger.info(f"ICE connection state: {state_name}")
        
        if state == GstWebRTC.WebRTCICEConnectionState.CONNECTED:
            self._ice_connected = True
            logger.info("✓ ICE Connected - Media should be flowing!")
        elif state == GstWebRTC.WebRTCICEConnectionState.COMPLETED:
            self._ice_connected = True
            logger.info("✓ ICE Completed - Connection fully established!")
        elif state == GstWebRTC.WebRTCICEConnectionState.FAILED:
            logger.error("✗ ICE Connection Failed!")
            self._ice_connected = False
            self._active_peer_id = None
            if self.on_error:
                GLib.idle_add(self.on_error, "ICE connection failed")
        elif state == GstWebRTC.WebRTCICEConnectionState.DISCONNECTED:
            logger.warning("ICE Disconnected - peer may have left")
            self._ice_connected = False
            self._active_peer_id = None
        elif state == GstWebRTC.WebRTCICEConnectionState.CLOSED:
            logger.info("ICE Closed")
            self._ice_connected = False
            self._active_peer_id = None

    def _on_ice_gathering_state(self, webrtc, pspec):
        """ICE gathering state changed"""
        state = webrtc.get_property('ice-gathering-state')
        state_name = GstWebRTC.WebRTCICEGatheringState(state).value_nick
        logger.info(f"ICE gathering state: {state_name}")

    def _handle_answer(self, sdp_text: str):
        """Handle SDP answer from peer"""
        logger.info("Processing SDP answer...")
        
        if not self.webrtc:
            logger.error("No webrtc element!")
            return
            
        try:
            res, sdp_msg = GstSdp.SDPMessage.new_from_text(sdp_text)
            if res != GstSdp.SDPResult.OK:
                logger.error(f"Failed to parse SDP: {res}")
                return
                
            answer = GstWebRTC.WebRTCSessionDescription.new(
                GstWebRTC.WebRTCSDPType.ANSWER, sdp_msg
            )
            
            promise = Gst.Promise.new()
            self.webrtc.emit('set-remote-description', answer, promise)
            promise.interrupt()
            
            logger.info("Remote description set successfully")
            
        except Exception as e:
            logger.error(f"Error handling answer: {e}")
            import traceback
            traceback.print_exc()

    def _handle_ice_candidate(self, candidate: str, sdp_mid: str, sdp_mline_index: int):
        """Handle ICE candidate from peer"""
        if not self.webrtc:
            logger.error("No webrtc element for ICE candidate!")
            return
            
        try:
            # Handle None values
            if sdp_mline_index is None:
                sdp_mline_index = 0
            if sdp_mid is None:
                sdp_mid = "0"
                
            logger.debug(f"Adding ICE candidate: mline={sdp_mline_index}, mid={sdp_mid}")
            self.webrtc.emit('add-ice-candidate', sdp_mline_index, candidate)
            
        except Exception as e:
            logger.error(f"Error adding ICE candidate: {e}")

    def _on_bus_error(self, bus, message):
        """Handle GStreamer bus error"""
        err, debug = message.parse_error()
        logger.error(f"Pipeline error: {err.message}")
        logger.error(f"Debug info: {debug}")
        
        self._set_state(StreamerState.ERROR)
        if self.on_error:
            GLib.idle_add(self.on_error, err.message)

    def _on_bus_eos(self, bus, message):
        """Handle end of stream"""
        logger.info("End of stream")

    def _on_bus_state_changed(self, bus, message):
        """Handle state change"""
        if message.src == self.pipeline:
            old, new, pending = message.parse_state_changed()
            logger.debug(f"Pipeline state: {old.value_nick} -> {new.value_nick}")

    def _update_stats(self) -> bool:
        """Update and report statistics"""
        if not self.pipeline or self.state != StreamerState.STREAMING:
            return False
            
        try:
            stats = {
                'connected_clients': 1 if self._active_peer_id and self._ice_connected else 0,
                'bitrate': 0,
                'fps': 0,
                'resolution': f"{self.config.width}x{self.config.height}",
                'audio_enabled': self.config.audio and self.config.audio_source != 'none',
                'encoding': 'VP8',
                'latency_ms': 0,
                'bytes_sent': 0,
                'packets_sent': 0
            }
            
            # Try to get real stats from webrtcbin
            if self.webrtc:
                try:
                    promise = Gst.Promise.new()
                    self.webrtc.emit('get-stats', None, promise)
                    promise.wait()
                    
                    reply = promise.get_reply()
                    if reply:
                        # Parse WebRTC stats
                        stats.update(self._parse_webrtc_stats(reply))
                except Exception as e:
                    logger.debug(f"Could not get WebRTC stats: {e}")
            
            # Get video encoder stats for bitrate/fps
            if self.pipeline:
                try:
                    # Look for vp8enc element
                    enc = self.pipeline.get_by_name('webrtc')
                    if enc:
                        # Get position and duration for elapsed time
                        ret, position = self.pipeline.query_position(Gst.Format.TIME)
                        if ret and position > 0:
                            stats['stream_time'] = position // Gst.SECOND
                except Exception as e:
                    logger.debug(f"Could not get encoder stats: {e}")
            
            # Use configured values as fallback
            if stats['bitrate'] == 0:
                stats['bitrate'] = self.config.bitrate
            if stats['fps'] == 0:
                stats['fps'] = self.config.fps
                
            if self.on_stats_update:
                self.on_stats_update(stats)
                
        except Exception as e:
            logger.error(f"Stats update error: {e}")
            
        return True  # Continue timer
    
    def _parse_webrtc_stats(self, stats_struct) -> dict:
        """Parse WebRTC statistics from GstStructure"""
        result = {}
        
        try:
            n_fields = stats_struct.n_fields()
            total_bytes_sent = 0
            total_packets_sent = 0
            
            for i in range(n_fields):
                field_name = stats_struct.nth_field_name(i)
                value = stats_struct.get_value(field_name)
                
                # Each stat is a GstStructure with type and values
                if hasattr(value, 'get_string'):
                    stat_type = value.get_string('type')
                    
                    if stat_type in ['outbound-rtp', 'remote-inbound-rtp']:
                        # Get bytes sent
                        bytes_sent = value.get_uint64('bytes-sent')
                        if bytes_sent:
                            total_bytes_sent += bytes_sent[1] if isinstance(bytes_sent, tuple) else bytes_sent
                            
                        # Get packets sent
                        packets_sent = value.get_uint64('packets-sent')
                        if packets_sent:
                            total_packets_sent += packets_sent[1] if isinstance(packets_sent, tuple) else packets_sent
                            
                        # Get bitrate from remote stats (more accurate)
                        if stat_type == 'remote-inbound-rtp':
                            rtt = value.get_double('round-trip-time')
                            if rtt:
                                result['latency_ms'] = int((rtt[1] if isinstance(rtt, tuple) else rtt) * 1000)
                    
                    elif stat_type == 'candidate-pair' and value.get_string('state') == 'succeeded':
                        # Get current bitrate from active candidate pair
                        current_rtt = value.get_double('current-round-trip-time')
                        if current_rtt:
                            result['latency_ms'] = int((current_rtt[1] if isinstance(current_rtt, tuple) else current_rtt) * 1000)
            
            result['bytes_sent'] = total_bytes_sent
            result['packets_sent'] = total_packets_sent
            
            # Calculate approximate bitrate from bytes sent
            if total_bytes_sent > 0 and hasattr(self, '_last_bytes_sent'):
                bytes_diff = total_bytes_sent - self._last_bytes_sent
                result['bitrate'] = int((bytes_diff * 8) / 1000)  # kbps
            self._last_bytes_sent = total_bytes_sent
            
        except Exception as e:
            logger.debug(f"Error parsing WebRTC stats: {e}")
            
        return result

    def set_bitrate(self, bitrate_kbps: int):
        """Dynamically change the video bitrate"""
        self.config.bitrate = bitrate_kbps
        if self.pipeline:
            try:
                # Find VP8 encoder and update bitrate
                enc = None
                iterator = self.pipeline.iterate_elements()
                while True:
                    result, element = iterator.next()
                    if result == Gst.IteratorResult.DONE:
                        break
                    if result == Gst.IteratorResult.OK:
                        if 'vp8enc' in element.get_name() or element.get_factory().get_name() == 'vp8enc':
                            enc = element
                            break
                
                if enc:
                    enc.set_property('target-bitrate', bitrate_kbps * 1000)
                    logger.info(f"Bitrate updated to {bitrate_kbps} kbps")
                    return True
            except Exception as e:
                logger.error(f"Failed to update bitrate: {e}")
        return False
    
    def set_quality(self, preset: str):
        """Set quality preset: low, medium, high, ultra"""
        presets = {
            'low': {'width': 854, 'height': 480, 'fps': 30, 'bitrate': 1500},
            'medium': {'width': 1280, 'height': 720, 'fps': 30, 'bitrate': 3000},
            'high': {'width': 1920, 'height': 1080, 'fps': 30, 'bitrate': 6000},
            'ultra': {'width': 1920, 'height': 1080, 'fps': 60, 'bitrate': 12000},
        }
        
        if preset not in presets:
            logger.warning(f"Unknown quality preset: {preset}")
            return False
        
        settings = presets[preset]
        self.config.width = settings['width']
        self.config.height = settings['height']
        self.config.fps = settings['fps']
        self.config.bitrate = settings['bitrate']
        
        # Update bitrate immediately if streaming
        if self.pipeline and self.state == StreamerState.STREAMING:
            self.set_bitrate(settings['bitrate'])
            logger.info(f"Quality preset set to {preset}: {settings}")
        
        return True
    
    def request_keyframe(self):
        """Request a keyframe from the video encoder"""
        if self.pipeline:
            try:
                # Send force-keyunit event
                event = Gst.Event.new_custom(
                    Gst.EventType.CUSTOM_UPSTREAM,
                    Gst.Structure.new_from_string("GstForceKeyUnit")
                )
                self.pipeline.send_event(event)
                logger.info("Keyframe requested")
                return True
            except Exception as e:
                logger.error(f"Failed to request keyframe: {e}")
        return False
    
    def set_audio_enabled(self, enabled: bool):
        """Enable or disable audio streaming"""
        self.config.audio = enabled
        if not enabled:
            self.config.audio_source = 'none'
        else:
            self.config.audio_source = 'system'
        logger.info(f"Audio {'enabled' if enabled else 'disabled'}")
        return True

