<div align="center">

<img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux" width="150" height="150">

# StreamLinux

### Stream Your Linux Desktop to Android

**Open-source screen mirroring from Linux to Android using WebRTC**

[![Version](https://img.shields.io/badge/version-0.2.0--alpha-blue?style=for-the-badge)](https://github.com/MrVanguardia/streamlinux/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Android-orange?style=for-the-badge)](https://github.com/MrVanguardia/streamlinux)

</div>

---

## Overview

StreamLinux enables real-time screen and audio streaming from Linux systems to Android devices over local network (WiFi) or USB connection. Built with security and privacy as core principles, all data remains on your local network with end-to-end encryption.

### Key Features

| Feature | Description |
|---------|-------------|
| **WebRTC Streaming** | Low-latency video and audio transmission using industry-standard protocols |
| **Dual Connection Modes** | Connect via WiFi (LAN) or USB for ultra-low latency |
| **Hardware Acceleration** | VAAPI support for efficient encoding |
| **System Audio Capture** | Stream audio from PipeWire/PulseAudio |
| **QR Code Authentication** | Secure, easy device pairing |
| **Privacy-First Design** | No external servers, no accounts, no data collection |

### Technical Specifications

| Parameter | Value |
|-----------|-------|
| Video Codec | VP8 |
| Audio Codec | Opus |
| Resolution | Up to 1920x1080 |
| Frame Rate | Up to 60 FPS |
| Video Bitrate | 8 Mbps (configurable) |
| Audio Bitrate | 320 kbps |
| Typical Latency | 50-150ms |

---

## Downloads

### Version 0.2.0-alpha

| Platform | Package | Architecture | Download |
|----------|---------|--------------|----------|
| **Linux (Fedora/RHEL)** | RPM | x86_64 | [streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm](releases/rpm/streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm) |
| **Android** | APK | arm64-v8a, armeabi-v7a, x86_64 | [streamlinux-0.2.0-debug.apk](releases/apk/streamlinux-0.2.0-debug.apk) |

### System Requirements

**Linux Host:**
- Fedora 38+ / Ubuntu 22.04+ / Arch Linux
- Wayland or X11 display server
- GStreamer 1.20+
- PipeWire or PulseAudio

**Android Client:**
- Android 8.0+ (API 26)
- Minimum 2GB RAM
- WiFi or USB connection

---

## Installation

### Linux (Fedora/RHEL)

```bash
sudo dnf install streamlinux-0.2.0-1.alpha.fc43.x86_64.rpm
```

### Android

1. Download the APK file
2. Enable "Install from unknown sources" in Android settings
3. Install the APK

---

## Usage

### WiFi Connection (LAN)

1. Launch StreamLinux from the applications menu
2. Select the screen to share when prompted
3. Open the StreamLinux app on Android
4. Scan the QR code displayed on the Linux screen
5. Streaming begins automatically

### USB Connection

1. Connect Android device via USB
2. Enable USB debugging on Android
3. Launch StreamLinux on Linux (device detected automatically)
4. Click "Start USB" in the application
5. Open Android app and scan the QR code

---

## Security

StreamLinux implements multiple security layers to protect your data:

### Authentication

- **Cryptographic Tokens**: Generated using `secrets.token_urlsafe()` (CSPRNG)
- **Token Expiration**: Automatic expiration with 60-second rotation
- **HMAC Signatures**: Token integrity verification using HMAC-SHA256
- **Timing-Safe Comparison**: Protection against timing attacks

### Encryption

- **DTLS-SRTP**: All media encrypted end-to-end (WebRTC standard)
- **No External Servers**: Direct peer-to-peer connection
- **Local Network Only**: Connections restricted to LAN addresses

### Privacy

| Aspect | Implementation |
|--------|----------------|
| Data Collection | None |
| External Servers | None required |
| Account Required | No |
| Internet Required | No (optional for STUN) |
| Telemetry | None |

### Android Permissions

The Android client requests only essential permissions:
- `INTERNET` - Network communication
- `ACCESS_NETWORK_STATE` - Connection status
- `CAMERA` - QR code scanning only
- `WAKE_LOCK` - Prevent screen timeout during streaming

The app does NOT request storage, location, or contact permissions.

---

## Architecture

```
Linux Host                           Android Client
+------------------+                 +------------------+
|  Screen Capture  |                 |   QR Scanner     |
|  (PipeWire/X11)  |                 |                  |
+--------+---------+                 +--------+---------+
         |                                    |
+--------v---------+                 +--------v---------+
|  GStreamer       |                 |  Signaling       |
|  VP8 + Opus      |                 |  Client          |
+--------+---------+                 +--------+---------+
         |                                    |
+--------v---------+    WebSocket    +--------v---------+
|  Signaling       |<--------------->|  WebRTC          |
|  Server (Go)     |                 |  PeerConnection  |
+--------+---------+                 +--------+---------+
         |                                    |
+--------v---------+    DTLS-SRTP    +--------v---------+
|  WebRTC          |<--------------->|  MediaCodec      |
|  webrtcbin       |                 |  Decoder         |
+------------------+                 +------------------+
```

---

## Building from Source

### Linux Host

```bash
# Install dependencies (Fedora)
sudo dnf install python3-gobject gtk4 libadwaita gstreamer1-plugins-bad-free \
    gstreamer1-plugins-good gstreamer1-vaapi pipewire-gstreamer golang

# Build signaling server
cd signaling-server
go build -o signaling-server ./cmd/server/

# Run from source
cd linux-gui
python3 streamlinux_gui.py
```

### Android Client

```bash
# Requires Android SDK and Java 21
cd android-client
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk
./gradlew assembleDebug
```

### RPM Package

```bash
cd linux-gui/packaging/rpm
./build-rpm.sh --rebuild
```

---

## Configuration

Configuration file location: `~/.config/streamlinux/settings.json`

```json
{
    "port": 54321,
    "resolution": "1920x1080",
    "framerate": 60,
    "bitrate": 8000000,
    "audio_enabled": true,
    "audio_bitrate": 320000
}
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| QR code not scanning | Ensure camera permission is granted; improve lighting |
| No video on Android | Check if screen capture permission was granted on Linux |
| Connection timeout | Verify both devices are on the same network |
| Audio not working | Check PipeWire/PulseAudio monitor availability |
| High latency | Use USB connection for lower latency; reduce resolution |

---

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/improvement`)
3. Commit changes (`git commit -am 'Add improvement'`)
4. Push to branch (`git push origin feature/improvement`)
5. Open a Pull Request

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## Acknowledgments

- [GStreamer](https://gstreamer.freedesktop.org/) - Media framework
- [WebRTC](https://webrtc.org/) - Real-time communication
- [GTK4](https://gtk.org/) - User interface toolkit
- [libadwaita](https://gnome.pages.gitlab.gnome.org/libadwaita/) - GNOME design patterns

---

<div align="center">

**StreamLinux** - Developed by [Vanguardia Studio](https://vanguardiastudio.us)

</div>
