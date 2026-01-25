# StreamLinux

**Sistema de Streaming de Pantalla y Audio Linux → Android**

StreamLinux permite transmitir la pantalla y el audio de un sistema Linux a dispositivos Android en tiempo real con baja latencia, utilizando WebRTC como protocolo de transporte.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Android-green.svg)
![Version](https://img.shields.io/badge/version-1.0.0-orange.svg)

> ⚠️ **VERSIÓN EXPERIMENTAL** - Esta es una versión en desarrollo activo.

## 📥 Instalación Rápida

### Linux (Fedora, Ubuntu, Mint, Arch, openSUSE)

```bash
# 1. Descargar
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.0.0/streamlinux-1.0.0-linux-universal.tar.gz

# 2. Extraer
tar -xzf streamlinux-1.0.0-linux-universal.tar.gz

# 3. Entrar a la carpeta
cd streamlinux

# 4. Instalar
sudo bash install.sh
```

**⚠️ IMPORTANTE:** Usar `sudo bash install.sh` (no `sudo ./install.sh`)

### Android

Descarga el APK desde [Releases](https://github.com/MrVanguardia/streamlinux/releases/tag/v1.0.0)

### Si hay problemas en Linux:
```bash
# Diagnóstico
bash install.sh diagnose

# Instalar dependencias Python manualmente
pip3 install --user pillow qrcode websocket-client

# Para Ubuntu/Mint (GTK4)
sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1
```

## 🎯 Características

- **Captura de Pantalla**
  - Soporte para X11 (XCB/SHM) y Wayland (xdg-desktop-portal/PipeWire)
  - Auto-detección del backend gráfico
  - Captura de monitor específico o pantalla completa

- **Captura de Audio**
  - PipeWire (preferido) y PulseAudio (fallback)
  - Captura de salida del sistema (monitor)

- **Codificación de Video**
  - H.264 con aceleración por hardware (VAAPI, NVENC, AMF)
  - Fallback a codificación por software (libx264)
  - Configuración adaptativa de bitrate

- **Codificación de Audio**
  - Codec Opus para baja latencia
  - 48kHz, estéreo

- **Transporte**
  - WebRTC con DTLS para seguridad
  - Conexión P2P cuando es posible
  - Servidor de señalización WebSocket

- **Cliente Android**
  - Decodificación por hardware usando MediaCodec
  - Sincronización A/V precisa
  - Descubrimiento automático en LAN
  - Conexión por código QR

## 📋 Requisitos

### Linux Host

```bash
# Dependencias de compilación
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libx11-dev \
    libxcb1-dev \
    libxcb-shm0-dev \
    libxcb-randr0-dev \
    libpipewire-0.3-dev \
    libpulse-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libopus-dev \
    libva-dev \
    libgio2.0-cil-dev
```

### Android Client

- Android Studio Arctic Fox o superior
- NDK 25 o superior
- SDK mínimo: API 24 (Android 7.0)

### Servidor de Señalización

- Go 1.21 o superior

## 🚀 Compilación

### Linux Host

```bash
cd linux-host
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Opciones de CMake:
- `-DENABLE_X11=ON/OFF` - Soporte X11
- `-DENABLE_WAYLAND=ON/OFF` - Soporte Wayland  
- `-DENABLE_VAAPI=ON/OFF` - Aceleración VAAPI
- `-DENABLE_NVENC=ON/OFF` - Aceleración NVIDIA

### Cliente Android

```bash
cd android-client
./gradlew assembleRelease
```

### Servidor de Señalización

```bash
cd signaling-server
go build -o signaling-server ./cmd/server
```

## 📖 Uso

### 1. Iniciar el Servidor de Señalización

```bash
./signaling-server --port 8080 --mdns
```

### 2. Iniciar el Host Linux

```bash
./stream_linux --server ws://localhost:8080/ws --room my-room
```

Opciones disponibles:
```
--server, -s      URL del servidor de señalización
--room, -r        ID de la sala (auto-generado si no se especifica)
--monitor, -m     Monitor a capturar (0 = primario)
--fps, -f         Frames por segundo (default: 60)
--bitrate, -b     Bitrate de video en kbps (default: 5000)
--quality, -q     Preajuste de calidad: low, medium, high, ultra
--audio           Habilitar captura de audio (default: true)
--hardware        Usar codificación por hardware (default: true)
```

### 3. Conectar desde Android

1. Abrir la app StreamLinux
2. La app descubrirá automáticamente hosts en la red local
3. Alternativamente, escanear el código QR mostrado por el host
4. Seleccionar el host para iniciar la transmisión

## 🏗️ Arquitectura

```
┌─────────────────────────────────────────────────────────────────┐
│                         LINUX HOST                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ Display      │  │ Audio        │  │ Encoder              │   │
│  │ Backend      │  │ Capture      │  │ (H.264/Opus)         │   │
│  │ (X11/Wayland)│  │ (PipeWire/   │  │ HW: VAAPI/NVENC/AMF  │   │
│  │              │  │  PulseAudio) │  │ SW: libx264/libopus  │   │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘   │
│         │                 │                      │               │
│         └────────────┬────┴──────────────────────┘               │
│                      │                                           │
│              ┌───────▼────────┐                                  │
│              │ A/V Synchronizer│                                  │
│              └───────┬────────┘                                  │
│                      │                                           │
│              ┌───────▼────────┐                                  │
│              │ WebRTC Transport│                                  │
│              └───────┬────────┘                                  │
└──────────────────────┼───────────────────────────────────────────┘
                       │ DTLS/SRTP
                       │
              ┌────────▼────────┐
              │ Signaling Server │
              │   (WebSocket)    │
              └────────┬────────┘
                       │
┌──────────────────────┼───────────────────────────────────────────┐
│                      │              ANDROID CLIENT               │
│              ┌───────▼────────┐                                  │
│              │ WebRTC Client   │                                  │
│              └───────┬────────┘                                  │
│                      │                                           │
│              ┌───────▼────────┐                                  │
│              │ A/V Synchronizer│                                  │
│              └───────┬────────┘                                  │
│                      │                                           │
│         ┌────────────┴────────────┐                              │
│         │                         │                              │
│  ┌──────▼───────┐         ┌───────▼──────┐                       │
│  │ Video Decoder │         │ Audio Decoder│                       │
│  │ (MediaCodec)  │         │ (Opus/OpenSL)│                       │
│  └──────┬───────┘         └───────┬──────┘                       │
│         │                         │                              │
│  ┌──────▼───────┐         ┌───────▼──────┐                       │
│  │ SurfaceView   │         │ AudioTrack   │                       │
│  └──────────────┘         └──────────────┘                       │
└──────────────────────────────────────────────────────────────────┘
```

## 📁 Estructura del Proyecto

```
streamlinux/
├── linux-host/              # Aplicación host Linux (C++)
│   ├── include/             # Headers
│   ├── src/                 # Código fuente
│   │   ├── capture/         # Backends de captura
│   │   ├── audio/           # Captura de audio
│   │   ├── encoding/        # Codificadores
│   │   ├── sync/            # Sincronización A/V
│   │   ├── transport/       # WebRTC
│   │   └── cli/             # Interfaz de línea de comandos
│   └── CMakeLists.txt
│
├── android-client/          # Cliente Android (Kotlin + NDK)
│   ├── app/
│   │   └── src/main/
│   │       ├── java/        # Código Kotlin
│   │       ├── cpp/         # Código nativo C++
│   │       └── res/         # Recursos
│   └── build.gradle.kts
│
└── signaling-server/        # Servidor de señalización (Go)
    ├── cmd/server/          # Punto de entrada
    └── internal/            # Paquetes internos
        ├── signaling/       # Lógica de señalización
        ├── discovery/       # mDNS
        └── qr/              # Generación de QR
```

## 🔧 Configuración

### Archivo de Configuración (Linux Host)

Crear `~/.config/streamlinux/config.toml`:

```toml
[video]
fps = 60
bitrate = 5000
preset = "medium"  # ultrafast, superfast, veryfast, faster, fast, medium
hardware = true

[audio]
enabled = true
sample_rate = 48000
channels = 2

[network]
signaling_server = "ws://localhost:8080/ws"
stun_server = "stun:stun.l.google.com:19302"

[capture]
backend = "auto"  # auto, x11, wayland
monitor = 0
```

## 🐛 Solución de Problemas

### "No se detecta el backend gráfico"

```bash
# Verificar sesión
echo $XDG_SESSION_TYPE

# Para X11
xdpyinfo | head

# Para Wayland
echo $WAYLAND_DISPLAY
```

### "Error de codificación de hardware"

```bash
# Verificar VAAPI
vainfo

# Verificar NVIDIA
nvidia-smi
```

### "Audio no capturado"

```bash
# Listar dispositivos PipeWire
pw-cli list-objects

# Listar dispositivos PulseAudio  
pactl list sources
```

## 📜 Licencia

Este proyecto está licenciado bajo la Licencia MIT - ver el archivo [LICENSE](LICENSE) para más detalles.

## 🤝 Contribuir

Las contribuciones son bienvenidas. Por favor, abra un issue primero para discutir los cambios que desea realizar.

1. Fork el repositorio
2. Cree su rama de características (`git checkout -b feature/AmazingFeature`)
3. Commit sus cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abra un Pull Request
