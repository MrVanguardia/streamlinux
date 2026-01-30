<div align="center">

<img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux" width="120" height="120">

# StreamLinux

**Real-time Screen Streaming from Linux to Android**

[![Version](https://img.shields.io/badge/version-0.2.0--alpha-0366d6?style=flat-square)](https://github.com/MrVanguardia/streamlinux/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-28a745?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Android-6f42c1?style=flat-square)](https://github.com/MrVanguardia/streamlinux)

[Download](#downloads) | [Installation](#installation) | [Usage](#usage) | [Build](#building-from-source)

</div>

---

## About

StreamLinux enables low-latency screen and audio streaming from Linux to Android devices using WebRTC technology. Designed for privacy and security, all streaming occurs directly over your local network without external servers.

**Core Capabilities:**

- WebRTC-based streaming with VP8 video and Opus audio codecs
- WiFi (LAN) and USB connection modes
- Hardware-accelerated encoding via VAAPI
- System audio capture through PipeWire and PulseAudio
- QR code-based device pairing
- End-to-end encryption using DTLS-SRTP

---

## Downloads

### Latest Release: 0.2.0-alpha

| Platform | Package | Download |
|----------|---------|----------|
| Linux (Fedora/RHEL) | RPM | [streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm](releases/rpm/streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm) |
| Android | APK | [streamlinux-0.2.0-debug.apk](releases/apk/streamlinux-0.2.0-debug.apk) |

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

StreamLinux implements security measures to protect your data:

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
|  Server          |                    +--------+---------+
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

## Troubleshooting

| Issue | Solution |
|-------|----------|
| QR code not scanning | Check camera permission and lighting |
| No video | Verify screen capture permission on Linux |
| Connection timeout | Ensure devices are on same network |
| No audio | Check PipeWire/PulseAudio configuration |
| High latency | Use USB connection or reduce resolution |

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

<div align="center">

Developed by [Vanguardia Studio](https://vanguardiastudio.us)

</div>
