# StreamLinux - Guía para Copilot

## Descripción del Proyecto

StreamLinux es un sistema de streaming de pantalla y audio desde Linux a Android, diseñado para baja latencia usando WebRTC.

## Estructura del Proyecto

### Linux Host (`/linux-host`)
- **Lenguaje**: C++20
- **Build System**: CMake
- **Arquitectura**: Modular con interfaces abstractas

#### Módulos principales:
- `capture/`: Backends de captura (X11 via XCB, Wayland via PipeWire)
- `audio/`: Captura de audio (PipeWire, PulseAudio)
- `encoding/`: Codificadores (H.264 FFmpeg, Opus)
- `sync/`: Sincronización A/V
- `transport/`: WebRTC
- `cli/`: Interfaz de línea de comandos

#### Patrones de código:
```cpp
// Usar Result<T> para manejo de errores
Result<VideoFrame> captureFrame();

// Interfaces con I-prefix
class IDisplayBackend { ... };
class IAudioCapture { ... };

// Compilación condicional para features
#ifdef HAVE_VAAPI
    // código VAAPI
#endif
```

### Android Client (`/android-client`)
- **Lenguaje**: Kotlin (UI), C++ (NDK)
- **UI Framework**: Jetpack Compose
- **Build System**: Gradle con Kotlin DSL

#### Módulos principales:
- `network/`: WebRTC client, LAN discovery
- `ui/`: Actividades y composables
- `cpp/`: Decodificadores nativos (MediaCodec, OpenSL ES)

#### Patrones de código:
```kotlin
// Compose para UI
@Composable
fun StreamScreen() { ... }

// Coroutines para async
suspend fun discoverHosts(): List<Host>
```

### Signaling Server (`/signaling-server`)
- **Lenguaje**: Go 1.21+
- **Framework**: gorilla/websocket
- **Arquitectura**: Hub pattern para WebSocket

## Convenciones de Código

### C++ (Linux Host)
- Namespaces: `stream_linux::`
- Headers: `#pragma once`
- Smart pointers: `std::unique_ptr`, `std::shared_ptr`
- Error handling: `Result<T>` pattern con `std::expected`
- Logging: Custom macros `LOG_INFO()`, `LOG_ERROR()`

### Kotlin (Android)
- Package: `com.streamlinux.client`
- Nombres descriptivos en camelCase
- Usar `by viewModels()` para ViewModels
- Preferir `Flow` sobre `LiveData`

### Go (Signaling)
- Package names en minúsculas
- Interfaces pequeñas y enfocadas
- Usar `zap` para logging estructurado

## Dependencias Principales

### Linux Host
- FFmpeg (libavcodec, libavformat, libavutil, libswscale, libswresample)
- XCB (x11-xcb, xcb-shm, xcb-randr)
- PipeWire 0.3+
- PulseAudio
- Opus
- GIO/D-Bus (para xdg-desktop-portal)

### Android
- Jetpack Compose
- AndroidX
- NDK 25+
- MediaCodec API
- OpenSL ES

### Signaling Server
- gorilla/websocket
- go-qrcode
- zap (logging)
- viper (config)

## Tareas Comunes

### Agregar nuevo backend de captura
1. Crear header en `include/stream_linux/`
2. Implementar `IDisplayBackend` interface
3. Registrar en `backend_detector.cpp`
4. Agregar flags CMake si es necesario

### Agregar nuevo encoder
1. Implementar `IVideoEncoder` o `IAudioEncoder`
2. Crear factory function en `video_encoder.cpp`
3. Actualizar CLI options

### Modificar protocolo de señalización
1. Actualizar tipos en `signaling/hub.go`
2. Actualizar cliente WebRTC en Android
3. Actualizar `webrtc_transport.cpp` en host

## Testing

```bash
# Linux Host
cd linux-host/build
ctest --output-on-failure

# Android
cd android-client
./gradlew test

# Signaling
cd signaling-server
go test ./...
```

## Debugging Tips

- Linux: Usar `GDB` o `valgrind` para memory issues
- Android: Android Studio debugger + Logcat
- WebRTC: Habilitar logging verbose en ambos extremos
- Network: Wireshark para analizar tráfico WebSocket/DTLS
