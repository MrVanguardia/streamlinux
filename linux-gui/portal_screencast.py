#!/usr/bin/env python3
"""
Portal Screencast - Wayland screen capture via xdg-desktop-portal.
Creates a PipeWire stream that can be used with GStreamer pipewiresrc.
Uses GIO for D-Bus communication (no dbus-python dependency).
"""

import gi
gi.require_version('Gst', '1.0')
from gi.repository import GLib, Gio, Gst
import os
import logging
import threading

logger = logging.getLogger('PortalScreencast')


class PortalScreencast:
    """
    Handle Wayland screen capture via xdg-desktop-portal using GIO.
    This creates a PipeWire stream that can be used with pipewiresrc.
    """
    
    PORTAL_BUS = 'org.freedesktop.portal.Desktop'
    PORTAL_PATH = '/org/freedesktop/portal/desktop'
    PORTAL_SCREENCAST = 'org.freedesktop.portal.ScreenCast'
    PORTAL_REQUEST = 'org.freedesktop.portal.Request'
    
    # Source types
    MONITOR = 1
    WINDOW = 2
    VIRTUAL = 4
    
    # Cursor modes
    CURSOR_HIDDEN = 1
    CURSOR_EMBEDDED = 2
    CURSOR_METADATA = 4
    
    def __init__(self):
        self.session_path = None
        self.pipewire_fd = None
        self.pipewire_node_id = None
        self._token_counter = 0
        self._error = None
        self._done = threading.Event()
        self._bus = None
        self._portal = None
        self._response_data = None
        self._source_type = self.MONITOR | self.WINDOW
        self._cursor_mode = self.CURSOR_EMBEDDED
        
    def _get_token(self):
        self._token_counter += 1
        return f"streamlinux_{os.getpid()}_{self._token_counter}"
    
    def _get_sender_name(self):
        """Get unique sender name for D-Bus (without the leading colon and with dots replaced)"""
        if self._bus:
            name = self._bus.get_unique_name()
            # Remove leading ':' and replace '.' with '_'
            return name[1:].replace('.', '_')
        return f"pid{os.getpid()}"
    
    def start_capture(self, source_type=None, cursor_mode=None) -> bool:
        """
        Start screen capture. Shows portal dialog for user to select what to share.
        Returns True if successful, False otherwise.
        After success, pipewire_node_id will be set.
        """
        if source_type is not None:
            self._source_type = source_type
        if cursor_mode is not None:
            self._cursor_mode = cursor_mode
        
        self._error = None
        
        try:
            # Connect to session bus
            self._bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
            
            # Get portal proxy
            self._portal = Gio.DBusProxy.new_sync(
                self._bus,
                Gio.DBusProxyFlags.NONE,
                None,
                self.PORTAL_BUS,
                self.PORTAL_PATH,
                self.PORTAL_SCREENCAST,
                None
            )
            
            # Step 1: Create session
            logger.info("Creating screencast session...")
            if not self._create_session():
                logger.error(f"CreateSession failed: {self._error}")
                return False
            
            # Step 2: Select sources (shows dialog)
            logger.info("Requesting source selection (dialog will appear)...")
            if not self._select_sources():
                logger.error(f"SelectSources failed: {self._error}")
                return False
            
            # Step 3: Start the stream
            logger.info("Starting PipeWire stream...")
            if not self._start_stream():
                logger.error(f"Start failed: {self._error}")
                return False
            
            logger.info(f"✓ Screencast started! PipeWire node ID: {self.pipewire_node_id}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to start screencast: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def _wait_for_response(self, request_path, timeout_ms=30000):
        """Wait for portal response signal"""
        self._done.clear()
        self._response_data = None
        
        # Subscribe to response signal
        subscription_id = self._bus.signal_subscribe(
            self.PORTAL_BUS,
            self.PORTAL_REQUEST,
            'Response',
            request_path,
            None,
            Gio.DBusSignalFlags.NO_MATCH_RULE,
            self._on_response,
            None
        )
        
        # Wait with timeout (poll GLib events)
        waited = 0
        while not self._done.is_set() and waited < timeout_ms:
            # Process pending GLib events
            context = GLib.MainContext.default()
            while context.pending():
                context.iteration(False)
            
            if self._done.wait(0.1):
                break
            waited += 100
        
        # Unsubscribe
        self._bus.signal_unsubscribe(subscription_id)
        
        if not self._done.is_set():
            self._error = f"Timeout waiting for portal response"
            return None
        
        return self._response_data
    
    def _on_response(self, connection, sender, path, interface, signal, params, user_data):
        """Handle portal response signal"""
        response_code = params[0]
        results = params[1]
        
        logger.debug(f"Portal response: code={response_code}, path={path}")
        
        self._response_data = (response_code, dict(results))
        self._done.set()
    
    def _create_session(self) -> bool:
        """Create a screencast session"""
        token = self._get_token()
        session_token = self._get_token()
        sender = self._get_sender_name()
        
        # Build options as GLib.VariantBuilder
        options_builder = GLib.VariantBuilder.new(GLib.VariantType.new('a{sv}'))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('handle_token'),
            GLib.Variant.new_variant(GLib.Variant.new_string(token))
        ))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('session_handle_token'),
            GLib.Variant.new_variant(GLib.Variant.new_string(session_token))
        ))
        options = options_builder.end()
        
        # Expected request path - subscribe BEFORE calling
        expected_request_path = f"/org/freedesktop/portal/desktop/request/{sender}/{token}"
        logger.debug(f"Expected request path: {expected_request_path}")
        
        # Subscribe to response signal BEFORE making the call
        self._done.clear()
        self._response_data = None
        subscription_id = self._bus.signal_subscribe(
            self.PORTAL_BUS,
            self.PORTAL_REQUEST,
            'Response',
            expected_request_path,
            None,
            Gio.DBusSignalFlags.NO_MATCH_RULE,
            self._on_response,
            None
        )
        
        # Build the full argument tuple
        args = GLib.Variant.new_tuple(options)
        
        # Call CreateSession
        result = self._portal.call_sync(
            'CreateSession',
            args,
            Gio.DBusCallFlags.NONE,
            -1,
            None
        )
        
        actual_request_path = result.unpack()[0]
        logger.debug(f"Actual request path: {actual_request_path}")
        
        # If paths don't match, resubscribe
        if actual_request_path != expected_request_path:
            logger.debug(f"Paths don't match, resubscribing...")
            self._bus.signal_unsubscribe(subscription_id)
            subscription_id = self._bus.signal_subscribe(
                self.PORTAL_BUS,
                self.PORTAL_REQUEST,
                'Response',
                actual_request_path,
                None,
                Gio.DBusSignalFlags.NO_MATCH_RULE,
                self._on_response,
                None
            )
        
        # Wait for response by polling GLib context
        waited = 0
        timeout_ms = 30000
        context = GLib.MainContext.default()
        while not self._done.is_set() and waited < timeout_ms:
            while context.pending():
                context.iteration(False)
            if self._done.wait(0.1):
                break
            waited += 100
        
        # Unsubscribe
        self._bus.signal_unsubscribe(subscription_id)
        
        if not self._done.is_set():
            self._error = "Timeout waiting for CreateSession response"
            return False
        
        response_code, results = self._response_data
        
        if response_code == 0:
            self.session_path = results.get('session_handle', '')
            logger.info(f"Session created: {self.session_path}")
            return True
        else:
            self._error = f"CreateSession returned code {response_code}"
            return False
    
    def _select_sources(self) -> bool:
        """Select sources to capture (shows dialog)"""
        token = self._get_token()
        sender = self._get_sender_name()
        
        # Build options
        options_builder = GLib.VariantBuilder.new(GLib.VariantType.new('a{sv}'))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('handle_token'),
            GLib.Variant.new_variant(GLib.Variant.new_string(token))
        ))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('types'),
            GLib.Variant.new_variant(GLib.Variant.new_uint32(self._source_type))
        ))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('multiple'),
            GLib.Variant.new_variant(GLib.Variant.new_boolean(False))
        ))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('cursor_mode'),
            GLib.Variant.new_variant(GLib.Variant.new_uint32(self._cursor_mode))
        ))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('persist_mode'),
            GLib.Variant.new_variant(GLib.Variant.new_uint32(2))
        ))
        options = options_builder.end()
        
        expected_request_path = f"/org/freedesktop/portal/desktop/request/{sender}/{token}"
        
        # Subscribe BEFORE calling
        self._done.clear()
        self._response_data = None
        subscription_id = self._bus.signal_subscribe(
            self.PORTAL_BUS,
            self.PORTAL_REQUEST,
            'Response',
            expected_request_path,
            None,
            Gio.DBusSignalFlags.NO_MATCH_RULE,
            self._on_response,
            None
        )
        
        # Build args tuple: (session_path, options)
        args = GLib.Variant.new_tuple(
            GLib.Variant.new_object_path(self.session_path),
            options
        )
        
        result = self._portal.call_sync(
            'SelectSources',
            args,
            Gio.DBusCallFlags.NONE,
            -1,
            None
        )
        
        actual_request_path = result.unpack()[0]
        logger.debug(f"SelectSources request path: {actual_request_path}")
        
        # Resubscribe if needed
        if actual_request_path != expected_request_path:
            self._bus.signal_unsubscribe(subscription_id)
            subscription_id = self._bus.signal_subscribe(
                self.PORTAL_BUS,
                self.PORTAL_REQUEST,
                'Response',
                actual_request_path,
                None,
                Gio.DBusSignalFlags.NO_MATCH_RULE,
                self._on_response,
                None
            )
        
        # Wait for response - longer timeout for user interaction (2 minutes)
        waited = 0
        timeout_ms = 120000
        context = GLib.MainContext.default()
        while not self._done.is_set() and waited < timeout_ms:
            while context.pending():
                context.iteration(False)
            if self._done.wait(0.1):
                break
            waited += 100
        
        self._bus.signal_unsubscribe(subscription_id)
        
        if not self._done.is_set():
            self._error = "Timeout waiting for SelectSources response"
            return False
        
        response_code, results = self._response_data
        
        if response_code == 0:
            logger.info("Sources selected successfully")
            return True
        elif response_code == 1:
            self._error = "User cancelled source selection"
            return False
        else:
            self._error = f"SelectSources returned code {response_code}"
            return False
    
    def _start_stream(self) -> bool:
        """Start the PipeWire stream"""
        token = self._get_token()
        sender = self._get_sender_name()
        
        # Build options
        options_builder = GLib.VariantBuilder.new(GLib.VariantType.new('a{sv}'))
        options_builder.add_value(GLib.Variant.new_dict_entry(
            GLib.Variant.new_string('handle_token'),
            GLib.Variant.new_variant(GLib.Variant.new_string(token))
        ))
        options = options_builder.end()
        
        expected_request_path = f"/org/freedesktop/portal/desktop/request/{sender}/{token}"
        
        # Subscribe BEFORE calling
        self._done.clear()
        self._response_data = None
        subscription_id = self._bus.signal_subscribe(
            self.PORTAL_BUS,
            self.PORTAL_REQUEST,
            'Response',
            expected_request_path,
            None,
            Gio.DBusSignalFlags.NO_MATCH_RULE,
            self._on_response,
            None
        )
        
        # Build args tuple: (session_path, parent_window, options)
        args = GLib.Variant.new_tuple(
            GLib.Variant.new_object_path(self.session_path),
            GLib.Variant.new_string(''),  # parent_window
            options
        )
        
        result = self._portal.call_sync(
            'Start',
            args,
            Gio.DBusCallFlags.NONE,
            -1,
            None
        )
        
        actual_request_path = result.unpack()[0]
        logger.debug(f"Start request path: {actual_request_path}")
        
        # Resubscribe if needed
        if actual_request_path != expected_request_path:
            self._bus.signal_unsubscribe(subscription_id)
            subscription_id = self._bus.signal_subscribe(
                self.PORTAL_BUS,
                self.PORTAL_REQUEST,
                'Response',
                actual_request_path,
                None,
                Gio.DBusSignalFlags.NO_MATCH_RULE,
                self._on_response,
                None
            )
        
        # Wait for response
        waited = 0
        timeout_ms = 30000
        context = GLib.MainContext.default()
        while not self._done.is_set() and waited < timeout_ms:
            while context.pending():
                context.iteration(False)
            if self._done.wait(0.1):
                break
            waited += 100
        
        self._bus.signal_unsubscribe(subscription_id)
        
        if not self._done.is_set():
            self._error = "Timeout waiting for Start response"
            return False
        
        response_code, results = self._response_data
        
        if response_code == 0:
            streams = results.get('streams', [])
            if streams:
                # streams is array of (node_id, properties)
                stream = streams[0]
                self.pipewire_node_id = int(stream[0])
                logger.info(f"PipeWire node ID: {self.pipewire_node_id}")
                
                # Log stream properties if available
                if len(stream) > 1:
                    props = dict(stream[1])
                    logger.debug(f"Stream properties: {props}")
                
                return True
            else:
                self._error = "No streams returned from portal"
                return False
        else:
            self._error = f"Start returned code {response_code}"
            return False
    
    def stop(self):
        """Stop the screencast session and cleanup all resources"""
        logger.debug("Stopping screencast session...")
        
        # Close the portal session
        if self.session_path and self._bus:
            try:
                session_proxy = Gio.DBusProxy.new_sync(
                    self._bus,
                    Gio.DBusProxyFlags.NONE,
                    None,
                    self.PORTAL_BUS,
                    self.session_path,
                    'org.freedesktop.portal.Session',
                    None
                )
                session_proxy.call_sync('Close', None, Gio.DBusCallFlags.NONE, -1, None)
                logger.info("Portal session closed")
            except Exception as e:
                logger.debug(f"Error closing session: {e}")
        
        # Close PipeWire file descriptor if open
        if self.pipewire_fd is not None and self.pipewire_fd >= 0:
            try:
                os.close(self.pipewire_fd)
                logger.debug("PipeWire fd closed")
            except Exception as e:
                logger.debug(f"Error closing PipeWire fd: {e}")
        
        # Reset all state
        self.session_path = None
        self.pipewire_node_id = None
        self.pipewire_fd = None
        self._portal = None
        self._bus = None
        self._error = None
        self._response_data = None
        self._done.clear()
        
        logger.debug("Screencast resources cleaned up")
    
    def get_gst_source(self) -> str:
        """Get GStreamer source element string for pipewiresrc"""
        if self.pipewire_node_id:
            return f'pipewiresrc path={self.pipewire_node_id} do-timestamp=true keepalive-time=1000 resend-last=true'
        return None


# Singleton instance for the application
_portal_instance = None


def get_portal() -> PortalScreencast:
    """Get or create the portal screencast instance"""
    global _portal_instance
    if _portal_instance is None:
        _portal_instance = PortalScreencast()
    return _portal_instance


def reset_portal():
    """Reset the portal singleton instance. Call this when stopping streaming to allow a fresh start."""
    global _portal_instance
    if _portal_instance is not None:
        try:
            _portal_instance.stop()
        except Exception:
            pass
        _portal_instance = None


def is_wayland() -> bool:
    """Check if running under Wayland"""
    return os.environ.get('XDG_SESSION_TYPE', '').lower() == 'wayland'


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG, format='%(levelname)s:%(name)s:%(message)s')
    
    print("=" * 60)
    print("Portal Screencast Test")
    print("=" * 60)
    
    session_type = os.environ.get('XDG_SESSION_TYPE', 'unknown')
    print(f"Session type: {session_type}")
    
    if not is_wayland():
        print("\n⚠ Not running under Wayland - this tool is primarily for Wayland")
        print("  Use ximagesrc for X11 sessions")
        print("  Continuing anyway for testing...\n")
    
    portal = PortalScreencast()
    
    print("\nStarting screencast...")
    print("A dialog should appear to select what to share.\n")
    
    if portal.start_capture():
        print(f"\n✓ Success!")
        print(f"PipeWire node ID: {portal.pipewire_node_id}")
        print(f"\nGStreamer source: {portal.get_gst_source()}")
        print(f"\nTest with:")
        print(f"  gst-launch-1.0 {portal.get_gst_source()} ! videoconvert ! autovideosink")
        
        input("\nPress Enter to stop...")
        portal.stop()
    else:
        print(f"\n✗ Failed to start screencast: {portal._error}")
        print("\nTroubleshooting:")
        print("  1. Make sure xdg-desktop-portal-gnome is installed")
        print("  2. Check if portal service is running: systemctl --user status xdg-desktop-portal")
        print("  3. Try restarting the portal: systemctl --user restart xdg-desktop-portal")
        exit(1)
