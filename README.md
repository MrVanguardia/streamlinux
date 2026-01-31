<div align="center">

<img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux" width="120" height="120">

# StreamLinux

**Real-time Screen Streaming from Linux to Android**

[![Version](https://img.shields.io/badge/version-0.2.0--alpha-0366d6?style=flat-square)](https://github.com/MrVanguardia/streamlinux/releases/latest)
[![Status](https://img.shields.io/badge/status-alpha-ff6b6b?style=flat-square)](https://github.com/MrVanguardia/streamlinux)
[![License](https://img.shields.io/badge/license-MIT-28a745?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Android-6f42c1?style=flat-square)](https://github.com/MrVanguardia/streamlinux)

[Download](#downloads) | [Installation](#installation) | [Usage](#usage) | [Technologies](#technologies) | [Build](#building-from-source)

</div>

---

> **Alpha Software** - This project is in early development. Expect bugs and incomplete features. Not recommended for production use.

---

## About

StreamLinux enables low-latency screen and audio streaming from Linux to Android devices using WebRTC technology. Designed with privacy and security as core principles, all streaming occurs directly over your local network without external servers.

**Core Capabilities:**

- WebRTC-based streaming with VP8 video and Opus audio codecs
- WiFi (LAN) and USB connection modes
- Hardware-accelerated encoding via VAAPI
- System audio capture through PipeWire and PulseAudio
- QR code-based device pairing
- End-to-end encryption using DTLS-SRTP

---

## Technologies

StreamLinux is built using industry-standard open source technologies:

### Linux Host

| Technology | Purpose |
|------------|---------|
| **Python 3** | Main application logic |
| **GTK4** | Modern graphical user interface |
| **libadwaita** | GNOME design patterns and adaptive layouts |
| **GStreamer** | Media pipeline and streaming framework |
| **WebRTC (webrtcbin)** | Real-time peer-to-peer communication |
| **PipeWire** | Screen capture on Wayland, audio routing |
| **PulseAudio** | Audio capture (legacy support) |
| **GLib/GIO** | Async I/O, D-Bus integration |
| **xdg-desktop-portal** | Secure screen capture permissions |

### Android Client

| Technology | Purpose |
|------------|---------|
| **Kotlin** | Application logic and UI |
| **Jetpack Compose** | Modern declarative UI framework |
| **WebRTC (libwebrtc)** | Real-time streaming reception |
| **MediaCodec** | Hardware-accelerated video decoding |
| **CameraX** | QR code scanning |
| **Coroutines** | Asynchronous programming |
| **Material Design 3** | UI components and theming |

### Signaling Server

| Technology | Purpose |
|------------|---------|
| **Go** | High-performance server runtime |
| **gorilla/websocket** | WebSocket connections |
| **mDNS** | Local network service discovery |
| **TLS** | Secure signaling channel |

### Protocols & Standards

| Protocol | Purpose |
|----------|---------|
| **WebRTC** | Real-time media transport |
| **VP8** | Video codec |
| **Opus** | Audio codec |
| **DTLS-SRTP** | Media encryption |
| **ICE/STUN** | NAT traversal |
| **WebSocket** | Signaling transport |
| **HMAC-SHA256** | Token authentication |

[![Security Policy](https://img.shields.io/badge/Security-Policy-red?style=flat-square&logo=shield)](SECURITY.md)

---

## Downloads

### Latest Release: 0.2.0-alpha

| Platform | Package | Download |
|----------|---------|----------|
| Linux (Fedora/RHEL) | RPM | [streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm](https://github.com/MrVanguardia/streamlinux/releases/download/v0.2.0-alpha/streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm) |
| Android | APK | [streamlinux-0.2.0-debug.apk](https://github.com/MrVanguardia/streamlinux/releases/download/v0.2.0-alpha/streamlinux-0.2.0-debug.apk) |

[![Download](https://img.shields.io/badge/Download-Releases-blue?style=for-the-badge&logo=github)](https://github.com/MrVanguardia/streamlinux/releases)

**Requirements:**

| Linux Host | Android Client |
|------------|----------------|
| Fedora 38+ / Ubuntu 22.04+ | Android 8.0+ (API 26) |
| Wayland or X11 | 2GB RAM minimum |
| GStreamer 1.20+ | WiFi or USB connection |
| PipeWire or PulseAudio | |

---

## Installation

**Linux (Fedora/RHEL)**

```bash
sudo dnf install streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm
```

**Android**

Download the APK, enable installation from unknown sources, and install.

---

## Usage

### WiFi Connection

1. Launch StreamLinux from applications menu
2. Grant screen capture permission when prompted
3. Open StreamLinux on Android device
4. Scan the QR code shown on Linux
5. Streaming starts automatically

### USB Connection

1. Connect Android via USB with debugging enabled
2. Launch StreamLinux on Linux
3. Click "Start USB" in the application
4. Scan QR code from Android app

---

## Security

StreamLinux implements multiple security layers:

| Layer | Implementation |
|-------|----------------|
| Authentication | Cryptographic tokens with automatic expiration |
| Token Integrity | HMAC-SHA256 signatures |
| Media Encryption | DTLS-SRTP (WebRTC standard) |
| Network | Local network only, no external servers |
| Data Collection | None |

**Android Permissions:** Camera (QR scanning), Internet, Network State, Wake Lock. No storage, location, or contact access.

---

## Building from Source

### Linux

```bash
# Dependencies (Fedora)
sudo dnf install python3-gobject gtk4 libadwaita gstreamer1-plugins-bad-free \
    gstreamer1-plugins-good gstreamer1-vaapi pipewire-gstreamer golang

# Build signaling server
cd signaling-server && go build -o signaling-server ./cmd/server/

# Run
cd linux-gui && python3 streamlinux_gui.py
```

### Android

```bash
cd android-client
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk
./gradlew assembleDebug
```

### RPM Package

```bash
cd linux-gui/packaging/rpm && ./build-rpm.sh --rebuild
```

---

## Architecture

```
Linux Host                              Android Client
+------------------+                    +------------------+
|  Screen Capture  |                    |   QR Scanner     |
|  (PipeWire/X11)  |                    +--------+---------+
+--------+---------+                             |
         |                              +--------v---------+
+--------v---------+                    |  Signaling       |
|  GStreamer       |                    |  Client          |
|  VP8 + Opus      |                    +--------+---------+
+--------+---------+                             |
         |                              +--------v---------+
+--------v---------+    WebSocket       |  WebRTC          |
|  Signaling       |<------------------>|  PeerConnection  |
|  Server (Go)     |                    +--------+---------+
+--------+---------+                             |
         |                              +--------v---------+
+--------v---------+    DTLS-SRTP       |  MediaCodec      |
|  WebRTC          |<------------------>|  Decoder         |
+------------------+                    +------------------+
```

---

## Specifications

| Parameter | Value |
|-----------|-------|
| Video Codec | VP8 |
| Audio Codec | Opus |
| Max Resolution | 1920x1080 |
| Max Frame Rate | 60 FPS |
| Video Bitrate | Up to 8 Mbps |
| Audio Bitrate | 320 kbps |
| Latency | 50-150ms typical |

---

## Roadmap

- [ ] Hardware encoding (VAAPI, NVENC)
- [ ] H.264/H.265 codec support
- [ ] Touch input from Android to Linux
- [ ] Multi-monitor selection
- [ ] Flatpak packaging
- [ ] Play Store release

---

## ✨ Contributors

<div align="center">

### Thanks to these amazing people! 

<table>
  <tr>
    <td align="center">
      <a href="https://github.com/MrVanguardia">
        <img src="https://images.weserv.nl/?url=avatars.githubusercontent.com/MrVanguardia?v=4&h=100&w=100&fit=cover&mask=circle&maxage=7d" width="100" height="100"/>
      </a>
      <br/>
      <b><a href="https://github.com/MrVanguardia">MrVanguardia</a></b>
      <br/>
      💻 Code • 🎨 Design • 📦 Packaging • 🚀 Deployment
      <br/>
      <i>Creator & Lead Developer</i>
    </td>
    <td align="center">
      <a href="https://github.com/ST-2">
        <img src="https://images.weserv.nl/?url=avatars.githubusercontent.com/ST-2?v=4&h=100&w=100&fit=cover&mask=circle&maxage=7d" width="100" height="100"/>
      </a>
      <br/>
      <b><a href="https://github.com/ST-2">ST-2</a></b>
      <br/>
      🔒 Security • 🧪 Testing • 🛡️ Privacy
      <br/>
      <i>Security & QA Specialist</i>
    </td>
  </tr>
</table>

</div>

---

## Contributing

Contributions are welcome! Please open an issue first to discuss proposed changes.

If you'd like to contribute, check out our [open issues](https://github.com/MrVanguardia/streamlinux/issues) or propose a new feature.

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

<div align="center">

**StreamLinux** is an open source project by [Vanguardia Studio](https://vanguardiastudio.us)

<br />

Made with ❤️ in Dominican Republic 🇩🇴

</div>
