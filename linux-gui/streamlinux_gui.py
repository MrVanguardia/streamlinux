#!/usr/bin/env python3
"""
StreamLinux - Linux Host Control Panel
Modern GTK4/Adwaita application for controlling screen streaming to Android devices.
Redesigned with better organization and sections.
Now with real WebRTC streaming and persistent settings!
"""

import os
import sys

# Add StreamLinux library path for installed packages
# This allows imports from /usr/share/streamlinux/ when installed via RPM
_streamlinux_lib_paths = [
    '/usr/share/streamlinux',
    '/usr/local/share/streamlinux',
    os.path.dirname(os.path.abspath(__file__))  # For local development
]
for _path in _streamlinux_lib_paths:
    if os.path.isdir(_path) and _path not in sys.path:
        sys.path.insert(0, _path)

import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')

from gi.repository import Gtk, Adw, GLib, Gio, Gdk, GdkPixbuf
import socket
import json
import threading
import subprocess
import time
import uuid
import hashlib
from pathlib import Path
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Dict, Optional

# Try to import QR code library
try:
    import qrcode
    from io import BytesIO
    HAS_QRCODE = True
except ImportError:
    HAS_QRCODE = False
    print("Warning: qrcode library not found. Install with: pip install qrcode[pil]")

# Import WebRTC streamer
try:
    from webrtc_streamer import WebRTCStreamer, StreamConfig, StreamerState
    HAS_WEBRTC = True
except ImportError:
    HAS_WEBRTC = False
    print("Warning: WebRTC streamer not available")

# Import USB Manager for ADB port forwarding
try:
    from usb_manager import USBManager, USBState, get_usb_manager
    HAS_USB = True
except ImportError:
    HAS_USB = False
    print("Warning: USB manager not available - USB streaming disabled")

# Import i18n module
try:
    from i18n import get_i18n, _, set_language, get_language
    HAS_I18N = True
except ImportError:
    HAS_I18N = False
    print("Warning: i18n module not available - using English only")
    def _(key, **kwargs): return key
    def get_i18n(): return None
    def set_language(lang): pass
    def get_language(): return 'en'

# Import Security module
try:
    from security import SecurityManager, get_security_manager, PendingConnection, AuthorizedDevice
    HAS_SECURITY = True
except ImportError:
    HAS_SECURITY = False
    print("Warning: Security module not available - connections will not be secured")


class AppConfig:
    """Persistent configuration manager using JSON file storage"""
    
    CONFIG_DIR = Path.home() / ".config" / "streamlinux"
    CONFIG_FILE = CONFIG_DIR / "settings.json"
    
    # Default settings
    DEFAULTS = {
        # Video settings
        'video_encoder': 'auto',  # auto, vaapi, nvenc, software
        'video_bitrate': 8,       # Mbps
        'video_preset': 'veryfast',  # ultrafast, veryfast, fast, medium, slow
        'video_quality': 'auto',   # auto, 1080p, 720p, 480p
        'video_fps': 60,
        
        # Capture settings
        'capture_method': 'auto',  # auto, pipewire, xdg-portal, x11
        'capture_screen': 0,       # Screen index or -1 for all
        
        # Audio settings
        'audio_enabled': True,
        'audio_source': 'system',  # system, microphone, both, none
        'audio_codec': 'opus',     # opus, aac
        'audio_bitrate': 320,      # kbps
        
        # Network settings
        'port': 54321,
        'stun_server': 'stun:stun.l.google.com:19302',
        'turn_server': '',
        
        # Security settings
        'security_require_auth': True,    # Require authentication for connections
        'security_require_pin': True,     # Show PIN for new devices
        'security_auto_trust': False,     # Auto-trust devices after first connection
        'security_token_expiry': 300,     # Token validity in seconds (5 min)
        
        # Startup settings
        'autostart': False,
        'autostream': False,
        'minimize_to_tray': False,
        
        # Debug settings
        'verbose_logging': False,
        'show_stats_overlay': False,
        
        # UI settings
        'window_width': 800,
        'window_height': 600,
        'language': 'system',  # system, en, es
        
        # USB settings
        'usb_boost_enabled': True,       # Auto-boost quality for USB
        'usb_bitrate_multiplier': 2.0,   # Multiply bitrate for USB (2x)
    }
    
    _instance = None
    _listeners: Dict[str, list] = {}
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance
    
    def __init__(self):
        if self._initialized:
            return
        self._initialized = True
        self._config = dict(self.DEFAULTS)
        self._listeners = {}
        self._load()
    
    def _load(self):
        """Load configuration from file"""
        try:
            if self.CONFIG_FILE.exists():
                with open(self.CONFIG_FILE, 'r') as f:
                    saved = json.load(f)
                    # Merge with defaults (to handle new settings)
                    for key, value in saved.items():
                        if key in self.DEFAULTS:
                            self._config[key] = value
                print(f"Loaded config from {self.CONFIG_FILE}")
        except Exception as e:
            print(f"Error loading config: {e}")
    
    def _save(self):
        """Save configuration to file"""
        try:
            self.CONFIG_DIR.mkdir(parents=True, exist_ok=True)
            with open(self.CONFIG_FILE, 'w') as f:
                json.dump(self._config, f, indent=2)
            print(f"Saved config to {self.CONFIG_FILE}")
        except Exception as e:
            print(f"Error saving config: {e}")
    
    def get(self, key: str, default: Any = None) -> Any:
        """Get a configuration value"""
        return self._config.get(key, default if default is not None else self.DEFAULTS.get(key))
    
    def set(self, key: str, value: Any, save: bool = True):
        """Set a configuration value"""
        old_value = self._config.get(key)
        self._config[key] = value
        if save:
            self._save()
        # Notify listeners
        if key in self._listeners and old_value != value:
            for callback in self._listeners[key]:
                try:
                    callback(key, value)
                except Exception as e:
                    print(f"Error in config listener: {e}")
    
    def add_listener(self, key: str, callback: Callable[[str, Any], None]):
        """Add a listener for configuration changes"""
        if key not in self._listeners:
            self._listeners[key] = []
        self._listeners[key].append(callback)
    
    def remove_listener(self, key: str, callback: Callable[[str, Any], None]):
        """Remove a listener"""
        if key in self._listeners and callback in self._listeners[key]:
            self._listeners[key].remove(callback)
    
    def reset_to_defaults(self):
        """Reset all settings to defaults"""
        self._config = dict(self.DEFAULTS)
        self._save()
        # Notify all listeners
        for key, listeners in self._listeners.items():
            for callback in listeners:
                try:
                    callback(key, self._config.get(key))
                except Exception as e:
                    print(f"Error in config listener: {e}")
    
    @property
    def all(self) -> dict:
        """Get all configuration values"""
        return dict(self._config)


class StreamState(Enum):
    STOPPED = "stopped"
    STARTING = "starting"
    STREAMING = "streaming"
    ERROR = "error"


class StreamLinuxApp(Adw.Application):
    """Main application class"""
    
    def __init__(self):
        super().__init__(
            application_id='com.streamlinux.host',
            flags=Gio.ApplicationFlags.FLAGS_NONE
        )
        self.window = None
        
    def do_activate(self):
        if not self.window:
            self.window = MainWindow(application=self)
        self.window.present()
        
    def do_startup(self):
        Adw.Application.do_startup(self)
        
        # Load language setting from config
        config = AppConfig()
        lang = config.get('language')
        if lang and lang != 'system':
            set_language(lang)
        
        # Create app menu actions
        self.create_action('about', self.on_about)
        self.create_action('preferences', self.on_preferences)
        self.create_action('quit', self.on_quit)
        
    def create_action(self, name, callback):
        action = Gio.SimpleAction.new(name, None)
        action.connect('activate', callback)
        self.add_action(action)
        
    def on_about(self, action, param):
        self.window.show_about()
        
    def on_preferences(self, action, param):
        self.window.show_preferences()
        
    def on_quit(self, action, param):
        self.quit()


class MainWindow(Adw.ApplicationWindow):
    """Main application window with organized sections"""
    
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        
        self.set_title("StreamLinux")
        
        # State
        self.stream_state = StreamState.STOPPED
        self.stream_process = None
        self.signaling_process = None
        self.start_time = None
        self.connected_clients = []
        
        # WebRTC Streamer
        self.streamer = None
        
        # Security Manager
        self.security_manager = None
        if HAS_SECURITY:
            self.security_manager = get_security_manager()
            self.security_manager.on_connection_request = self._on_connection_request
            self.security_manager.on_connection_authorized = self._on_connection_authorized
            self.security_manager.on_connection_rejected = self._on_connection_rejected
        
        # USB Manager for ADB port forwarding
        self.usb_manager = None
        if HAS_USB:
            self.usb_manager = get_usb_manager()
            self.usb_manager.on_device_connected = self._on_usb_device_connected
            self.usb_manager.on_device_disconnected = self._on_usb_device_disconnected
            self.usb_manager.on_state_changed = self._on_usb_state_changed
        
        # Load persistent configuration
        self.config = AppConfig()
        
        # Set window size from config
        self.set_default_size(
            self.config.get('window_width'),
            self.config.get('window_height')
        )
        
        # Track window size changes
        self.connect('notify::default-width', self._on_window_size_changed)
        self.connect('notify::default-height', self._on_window_size_changed)
        
        # Handle window close to clean up resources
        self.connect('close-request', self._on_close_request)
        
        # Get local IP
        self.local_ip = self.get_local_ip()
        
        # Build UI
        self.build_ui()
        
        # Start update timer
        GLib.timeout_add(1000, self.update_ui)
        
        # Auto-start signaling server
        self.start_signaling_server()
        
        # Auto-stream if configured
        if self.config.get('autostream'):
            GLib.timeout_add(1000, self._delayed_autostart)
    
    def _on_window_size_changed(self, *args):
        """Save window size when changed"""
        width = self.get_default_size()[0]
        height = self.get_default_size()[1]
        if width > 0 and height > 0:
            self.config.set('window_width', width, save=False)
            self.config.set('window_height', height)
    
    def _delayed_autostart(self):
        """Start streaming after a short delay (for autostream)"""
        if self.config.get('autostream') and self.stream_state == StreamState.STOPPED:
            self.start_streaming()
        return False  # Don't repeat
    
    def _on_close_request(self, window):
        """Handle window close request - cleanup all resources"""
        print("🔄 Cerrando aplicación...")
        
        # Stop streaming first
        if self.stream_state != StreamState.STOPPED:
            print("  ⏹️ Deteniendo streaming...")
            if self.streamer:
                self.streamer.stop()
                self.streamer = None
        
        # Stop USB manager
        if hasattr(self, 'usb_manager') and self.usb_manager:
            print("  🔌 Limpiando USB manager...")
            self.usb_manager.stop_device_monitor()
        
        # Stop signaling server
        if hasattr(self, 'signaling_process') and self.signaling_process:
            print("  🌐 Deteniendo servidor de señalización...")
            try:
                self.signaling_process.terminate()
                self.signaling_process.wait(timeout=2)
            except:
                self.signaling_process.kill()
            self.signaling_process = None
        
        # Save window state
        width = self.get_default_size()[0]
        height = self.get_default_size()[1]
        if width > 0 and height > 0:
            self.config.set('window_width', width, save=False)
            self.config.set('window_height', height)
        
        print("✅ Aplicación cerrada correctamente")
        return False  # Allow window to close
        
    def get_local_ip(self):
        """Get the local IP address"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            return ip
        except:
            return "127.0.0.1"
    
    def build_ui(self):
        """Build the main UI with navigation"""
        # Main layout
        self.main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.set_content(self.main_box)
        
        # Header bar
        header = Adw.HeaderBar()
        
        # Title
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        icon = Gtk.Image.new_from_icon_name("video-display-symbolic")
        icon.set_pixel_size(20)
        title_box.append(icon)
        title_label = Gtk.Label(label="StreamLinux")
        title_label.add_css_class("heading")
        title_box.append(title_label)
        header.set_title_widget(title_box)
        
        # Menu button
        menu_button = Gtk.MenuButton()
        menu_button.set_icon_name("open-menu-symbolic")
        menu_button.set_tooltip_text(_('tooltip_settings'))
        
        menu = Gio.Menu()
        menu.append(_('preferences'), "app.preferences")
        menu.append(_('about'), "app.about")
        menu.append(_('close'), "app.quit")
        menu_button.set_menu_model(menu)
        header.pack_end(menu_button)
        
        self.main_box.append(header)
        
        # Toast overlay
        self.toast_overlay = Adw.ToastOverlay()
        self.main_box.append(self.toast_overlay)
        
        # Navigation view
        self.nav_view = Adw.NavigationView()
        self.toast_overlay.set_child(self.nav_view)
        
        # Main page
        main_page = self.create_main_page()
        self.nav_view.push(main_page)
        
    def create_main_page(self):
        """Create the main page with sections"""
        page = Adw.NavigationPage()
        page.set_title("Inicio")
        
        # Scrolled content
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_vexpand(True)
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        page.set_child(scrolled)
        
        # Clamp for responsive width
        clamp = Adw.Clamp()
        clamp.set_maximum_size(800)
        clamp.set_margin_start(16)
        clamp.set_margin_end(16)
        clamp.set_margin_top(16)
        clamp.set_margin_bottom(16)
        scrolled.set_child(clamp)
        
        # Main content box
        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=24)
        clamp.set_child(content)
        
        # === STATUS SECTION ===
        content.append(self.create_status_section())
        
        # === TWO COLUMN LAYOUT ===
        columns = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=16)
        columns.set_homogeneous(False)
        content.append(columns)
        
        # Left column - QR Code
        left_col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        left_col.set_hexpand(True)
        columns.append(left_col)
        left_col.append(self.create_qr_section())
        
        # Right column - Quick Settings
        right_col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        right_col.set_hexpand(True)
        columns.append(right_col)
        right_col.append(self.create_quick_settings_section())
        
        # === USB CONNECTION SECTION ===
        if HAS_USB:
            content.append(self.create_usb_section())
        
        # === CONNECTION INFO SECTION ===
        content.append(self.create_connection_section())
        
        # === STATS SECTION ===
        content.append(self.create_stats_section())
        
        return page
    
    def create_status_section(self):
        """Create the main status/control section"""
        group = Adw.PreferencesGroup()
        group.set_title(_('section_status'))
        group.set_description(_('app_subtitle'))
        
        # Status row with big button
        row = Adw.ActionRow()
        
        # Status indicator
        self.status_dot = Gtk.Box()
        self.status_dot.set_size_request(12, 12)
        self.status_dot.add_css_class("status-dot")
        self.status_dot.add_css_class("status-stopped")
        row.add_prefix(self.status_dot)
        
        # Status text
        self.status_title = _('status_stopped')
        self.status_subtitle = _('tooltip_start')
        row.set_title(self.status_title)
        row.set_subtitle(self.status_subtitle)
        
        # Big toggle button
        self.toggle_btn = Gtk.Button()
        self.toggle_btn.set_valign(Gtk.Align.CENTER)
        self.toggle_btn.add_css_class("suggested-action")
        self.toggle_btn.add_css_class("pill")
        self.toggle_btn.connect("clicked", self.on_toggle_streaming)
        
        btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        btn_box.set_margin_start(12)
        btn_box.set_margin_end(12)
        btn_box.set_margin_top(8)
        btn_box.set_margin_bottom(8)
        
        self.toggle_icon = Gtk.Image.new_from_icon_name("media-playback-start-symbolic")
        btn_box.append(self.toggle_icon)
        
        self.toggle_label = Gtk.Label(label=_('start_stream'))
        self.toggle_label.add_css_class("heading")
        btn_box.append(self.toggle_label)
        
        self.toggle_btn.set_child(btn_box)
        row.add_suffix(self.toggle_btn)
        
        group.add(row)
        
        # Uptime row
        self.uptime_row = Adw.ActionRow()
        self.uptime_row.set_title(_('stream_time'))
        self.uptime_row.set_subtitle("--:--:--")
        self.uptime_row.add_prefix(Gtk.Image.new_from_icon_name("preferences-system-time-symbolic"))
        self.uptime_row.set_visible(False)
        group.add(self.uptime_row)
        
        return group
    
    def create_qr_section(self):
        """Create QR code section"""
        group = Adw.PreferencesGroup()
        group.set_title(_('connection_qr'))
        group.set_description(_('msg_qr_scan'))
        
        # QR container
        qr_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        qr_box.set_halign(Gtk.Align.CENTER)
        qr_box.set_margin_top(8)
        qr_box.set_margin_bottom(8)
        
        # QR Frame
        qr_frame = Gtk.Frame()
        qr_frame.add_css_class("card")
        
        qr_inner = Gtk.Box()
        qr_inner.set_margin_start(16)
        qr_inner.set_margin_end(16)
        qr_inner.set_margin_top(16)
        qr_inner.set_margin_bottom(16)
        
        self.qr_image = Gtk.Picture()
        self.qr_image.set_size_request(180, 180)
        self.qr_image.set_content_fit(Gtk.ContentFit.CONTAIN)
        qr_inner.append(self.qr_image)
        
        qr_frame.set_child(qr_inner)
        qr_box.append(qr_frame)
        
        # Regenerate button
        regen_btn = Gtk.Button(label=_('connection_show_qr'))
        regen_btn.set_halign(Gtk.Align.CENTER)
        regen_btn.connect("clicked", lambda b: self.update_qr_code())
        qr_box.append(regen_btn)
        
        # Add to group
        row = Adw.ActionRow()
        row.set_activatable(False)
        row.set_child(qr_box)
        group.add(row)
        
        # Generate initial QR and start refresh timer
        self.update_qr_code()
        self._start_qr_refresh_timer()
        
        return group
    
    def create_quick_settings_section(self):
        """Create quick settings section with persistent configuration"""
        group = Adw.PreferencesGroup()
        group.set_title(_('section_stream_settings'))
        group.set_description(_('app_subtitle'))
        
        # Quality
        self.quality_row = Adw.ComboRow()
        self.quality_row.set_title(_('video_quality'))
        self.quality_row.set_subtitle(_('resolution'))
        quality_options = [_('quality_auto'), "1080p", "720p", "480p"]
        self.quality_row.set_model(Gtk.StringList.new(quality_options))
        # Set from config
        quality_val = self.config.get('video_quality')
        quality_idx = {'auto': 0, '1080p': 1, '720p': 2, '480p': 3}.get(quality_val, 0)
        self.quality_row.set_selected(quality_idx)
        self.quality_row.add_prefix(Gtk.Image.new_from_icon_name("video-display-symbolic"))
        self.quality_row.connect("notify::selected", self._on_quality_changed)
        group.add(self.quality_row)
        
        # FPS
        self.fps_row = Adw.ComboRow()
        self.fps_row.set_title(_('fps'))
        self.fps_row.set_subtitle(_('video_fps'))
        self.fps_row.set_model(Gtk.StringList.new(["60 FPS", "30 FPS", "24 FPS"]))
        fps_val = self.config.get('video_fps')
        fps_idx = {60: 0, 30: 1, 24: 2}.get(fps_val, 0)
        self.fps_row.set_selected(fps_idx)
        self.fps_row.add_prefix(Gtk.Image.new_from_icon_name("view-refresh-symbolic"))
        self.fps_row.connect("notify::selected", self._on_fps_changed)
        group.add(self.fps_row)
        
        # Audio
        self.audio_row = Adw.SwitchRow()
        self.audio_row.set_title(_('audio_system'))
        self.audio_row.set_subtitle(_('audio_enabled'))
        self.audio_row.set_active(self.config.get('audio_enabled'))
        self.audio_row.add_prefix(Gtk.Image.new_from_icon_name("audio-speakers-symbolic"))
        self.audio_row.connect("notify::active", self._on_audio_changed)
        group.add(self.audio_row)
        
        # Hardware encoding
        self.hw_row = Adw.SwitchRow()
        self.hw_row.set_title(_('encoder'))
        self.hw_row.set_subtitle("VAAPI / NVENC")
        hw_enabled = self.config.get('video_encoder') in ['auto', 'vaapi', 'nvenc']
        self.hw_row.set_active(hw_enabled)
        self.hw_row.add_prefix(Gtk.Image.new_from_icon_name("applications-games-symbolic"))
        self.hw_row.connect("notify::active", self._on_hw_encoding_changed)
        group.add(self.hw_row)
        
        # More settings button
        more_row = Adw.ActionRow()
        more_row.set_title(_('preferences'))
        more_row.set_activatable(True)
        more_row.add_prefix(Gtk.Image.new_from_icon_name("emblem-system-symbolic"))
        more_row.add_suffix(Gtk.Image.new_from_icon_name("go-next-symbolic"))
        more_row.connect("activated", lambda r: self.show_preferences())
        group.add(more_row)
        
        return group
    
    def _on_quality_changed(self, row, param):
        """Handle quality setting change"""
        idx = row.get_selected()
        quality_map = {0: 'auto', 1: '1080p', 2: '720p', 3: '480p'}
        self.config.set('video_quality', quality_map.get(idx, 'auto'))
        self._show_setting_changed_toast("Calidad")
    
    def _on_fps_changed(self, row, param):
        """Handle FPS setting change"""
        idx = row.get_selected()
        fps_map = {0: 60, 1: 30, 2: 24}
        self.config.set('video_fps', fps_map.get(idx, 60))
        self._show_setting_changed_toast("FPS")
    
    def _on_audio_changed(self, row, param):
        """Handle audio setting change"""
        self.config.set('audio_enabled', row.get_active())
        self._show_setting_changed_toast("Audio")
    
    def _on_hw_encoding_changed(self, row, param):
        """Handle hardware encoding setting change"""
        if row.get_active():
            self.config.set('video_encoder', 'auto')
        else:
            self.config.set('video_encoder', 'software')
        self._show_setting_changed_toast("Codificación")
    
    def _show_setting_changed_toast(self, setting_name: str):
        """Show toast when setting changes"""
        toast = Adw.Toast.new(f"✓ {setting_name} actualizado")
        toast.set_timeout(1)
        self.toast_overlay.add_toast(toast)
    
    def create_connection_section(self):
        """Create connection info section"""
        group = Adw.PreferencesGroup()
        group.set_title(_('section_connection'))
        group.set_description(_('connection_wifi'))
        
        # IP Address
        ip_row = Adw.ActionRow()
        ip_row.set_title(_('connection_ip'))
        ip_row.set_subtitle(self.local_ip)
        ip_row.add_prefix(Gtk.Image.new_from_icon_name("network-wired-symbolic"))
        
        copy_ip_btn = Gtk.Button(icon_name="edit-copy-symbolic")
        copy_ip_btn.set_valign(Gtk.Align.CENTER)
        copy_ip_btn.set_tooltip_text("Copiar IP")
        copy_ip_btn.add_css_class("flat")
        copy_ip_btn.connect("clicked", lambda b: self.copy_to_clipboard(self.local_ip))
        ip_row.add_suffix(copy_ip_btn)
        group.add(ip_row)
        
        # Port (from config)
        self.port_row = Adw.ActionRow()
        self.port_row.set_title(_('connection_port'))
        self.port_row.set_subtitle(str(self.config.get('port')))
        self.port_row.add_prefix(Gtk.Image.new_from_icon_name("network-server-symbolic"))
        group.add(self.port_row)
        
        # Hostname
        hostname_row = Adw.ActionRow()
        hostname_row.set_title("Hostname")
        hostname_row.set_subtitle(socket.gethostname())
        hostname_row.add_prefix(Gtk.Image.new_from_icon_name("computer-symbolic"))
        group.add(hostname_row)
        
        return group
    
    def create_stats_section(self):
        """Create statistics section"""
        group = Adw.PreferencesGroup()
        group.set_title(_('section_advanced'))
        group.set_description(_('latency'))
        
        # Stats grid in a flow box for responsive layout
        stats_box = Gtk.FlowBox()
        stats_box.set_selection_mode(Gtk.SelectionMode.NONE)
        stats_box.set_homogeneous(True)
        stats_box.set_min_children_per_line(2)
        stats_box.set_max_children_per_line(4)
        stats_box.set_row_spacing(12)
        stats_box.set_column_spacing(12)
        stats_box.set_margin_top(8)
        stats_box.set_margin_bottom(8)
        
        # FPS stat
        self.fps_stat = self.create_stat_card("0", "FPS", "video-display-symbolic")
        stats_box.append(self.fps_stat)
        
        # Bitrate stat
        self.bitrate_stat = self.create_stat_card("0", "Mbps", "network-transmit-symbolic")
        stats_box.append(self.bitrate_stat)
        
        # Clients stat
        self.clients_stat = self.create_stat_card("0", _('connected_clients'), "phone-symbolic")
        stats_box.append(self.clients_stat)
        
        # Latency stat
        self.latency_stat = self.create_stat_card("--", "ms", "preferences-system-time-symbolic")
        stats_box.append(self.latency_stat)
        
        # Add to group via action row
        row = Adw.ActionRow()
        row.set_activatable(False)
        row.set_child(stats_box)
        group.add(row)
        
        return group
    
    def create_stat_card(self, value, label, icon_name):
        """Create a single stat card widget"""
        card = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        card.add_css_class("card")
        card.set_size_request(120, 100)
        card.set_halign(Gtk.Align.CENTER)
        card.set_valign(Gtk.Align.CENTER)
        
        inner = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        inner.set_margin_start(16)
        inner.set_margin_end(16)
        inner.set_margin_top(12)
        inner.set_margin_bottom(12)
        inner.set_halign(Gtk.Align.CENTER)
        
        # Icon
        icon = Gtk.Image.new_from_icon_name(icon_name)
        icon.set_pixel_size(24)
        icon.add_css_class("dim-label")
        inner.append(icon)
        
        # Value
        value_label = Gtk.Label(label=value)
        value_label.add_css_class("title-1")
        value_label.set_name(f"stat-value")
        inner.append(value_label)
        
        # Label
        name_label = Gtk.Label(label=label)
        name_label.add_css_class("dim-label")
        name_label.add_css_class("caption")
        inner.append(name_label)
        
        card.append(inner)
        
        # Store value label reference for updates
        card.value_label = value_label
        
        return card
    
    def create_usb_section(self):
        """Create USB connection section"""
        group = Adw.PreferencesGroup()
        group.set_title(_('connection_usb'))
        group.set_description(_('usb_boost_desc'))
        
        # USB Status row
        self.usb_status_row = Adw.ActionRow()
        self.usb_status_row.set_title(_('connection_usb'))
        self.usb_status_row.set_subtitle(_('usb_disconnected'))
        
        self.usb_status_icon = Gtk.Image.new_from_icon_name("phone-symbolic")
        self.usb_status_row.add_prefix(self.usb_status_icon)
        
        # USB indicator dot
        self.usb_dot = Gtk.Box()
        self.usb_dot.set_size_request(10, 10)
        self.usb_dot.add_css_class("status-dot")
        self.usb_dot.add_css_class("status-stopped")
        self.usb_status_row.add_suffix(self.usb_dot)
        
        group.add(self.usb_status_row)
        
        # Port forwarding row
        self.usb_forward_row = Adw.SwitchRow()
        self.usb_forward_row.set_title(_('usb_port_forward'))
        self.usb_forward_row.set_subtitle(_('usb_port_forward_desc'))
        self.usb_forward_row.set_active(False)
        self.usb_forward_row.set_sensitive(False)  # Enabled when device connected
        self.usb_forward_row.add_prefix(Gtk.Image.new_from_icon_name("network-transmit-symbolic"))
        self.usb_forward_row.connect("notify::active", self._on_usb_forward_toggled)
        group.add(self.usb_forward_row)
        
        # USB Quality Boost row
        self.usb_boost_row = Adw.SwitchRow()
        self.usb_boost_row.set_title(_('usb_boost'))
        self.usb_boost_row.set_subtitle(_('usb_boost_subtitle'))
        self.usb_boost_row.set_active(self.config.get('usb_boost_enabled'))
        self.usb_boost_row.add_prefix(Gtk.Image.new_from_icon_name("applications-multimedia-symbolic"))
        self.usb_boost_row.connect("notify::active", self._on_usb_boost_toggled)
        group.add(self.usb_boost_row)
        
        # Info about USB connection
        info_row = Adw.ActionRow()
        info_row.set_title(_('usb_benefits'))
        info_row.set_subtitle(_('usb_benefits_desc'))
        info_row.add_prefix(Gtk.Image.new_from_icon_name("dialog-information-symbolic"))
        group.add(info_row)
        
        # Start USB monitoring
        if self.usb_manager:
            self.usb_manager.start_device_monitor()
            GLib.timeout_add(500, self._update_usb_status_initial)
        
        return group
    
    def _on_usb_boost_toggled(self, row, param):
        """Handle USB boost toggle"""
        self.config.set('usb_boost_enabled', row.get_active())
        state = _('enabled') if row.get_active() else _('disabled')
        toast = Adw.Toast.new(f"{_('usb_quality')} {state}")
        toast.set_timeout(2)
        self.toast_overlay.add_toast(toast)
    
    def _update_usb_status_initial(self):
        """Initial USB status check"""
        if self.usb_manager:
            self._update_usb_ui()
        return False  # Don't repeat
    
    def _update_usb_ui(self):
        """Update USB UI based on current state"""
        if not self.usb_manager or not hasattr(self, 'usb_status_row'):
            return
        
        state = self.usb_manager.state
        devices = self.usb_manager.connected_devices
        is_forwarding = self.usb_manager.is_forwarding
        
        # Update status based on state
        if devices:
            device = list(devices.values())[0]
            device_name = device.model or device.serial
            
            if is_forwarding:
                self.usb_status_row.set_title(_('usb_connected'))
                self.usb_status_row.set_subtitle(f"{device_name} • {_('usb_port_forward_active')}")
                self.usb_dot.remove_css_class("status-stopped")
                self.usb_dot.add_css_class("status-running")
                self.usb_status_icon.set_from_icon_name("phone-symbolic")
            else:
                self.usb_status_row.set_title(_('usb_device_detected'))
                self.usb_status_row.set_subtitle(f"{device_name} • {_('usb_activate_forward')}")
                self.usb_dot.remove_css_class("status-stopped")
                self.usb_dot.add_css_class("status-warning")
                
            self.usb_forward_row.set_sensitive(True)
            self.usb_forward_row.set_active(is_forwarding)
        else:
            self.usb_status_row.set_title(_('usb_no_devices'))
            self.usb_status_row.set_subtitle(_('usb_connect_android'))
            self.usb_dot.remove_css_class("status-running")
            self.usb_dot.remove_css_class("status-warning")
            self.usb_dot.add_css_class("status-stopped")
            self.usb_forward_row.set_sensitive(False)
            self.usb_forward_row.set_active(False)
    
    def _on_usb_forward_toggled(self, row, param):
        """Handle USB port forward toggle"""
        if not self.usb_manager:
            return
        
        if row.get_active():
            # Get the first connected device serial (if any)
            devices = self.usb_manager.connected_devices
            device_serial = list(devices.keys())[0] if devices else None
            
            success = self.usb_manager.start_port_forwarding(device_serial)
            if success:
                toast = Adw.Toast.new(_('usb_forward_enabled'))
                toast.set_timeout(3)
                self.toast_overlay.add_toast(toast)
            else:
                row.set_active(False)
                toast = Adw.Toast.new(_('usb_forward_error'))
                toast.set_timeout(3)
                self.toast_overlay.add_toast(toast)
        else:
            self.usb_manager.stop_port_forwarding()
            toast = Adw.Toast.new(_('usb_forward_disabled'))
            toast.set_timeout(2)
            self.toast_overlay.add_toast(toast)
        
        self._update_usb_ui()
    
    def _on_usb_device_connected(self, device):
        """Callback when USB device is connected"""
        GLib.idle_add(self._handle_usb_connected, device)
    
    def _handle_usb_connected(self, device):
        """Handle USB device connection on main thread"""
        device_name = device.model or device.serial
        toast = Adw.Toast.new(f"📱 {_('usb_android_connected')}: {device_name}")
        toast.set_timeout(3)
        toast.set_button_label(_('usb_activate'))
        toast.connect("button-clicked", lambda t: self._activate_usb_forwarding())
        self.toast_overlay.add_toast(toast)
        self._update_usb_ui()
        return False
    
    def _activate_usb_forwarding(self):
        """Activate USB port forwarding"""
        if hasattr(self, 'usb_forward_row'):
            self.usb_forward_row.set_active(True)
    
    def _on_usb_device_disconnected(self, serial):
        """Callback when USB device is disconnected"""
        GLib.idle_add(self._handle_usb_disconnected, serial)
    
    def _handle_usb_disconnected(self, serial):
        """Handle USB device disconnection on main thread"""
        toast = Adw.Toast.new(_('usb_android_disconnected'))
        toast.set_timeout(3)
        self.toast_overlay.add_toast(toast)
        self._update_usb_ui()
        return False
    
    def _on_usb_state_changed(self, new_state):
        """Callback when USB state changes"""
        GLib.idle_add(self._update_usb_ui)

    # === Security Callbacks ===
    
    def _on_connection_request(self, pending):
        """Called when a new device tries to connect and needs authorization"""
        GLib.idle_add(self._show_auth_dialog, pending)
    
    def _show_auth_dialog(self, pending):
        """Show authorization dialog for pending connection"""
        dialog = Adw.AlertDialog()
        dialog.set_heading(_('security_connection_request'))
        dialog.set_body(
            f"{_('security_device_wants_connect')}\n\n"
            f"📱 {_('device')}: {pending.device_name}\n"
            f"🔑 PIN: {pending.pin}\n\n"
            f"{_('security_enter_pin_on_device')}"
        )
        
        dialog.add_response("reject", _('btn_reject'))
        dialog.add_response("approve", _('btn_approve'))
        dialog.set_response_appearance("reject", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_response_appearance("approve", Adw.ResponseAppearance.SUGGESTED)
        dialog.set_default_response("approve")
        dialog.set_close_response("reject")
        
        # Store pending info for callback
        dialog._pending = pending
        dialog.connect("response", self._on_auth_dialog_response)
        dialog.present(self)
        
        # Also show a toast with the PIN
        toast = Adw.Toast.new(f"🔐 PIN: {pending.pin}")
        toast.set_timeout(30)
        self.toast_overlay.add_toast(toast)
        
        return False
    
    def _on_auth_dialog_response(self, dialog, response):
        """Handle authorization dialog response"""
        pending = dialog._pending
        if response == "approve":
            if self.security_manager:
                self.security_manager.approve_pending_connection(pending.connection_id)
        else:
            if self.security_manager:
                self.security_manager.reject_pending_connection(pending.connection_id)
    
    def _on_connection_authorized(self, device_id, device_name):
        """Called when a device is authorized"""
        GLib.idle_add(self._handle_connection_authorized, device_id, device_name)
    
    def _handle_connection_authorized(self, device_id, device_name):
        """Handle authorized connection on main thread"""
        toast = Adw.Toast.new(f"✅ {_('security_device_authorized')}: {device_name}")
        toast.set_timeout(5)
        self.toast_overlay.add_toast(toast)
        return False
    
    def _on_connection_rejected(self, device_id, reason):
        """Called when a connection is rejected"""
        GLib.idle_add(self._handle_connection_rejected, device_id, reason)
    
    def _handle_connection_rejected(self, device_id, reason):
        """Handle rejected connection on main thread"""
        toast = Adw.Toast.new(f"❌ {_('security_connection_rejected')}: {reason}")
        toast.set_timeout(5)
        self.toast_overlay.add_toast(toast)
        return False

    def _get_machine_id(self) -> str:
        """Get unique machine identifier"""
        try:
            # Try to read machine-id
            with open('/etc/machine-id', 'r') as f:
                return f.read().strip()[:16]
        except:
            # Fallback to hostname hash
            return hashlib.md5(socket.gethostname().encode()).hexdigest()[:16]
    
    def _generate_session_token(self) -> str:
        """Generate unique session token for this machine"""
        machine_id = self._get_machine_id()
        timestamp = int(time.time() // 60)  # Changes every minute
        token_data = f"{machine_id}-{timestamp}"
        return hashlib.sha256(token_data.encode()).hexdigest()[:12]
    
    def update_qr_code(self):
        """Generate and display QR code with secure token"""
        if not HAS_QRCODE:
            return
        
        # Use security manager for secure tokens if available
        if HAS_SECURITY and self.security_manager:
            connection_info = self.security_manager.generate_connection_info(
                host_ip=self.local_ip,
                port=self.config.get('port'),
                hostname=socket.gethostname()
            )
            data = json.dumps(connection_info)
            session_token = connection_info.get('token', '')[:12]
        else:
            # Fallback to old method (less secure)
            session_token = self._generate_session_token()
            data = json.dumps({
                "version": 1,
                "name": socket.gethostname(),
                "address": self.local_ip,
                "port": self.config.get('port'),
                "machine_id": self._get_machine_id(),
                "token": session_token,
                "timestamp": int(time.time())
            })
        
        # Generate QR code
        qr = qrcode.QRCode(
            version=1,
            error_correction=qrcode.constants.ERROR_CORRECT_L,
            box_size=8,
            border=2,
        )
        qr.add_data(data)
        qr.make(fit=True)
        
        # Create image
        img = qr.make_image(fill_color="#6C63FF", back_color="white")
        
        # Convert to GdkPixbuf
        buffer = BytesIO()
        img.save(buffer, format='PNG')
        buffer.seek(0)
        
        loader = GdkPixbuf.PixbufLoader.new_with_type('png')
        loader.write(buffer.read())
        loader.close()
        pixbuf = loader.get_pixbuf()
        
        # Set to image widget
        try:
            texture = Gdk.Texture.new_for_pixbuf(pixbuf)
        except AttributeError:
            texture = Gdk.Texture.new_for_pixbuf(pixbuf)
        self.qr_image.set_paintable(texture)
        
        print(f"🔒 QR updated with secure token: {session_token}...")
    
    def _start_qr_refresh_timer(self):
        """Start timer to refresh QR code every minute"""
        if hasattr(self, '_qr_timer_id') and self._qr_timer_id:
            GLib.source_remove(self._qr_timer_id)
        self._qr_timer_id = GLib.timeout_add_seconds(60, self._on_qr_refresh_timer)
    
    def _on_qr_refresh_timer(self) -> bool:
        """Timer callback to refresh QR code"""
        self.update_qr_code()
        return True  # Continue timer
    
    def on_toggle_streaming(self, button):
        """Toggle streaming on/off"""
        if self.stream_state == StreamState.STOPPED:
            self.start_streaming()
        else:
            self.stop_streaming()
    
    def start_signaling_server(self):
        """Start the signaling server if not running"""
        # Find the signaling server binary - check multiple locations
        possible_paths = [
            Path("/usr/lib64/signaling-server/signaling-server"),  # Fedora 64-bit RPM
            Path("/usr/lib/signaling-server/signaling-server"),    # Other RPM distros
            Path("/usr/local/lib/signaling-server/signaling-server"),  # Manual install
            Path(__file__).parent.parent / "signaling-server" / "signaling-server",  # Dev
            Path(__file__).parent / "signaling-server",  # Same dir
            Path("/usr/share/streamlinux/signaling-server"),  # Alternative install
        ]
        
        server_path = None
        for path in possible_paths:
            if path.exists():
                server_path = path
                break
        
        port = self.config.get('port')
        
        if server_path is None:
            print(f"Signaling server not found in any of: {possible_paths}")
            return
            
        # Check if already running on port
        try:
            import socket
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            result = s.connect_ex(('127.0.0.1', port))
            s.close()
            
            if result == 0:
                print("Signaling server already running")
                return
        except:
            pass
            
        # Start the server
        try:
            self.signaling_process = subprocess.Popen(
                [str(server_path), '-port', str(port)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                start_new_session=True
            )
            print(f"Started signaling server on port {port}")
        except Exception as e:
            print(f"Failed to start signaling server: {e}")
    
    def start_streaming(self):
        """Start the streaming service with real WebRTC using persistent config"""
        if not HAS_WEBRTC:
            toast = Adw.Toast.new(_('error_webrtc_unavailable'))
            toast.set_timeout(3)
            self.toast_overlay.add_toast(toast)
            return
        
        # Safety check: ensure clean state before starting
        if self.streamer is not None:
            print("Warning: Streamer already exists, cleaning up first...")
            try:
                self.streamer.stop()
            except Exception:
                pass
            self.streamer = None
        
        # Reset portal singleton to ensure fresh capture session
        try:
            from portal_screencast import reset_portal
            reset_portal()
        except ImportError:
            pass
        except Exception as e:
            print(f"Warning: Could not reset portal: {e}")
            
        self.stream_state = StreamState.STARTING
        self.start_time = datetime.now()
        
        # Update UI to starting state
        self.update_status_ui()
        
        # Get settings from config
        quality = self.config.get('video_quality')
        fps = self.config.get('video_fps')
        bitrate = self.config.get('video_bitrate')
        audio_enabled = self.config.get('audio_enabled')
        audio_source = self.config.get('audio_source')
        audio_bitrate = self.config.get('audio_bitrate')
        encoder = self.config.get('video_encoder')
        capture_method = self.config.get('capture_method')
        
        # USB quality boost: if USB is connected and boost is enabled
        usb_active = False
        if HAS_USB and self.usb_manager and self.usb_manager.is_forwarding:
            usb_active = True
            if self.config.get('usb_boost_enabled'):
                multiplier = self.config.get('usb_bitrate_multiplier')
                bitrate = int(bitrate * multiplier)  # Double bitrate for USB
                audio_bitrate = min(int(audio_bitrate * 1.5), 510)  # Boost audio too, max 510kbps
                print(f"🚀 USB boost enabled: bitrate={bitrate}Mbps, audio={audio_bitrate}kbps")
                
                toast = Adw.Toast.new(f"⚡ USB Mode: {_('usb_quality_improved')} ({bitrate}Mbps)")
                toast.set_timeout(3)
                self.toast_overlay.add_toast(toast)
        
        # Calculate resolution
        if quality == 'auto' or quality == '1080p':
            width, height = 1920, 1080
        elif quality == '720p':
            width, height = 1280, 720
        elif quality == '480p':
            width, height = 854, 480
        else:
            width, height = 1920, 1080
        
        # Determine hw encoding
        hw_encoding = encoder in ['auto', 'vaapi', 'nvenc']
        
        # Get capture settings
        capture_screen = self.config.get('capture_screen')
        
        # Create streamer config
        config = StreamConfig(
            width=width,
            height=height,
            fps=fps,
            bitrate=bitrate * 1000,  # Convert to kbps
            audio=audio_enabled,
            audio_source=audio_source,
            audio_bitrate=audio_bitrate,
            hw_encoding=hw_encoding,
            capture_method=capture_method,
            capture_screen=capture_screen
        )
        
        # Log if verbose
        if self.config.get('verbose_logging'):
            usb_label = " [USB BOOST]" if usb_active else ""
            print(f"Starting stream with config: {width}x{height}@{fps}fps, {bitrate}Mbps, audio={audio_enabled} ({audio_source}), hw={hw_encoding}, screen={capture_screen}{usb_label}")
        
        # Create streamer
        self.streamer = WebRTCStreamer(config)
        
        # Check if we're on Wayland and need to setup portal first
        from webrtc_streamer import is_wayland
        if is_wayland():
            print("Wayland session detected - initializing screen capture portal...")
            toast = Adw.Toast.new(_('wayland_select_screen'))
            toast.set_timeout(5)
            self.toast_overlay.add_toast(toast)
            
            # Setup Wayland capture - this will show a dialog
            if not self.streamer.setup_wayland_capture():
                toast = Adw.Toast.new(_('error_screen_capture'))
                toast.set_timeout(5)
                self.toast_overlay.add_toast(toast)
                self.stream_state = StreamState.ERROR
                self.update_status_ui()
                return
            
            print("✓ Wayland capture initialized successfully")
        
        # Set up callbacks
        self.streamer.on_state_changed = self._on_streamer_state_changed
        self.streamer.on_stats_update = self._on_stats_update
        self.streamer.on_client_connected = self._on_client_connected
        self.streamer.on_client_disconnected = self._on_client_disconnected
        self.streamer.on_error = self._on_streamer_error
        
        # Connect to signaling server
        port = self.config.get('port')
        signaling_url = f"ws://{self.local_ip}:{port}/ws/signaling"
        self.streamer.start(signaling_url)
        
        # Show toast
        toast = Adw.Toast.new(_('connecting'))
        toast.set_timeout(2)
        self.toast_overlay.add_toast(toast)
        
    def _on_streamer_state_changed(self, state: 'StreamerState'):
        """Handle streamer state changes"""
        GLib.idle_add(self._update_from_streamer_state, state)
        
    def _update_from_streamer_state(self, state: 'StreamerState'):
        """Update UI from streamer state (in main thread)"""
        if state == StreamerState.STOPPED:
            self.stream_state = StreamState.STOPPED
        elif state == StreamerState.CONNECTING:
            self.stream_state = StreamState.STARTING
        elif state == StreamerState.WAITING:
            self.stream_state = StreamState.STREAMING
            toast = Adw.Toast.new(_('waiting_connections'))
            toast.set_timeout(2)
            self.toast_overlay.add_toast(toast)
        elif state == StreamerState.STREAMING:
            self.stream_state = StreamState.STREAMING
            toast = Adw.Toast.new(_('streaming'))
            toast.set_timeout(2)
            self.toast_overlay.add_toast(toast)
        elif state == StreamerState.ERROR:
            self.stream_state = StreamState.ERROR
            
        self.update_status_ui()
        
    def _on_stats_update(self, stats: dict):
        """Handle stats update"""
        GLib.idle_add(self._apply_stats, stats)
        
    def _apply_stats(self, stats: dict):
        """Apply stats to UI (in main thread)"""
        # FPS
        fps = stats.get('fps', 0)
        self.fps_stat.value_label.set_label(str(fps))
        
        # Bitrate (in kbps)
        bitrate = stats.get('bitrate', 0)
        if bitrate >= 1000:
            self.bitrate_stat.value_label.set_label(f"{bitrate/1000:.1f} Mbps")
        else:
            self.bitrate_stat.value_label.set_label(f"{bitrate} kbps")
        
        # Connected clients
        clients = stats.get('connected_clients', 0)
        self.clients_stat.value_label.set_label(str(clients))
        
        # Latency
        latency = stats.get('latency_ms', 0)
        if latency > 0:
            self.latency_stat.value_label.set_label(f"{latency} ms")
        else:
            self.latency_stat.value_label.set_label("--")
        
        # Update stats in status bar if streaming
        if self.stream_state == StreamState.STREAMING:
            resolution = stats.get('resolution', '')
            audio = "🔊" if stats.get('audio_enabled', False) else "🔇"
            encoding = stats.get('encoding', 'VP8')
            
            # Format bytes sent
            bytes_sent = stats.get('bytes_sent', 0)
            if bytes_sent >= 1024 * 1024 * 1024:
                data_str = f"{bytes_sent / (1024*1024*1024):.1f} GB"
            elif bytes_sent >= 1024 * 1024:
                data_str = f"{bytes_sent / (1024*1024):.1f} MB"
            elif bytes_sent >= 1024:
                data_str = f"{bytes_sent / 1024:.1f} KB"
            else:
                data_str = f"{bytes_sent} B"
            
            # Show stream time if available
            stream_time = stats.get('stream_time', 0)
            if stream_time > 0:
                minutes = stream_time // 60
                seconds = stream_time % 60
                time_str = f"{minutes:02d}:{seconds:02d}"
            else:
                time_str = ""
            
            # Update window subtitle with live stats
            subtitle_parts = [resolution, f"{encoding}", audio]
            if data_str != "0 B":
                subtitle_parts.append(f"📤 {data_str}")
            if time_str:
                subtitle_parts.append(f"⏱️ {time_str}")
            
            # self.set_subtitle(" | ".join(subtitle_parts))
        
    def _on_client_connected(self, peer_id: str):
        """Handle client connected"""
        GLib.idle_add(self._show_client_connected, peer_id)
        
    def _show_client_connected(self, peer_id: str):
        """Show client connected notification"""
        self.connected_clients.append(peer_id)
        self.clients_stat.value_label.set_label(str(len(self.connected_clients)))
        
        toast = Adw.Toast.new(_('client_connected'))
        toast.set_timeout(3)
        self.toast_overlay.add_toast(toast)
        
    def _on_client_disconnected(self, peer_id: str):
        """Handle client disconnected"""
        GLib.idle_add(self._show_client_disconnected, peer_id)
        
    def _show_client_disconnected(self, peer_id: str):
        """Show client disconnected notification"""
        if peer_id in self.connected_clients:
            self.connected_clients.remove(peer_id)
        self.clients_stat.value_label.set_label(str(len(self.connected_clients)))
        
        toast = Adw.Toast.new(_('client_disconnected'))
        toast.set_timeout(2)
        self.toast_overlay.add_toast(toast)
        
    def _on_streamer_error(self, error: str):
        """Handle streamer error"""
        GLib.idle_add(self._show_error, error)
        
    def _show_error(self, error: str):
        """Show error notification"""
        self.stream_state = StreamState.ERROR
        self.update_status_ui()
        
        toast = Adw.Toast.new(f"{_('error')}: {error[:50]}...")
        toast.set_timeout(5)
        self.toast_overlay.add_toast(toast)
        
    def stop_streaming(self):
        """Stop the streaming service and cleanup all resources"""
        print("Stopping streaming service...")
        
        # Stop WebRTC streamer (this will also stop portal capture)
        if self.streamer:
            try:
                self.streamer.stop()
            except Exception as e:
                print(f"Warning: Error stopping streamer: {e}")
            finally:
                self.streamer = None
        
        # Reset portal singleton if available (ensures fresh state for next start)
        try:
            from portal_screencast import reset_portal
            reset_portal()
        except ImportError:
            pass
        except Exception as e:
            print(f"Warning: Error resetting portal: {e}")
        
        # Reset state
        self.stream_state = StreamState.STOPPED
        self.start_time = None
        self.connected_clients.clear()
        
        # Reset stats display
        self.fps_stat.value_label.set_label("0")
        self.bitrate_stat.value_label.set_label("0")
        self.latency_stat.value_label.set_label("--")
        self.clients_stat.value_label.set_label("0")
        
        # Update UI
        self.update_status_ui()
        
        print("✓ Streaming stopped and resources cleaned up")
        
        # Show toast
        toast = Adw.Toast.new(_('streaming_stopped'))
        toast.set_timeout(2)
        self.toast_overlay.add_toast(toast)
    
    def update_status_ui(self):
        """Update the status section UI"""
        # Remove old classes
        self.status_dot.remove_css_class("status-stopped")
        self.status_dot.remove_css_class("status-starting")
        self.status_dot.remove_css_class("status-streaming")
        
        self.toggle_btn.remove_css_class("suggested-action")
        self.toggle_btn.remove_css_class("destructive-action")
        
        if self.stream_state == StreamState.STOPPED:
            self.status_dot.add_css_class("status-stopped")
            self.toggle_icon.set_from_icon_name("media-playback-start-symbolic")
            self.toggle_label.set_label(_('start'))
            self.toggle_btn.add_css_class("suggested-action")
            self.toggle_btn.set_sensitive(True)
            self.uptime_row.set_visible(False)
            
        elif self.stream_state == StreamState.STARTING:
            self.status_dot.add_css_class("status-starting")
            self.toggle_icon.set_from_icon_name("content-loading-symbolic")
            self.toggle_label.set_label(_('starting'))
            self.toggle_btn.set_sensitive(False)
            
        elif self.stream_state == StreamState.STREAMING:
            self.status_dot.add_css_class("status-streaming")
            self.toggle_icon.set_from_icon_name("media-playback-stop-symbolic")
            self.toggle_label.set_label(_('stop'))
            self.toggle_btn.add_css_class("destructive-action")
            self.toggle_btn.set_sensitive(True)
            self.uptime_row.set_visible(True)
    
    def update_ui(self):
        """Periodic UI update"""
        if self.stream_state == StreamState.STREAMING:
            # Update uptime
            if self.start_time:
                elapsed = datetime.now() - self.start_time
                hours, remainder = divmod(int(elapsed.total_seconds()), 3600)
                minutes, seconds = divmod(remainder, 60)
                self.uptime_row.set_subtitle(f"{hours:02d}:{minutes:02d}:{seconds:02d}")
            
            # Stats are updated via streamer callbacks
        else:
            self.fps_stat.value_label.set_label("0")
            self.bitrate_stat.value_label.set_label("0")
            self.latency_stat.value_label.set_label("--")
            self.clients_stat.value_label.set_label("0")
        
        return True
    
    def copy_to_clipboard(self, text):
        """Copy text to clipboard"""
        clipboard = Gdk.Display.get_default().get_clipboard()
        clipboard.set(text)
        
        toast = Adw.Toast.new(_('success'))
        toast.set_timeout(2)
        self.toast_overlay.add_toast(toast)
    
    def show_preferences(self):
        """Show preferences dialog"""
        dialog = PreferencesDialog(transient_for=self)
        dialog.connect("close-request", self._on_preferences_closed)
        dialog.present()
    
    def _on_preferences_closed(self, dialog):
        """Refresh UI when preferences are closed"""
        # Refresh quick settings to match any changes in preferences
        self._refresh_quick_settings()
        # Update QR code in case port changed
        self.update_qr_code()
        # Update port display
        self.port_row.set_subtitle(str(self.config.get('port')))
        return False
    
    def _refresh_quick_settings(self):
        """Refresh quick settings UI from config"""
        # Quality
        quality_val = self.config.get('video_quality')
        quality_idx = {'auto': 0, '1080p': 1, '720p': 2, '480p': 3}.get(quality_val, 0)
        self.quality_row.set_selected(quality_idx)
        
        # FPS
        fps_val = self.config.get('video_fps')
        fps_idx = {60: 0, 30: 1, 24: 2}.get(fps_val, 0)
        self.fps_row.set_selected(fps_idx)
        
        # Audio
        self.audio_row.set_active(self.config.get('audio_enabled'))
        
        # Hardware encoding
        encoder = self.config.get('video_encoder')
        self.hw_row.set_active(encoder in ['auto', 'vaapi', 'nvenc'])
    
    def show_about(self):
        """Show about dialog"""
        about = Adw.AboutWindow(
            transient_for=self,
            application_name="StreamLinux",
            application_icon="video-display",
            developer_name="Vanguardia Studio",
            version="0.1.0-alpha",
            copyright="© 2024 Vanguardia Studio",
            license_type=Gtk.License.MIT_X11,
            website="https://vanguardiastudio.us/",
            issue_url="https://github.com/MrVanguardia/streamlinux/issues",
            comments=_('about_description')
        )
        about.add_credit_section(_('about_technologies'), ["GTK4", "libadwaita", "WebRTC", "GStreamer", "PipeWire"])
        about.add_credit_section(_('about_developer'), ["MrVanguardia - Vanguardia Studio"])
        about.add_link(_('about_website'), "https://vanguardiastudio.us/")
        about.add_link(_('about_github'), "https://github.com/MrVanguardia/streamlinux")
        about.present()


class PreferencesDialog(Adw.PreferencesWindow):
    """Preferences dialog with organized sections and persistent settings"""
    
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        
        self.set_title(_('preferences'))
        self.set_default_size(600, 500)
        self.set_modal(True)
        
        # Get config instance
        self.config = AppConfig()
        
        # General page (with language selector)
        self.add(self.create_general_page())
        
        # Video page
        self.add(self.create_video_page())
        
        # Audio page
        self.add(self.create_audio_page())
        
        # Network page
        self.add(self.create_network_page())
        
        # Advanced page
        self.add(self.create_advanced_page())
    
    def create_general_page(self):
        """Create general settings page with language selector"""
        page = Adw.PreferencesPage()
        page.set_title(_('pref_general'))
        page.set_icon_name("preferences-system-symbolic")
        
        # Language group
        lang_group = Adw.PreferencesGroup()
        lang_group.set_title(_('language'))
        lang_group.set_description(_('language_desc'))
        page.add(lang_group)
        
        # Language selector
        self.language_row = Adw.ComboRow()
        self.language_row.set_title(_('language'))
        self.language_row.set_subtitle(_('language_subtitle'))
        self.language_row.set_model(Gtk.StringList.new([
            _('language_system'),
            "English",
            "Español"
        ]))
        
        # Get current language setting
        lang_val = self.config.get('language')
        lang_idx = {'system': 0, 'en': 1, 'es': 2}.get(lang_val, 0)
        self.language_row.set_selected(lang_idx)
        self.language_row.connect("notify::selected", self._on_language_changed)
        lang_group.add(self.language_row)
        
        # Note about restart
        restart_label = Gtk.Label(label=_('language_restart'))
        restart_label.add_css_class("dim-label")
        restart_label.add_css_class("caption")
        restart_label.set_margin_top(8)
        
        restart_row = Adw.ActionRow()
        restart_row.set_activatable(False)
        restart_row.set_child(restart_label)
        lang_group.add(restart_row)
        
        return page
    
    def _on_language_changed(self, row, param):
        """Handle language selection change"""
        idx = row.get_selected()
        lang_map = {0: 'system', 1: 'en', 2: 'es'}
        lang = lang_map.get(idx, 'system')
        self.config.set('language', lang)
        
        # Apply language immediately
        from i18n import set_language
        set_language(lang)
    
    def create_video_page(self):
        """Create video settings page"""
        page = Adw.PreferencesPage()
        page.set_title(_('pref_video'))
        page.set_icon_name("video-display-symbolic")
        
        # Encoder group
        encoder_group = Adw.PreferencesGroup()
        encoder_group.set_title(_('pref_encoding'))
        encoder_group.set_description(_('pref_encoding_desc'))
        page.add(encoder_group)
        
        # Encoder selection
        self.encoder_row = Adw.ComboRow()
        self.encoder_row.set_title(_('pref_encoder'))
        self.encoder_row.set_subtitle(_('pref_encoder_desc'))
        self.encoder_row.set_model(Gtk.StringList.new([
            _('encoder_auto'),
            _('encoder_vaapi'),
            _('encoder_nvenc'),
            _('encoder_software')
        ]))
        encoder_val = self.config.get('video_encoder')
        encoder_idx = {'auto': 0, 'vaapi': 1, 'nvenc': 2, 'software': 3}.get(encoder_val, 0)
        self.encoder_row.set_selected(encoder_idx)
        self.encoder_row.connect("notify::selected", self._on_encoder_changed)
        encoder_group.add(self.encoder_row)
        
        # Bitrate
        self.bitrate_row = Adw.SpinRow.new_with_range(1, 50, 1)
        self.bitrate_row.set_title(_('pref_bitrate'))
        self.bitrate_row.set_subtitle(_('pref_bitrate_desc'))
        self.bitrate_row.set_value(self.config.get('video_bitrate'))
        self.bitrate_row.connect("notify::value", self._on_bitrate_changed)
        encoder_group.add(self.bitrate_row)
        
        # Preset
        self.preset_row = Adw.ComboRow()
        self.preset_row.set_title(_('pref_preset'))
        self.preset_row.set_subtitle(_('pref_preset_desc'))
        self.preset_row.set_model(Gtk.StringList.new([
            _('preset_ultrafast'),
            _('preset_veryfast'),
            _('preset_fast'),
            _('preset_medium'),
            _('preset_slow')
        ]))
        preset_val = self.config.get('video_preset')
        preset_idx = {'ultrafast': 0, 'veryfast': 1, 'fast': 2, 'medium': 3, 'slow': 4}.get(preset_val, 1)
        self.preset_row.set_selected(preset_idx)
        self.preset_row.connect("notify::selected", self._on_preset_changed)
        encoder_group.add(self.preset_row)
        
        # Capture group
        capture_group = Adw.PreferencesGroup()
        capture_group.set_title(_('pref_capture'))
        capture_group.set_description(_('pref_capture_desc'))
        page.add(capture_group)
        
        # Capture method
        self.capture_row = Adw.ComboRow()
        self.capture_row.set_title(_('pref_capture_method'))
        self.capture_row.set_subtitle(_('pref_capture_method_desc'))
        self.capture_row.set_model(Gtk.StringList.new([
            _('capture_auto'),
            _('capture_pipewire'),
            _('capture_portal'),
            _('capture_x11')
        ]))
        capture_val = self.config.get('capture_method')
        capture_idx = {'auto': 0, 'pipewire': 1, 'xdg-portal': 2, 'x11': 3}.get(capture_val, 0)
        self.capture_row.set_selected(capture_idx)
        self.capture_row.connect("notify::selected", self._on_capture_changed)
        capture_group.add(self.capture_row)
        
        # Screen selection
        self.screen_row = Adw.ComboRow()
        self.screen_row.set_title(_('pref_screen'))
        self.screen_row.set_model(Gtk.StringList.new([
            _('screen_primary'),
            _('screen_all'),
            _('screen_window')
        ]))
        screen_val = self.config.get('capture_screen')
        screen_idx = max(0, min(2, screen_val + 1))  # -1 -> 0, 0 -> 1, etc
        self.screen_row.set_selected(screen_idx)
        self.screen_row.connect("notify::selected", self._on_screen_changed)
        capture_group.add(self.screen_row)
        
        return page
    
    def _on_encoder_changed(self, row, param):
        idx = row.get_selected()
        encoder_map = {0: 'auto', 1: 'vaapi', 2: 'nvenc', 3: 'software'}
        self.config.set('video_encoder', encoder_map.get(idx, 'auto'))
    
    def _on_bitrate_changed(self, row, param):
        self.config.set('video_bitrate', int(row.get_value()))
    
    def _on_preset_changed(self, row, param):
        idx = row.get_selected()
        preset_map = {0: 'ultrafast', 1: 'veryfast', 2: 'fast', 3: 'medium', 4: 'slow'}
        self.config.set('video_preset', preset_map.get(idx, 'veryfast'))
    
    def _on_capture_changed(self, row, param):
        idx = row.get_selected()
        capture_map = {0: 'auto', 1: 'pipewire', 2: 'xdg-portal', 3: 'x11'}
        self.config.set('capture_method', capture_map.get(idx, 'auto'))
    
    def _on_screen_changed(self, row, param):
        idx = row.get_selected()
        screen_map = {0: 0, 1: -1, 2: -2}  # 0=primary, -1=all, -2=window
        self.config.set('capture_screen', screen_map.get(idx, 0))
    
    def create_audio_page(self):
        """Create audio settings page"""
        page = Adw.PreferencesPage()
        page.set_title(_('pref_audio'))
        page.set_icon_name("audio-speakers-symbolic")
        
        # Capture group
        capture_group = Adw.PreferencesGroup()
        capture_group.set_title(_('pref_audio_capture'))
        page.add(capture_group)
        
        # Audio source
        self.audio_source_row = Adw.ComboRow()
        self.audio_source_row.set_title(_('pref_audio_source'))
        self.audio_source_row.set_model(Gtk.StringList.new([
            _('audio_system'),
            _('audio_microphone'),
            _('audio_both'),
            _('audio_none')
        ]))
        source_val = self.config.get('audio_source')
        source_idx = {'system': 0, 'microphone': 1, 'both': 2, 'none': 3}.get(source_val, 0)
        self.audio_source_row.set_selected(source_idx)
        self.audio_source_row.connect("notify::selected", self._on_audio_source_changed)
        capture_group.add(self.audio_source_row)
        
        # Quality group
        quality_group = Adw.PreferencesGroup()
        quality_group.set_title(_('pref_audio_quality'))
        page.add(quality_group)
        
        # Codec
        self.audio_codec_row = Adw.ComboRow()
        self.audio_codec_row.set_title(_('pref_codec'))
        self.audio_codec_row.set_model(Gtk.StringList.new([_('codec_opus'), "AAC"]))
        codec_val = self.config.get('audio_codec')
        codec_idx = {'opus': 0, 'aac': 1}.get(codec_val, 0)
        self.audio_codec_row.set_selected(codec_idx)
        self.audio_codec_row.connect("notify::selected", self._on_audio_codec_changed)
        quality_group.add(self.audio_codec_row)
        
        # Bitrate
        self.audio_bitrate_row = Adw.ComboRow()
        self.audio_bitrate_row.set_title(_('pref_quality'))
        self.audio_bitrate_row.set_model(Gtk.StringList.new([
            _('quality_high') + " (320 kbps)",
            _('quality_medium') + " (192 kbps)",
            _('quality_low') + " (128 kbps)"
        ]))
        bitrate_val = self.config.get('audio_bitrate')
        bitrate_idx = {320: 0, 192: 1, 128: 2}.get(bitrate_val, 0)
        self.audio_bitrate_row.set_selected(bitrate_idx)
        self.audio_bitrate_row.connect("notify::selected", self._on_audio_bitrate_changed)
        quality_group.add(self.audio_bitrate_row)
        
        return page
    
    def _on_audio_source_changed(self, row, param):
        idx = row.get_selected()
        source_map = {0: 'system', 1: 'microphone', 2: 'both', 3: 'none'}
        self.config.set('audio_source', source_map.get(idx, 'system'))
        # Also update audio_enabled based on selection
        self.config.set('audio_enabled', idx != 3)
    
    def _on_audio_codec_changed(self, row, param):
        idx = row.get_selected()
        codec_map = {0: 'opus', 1: 'aac'}
        self.config.set('audio_codec', codec_map.get(idx, 'opus'))
    
    def _on_audio_bitrate_changed(self, row, param):
        idx = row.get_selected()
        bitrate_map = {0: 320, 1: 192, 2: 128}
        self.config.set('audio_bitrate', bitrate_map.get(idx, 320))
    
    def create_network_page(self):
        """Create network settings page"""
        page = Adw.PreferencesPage()
        page.set_title(_('pref_network'))
        page.set_icon_name("network-wired-symbolic")
        
        # Server group
        server_group = Adw.PreferencesGroup()
        server_group.set_title(_('pref_server'))
        page.add(server_group)
        
        # Port
        self.port_row = Adw.SpinRow.new_with_range(1024, 65535, 1)
        self.port_row.set_title(_('pref_port'))
        self.port_row.set_subtitle(_('pref_port_desc'))
        self.port_row.set_value(self.config.get('port'))
        self.port_row.connect("notify::value", self._on_port_changed)
        server_group.add(self.port_row)
        
        # Note about restart
        restart_label = Gtk.Label(label=_('pref_port_restart'))
        restart_label.add_css_class("dim-label")
        restart_label.add_css_class("caption")
        restart_label.set_margin_top(8)
        
        restart_row = Adw.ActionRow()
        restart_row.set_activatable(False)
        restart_row.set_child(restart_label)
        server_group.add(restart_row)
        
        # WebRTC group
        webrtc_group = Adw.PreferencesGroup()
        webrtc_group.set_title("WebRTC")
        page.add(webrtc_group)
        
        # STUN server
        self.stun_row = Adw.EntryRow()
        self.stun_row.set_title(_('pref_stun'))
        self.stun_row.set_text(self.config.get('stun_server'))
        self.stun_row.connect("changed", self._on_stun_changed)
        webrtc_group.add(self.stun_row)
        
        # TURN server (optional)
        self.turn_row = Adw.EntryRow()
        self.turn_row.set_title(_('pref_turn'))
        self.turn_row.set_text(self.config.get('turn_server'))
        self.turn_row.connect("changed", self._on_turn_changed)
        webrtc_group.add(self.turn_row)
        
        return page
    
    def _on_port_changed(self, row, param):
        self.config.set('port', int(row.get_value()))
    
    def _on_stun_changed(self, row):
        self.config.set('stun_server', row.get_text())
    
    def _on_turn_changed(self, row):
        self.config.set('turn_server', row.get_text())
    
    def create_advanced_page(self):
        """Create advanced settings page"""
        page = Adw.PreferencesPage()
        page.set_title(_('pref_advanced'))
        page.set_icon_name("applications-system-symbolic")
        
        # Startup group
        startup_group = Adw.PreferencesGroup()
        startup_group.set_title(_('pref_startup'))
        page.add(startup_group)
        
        # Auto-start
        self.autostart_row = Adw.SwitchRow()
        self.autostart_row.set_title(_('pref_autostart'))
        self.autostart_row.set_subtitle(_('pref_autostart_desc'))
        self.autostart_row.set_active(self.config.get('autostart'))
        self.autostart_row.connect("notify::active", self._on_autostart_changed)
        startup_group.add(self.autostart_row)
        
        # Auto-stream
        self.autostream_row = Adw.SwitchRow()
        self.autostream_row.set_title(_('pref_autostream'))
        self.autostream_row.set_subtitle(_('pref_autostream_desc'))
        self.autostream_row.set_active(self.config.get('autostream'))
        self.autostream_row.connect("notify::active", self._on_autostream_changed)
        startup_group.add(self.autostream_row)
        
        # Minimize to tray
        self.tray_row = Adw.SwitchRow()
        self.tray_row.set_title(_('pref_tray'))
        self.tray_row.set_subtitle(_('pref_tray_desc'))
        self.tray_row.set_active(self.config.get('minimize_to_tray'))
        self.tray_row.connect("notify::active", self._on_tray_changed)
        startup_group.add(self.tray_row)
        
        # Debug group
        debug_group = Adw.PreferencesGroup()
        debug_group.set_title(_('pref_debug'))
        page.add(debug_group)
        
        # Verbose logging
        self.log_row = Adw.SwitchRow()
        self.log_row.set_title(_('pref_verbose'))
        self.log_row.set_subtitle(_('pref_verbose_desc'))
        self.log_row.set_active(self.config.get('verbose_logging'))
        self.log_row.connect("notify::active", self._on_log_changed)
        debug_group.add(self.log_row)
        
        # Show stats overlay
        self.stats_row = Adw.SwitchRow()
        self.stats_row.set_title(_('pref_stats_overlay'))
        self.stats_row.set_subtitle(_('pref_stats_overlay_desc'))
        self.stats_row.set_active(self.config.get('show_stats_overlay'))
        self.stats_row.connect("notify::active", self._on_stats_changed)
        debug_group.add(self.stats_row)
        
        # Reset group
        reset_group = Adw.PreferencesGroup()
        reset_group.set_title(_('pref_data'))
        page.add(reset_group)
        
        # Reset to defaults
        reset_row = Adw.ActionRow()
        reset_row.set_title(_('pref_reset'))
        reset_row.set_subtitle(_('pref_reset_desc'))
        reset_row.set_activatable(True)
        reset_row.add_prefix(Gtk.Image.new_from_icon_name("edit-clear-all-symbolic"))
        reset_row.add_suffix(Gtk.Image.new_from_icon_name("go-next-symbolic"))
        reset_row.connect("activated", self._on_reset_clicked)
        reset_group.add(reset_row)
        
        return page
    
    def _on_autostart_changed(self, row, param):
        enabled = row.get_active()
        self.config.set('autostart', enabled)
        # Actually set up autostart
        self._setup_autostart(enabled)
    
    def _setup_autostart(self, enabled: bool):
        """Set up or remove autostart desktop entry"""
        autostart_dir = Path.home() / ".config" / "autostart"
        desktop_file = autostart_dir / "com.streamlinux.host.desktop"
        
        if enabled:
            autostart_dir.mkdir(parents=True, exist_ok=True)
            content = f"""[Desktop Entry]
Type=Application
Name=StreamLinux
Comment=Stream your Linux screen to Android
Exec=python3 {Path(__file__).resolve()}
Icon=video-display
Terminal=false
Categories=Network;AudioVideo;
X-GNOME-Autostart-enabled=true
"""
            with open(desktop_file, 'w') as f:
                f.write(content)
            print(f"Created autostart entry: {desktop_file}")
        else:
            if desktop_file.exists():
                desktop_file.unlink()
                print(f"Removed autostart entry: {desktop_file}")
    
    def _on_autostream_changed(self, row, param):
        self.config.set('autostream', row.get_active())
    
    def _on_tray_changed(self, row, param):
        self.config.set('minimize_to_tray', row.get_active())
    
    def _on_log_changed(self, row, param):
        self.config.set('verbose_logging', row.get_active())
    
    def _on_stats_changed(self, row, param):
        self.config.set('show_stats_overlay', row.get_active())
    
    def _on_reset_clicked(self, row):
        """Show confirmation dialog for reset"""
        dialog = Adw.MessageDialog(
            transient_for=self,
            heading=_('reset_confirm_title'),
            body=_('reset_confirm_body')
        )
        dialog.add_response("cancel", _('cancel'))
        dialog.add_response("reset", _('reset'))
        dialog.set_response_appearance("reset", Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.connect("response", self._on_reset_response)
        dialog.present()
    
    def _on_reset_response(self, dialog, response):
        if response == "reset":
            self.config.reset_to_defaults()
            # Close and reopen preferences to refresh values
            self.close()


# Custom CSS
CSS = """
.status-dot {
    border-radius: 50%;
    min-width: 12px;
    min-height: 12px;
    margin-right: 8px;
}

.status-stopped {
    background-color: @error_color;
}

.status-starting {
    background-color: @warning_color;
}

.status-streaming {
    background-color: #4ADE80;
}

.status-running {
    background-color: #00BCD4;
}

.status-warning {
    background-color: #FFC107;
}

.title-1 {
    font-size: 24px;
    font-weight: bold;
}

.caption {
    font-size: 11px;
}
"""


def main():
    """Main entry point"""
    # Load CSS
    css_provider = Gtk.CssProvider()
    css_provider.load_from_data(CSS.encode())
    Gtk.StyleContext.add_provider_for_display(
        Gdk.Display.get_default(),
        css_provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )
    
    # Run application
    app = StreamLinuxApp()
    return app.run(sys.argv)


if __name__ == "__main__":
    sys.exit(main())
