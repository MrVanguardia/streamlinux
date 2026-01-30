#!/usr/bin/env python3
"""
USB Manager for StreamLinux
Handles ADB port forwarding for USB streaming to Android devices.
USB connection provides the lowest latency and most stable connection.
"""

import subprocess
import threading
import time
import logging
import re
from typing import Optional, Callable, List, Dict
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger('USBManager')

class USBState(Enum):
    DISCONNECTED = "disconnected"
    CONNECTED = "connected"
    FORWARDING = "forwarding"
    ERROR = "error"


@dataclass
class AndroidDevice:
    """Represents a connected Android device"""
    serial: str
    model: str = "Unknown"
    state: str = "device"
    usb_speed: str = ""
    
    @property
    def display_name(self) -> str:
        return f"{self.model} ({self.serial[:8]}...)" if len(self.serial) > 8 else f"{self.model} ({self.serial})"


class USBManager:
    """
    Manages USB connections and ADB port forwarding for Android devices.
    
    When a device is connected via USB and ADB forwarding is set up,
    the Android app can connect to localhost:54321 for the lowest latency
    streaming experience.
    """
    
    # Default port for StreamLinux signaling
    DEFAULT_PORT = 54321
    
    def __init__(self, port: int = DEFAULT_PORT):
        self.port = port
        self.state = USBState.DISCONNECTED
        self._devices: List[AndroidDevice] = []
        self._forwarding_active = False
        self._monitor_thread: Optional[threading.Thread] = None
        self._stop_monitor = False
        
        # Callbacks
        self.on_device_connected: Optional[Callable[[AndroidDevice], None]] = None
        self.on_device_disconnected: Optional[Callable[[str], None]] = None
        self.on_state_changed: Optional[Callable[[USBState], None]] = None
        self.on_forwarding_started: Optional[Callable[[], None]] = None
        self.on_forwarding_stopped: Optional[Callable[[], None]] = None
        
        # Check if ADB is available
        self._adb_available = self._check_adb()
        if not self._adb_available:
            logger.warning("ADB not found. USB streaming will not be available.")
            logger.info("Install ADB with: sudo dnf install android-tools (Fedora) or sudo apt install adb (Ubuntu)")
    
    def _check_adb(self) -> bool:
        """Check if ADB is installed and available"""
        try:
            result = subprocess.run(
                ['adb', 'version'],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                version_match = re.search(r'version (\d+\.\d+\.\d+)', result.stdout)
                if version_match:
                    logger.info(f"ADB found: version {version_match.group(1)}")
                return True
        except FileNotFoundError:
            logger.warning("ADB command not found")
        except subprocess.TimeoutExpired:
            logger.warning("ADB command timed out")
        except Exception as e:
            logger.error(f"Error checking ADB: {e}")
        return False
    
    def _set_state(self, new_state: USBState):
        """Update state and notify listeners"""
        if self.state != new_state:
            old_state = self.state
            self.state = new_state
            logger.info(f"USB state: {old_state.value} -> {new_state.value}")
            if self.on_state_changed:
                try:
                    self.on_state_changed(new_state)
                except Exception as e:
                    logger.error(f"Error in state change callback: {e}")
    
    @property
    def connected_devices(self) -> Dict[str, AndroidDevice]:
        """Get connected devices as a dictionary keyed by serial number"""
        devices = self.get_connected_devices()
        return {d.serial: d for d in devices}
    
    def get_connected_devices(self) -> List[AndroidDevice]:
        """Get list of connected Android devices via ADB"""
        devices = []
        
        if not self._adb_available:
            return devices
        
        try:
            result = subprocess.run(
                ['adb', 'devices', '-l'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            if result.returncode == 0:
                for line in result.stdout.strip().split('\n')[1:]:
                    if not line.strip():
                        continue
                    
                    parts = line.split()
                    if len(parts) >= 2 and parts[1] in ('device', 'recovery'):
                        serial = parts[0]
                        model = "Android Device"
                        usb_speed = ""
                        
                        # Parse additional info
                        for part in parts[2:]:
                            if part.startswith('model:'):
                                model = part.split(':')[1].replace('_', ' ')
                            elif part.startswith('usb:'):
                                usb_speed = part.split(':')[1]
                        
                        devices.append(AndroidDevice(
                            serial=serial,
                            model=model,
                            state=parts[1],
                            usb_speed=usb_speed
                        ))
        except Exception as e:
            logger.error(f"Error getting devices: {e}")
        
        return devices
    
    def start_port_forwarding(self, device_serial: Optional[str] = None) -> bool:
        """
        Start ADB reverse port forwarding for USB streaming.
        This makes localhost:54321 on the Android device connect to the Linux host's port.
        Using 'adb reverse' allows Android to access the host's signaling server via localhost.
        """
        if not self._adb_available:
            logger.error("ADB not available")
            return False
        
        try:
            # Build ADB command - use 'reverse' so Android can reach host via localhost
            cmd = ['adb']
            if device_serial:
                cmd.extend(['-s', str(device_serial)])
            # 'reverse' makes Android's localhost:port -> Host's localhost:port
            port_str = str(self.port)
            cmd.extend(['reverse', f'tcp:{port_str}', f'tcp:{port_str}'])
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            
            if result.returncode == 0:
                self._forwarding_active = True
                self._set_state(USBState.FORWARDING)
                logger.info(f"✓ ADB reverse started: Android localhost:{self.port} -> Host localhost:{self.port}")
                if self.on_forwarding_started:
                    self.on_forwarding_started()
                return True
            else:
                logger.error(f"Failed to start port forwarding: {result.stderr}")
                self._set_state(USBState.ERROR)
                return False
                
        except Exception as e:
            logger.error(f"Error starting port forwarding: {e}")
            self._set_state(USBState.ERROR)
            return False
    
    def stop_port_forwarding(self, device_serial: Optional[str] = None) -> bool:
        """Stop ADB reverse port forwarding"""
        if not self._adb_available:
            return False
        
        try:
            cmd = ['adb']
            if device_serial:
                cmd.extend(['-s', device_serial])
            cmd.extend(['reverse', '--remove', f'tcp:{self.port}'])
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            
            self._forwarding_active = False
            if self._devices:
                self._set_state(USBState.CONNECTED)
            else:
                self._set_state(USBState.DISCONNECTED)
            
            logger.info("ADB reverse forwarding stopped")
            if self.on_forwarding_stopped:
                self.on_forwarding_stopped()
            return True
            
        except Exception as e:
            logger.error(f"Error stopping port forwarding: {e}")
            return False
    
    def stop_all_forwarding(self) -> bool:
        """Stop all ADB reverse port forwarding"""
        if not self._adb_available:
            return False
        
        try:
            result = subprocess.run(
                ['adb', 'reverse', '--remove-all'],
                capture_output=True,
                text=True,
                timeout=5
            )
            self._forwarding_active = False
            logger.info("All ADB port forwarding stopped")
            return True
        except Exception as e:
            logger.error(f"Error stopping all forwarding: {e}")
            return False
    
    def get_forwarding_status(self) -> Dict[str, str]:
        """Get current port forwarding status"""
        status = {}
        
        if not self._adb_available:
            return status
        
        try:
            result = subprocess.run(
                ['adb', 'forward', '--list'],
                capture_output=True,
                text=True,
                timeout=5
            )
            
            if result.returncode == 0:
                for line in result.stdout.strip().split('\n'):
                    if line.strip():
                        parts = line.split()
                        if len(parts) >= 3:
                            serial = parts[0]
                            local = parts[1]
                            remote = parts[2]
                            status[serial] = f"{local} -> {remote}"
        except Exception as e:
            logger.error(f"Error getting forwarding status: {e}")
        
        return status
    
    def start_device_monitor(self):
        """Start monitoring for device connections"""
        if self._monitor_thread and self._monitor_thread.is_alive():
            return
        
        self._stop_monitor = False
        self._monitor_thread = threading.Thread(target=self._monitor_devices, daemon=True)
        self._monitor_thread.start()
        logger.info("Device monitor started")
    
    def stop_device_monitor(self):
        """Stop device monitoring and clean up resources"""
        self._stop_monitor = True
        if self._monitor_thread:
            self._monitor_thread.join(timeout=2)
        # Clean up any active forwarding
        if self._forwarding_active:
            try:
                self.stop_port_forwarding()
            except Exception as e:
                logger.error(f"Error stopping forwarding during cleanup: {e}")
        self._devices.clear()
        logger.info("Device monitor stopped and resources cleaned up")
    
    def _monitor_devices(self):
        """Background thread to monitor device connections"""
        previous_serials = set()
        
        while not self._stop_monitor:
            try:
                current_devices = self.get_connected_devices()
                current_serials = {d.serial for d in current_devices}
                
                # Check for new devices
                for device in current_devices:
                    if device.serial not in previous_serials:
                        logger.info(f"Device connected: {device.display_name}")
                        self._devices.append(device)
                        if self.state == USBState.DISCONNECTED:
                            self._set_state(USBState.CONNECTED)
                        if self.on_device_connected:
                            self.on_device_connected(device)
                        # Note: Port forwarding is NOT auto-started
                        # User must manually enable it in the GUI
                
                # Check for disconnected devices
                for serial in previous_serials:
                    if serial not in current_serials:
                        logger.info(f"Device disconnected: {serial}")
                        self._devices = [d for d in self._devices if d.serial != serial]
                        if self.on_device_disconnected:
                            self.on_device_disconnected(serial)
                        
                        if not self._devices:
                            self._forwarding_active = False
                            self._set_state(USBState.DISCONNECTED)
                
                previous_serials = current_serials
                
            except Exception as e:
                logger.error(f"Device monitor error: {e}")
            
            time.sleep(2)  # Check every 2 seconds
    
    @property
    def is_available(self) -> bool:
        """Check if USB streaming is available"""
        return self._adb_available
    
    @property
    def is_forwarding(self) -> bool:
        """Check if port forwarding is active"""
        return self._forwarding_active
    
    @property
    def devices(self) -> List[AndroidDevice]:
        """Get list of connected devices"""
        return self._devices.copy()
    
    def cleanup(self):
        """Cleanup resources"""
        self.stop_device_monitor()
        if self._forwarding_active:
            self.stop_all_forwarding()


# Singleton instance
_usb_manager: Optional[USBManager] = None

def get_usb_manager() -> USBManager:
    """Get the singleton USB manager instance"""
    global _usb_manager
    if _usb_manager is None:
        _usb_manager = USBManager()
    return _usb_manager
