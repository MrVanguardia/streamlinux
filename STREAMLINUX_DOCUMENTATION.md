# StreamLinux v0.2.0-alpha

## 📋 Documentación Completa

**Fecha de actualización:** 30 de enero de 2026  
**Versión:** 0.2.0-alpha  
**Estado:** Funcional - LAN y USB

---

## 📖 Descripción General

StreamLinux es una solución de streaming de pantalla y audio desde sistemas Linux a dispositivos Android, diseñada con enfoque en **baja latencia**, **seguridad** y **privacidad**. Utiliza tecnología WebRTC para transmisión en tiempo real.

### Características Principales

| Característica | Estado | Descripción |
|---------------|--------|-------------|
| 🖥️ Captura de pantalla | ✅ | Wayland (PipeWire) y X11 (XCB) |
| 🔊 Captura de audio | ✅ | PulseAudio/PipeWire |
| 📡 Conexión LAN | ✅ | WiFi en la misma red |
| 🔌 Conexión USB | ✅ | ADB port forwarding |
| 🔐 Autenticación QR | ✅ | Tokens seguros con expiración |
| 🎬 Codificación VP8 | ✅ | Compatible con WebRTC |
| 🎵 Audio Opus | ✅ | Alta calidad, baja latencia |

---

## 🏗️ Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                     LINUX HOST (PC)                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  Wayland    │  │  PulseAudio │  │   Signaling Server  │ │
│  │  PipeWire   │  │  Monitor    │  │   (Go WebSocket)    │ │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
│         │                │                     │            │
│  ┌──────▼────────────────▼──────┐              │            │
│  │      GStreamer Pipeline      │              │            │
│  │  VP8 Encoder + Opus Encoder  │              │            │
│  └──────────────┬───────────────┘              │            │
│                 │                              │            │
│  ┌──────────────▼──────────────────────────────▼──────────┐ │
│  │                    WebRTC Transport                    │ │
│  │         (ICE + DTLS-SRTP + SCTP DataChannel)          │ │
│  └────────────────────────────┬───────────────────────────┘ │
└───────────────────────────────┼─────────────────────────────┘
                                │
                    ┌───────────▼───────────┐
                    │    Network Layer      │
                    │  (LAN WiFi / USB ADB) │
                    └───────────┬───────────┘
                                │
┌───────────────────────────────▼─────────────────────────────┐
│                    ANDROID CLIENT                           │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │                WebRTC PeerConnection                │   │
│  └──────────────────────┬──────────────────────────────┘   │
│                         │                                   │
│  ┌──────────────────────▼──────────────────────────────┐   │
│  │              MediaCodec Decoder (VP8)               │   │
│  │              + OpenSL ES Audio                      │   │
│  └──────────────────────┬──────────────────────────────┘   │
│                         │                                   │
│  ┌──────────────────────▼──────────────────────────────┐   │
│  │           EGL Surface Renderer (OpenGL ES)          │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔐 Seguridad y Privacidad

### 1. Autenticación por Token QR

StreamLinux implementa un sistema de autenticación robusto basado en tokens QR:

```python
# Estructura del token
token = f"{random_token}:{timestamp}:{session_id}"

# Ejemplo: -FAU0oQCT6oHEuxWmhxCRAQQLHkEFSCm:1769790786:25JP6yWJDps4A1Cb
```

| Componente | Descripción | Seguridad |
|------------|-------------|-----------|
| `random_token` | 32 caracteres aleatorios | `secrets.token_urlsafe(24)` - Criptográficamente seguro |
| `timestamp` | Unix timestamp de creación | Permite validar expiración |
| `session_id` | ID único de sesión | Previene replay attacks |

#### Características de Seguridad del Token:

- ✅ **Generación criptográfica**: Usa `secrets` de Python (CSPRNG)
- ✅ **Regeneración automática**: Nuevo token cada 60 segundos
- ✅ **Expiración**: Tokens válidos solo por tiempo limitado
- ✅ **Uso único**: El token se invalida después de conexión exitosa
- ✅ **Sin almacenamiento persistente**: Tokens solo en memoria

### 2. Cifrado de Transporte (DTLS-SRTP)

WebRTC proporciona cifrado obligatorio end-to-end:

```
┌─────────────────────────────────────────────────────────────┐
│                    Capas de Seguridad                       │
├─────────────────────────────────────────────────────────────┤
│  Capa 4: Datos de aplicación (video/audio)                 │
│          └─► Cifrado con SRTP (AES-128-CTR)                │
│                                                             │
│  Capa 3: Control de sesión                                  │
│          └─► DTLS 1.2/1.3 (TLS sobre UDP)                  │
│                                                             │
│  Capa 2: Transporte                                         │
│          └─► ICE (verificación de conectividad)            │
│                                                             │
│  Capa 1: Red                                                │
│          └─► UDP/TCP sobre LAN o localhost (USB)           │
└─────────────────────────────────────────────────────────────┘
```

#### Algoritmos de Cifrado:

| Protocolo | Algoritmo | Uso |
|-----------|-----------|-----|
| DTLS | TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 | Handshake y control |
| SRTP | AES_CM_128_HMAC_SHA1_80 | Cifrado de media |
| ICE | STUN/TURN con credenciales | NAT traversal seguro |

### 3. Validación de Origen (CORS)

El servidor de señalización valida orígenes permitidos:

```go
func originAllowed(origin string) bool {
    // Solo permite conexiones locales
    if strings.Contains(origin, "localhost") ||
       strings.Contains(origin, "127.0.0.1") ||
       strings.HasPrefix(origin, "10.") ||        // LAN privada
       strings.HasPrefix(origin, "192.168.") ||   // LAN privada
       strings.HasPrefix(origin, "172.") {        // LAN privada
        return true
    }
    return false
}
```

### 4. Aislamiento de Red

| Modo | Conexiones Permitidas | Exposición |
|------|----------------------|------------|
| **USB** | Solo localhost:54321 | Ninguna - totalmente aislado |
| **LAN** | Solo red local (10.x, 192.168.x) | Mínima - solo dispositivos en la misma red |

### 5. Sin Servidor Externo

A diferencia de otras soluciones de streaming:

| Característica | StreamLinux | Otras Soluciones |
|---------------|-------------|------------------|
| Servidor en la nube | ❌ No | ✅ Sí (típicamente) |
| Datos a terceros | ❌ No | ⚠️ Posiblemente |
| Requiere cuenta | ❌ No | ✅ Generalmente |
| Funciona offline | ✅ Sí (LAN/USB) | ❌ No |

### 6. Permisos Mínimos

#### Linux Host:
- Acceso a PipeWire/PulseAudio (captura de pantalla/audio)
- Acceso a red local (puerto 54321)
- Sin acceso a archivos del usuario
- Sin acceso a internet (opcional para STUN)

#### Android Client:
```xml
<!-- Permisos requeridos -->
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.CAMERA" />  <!-- Solo para QR -->

<!-- NO requiere -->
<!-- android.permission.READ_EXTERNAL_STORAGE -->
<!-- android.permission.WRITE_EXTERNAL_STORAGE -->
<!-- android.permission.ACCESS_FINE_LOCATION -->
```

---

## 🔒 Modelo de Amenazas y Mitigaciones

### Amenazas Consideradas

| Amenaza | Riesgo | Mitigación |
|---------|--------|------------|
| **Intercepción de video** | Alto | DTLS-SRTP cifrado E2E |
| **Man-in-the-Middle** | Alto | Verificación de fingerprint DTLS |
| **Replay Attack** | Medio | Tokens con timestamp y uso único |
| **Acceso no autorizado** | Alto | Autenticación QR obligatoria |
| **Escaneo de puerto** | Bajo | Servidor solo en LAN, token requerido |
| **Fuerza bruta** | Bajo | Tokens de 32 caracteres (192 bits de entropía) |

### Superficie de Ataque

```
┌─────────────────────────────────────────────────┐
│           Superficie de Ataque Mínima           │
├─────────────────────────────────────────────────┤
│                                                 │
│  Expuesto:                                      │
│  ├─ Puerto 54321 (WebSocket) - Solo LAN        │
│  └─ QR Code (visible en pantalla)              │
│                                                 │
│  NO Expuesto:                                   │
│  ├─ Archivos del sistema                       │
│  ├─ Datos personales                           │
│  ├─ Credenciales                               │
│  └─ Internet (conexión directa P2P)            │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

## 📊 Especificaciones Técnicas

### Rendimiento de Streaming

| Parámetro | Valor | Configurable |
|-----------|-------|--------------|
| Resolución | 1920x1080 | ✅ |
| Framerate | 60 FPS | ✅ |
| Bitrate Video | 8 Mbps | ✅ |
| Codec Video | VP8 | Fijo (compatibilidad) |
| Bitrate Audio | 320 kbps | ✅ |
| Codec Audio | Opus | Fijo (óptimo para voz/música) |
| Latencia típica | 50-150ms | Depende de red |

### Compatibilidad

#### Linux Host:
- **Distribuciones**: Fedora 38+, Ubuntu 22.04+, Arch Linux
- **Display Server**: Wayland (preferido), X11
- **Audio**: PipeWire, PulseAudio
- **Dependencias**: GStreamer 1.20+, GTK4, Python 3.10+

#### Android Client:
- **Android**: 8.0+ (API 26)
- **Arquitecturas**: arm64-v8a, armeabi-v7a, x86_64
- **RAM mínima**: 2GB
- **Conexión**: WiFi o USB

---

## 📦 Estructura de Instaladores

### RPM (Linux/Fedora)

```
streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm
├── /usr/bin/streamlinux                    # Launcher script
├── /usr/share/streamlinux/
│   ├── streamlinux_gui.py                  # Aplicación principal
│   ├── webrtc_streamer.py                  # Motor de streaming
│   ├── portal_screencast.py                # Captura Wayland
│   ├── security.py                         # Módulo de seguridad
│   ├── i18n.py                             # Internacionalización
│   └── usb_manager.py                      # Gestión USB/ADB
├── /usr/lib64/streamlinux/
│   └── signaling-server                    # Servidor Go compilado
├── /usr/share/applications/
│   └── com.streamlinux.host.desktop        # Entrada de menú
├── /usr/share/icons/hicolor/scalable/apps/
│   └── streamlinux.svg                     # Icono
└── /usr/share/metainfo/
    └── com.streamlinux.host.metainfo.xml   # Metadatos AppStream
```

### APK (Android)

```
streamlinux-0.2.0-debug.apk (38 MB)
├── lib/
│   ├── arm64-v8a/libjingle_peerconnection_so.so
│   ├── armeabi-v7a/libjingle_peerconnection_so.so
│   └── x86_64/libjingle_peerconnection_so.so
├── classes.dex                             # Código Kotlin compilado
├── res/                                    # Recursos UI
└── AndroidManifest.xml                     # Permisos y config
```

---

## 🚀 Guía de Uso

### Conexión por LAN (WiFi)

1. **En Linux**: Abrir StreamLinux desde el menú de aplicaciones
2. **En Linux**: Seleccionar la pantalla a compartir cuando aparezca el diálogo
3. **En Android**: Abrir la app StreamLinux
4. **En Android**: Escanear el código QR mostrado en la pantalla de Linux
5. **Listo**: El streaming comenzará automáticamente

### Conexión por USB

1. **Conectar** el dispositivo Android por USB
2. **Habilitar** depuración USB en Android
3. **En Linux**: Abrir StreamLinux - detectará el dispositivo automáticamente
4. **En Linux**: Hacer clic en "Iniciar USB"
5. **En Android**: Abrir la app y escanear el QR
6. **Listo**: Streaming ultra-bajo latencia por USB

---

## 🔧 Configuración Avanzada

### Archivo de Configuración

Ubicación: `~/.config/streamlinux/settings.json`

```json
{
    "port": 54321,
    "resolution": "1920x1080",
    "framerate": 60,
    "bitrate": 8000000,
    "audio_enabled": true,
    "audio_bitrate": 320000,
    "auto_start_usb": true,
    "theme": "system"
}
```

---

## 📝 Changelog v0.2.0-alpha

### Nuevas Características
- ✅ Soporte completo para conexión LAN (WiFi)
- ✅ Autenticación por token QR con regeneración automática
- ✅ Servidor de señalización integrado (Go)
- ✅ Detección automática de dispositivos USB
- ✅ Captura de pantalla Wayland via xdg-desktop-portal

### Correcciones
- 🔧 Corregido error 401 en conexiones LAN
- 🔧 Corregida ruta del signaling server en RPM
- 🔧 Mejorada estabilidad de ICE negotiation
- 🔧 Corregido archivo .desktop para menú de aplicaciones

### Seguridad
- 🔐 Implementado cifrado DTLS-SRTP obligatorio
- 🔐 Tokens con expiración y uso único
- 🔐 Validación de orígenes para conexiones WebSocket
- 🔐 Sin exposición a internet (solo LAN/localhost)

---

## 📄 Licencia

StreamLinux es software de código abierto bajo licencia **MIT**.

```
MIT License

Copyright (c) 2026 Vanguardia Studio

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

---

## 🤝 Contribuir

1. Fork del repositorio
2. Crear rama de feature (`git checkout -b feature/nueva-caracteristica`)
3. Commit de cambios (`git commit -am 'Añadir nueva característica'`)
4. Push a la rama (`git push origin feature/nueva-caracteristica`)
5. Crear Pull Request

---

## 📞 Soporte

- **Issues**: GitHub Issues del proyecto
- **Email**: contact@vanguardiastudio.us
- **Documentación**: Este archivo y `QUICKSTART.md`

---

*Desarrollado con ❤️ por Vanguardia Studio*
