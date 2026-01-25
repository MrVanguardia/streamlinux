#!/usr/bin/env python3
"""
StreamLinux - Internationalization (i18n) Module
Supports English and Spanish with system language auto-detection
"""

import os
from typing import Dict, Optional

# Translation dictionaries
TRANSLATIONS: Dict[str, Dict[str, str]] = {
    'en': {
        # App
        'app_name': 'StreamLinux',
        'app_subtitle': 'Stream your screen to Android',
        
        # Main window sections
        'section_status': 'Status',
        'section_connection': 'Connection',
        'section_quick_actions': 'Quick Actions',
        'section_stream_settings': 'Stream Settings',
        'section_audio': 'Audio',
        'section_advanced': 'Advanced',
        
        # Status
        'status_ready': 'Ready to stream',
        'status_streaming': 'Streaming',
        'status_connecting': 'Connecting...',
        'status_waiting': 'Waiting for viewers...',
        'status_error': 'Error',
        'status_stopped': 'Stopped',
        
        # Stream info
        'stream_time': 'Stream time',
        'connected_clients': 'Connected clients',
        'resolution': 'Resolution',
        'bitrate': 'Bitrate',
        'fps': 'FPS',
        'encoding': 'Encoding',
        'latency': 'Latency',
        'none': 'None',
        
        # Connection
        'connection_wifi': 'WiFi Connection',
        'connection_usb': 'USB Connection',
        'connection_ip': 'IP Address',
        'connection_port': 'Port',
        'connection_qr': 'QR Code',
        'connection_show_qr': 'Show QR Code',
        'usb_connected': 'USB Connected',
        'usb_disconnected': 'USB Disconnected',
        'usb_boost_active': 'USB Boost Active',
        'usb_device': 'Device',
        
        # Quick actions
        'start_stream': 'Start Stream',
        'stop_stream': 'Stop Stream',
        'share_screen': 'Share Screen',
        'select_window': 'Select Window',
        
        # Settings
        'video_quality': 'Video Quality',
        'quality_auto': 'Auto',
        'quality_low': 'Low (480p)',
        'quality_medium': 'Medium (720p)',
        'quality_high': 'High (1080p)',
        'quality_ultra': 'Ultra (4K)',
        'video_bitrate': 'Video Bitrate',
        'video_fps': 'Frame Rate',
        'capture_method': 'Capture Method',
        'method_auto': 'Auto',
        'method_portal': 'Portal (Wayland)',
        'method_pipewire': 'PipeWire',
        'method_x11': 'X11',
        
        # Audio
        'audio_enabled': 'Enable Audio',
        'audio_source': 'Audio Source',
        'audio_system': 'System Audio',
        'audio_microphone': 'Microphone',
        'audio_both': 'Both',
        'audio_none': 'None',
        'audio_bitrate': 'Audio Bitrate',
        
        # Advanced
        'encoder': 'Encoder',
        'encoder_auto': 'Auto',
        'encoder_software': 'Software (x264)',
        'encoder_vaapi': 'VAAPI (Intel/AMD)',
        'encoder_nvenc': 'NVENC (NVIDIA)',
        'stun_server': 'STUN Server',
        'verbose_logging': 'Verbose Logging',
        'show_stats': 'Show Stats Overlay',
        
        # Preferences
        'preferences': 'Preferences',
        'language': 'Language',
        'language_system': 'System Default',
        'language_en': 'English',
        'language_es': 'Español',
        'autostart': 'Start on login',
        'autostream': 'Auto-start streaming',
        'minimize_tray': 'Minimize to tray',
        'usb_boost': 'USB Boost',
        'usb_boost_desc': 'Increase quality when USB connected',
        'reset_defaults': 'Reset to Defaults',
        
        # About
        'about': 'About',
        'about_description': 'Stream your Linux screen to Android devices with ultra-low latency using WebRTC.',
        'version': 'Version',
        'developer': 'Developer',
        'website': 'Website',
        'donate': 'Donate',
        'donate_paypal': 'Donate via PayPal',
        'license': 'License',
        
        # Dialogs
        'confirm': 'Confirm',
        'cancel': 'Cancel',
        'close': 'Close',
        'save': 'Save',
        'apply': 'Apply',
        'error': 'Error',
        'warning': 'Warning',
        'info': 'Information',
        'success': 'Success',
        
        # Messages
        'msg_stream_started': 'Stream started successfully',
        'msg_stream_stopped': 'Stream stopped',
        'msg_client_connected': 'Client connected',
        'msg_client_disconnected': 'Client disconnected',
        'msg_no_capture': 'No screen capture available',
        'msg_wayland_select': 'Please select a screen or window to share',
        'msg_reset_confirm': 'Are you sure you want to reset all settings to defaults?',
        'msg_qr_scan': 'Scan this QR code with the StreamLinux Android app',
        'msg_usb_detected': 'USB device detected',
        'msg_usb_boost_enabled': 'USB connection detected - Quality boosted!',
        
        # Errors
        'error_no_webrtc': 'WebRTC not available',
        'error_no_capture': 'Screen capture failed',
        'error_connection': 'Connection error',
        'error_signaling': 'Signaling server error',
        
        # Tooltips
        'tooltip_start': 'Start streaming your screen',
        'tooltip_stop': 'Stop the current stream',
        'tooltip_qr': 'Show QR code for quick connection',
        'tooltip_settings': 'Open preferences',
        
        # Main UI strings
        'menu_button': 'Menu',
        'menu_preferences': 'Preferences',
        'menu_about': 'About',
        'menu_quit': 'Quit',
        
        'status_title': 'Status',
        'status_subtitle': 'Ready to stream your screen',
        'toggle_start': 'Start Streaming',
        'uptime_title': 'Uptime',
        'start': 'Start',
        'stop': 'Stop',
        'starting': 'Starting...',
        
        'qr_title': 'QR Code',
        'qr_subtitle': 'Scan with the Android app to connect',
        'qr_regenerate': 'Regenerate QR',
        
        'quick_settings_title': 'Quick Settings',
        'quick_settings_subtitle': 'Adjust stream settings',
        'quality_title': 'Quality',
        'quality_subtitle': 'Video resolution',
        'fps_title': 'FPS',
        'fps_subtitle': 'Frames per second',
        'audio_title': 'Audio',
        'audio_subtitle': 'Include system audio',
        'hw_encoding_title': 'Hardware Encoding',
        'hw_encoding_subtitle': 'Use GPU for encoding',
        'more_settings': 'More Settings',
        
        'connection_title': 'Connection Info',
        'connection_subtitle': 'Connect from the same network',
        'ip_title': 'IP Address',
        'port_title': 'Port',
        'hostname_title': 'Hostname',
        
        'stats_title': 'Statistics',
        'stats_subtitle': 'Real-time stream metrics',
        'stat_fps': 'FPS',
        'stat_bitrate': 'Bitrate',
        'stat_clients': 'Clients',
        'stat_latency': 'Latency',
        
        # USB Section
        'usb_boost_desc': 'Connect your Android via USB for better performance',
        'usb_port_forward': 'Port Forwarding Active',
        'usb_port_forward_desc': 'Allows connection via localhost',
        'usb_boost_subtitle': 'Double bitrate automatically via USB',
        'usb_benefits': 'USB Benefits',
        'usb_benefits_desc': '⚡ Lower latency • 📊 More stable • 🔒 No WiFi',
        'usb_quality': 'USB Quality',
        'enabled': 'enabled',
        'disabled': 'disabled',
        'usb_connected': '✅ Connected and Ready',
        'usb_port_forward_active': 'Port forwarding active',
        'usb_device_detected': '📱 Device Detected',
        'usb_activate_forward': 'Activate port forwarding',
        'usb_no_devices': '📵 No devices',
        'usb_connect_android': 'Connect your Android via USB',
        'usb_forward_enabled': '✅ Port forwarding activated - USB connection ready',
        'usb_forward_error': '❌ Error activating port forwarding',
        'usb_forward_disabled': 'Port forwarding deactivated',
        'usb_android_connected': 'Android connected',
        'usb_activate': 'Activate USB',
        'usb_android_disconnected': '📵 Android device disconnected',
        'usb_quality_improved': 'Quality improved',
        
        # Streaming messages
        'error_webrtc_unavailable': 'Error: WebRTC not available',
        'wayland_select_screen': 'Select the screen to share...',
        'error_screen_capture': '❌ Error: Could not start screen capture',
        'connecting': 'Connecting to server...',
        'waiting_connections': '✓ Waiting for connections...',
        'streaming': '✓ Streaming!',
        'client_connected': '📱 Client connected',
        'client_disconnected': 'Client disconnected',
        'streaming_stopped': 'Streaming stopped',
        
        # About dialog
        'about_description': '⚠️ EXPERIMENTAL VERSION ⚠️\n\nStream your Linux screen to Android devices with low latency using WebRTC.\n\nThis software is under active development. Bugs or incomplete features may exist.',
        'about_technologies': 'Technologies',
        'about_developer': 'Developer',
        'about_website': '🌐 Website',
        'about_donate': '💖 Donate via PayPal',
        'about_github': '⭐ GitHub',
        
        # Preferences
        'pref_video': 'Video',
        'pref_encoding': 'Encoding',
        'pref_encoding_desc': 'Video encoder settings',
        'pref_encoder': 'Encoder',
        'pref_encoder_desc': 'Video encoding method',
        'pref_bitrate': 'Maximum Bitrate',
        'pref_bitrate_desc': 'Megabits per second',
        'pref_preset': 'Speed Preset',
        'pref_preset_desc': 'Balance between quality and CPU usage',
        'preset_ultrafast': 'Ultra fast',
        'preset_veryfast': 'Very fast',
        'preset_fast': 'Fast',
        'preset_medium': 'Medium',
        'preset_slow': 'Slow',
        'pref_capture': 'Capture',
        'pref_capture_desc': 'Screen capture method',
        'pref_capture_method': 'Capture Method',
        'pref_capture_method_desc': 'Backend to capture the screen',
        'capture_auto': 'Automatic',
        'capture_pipewire': 'PipeWire (Wayland)',
        'capture_portal': 'XDG Portal',
        'capture_x11': 'X11 (XCB)',
        'pref_screen': 'Screen to Capture',
        'screen_primary': 'Primary Screen',
        'screen_all': 'All Screens',
        'screen_window': 'Specific Window',
        
        'pref_audio': 'Audio',
        'pref_audio_capture': 'Audio Capture',
        'pref_audio_source': 'Audio Source',
        'pref_audio_quality': 'Audio Quality',
        'pref_codec': 'Codec',
        'codec_opus': 'Opus (Recommended)',
        'pref_quality': 'Quality',
        
        'pref_network': 'Network',
        'pref_server': 'Server',
        'pref_port': 'Port',
        'pref_port_desc': 'Port for WebSocket signaling',
        'pref_port_restart': '⚠️ Changing the port requires restarting the application',
        'pref_stun': 'STUN Server',
        'pref_turn': 'TURN Server (optional)',
        
        'pref_advanced': 'Advanced',
        'pref_startup': 'Startup',
        'pref_autostart': 'Start with system',
        'pref_autostart_desc': 'Start StreamLinux on session startup',
        'pref_autostream': 'Start streaming automatically',
        'pref_autostream_desc': 'Begin streaming when opening the app',
        'pref_tray': 'Minimize to tray',
        'pref_tray_desc': 'Continue in background when closing',
        'pref_debug': 'Debugging',
        'pref_verbose': 'Verbose logging',
        'pref_verbose_desc': 'Enable debug logs',
        'pref_stats_overlay': 'Show stats on screen',
        'pref_stats_overlay_desc': 'Overlay with FPS and bitrate',
        'pref_data': 'Data',
        'pref_reset': 'Reset Settings',
        'pref_reset_desc': 'Restore default values',
        'reset_confirm_title': 'Reset settings?',
        'reset_confirm_body': 'All changes will be lost and default values will be restored.',
        'reset': 'Reset',
        
        # General page
        'pref_general': 'General',
        'language_desc': 'Select your preferred language',
        'language_subtitle': 'Interface language',
        'language_restart': '⚠️ Changes will apply on next restart',
        
        # Security
        'security_section': 'Security',
        'security_connection_request': 'Connection Request',
        'security_device_wants_connect': 'A device wants to connect:',
        'security_enter_pin_on_device': 'Enter this PIN on the device, or click Approve.',
        'security_device_authorized': 'Device authorized',
        'security_connection_rejected': 'Connection rejected',
        'security_require_auth': 'Require authentication',
        'security_require_auth_desc': 'Devices must verify with PIN',
        'security_auto_trust': 'Auto-trust devices',
        'security_auto_trust_desc': 'Skip PIN after first connection',
        'security_manage_devices': 'Manage Devices',
        'security_manage_devices_desc': 'View and revoke authorized devices',
        'security_authorized_devices': 'Authorized Devices',
        'security_no_devices': 'No authorized devices',
        'security_device_trusted': 'Trusted',
        'security_device_last_connected': 'Last connected',
        'security_revoke_device': 'Revoke',
        'security_revoke_confirm': 'Revoke device access?',
        'security_revoke_confirm_body': 'This device will need to re-authenticate to connect.',
        'btn_approve': 'Approve',
        'btn_reject': 'Reject',
        'device': 'Device',
    },
    
    'es': {
        # App
        'app_name': 'StreamLinux',
        'app_subtitle': 'Transmite tu pantalla a Android',
        
        # Main window sections
        'section_status': 'Estado',
        'section_connection': 'Conexión',
        'section_quick_actions': 'Acciones Rápidas',
        'section_stream_settings': 'Configuración de Transmisión',
        'section_audio': 'Audio',
        'section_advanced': 'Avanzado',
        
        # Status
        'status_ready': 'Listo para transmitir',
        'status_streaming': 'Transmitiendo',
        'status_connecting': 'Conectando...',
        'status_waiting': 'Esperando espectadores...',
        'status_error': 'Error',
        'status_stopped': 'Detenido',
        
        # Stream info
        'stream_time': 'Tiempo de transmisión',
        'connected_clients': 'Clientes conectados',
        'resolution': 'Resolución',
        'bitrate': 'Tasa de bits',
        'fps': 'FPS',
        'encoding': 'Codificación',
        'latency': 'Latencia',
        'none': 'Ninguno',
        
        # Connection
        'connection_wifi': 'Conexión WiFi',
        'connection_usb': 'Conexión USB',
        'connection_ip': 'Dirección IP',
        'connection_port': 'Puerto',
        'connection_qr': 'Código QR',
        'connection_show_qr': 'Mostrar Código QR',
        'usb_connected': 'USB Conectado',
        'usb_disconnected': 'USB Desconectado',
        'usb_boost_active': 'Mejora USB Activa',
        'usb_device': 'Dispositivo',
        
        # Quick actions
        'start_stream': 'Iniciar Transmisión',
        'stop_stream': 'Detener Transmisión',
        'share_screen': 'Compartir Pantalla',
        'select_window': 'Seleccionar Ventana',
        
        # Settings
        'video_quality': 'Calidad de Video',
        'quality_auto': 'Automática',
        'quality_low': 'Baja (480p)',
        'quality_medium': 'Media (720p)',
        'quality_high': 'Alta (1080p)',
        'quality_ultra': 'Ultra (4K)',
        'video_bitrate': 'Tasa de bits de Video',
        'video_fps': 'Cuadros por Segundo',
        'capture_method': 'Método de Captura',
        'method_auto': 'Automático',
        'method_portal': 'Portal (Wayland)',
        'method_pipewire': 'PipeWire',
        'method_x11': 'X11',
        
        # Audio
        'audio_enabled': 'Habilitar Audio',
        'audio_source': 'Fuente de Audio',
        'audio_system': 'Audio del Sistema',
        'audio_microphone': 'Micrófono',
        'audio_both': 'Ambos',
        'audio_none': 'Ninguno',
        'audio_bitrate': 'Tasa de bits de Audio',
        
        # Advanced
        'encoder': 'Codificador',
        'encoder_auto': 'Automático',
        'encoder_software': 'Software (x264)',
        'encoder_vaapi': 'VAAPI (Intel/AMD)',
        'encoder_nvenc': 'NVENC (NVIDIA)',
        'stun_server': 'Servidor STUN',
        'verbose_logging': 'Registro Detallado',
        'show_stats': 'Mostrar Estadísticas',
        
        # Preferences
        'preferences': 'Preferencias',
        'language': 'Idioma',
        'language_system': 'Predeterminado del Sistema',
        'language_en': 'English',
        'language_es': 'Español',
        'autostart': 'Iniciar al arrancar',
        'autostream': 'Auto-iniciar transmisión',
        'minimize_tray': 'Minimizar a la bandeja',
        'usb_boost': 'Mejora USB',
        'usb_boost_desc': 'Aumentar calidad cuando USB está conectado',
        'reset_defaults': 'Restablecer Valores',
        
        # About
        'about': 'Acerca de',
        'about_description': 'Transmite tu pantalla de Linux a dispositivos Android con latencia ultra-baja usando WebRTC.',
        'version': 'Versión',
        'developer': 'Desarrollador',
        'website': 'Sitio Web',
        'donate': 'Donar',
        'donate_paypal': 'Donar vía PayPal',
        'license': 'Licencia',
        
        # Dialogs
        'confirm': 'Confirmar',
        'cancel': 'Cancelar',
        'close': 'Cerrar',
        'save': 'Guardar',
        'apply': 'Aplicar',
        'error': 'Error',
        'warning': 'Advertencia',
        'info': 'Información',
        'success': 'Éxito',
        
        # Messages
        'msg_stream_started': 'Transmisión iniciada correctamente',
        'msg_stream_stopped': 'Transmisión detenida',
        'msg_client_connected': 'Cliente conectado',
        'msg_client_disconnected': 'Cliente desconectado',
        'msg_no_capture': 'Captura de pantalla no disponible',
        'msg_wayland_select': 'Por favor selecciona una pantalla o ventana para compartir',
        'msg_reset_confirm': '¿Estás seguro de que quieres restablecer todos los ajustes?',
        'msg_qr_scan': 'Escanea este código QR con la app StreamLinux de Android',
        'msg_usb_detected': 'Dispositivo USB detectado',
        'msg_usb_boost_enabled': '¡Conexión USB detectada - Calidad mejorada!',
        
        # Errors
        'error_no_webrtc': 'WebRTC no disponible',
        'error_no_capture': 'Captura de pantalla fallida',
        'error_connection': 'Error de conexión',
        'error_signaling': 'Error del servidor de señalización',
        
        # Tooltips
        'tooltip_start': 'Iniciar transmisión de pantalla',
        'tooltip_stop': 'Detener la transmisión actual',
        'tooltip_qr': 'Mostrar código QR para conexión rápida',
        'tooltip_settings': 'Abrir preferencias',
        
        # Main UI strings
        'menu_button': 'Menú',
        'menu_preferences': 'Preferencias',
        'menu_about': 'Acerca de',
        'menu_quit': 'Salir',
        
        'status_title': 'Estado',
        'status_subtitle': 'Listo para transmitir tu pantalla',
        'toggle_start': 'Iniciar Transmisión',
        'uptime_title': 'Tiempo activo',
        'start': 'Iniciar',
        'stop': 'Detener',
        'starting': 'Iniciando...',
        
        'qr_title': 'Código QR',
        'qr_subtitle': 'Escanea con la app Android para conectar',
        'qr_regenerate': 'Regenerar QR',
        
        'quick_settings_title': 'Ajustes Rápidos',
        'quick_settings_subtitle': 'Configura la transmisión',
        'quality_title': 'Calidad',
        'quality_subtitle': 'Resolución de video',
        'fps_title': 'FPS',
        'fps_subtitle': 'Cuadros por segundo',
        'audio_title': 'Audio',
        'audio_subtitle': 'Incluir audio del sistema',
        'hw_encoding_title': 'Codificación Hardware',
        'hw_encoding_subtitle': 'Usar GPU para codificar',
        'more_settings': 'Más Ajustes',
        
        'connection_title': 'Información de Conexión',
        'connection_subtitle': 'Conecta desde la misma red',
        'ip_title': 'Dirección IP',
        'port_title': 'Puerto',
        'hostname_title': 'Nombre del equipo',
        
        'stats_title': 'Estadísticas',
        'stats_subtitle': 'Métricas de transmisión en tiempo real',
        'stat_fps': 'FPS',
        'stat_bitrate': 'Bitrate',
        'stat_clients': 'Clientes',
        'stat_latency': 'Latencia',
        
        # USB Section
        'usb_boost_desc': 'Conecta tu Android por USB para mejor rendimiento',
        'usb_port_forward': 'Port Forwarding Activo',
        'usb_port_forward_desc': 'Permite conexión por localhost',
        'usb_boost_subtitle': 'Duplica bitrate automáticamente por USB',
        'usb_benefits': 'Beneficios de USB',
        'usb_benefits_desc': '⚡ Menor latencia • 📊 Mayor estabilidad • 🔒 Sin WiFi',
        'usb_quality': 'Calidad USB',
        'enabled': 'activada',
        'disabled': 'desactivada',
        'usb_connected': '✅ Conectado y Listo',
        'usb_port_forward_active': 'Port forwarding activo',
        'usb_device_detected': '📱 Dispositivo Detectado',
        'usb_activate_forward': 'Activa port forwarding',
        'usb_no_devices': '📵 Sin dispositivos',
        'usb_connect_android': 'Conecta tu Android por USB',
        'usb_forward_enabled': '✅ Port forwarding activado - Conexión USB lista',
        'usb_forward_error': '❌ Error al activar port forwarding',
        'usb_forward_disabled': 'Port forwarding desactivado',
        'usb_android_connected': 'Android conectado',
        'usb_activate': 'Activar USB',
        'usb_android_disconnected': '📵 Dispositivo Android desconectado',
        'usb_quality_improved': 'Calidad mejorada',
        
        # Streaming messages
        'error_webrtc_unavailable': 'Error: WebRTC no disponible',
        'wayland_select_screen': 'Selecciona la pantalla a compartir...',
        'error_screen_capture': '❌ Error: No se pudo iniciar la captura de pantalla',
        'connecting': 'Conectando al servidor...',
        'waiting_connections': '✓ Esperando conexiones...',
        'streaming': '✓ ¡Transmitiendo!',
        'client_connected': '📱 Cliente conectado',
        'client_disconnected': 'Cliente desconectado',
        'streaming_stopped': 'Transmisión detenida',
        
        # About dialog
        'about_description': '⚠️ VERSIÓN EXPERIMENTAL ⚠️\n\nTransmite tu pantalla de Linux a dispositivos Android con baja latencia usando WebRTC.\n\nEste software está en desarrollo activo. Pueden existir errores o funcionalidades incompletas.',
        'about_technologies': 'Tecnologías',
        'about_developer': 'Desarrollador',
        'about_website': '🌐 Página Web',
        'about_donate': '💖 Donar con PayPal',
        'about_github': '⭐ GitHub',
        
        # Preferences
        'pref_video': 'Video',
        'pref_encoding': 'Codificación',
        'pref_encoding_desc': 'Ajustes del codificador de video',
        'pref_encoder': 'Codificador',
        'pref_encoder_desc': 'Método de codificación de video',
        'pref_bitrate': 'Bitrate Máximo',
        'pref_bitrate_desc': 'Megabits por segundo',
        'pref_preset': 'Preset de Velocidad',
        'pref_preset_desc': 'Balance entre calidad y uso de CPU',
        'preset_ultrafast': 'Ultra rápido',
        'preset_veryfast': 'Muy rápido',
        'preset_fast': 'Rápido',
        'preset_medium': 'Medio',
        'preset_slow': 'Lento',
        'pref_capture': 'Captura',
        'pref_capture_desc': 'Método de captura de pantalla',
        'pref_capture_method': 'Método de Captura',
        'pref_capture_method_desc': 'Backend para capturar la pantalla',
        'capture_auto': 'Automático',
        'capture_pipewire': 'PipeWire (Wayland)',
        'capture_portal': 'XDG Portal',
        'capture_x11': 'X11 (XCB)',
        'pref_screen': 'Pantalla a Capturar',
        'screen_primary': 'Pantalla Principal',
        'screen_all': 'Todas las Pantallas',
        'screen_window': 'Ventana Específica',
        
        'pref_audio': 'Audio',
        'pref_audio_capture': 'Captura de Audio',
        'pref_audio_source': 'Fuente de Audio',
        'pref_audio_quality': 'Calidad de Audio',
        'pref_codec': 'Códec',
        'codec_opus': 'Opus (Recomendado)',
        'pref_quality': 'Calidad',
        
        'pref_network': 'Red',
        'pref_server': 'Servidor',
        'pref_port': 'Puerto',
        'pref_port_desc': 'Puerto para señalización WebSocket',
        'pref_port_restart': '⚠️ Cambiar el puerto requiere reiniciar la aplicación',
        'pref_stun': 'Servidor STUN',
        'pref_turn': 'Servidor TURN (opcional)',
        
        'pref_advanced': 'Avanzado',
        'pref_startup': 'Inicio',
        'pref_autostart': 'Iniciar con el sistema',
        'pref_autostart_desc': 'Iniciar StreamLinux al arrancar la sesión',
        'pref_autostream': 'Iniciar streaming automáticamente',
        'pref_autostream_desc': 'Comenzar a transmitir al abrir la app',
        'pref_tray': 'Minimizar a la bandeja',
        'pref_tray_desc': 'Continuar en segundo plano al cerrar',
        'pref_debug': 'Depuración',
        'pref_verbose': 'Registro detallado',
        'pref_verbose_desc': 'Habilitar logs de depuración',
        'pref_stats_overlay': 'Mostrar estadísticas en pantalla',
        'pref_stats_overlay_desc': 'Overlay con FPS y bitrate',
        'pref_data': 'Datos',
        'pref_reset': 'Restablecer Ajustes',
        'pref_reset_desc': 'Volver a los valores predeterminados',
        'reset_confirm_title': '¿Restablecer ajustes?',
        'reset_confirm_body': 'Se perderán todos los cambios y se restaurarán los valores predeterminados.',
        'reset': 'Restablecer',
        
        # General page
        'pref_general': 'General',
        'language_desc': 'Selecciona tu idioma preferido',
        'language_subtitle': 'Idioma de la interfaz',
        'language_restart': '⚠️ Los cambios se aplicarán en el próximo reinicio',
        
        # Security
        'security_section': 'Seguridad',
        'security_connection_request': 'Solicitud de Conexión',
        'security_device_wants_connect': 'Un dispositivo quiere conectarse:',
        'security_enter_pin_on_device': 'Ingresa este PIN en el dispositivo, o haz clic en Aprobar.',
        'security_device_authorized': 'Dispositivo autorizado',
        'security_connection_rejected': 'Conexión rechazada',
        'security_require_auth': 'Requerir autenticación',
        'security_require_auth_desc': 'Los dispositivos deben verificarse con PIN',
        'security_auto_trust': 'Confiar automáticamente',
        'security_auto_trust_desc': 'Omitir PIN después de la primera conexión',
        'security_manage_devices': 'Gestionar Dispositivos',
        'security_manage_devices_desc': 'Ver y revocar dispositivos autorizados',
        'security_authorized_devices': 'Dispositivos Autorizados',
        'security_no_devices': 'No hay dispositivos autorizados',
        'security_device_trusted': 'De confianza',
        'security_device_last_connected': 'Última conexión',
        'security_revoke_device': 'Revocar',
        'security_revoke_confirm': '¿Revocar acceso del dispositivo?',
        'security_revoke_confirm_body': 'Este dispositivo necesitará volver a autenticarse para conectarse.',
        'btn_approve': 'Aprobar',
        'btn_reject': 'Rechazar',
        'device': 'Dispositivo',
    }
}


class I18n:
    """Internationalization class for StreamLinux"""
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance
    
    def __init__(self):
        if self._initialized:
            return
        self._initialized = True
        self._current_lang = 'en'
        self._listeners = []
        self._detect_system_language()
    
    def _detect_system_language(self):
        """Detect system language and set as default"""
        try:
            # Get system locale using modern method
            system_locale = os.environ.get('LANG', os.environ.get('LC_ALL', 'en_US.UTF-8'))
            
            # Extract language code
            lang_code = system_locale.split('_')[0].lower()
            
            # Check if we support this language
            if lang_code in TRANSLATIONS:
                self._current_lang = lang_code
            else:
                self._current_lang = 'en'
            
            print(f"System language detected: {system_locale} -> using: {self._current_lang}")
        except Exception as e:
            print(f"Error detecting system language: {e}")
            self._current_lang = 'en'
    
    def get_system_language(self) -> str:
        """Get the detected system language code"""
        try:
            system_locale = os.environ.get('LANG', os.environ.get('LC_ALL', 'en_US.UTF-8'))
            return system_locale.split('_')[0].lower()
        except:
            return 'en'
    
    @property
    def current_language(self) -> str:
        """Get current language code"""
        return self._current_lang
    
    @property
    def available_languages(self) -> Dict[str, str]:
        """Get available languages with their display names"""
        return {
            'system': self.get('language_system'),
            'en': 'English',
            'es': 'Español'
        }
    
    def set_language(self, lang_code: str):
        """Set the current language"""
        if lang_code == 'system':
            self._detect_system_language()
        elif lang_code in TRANSLATIONS:
            self._current_lang = lang_code
        else:
            print(f"Unsupported language: {lang_code}")
            return
        
        # Notify listeners
        for callback in self._listeners:
            try:
                callback(self._current_lang)
            except Exception as e:
                print(f"Error in language listener: {e}")
    
    def add_listener(self, callback):
        """Add a listener for language changes"""
        self._listeners.append(callback)
    
    def remove_listener(self, callback):
        """Remove a listener"""
        if callback in self._listeners:
            self._listeners.remove(callback)
    
    def get(self, key: str, **kwargs) -> str:
        """Get a translated string"""
        translations = TRANSLATIONS.get(self._current_lang, TRANSLATIONS['en'])
        text = translations.get(key, TRANSLATIONS['en'].get(key, key))
        
        # Format with kwargs if provided
        if kwargs:
            try:
                text = text.format(**kwargs)
            except:
                pass
        
        return text
    
    def __call__(self, key: str, **kwargs) -> str:
        """Shorthand for get()"""
        return self.get(key, **kwargs)


# Global instance
_i18n = None


def get_i18n() -> I18n:
    """Get the global I18n instance"""
    global _i18n
    if _i18n is None:
        _i18n = I18n()
    return _i18n


def _(key: str, **kwargs) -> str:
    """Translate a string (shorthand function)"""
    return get_i18n().get(key, **kwargs)


# For convenience
def set_language(lang_code: str):
    """Set the application language"""
    get_i18n().set_language(lang_code)


def get_language() -> str:
    """Get the current language code"""
    return get_i18n().current_language


if __name__ == '__main__':
    # Test
    i18n = get_i18n()
    print(f"Current language: {i18n.current_language}")
    print(f"System language: {i18n.get_system_language()}")
    print()
    print("English:")
    i18n.set_language('en')
    print(f"  {_('app_subtitle')}")
    print(f"  {_('start_stream')}")
    print()
    print("Spanish:")
    i18n.set_language('es')
    print(f"  {_('app_subtitle')}")
    print(f"  {_('start_stream')}")
