<div align="center">

<img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux" width="180" height="180">

# StreamLinux

### 🖥️ → 📱 Stream Your Linux Desktop to Android

**An open-source project for mirroring your Linux screen to Android devices**

[![Version](https://img.shields.io/badge/version-0.1.0--alpha-orange?style=for-the-badge&logo=v&logoColor=white)](https://github.com/MrVanguardia/streamlinux/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-00CC66?style=for-the-badge&logo=opensourceinitiative&logoColor=white)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20|%20Android-FF6600?style=for-the-badge&logo=linux&logoColor=white)](https://github.com/MrVanguardia/streamlinux)
[![Status](https://img.shields.io/badge/status-Alpha-red?style=for-the-badge)](https://github.com/MrVanguardia/streamlinux)

[![WebRTC](https://img.shields.io/badge/WebRTC-Powered-333333?style=flat-square&logo=webrtc&logoColor=white)](https://webrtc.org/)
[![GTK4](https://img.shields.io/badge/GTK4-Interface-4A86CF?style=flat-square&logo=gtk&logoColor=white)](https://gtk.org/)
[![GStreamer](https://img.shields.io/badge/GStreamer-Pipeline-E42D2D?style=flat-square&logo=gnome&logoColor=white)](https://gstreamer.freedesktop.org/)

---

[**📥 Download**](#-download) · [**🚀 Quick Start**](#-quick-start) · [**📖 Documentation**](#-how-it-works) · [**🤝 Contributing**](#-contributing)

</div>

---

> ⚠️ **Alpha Software Notice**
> 
> StreamLinux is currently in **early alpha development**. This means:
> - **Not production-ready**: Expect bugs and incomplete features
> - **Security not audited**: While we use standard WebRTC encryption, the codebase has not been professionally audited
> - **Breaking changes possible**: APIs and configuration may change between versions
> - **AI-assisted development**: Much of this codebase was developed with AI assistance — contributions, reviews, and improvements from experienced developers are very welcome!
> 
> We're being transparent about the project's state. If you find issues or have suggestions, please [open an issue](https://github.com/MrVanguardia/streamlinux/issues) or [contribute](CONTRIBUTING.md)!

---

## 🎯 Overview

**StreamLinux** aims to transform any Android device into a wireless display for your Linux desktop. Whether you need a second monitor, want to stream media to your tablet, or need to access your workstation remotely — StreamLinux provides an open-source solution using WebRTC technology.

<table>
<tr>
<td>

### Features

✅ **WebRTC-based** — Uses standard WebRTC for streaming  
✅ **Hardware Acceleration** — Supports VAAPI, NVENC when available  
✅ **System Audio** — Can capture PipeWire/PulseAudio output  
✅ **QR Code Connection** — Easy device pairing  
✅ **Open Source** — MIT licensed  

</td>
<td>

### Use Cases

🎮 **Gaming** — Play PC games on your tablet  
📺 **Second Display** — Extend your workspace  
🎬 **Media Streaming** — Watch content on another device  
💼 **Presentations** — Share your screen wirelessly  
📱 **Remote Access** — View your desktop remotely  

</td>
</tr>
</table>

---

## 📥 Download

<div align="center">

### Latest Release: v0.1.0-alpha

| Platform | Package | Architecture | Size | Download |
|:--------:|:--------|:------------:|:----:|:--------:|
| ![Fedora](https://img.shields.io/badge/-Fedora-51A2DA?style=flat-square&logo=fedora&logoColor=white) | RPM Package | x86_64 | ~2.6 MB | [⬇️ Download](https://github.com/MrVanguardia/streamlinux/releases/latest) |
| ![Linux](https://img.shields.io/badge/-Universal-FCC624?style=flat-square&logo=linux&logoColor=black) | Tarball | x86_64 | ~5.8 MB | [⬇️ Download](https://github.com/MrVanguardia/streamlinux/releases/latest) |
| ![Android](https://img.shields.io/badge/-Android-3DDC84?style=flat-square&logo=android&logoColor=white) | APK | Universal | ~32 MB | [⬇️ Download](https://github.com/MrVanguardia/streamlinux/releases/latest) |

</div>

> 💡 **Which package should I choose?**
> - **Fedora/RHEL/CentOS/Rocky**: Use the RPM package
> - **Ubuntu/Debian/Mint/Pop!_OS/Arch**: Use the Universal Tarball
> - **Android 8.0+**: Use the APK (enable "Unknown sources" in settings)

---

## 🚀 Quick Start

### 1️⃣ Install on Linux

<details open>
<summary><b>Fedora / RHEL / CentOS / Rocky Linux</b></summary>

```bash
# Download the RPM
wget https://github.com/MrVanguardia/streamlinux/releases/latest/download/streamlinux-0.1.0-1.fc43.x86_64.rpm

# Install
sudo rpm -ivh streamlinux-0.1.0-1.fc43.x86_64.rpm
```

</details>

<details>
<summary><b>Ubuntu / Debian / Other Distributions</b></summary>

```bash
# Download universal tarball
wget https://github.com/MrVanguardia/streamlinux/releases/latest/download/streamlinux-0.1.0-linux-universal.tar.gz

# Extract and install
tar -xzf streamlinux-0.1.0-linux-universal.tar.gz
cd streamlinux-0.1.0-linux
./install.sh
```

</details>

### 2️⃣ Install on Android

1. Download the APK from the [releases page](https://github.com/MrVanguardia/streamlinux/releases)
2. Enable "Install from unknown sources" in Android settings
3. Install the APK

### 3️⃣ Start Streaming

1. Launch **StreamLinux** on your Linux machine
2. Click **"Start Server"**
3. Scan the QR code with the Android app
4. Enjoy your stream!

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         LINUX HOST                              │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                   StreamLinux GUI                        │   │
│  │                (Python 3 + GTK4/Adwaita)                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                            │                                    │
│  ┌─────────────────────────┴─────────────────────────────┐     │
│  │                   GStreamer Pipeline                   │     │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │     │
│  │  │ScreenCapture │  │ VideoEncoder │  │ webrtcbin   │  │     │
│  │  │ (PipeWire/   │──│ (VP8/H264)   │──│ (WebRTC)    │  │     │
│  │  │  Portal)     │  │              │  │             │  │     │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │     │
│  │  ┌──────────────┐  ┌──────────────┐         │         │     │
│  │  │AudioCapture  │  │ AudioEncoder │         │         │     │
│  │  │ (PipeWire/   │──│ (Opus)       │─────────┘         │     │
│  │  │  PulseAudio) │  │              │                   │     │
│  │  └──────────────┘  └──────────────┘                   │     │
│  └───────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ WebRTC (DTLS/SRTP)
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    SIGNALING SERVER (Go)                        │
│                   WebSocket Connection                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ WebRTC (DTLS/SRTP)
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      ANDROID CLIENT                             │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Kotlin + Jetpack Compose UI                 │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐    │   │
│  │  │ WebRTC Peer  │──│ MediaCodec   │──│ SurfaceView │    │   │
│  │  │ Connection   │  │ (Decoder)    │  │ (Display)   │    │   │
│  │  └──────────────┘  └──────────────┘  └─────────────┘    │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Technology Stack

| Component | Technology | Purpose |
|:----------|:-----------|:--------|
| **Linux GUI** | Python 3 + GTK4 + libadwaita | Native desktop application |
| **Video Pipeline** | GStreamer | Screen capture and encoding |
| **Audio Capture** | PipeWire / PulseAudio | System audio |
| **Video Codec** | VP8 (libvpx) | Video encoding |
| **Audio Codec** | Opus | Audio encoding |
| **Transport** | WebRTC (via GStreamer webrtcbin) | Real-time streaming |
| **Signaling** | Go + gorilla/websocket | Connection negotiation |
| **Android App** | Kotlin + Jetpack Compose | Mobile client |

---

## 🔒 Security Notes

StreamLinux uses standard WebRTC security mechanisms:

| Feature | Description |
|:--------|:------------|
| **DTLS** | WebRTC traffic uses DTLS encryption (version depends on GStreamer/OpenSSL) |
| **SRTP** | Media streams use Secure RTP |
| **Local Network** | Designed for LAN use, no cloud servers involved |
| **No Telemetry** | No data collection |

### ⚠️ Security Disclaimer

> **This software has NOT been professionally security audited.**
> 
> - Security depends on the underlying GStreamer webrtcbin implementation
> - For sensitive use cases, please evaluate the security yourself or wait for community audits
> - We welcome security researchers to review and report issues via [SECURITY.md](SECURITY.md)
> 
> **Recommendations:**
> - Use on trusted networks only
> - Keep all software components updated
> - Consider this alpha software for the security model as well

---

## ⚙️ Configuration

### GUI Settings

The application provides a settings dialog where you can configure:

- **Video Resolution**: 480p, 720p, 1080p
- **Frame Rate**: 30 or 60 FPS
- **Video Bitrate**: 1000-20000 kbps
- **Audio Settings**: Enable/disable, sample rate
- **Encoder Selection**: Software (x264) or Hardware (VAAPI)

### Tuning Tips

<details>
<summary><b>🎮 For Gaming (Lower Latency)</b></summary>

1. Use 720p resolution
2. Set framerate to 60 FPS
3. Lower bitrate to 4000-6000 kbps
4. Use hardware encoder if available

</details>

<details>
<summary><b>📺 For Quality (Best Picture)</b></summary>

1. Use 1080p resolution
2. Increase bitrate to 8000-12000 kbps
3. Use 60 FPS mode
4. Connect via 5GHz WiFi

</details>

<details>
<summary><b>🔋 For Battery Life</b></summary>

1. Lower resolution to 720p or 480p
2. Reduce framerate to 30 FPS
3. Lower bitrate to 2000-3000 kbps
4. Disable audio if not needed

</details>

---

## 🔧 Troubleshooting

### Common Issues

<details>
<summary><b>❌ "Failed to start screen capture"</b></summary>

This usually means PipeWire screen capture portal is not available.

**Solutions:**
1. Make sure `xdg-desktop-portal` is installed and running
2. For Wayland: ensure `xdg-desktop-portal-gnome` (or your DE's portal) is installed
3. Try restarting the portal: `systemctl --user restart xdg-desktop-portal`

</details>

<details>
<summary><b>❌ No audio in stream</b></summary>

**Solutions:**
1. Ensure PipeWire or PulseAudio is running
2. Check that the correct audio device is selected in settings
3. Try restarting the audio server

</details>

<details>
<summary><b>❌ Android app can't connect</b></summary>

**Solutions:**
1. Ensure both devices are on the same WiFi network
2. Check if firewall is blocking the connection (ports 8080, 8443)
3. Try scanning the QR code again
4. Restart the server on Linux

</details>

<details>
<summary><b>❌ High latency or stuttering</b></summary>

**Solutions:**
1. Lower the resolution and/or bitrate
2. Use 5GHz WiFi instead of 2.4GHz
3. Move closer to the WiFi router
4. Close bandwidth-heavy applications

</details>

---

## 🤝 Contributing

**We welcome contributions!** This project is in early development and needs help in many areas:

- 🐛 **Bug reports and fixes**
- 📝 **Documentation improvements**
- 🔒 **Security audits and reviews**
- ⚡ **Performance optimizations**
- 🌍 **Translations**
- 💡 **Feature suggestions**

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```bash
# Clone the repository
git clone https://github.com/MrVanguardia/streamlinux.git
cd streamlinux

# Linux GUI (Python)
cd linux-gui
pip install -r requirements.txt
python streamlinux_gui.py

# Signaling Server (Go)
cd signaling-server
go build -o signaling-server ./cmd/server

# Android (Android Studio)
# Open android-client/ in Android Studio
```

---

## 🗺️ Roadmap

### Current Focus (v0.1.x)

- [ ] Stability improvements
- [ ] Better error handling
- [ ] Documentation
- [ ] Security review

### Future Ideas

- [ ] H.264 hardware encoding optimization
- [ ] Region selection (capture specific window/area)
- [ ] Multiple client support
- [ ] iOS client
- [ ] Flatpak packaging
- [ ] Touch input forwarding

---

## 📜 License

```
MIT License

Copyright (c) 2024 Vanguardia Studio

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

## 🙏 Acknowledgments

This project is built on top of excellent open-source technologies:

- [GStreamer](https://gstreamer.freedesktop.org/) — Multimedia framework
- [WebRTC](https://webrtc.org/) — Real-time communication
- [GTK4](https://gtk.org/) — UI toolkit
- [Kotlin](https://kotlinlang.org/) — Android development

---

<div align="center">

**Made by [Vanguardia Studio](https://vanguardiastudio.us)**

[![Website](https://img.shields.io/badge/Website-vanguardiastudio.us-0066FF?style=flat-square&logo=google-chrome&logoColor=white)](https://vanguardiastudio.us)
[![GitHub](https://img.shields.io/badge/GitHub-MrVanguardia-181717?style=flat-square&logo=github)](https://github.com/MrVanguardia)

---

⭐ If you find this project useful, please consider giving it a star!

</div>
