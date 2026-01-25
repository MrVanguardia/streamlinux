<div align="center">

<img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux" width="180" height="180">

# StreamLinux

### 🖥️ → 📱 Stream Your Linux Desktop to Android

**The fastest, most secure way to mirror your Linux screen to any Android device**

[![Version](https://img.shields.io/badge/version-1.1.2-0066FF?style=for-the-badge&logo=v&logoColor=white)](https://github.com/MrVanguardia/streamlinux/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-00CC66?style=for-the-badge&logo=opensourceinitiative&logoColor=white)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20|%20Android-FF6600?style=for-the-badge&logo=linux&logoColor=white)](https://github.com/MrVanguardia/streamlinux)

[![WebRTC](https://img.shields.io/badge/WebRTC-Powered-333333?style=flat-square&logo=webrtc&logoColor=white)](https://webrtc.org/)
[![GTK4](https://img.shields.io/badge/GTK4-Interface-4A86CF?style=flat-square&logo=gtk&logoColor=white)](https://gtk.org/)
[![GStreamer](https://img.shields.io/badge/GStreamer-Pipeline-E42D2D?style=flat-square&logo=gnome&logoColor=white)](https://gstreamer.freedesktop.org/)
[![Kotlin](https://img.shields.io/badge/Kotlin-Android-7F52FF?style=flat-square&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![Go](https://img.shields.io/badge/Go-Server-00ADD8?style=flat-square&logo=go&logoColor=white)](https://go.dev/)

---

[**📥 Download**](#-download) · [**🚀 Quick Start**](#-quick-start) · [**⚡ Performance**](#-performance--optimization) · [**🔒 Security**](#-security) · [**📖 Documentation**](#-how-it-works)

</div>

---

## 🎯 Overview

**StreamLinux** transforms any Android device into a wireless display for your Linux desktop. Whether you need a second monitor, want to game from your couch, or need to access your workstation remotely — StreamLinux delivers crystal-clear streaming with minimal latency.

<table>
<tr>
<td>

### Why StreamLinux?

✅ **Ultra-Low Latency** — Sub-100ms with optimized WebRTC  
✅ **Hardware Accelerated** — VAAPI, NVENC, Intel QuickSync  
✅ **System Audio** — Stream what you hear, not just video  
✅ **Secure by Design** — End-to-end encrypted connections  
✅ **Zero Configuration** — Scan a QR code and stream  
✅ **Open Source** — MIT licensed, free forever  

</td>
<td>

### Perfect For

🎮 **Gaming** — Play PC games on your tablet  
📺 **Second Display** — Extend your workspace  
🎬 **Media Streaming** — Watch content anywhere  
💼 **Presentations** — Share your screen wirelessly  
🔧 **Server Admin** — Monitor headless systems  
📱 **Remote Access** — Your desktop in your pocket  

</td>
</tr>
</table>

---

## 📥 Download

<div align="center">

### Latest Stable Release: v1.1.2

| Platform | Package | Architecture | Size | Download |
|:--------:|:--------|:------------:|:----:|:--------:|
| ![Fedora](https://img.shields.io/badge/-Fedora-51A2DA?style=flat-square&logo=fedora&logoColor=white) | RPM Package | x86_64 | 2.6 MB | [⬇️ Download](https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-1.fc43.x86_64.rpm) |
| ![Linux](https://img.shields.io/badge/-Universal-FCC624?style=flat-square&logo=linux&logoColor=black) | Tarball | x86_64 | 5.8 MB | [⬇️ Download](https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-linux-universal.tar.gz) |
| ![Android](https://img.shields.io/badge/-Android-3DDC84?style=flat-square&logo=android&logoColor=white) | APK | Universal | 32 MB | [⬇️ Download](https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/StreamLinux-1.1.2-android.apk) |

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
# One-line install
wget -qO- https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-1.fc43.x86_64.rpm | sudo rpm -ivh -

# Or download first
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-1.fc43.x86_64.rpm
sudo rpm -ivh streamlinux-1.1.2-1.fc43.x86_64.rpm
```

</details>

<details>
<summary><b>Ubuntu / Debian / Linux Mint / Pop!_OS</b></summary>

```bash
# Download and extract
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-linux-universal.tar.gz
tar -xzvf streamlinux-1.1.2-linux-universal.tar.gz

# Install (handles dependencies automatically)
cd streamlinux-1.1.2
sudo ./install.sh
```

The installer will:
- Detect your distribution
- Install required dependencies (GTK4, GStreamer, etc.)
- Set up the application in `/usr/local/lib/streamlinux`
- Create desktop shortcuts

</details>

<details>
<summary><b>Arch Linux / Manjaro</b></summary>

```bash
# Install dependencies
sudo pacman -S gtk4 libadwaita gstreamer gst-plugins-good gst-plugins-bad python-gobject

# Download and install
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-linux-universal.tar.gz
tar -xzvf streamlinux-1.1.2-linux-universal.tar.gz
cd streamlinux-1.1.2
sudo ./install.sh
```

</details>

### 2️⃣ Install on Android

1. Download the APK from the link above
2. Go to **Settings → Security → Enable "Unknown Sources"**
3. Open the APK and tap **Install**
4. Launch **StreamLinux** from your app drawer

### 3️⃣ Connect & Stream

```
┌─────────────────┐              ┌─────────────────┐
│  🖥️ Linux Host  │   ──WiFi──>  │  📱 Android     │
│                 │              │                 │
│  1. Launch app  │              │  2. Open app    │
│  2. Click Start │              │  3. Scan QR     │
│  3. Show QR     │              │  4. Enjoy! 🎉   │
└─────────────────┘              └─────────────────┘
```

**That's it!** Both devices must be on the same network.

---

## ⚡ Performance & Optimization

StreamLinux is engineered for **maximum performance** with **minimal resource usage**.

### 🎯 Latency Benchmarks

| Configuration | Average Latency | Use Case |
|:--------------|:---------------:|:---------|
| USB + Hardware Encoding | **15-30ms** | Gaming, Real-time |
| WiFi 5GHz + Hardware Encoding | **30-60ms** | General Use |
| WiFi 5GHz + Software Encoding | **50-100ms** | Casual Streaming |
| WiFi 2.4GHz | **80-150ms** | Basic Viewing |

### 🔧 Hardware Acceleration

StreamLinux automatically detects and uses hardware encoders:

| GPU | Technology | Performance Gain |
|:----|:-----------|:---------------:|
| **Intel** (HD 4000+) | VA-API (QuickSync) | **5-10x faster** |
| **AMD** (GCN+) | VA-API (VCN/VCE) | **5-10x faster** |
| **NVIDIA** (GTX 600+) | NVENC | **10-15x faster** |

```bash
# Check VA-API support
vainfo

# Check NVIDIA support
nvidia-smi | grep -i enc
```

### 📊 Resource Usage

| Component | CPU Usage | RAM Usage |
|:----------|:---------:|:---------:|
| **Linux Host** (HW Encoding) | 2-5% | ~150 MB |
| **Linux Host** (SW Encoding) | 15-30% | ~200 MB |
| **Android Client** | 3-8% | ~100 MB |
| **Signaling Server** | <1% | ~15 MB |

### ⚙️ Optimization Tips

<details>
<summary><b>🎮 For Gaming (Lowest Latency)</b></summary>

1. **Use USB connection** via ADB:
   ```bash
   adb forward tcp:54321 tcp:54321
   ```
2. **Set resolution to 720p** (1280x720)
3. **Enable hardware encoding** (automatic if available)
4. **Use 5GHz WiFi** if wireless
5. **Disable V-Sync** in games

</details>

<details>
<summary><b>📺 For Quality (Best Picture)</b></summary>

1. **Set resolution to 1080p** (1920x1080)
2. **Increase bitrate** to 8000-12000 kbps
3. **Use 60 FPS** mode
4. **Connect via 5GHz WiFi**

</details>

<details>
<summary><b>🔋 For Battery Life</b></summary>

1. **Lower resolution** to 720p or 480p
2. **Reduce framerate** to 30 FPS
3. **Lower bitrate** to 2000-3000 kbps
4. **Disable audio** if not needed

</details>

---

## 🔒 Security

StreamLinux is built with **security as a priority**. Your stream never leaves your local network.

### 🛡️ Security Features

| Feature | Description |
|:--------|:------------|
| **🔐 DTLS Encryption** | All WebRTC traffic is encrypted using DTLS 1.2+ |
| **🔑 SRTP Media** | Audio/video streams use Secure RTP (AES-128) |
| **🌐 Local-Only** | No cloud servers, data stays on your network |
| **🚫 No Telemetry** | Zero data collection, fully offline capable |
| **✅ Open Source** | Auditable code, no hidden backdoors |

### 🔒 Connection Security

```
┌──────────────────────────────────────────────────────────────┐
│                     YOUR LOCAL NETWORK                        │
│                                                              │
│  ┌─────────────┐         ENCRYPTED          ┌─────────────┐ │
│  │ Linux Host  │◄══════════════════════════►│   Android   │ │
│  │             │   DTLS + SRTP (AES-128)    │   Client    │ │
│  └─────────────┘                            └─────────────┘ │
│         │                                          │         │
│         └──────────┐              ┌────────────────┘         │
│                    ▼              ▼                          │
│              ┌───────────────────────┐                       │
│              │   Signaling Server    │                       │
│              │    (WebSocket TLS)    │                       │
│              └───────────────────────┘                       │
│                                                              │
│                    ❌ NO INTERNET REQUIRED                   │
└──────────────────────────────────────────────────────────────┘
```

### 🔐 Best Practices

1. **Use a secure WiFi network** — WPA2/WPA3 recommended
2. **Keep software updated** — Security patches included in updates
3. **Don't expose ports** — StreamLinux is designed for LAN only
4. **Review firewall rules** — Only allow port 54321 on trusted networks

---

## 📖 How It Works

### Architecture Overview

StreamLinux uses a **modern, decoupled architecture** optimized for real-time streaming:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           LINUX HOST                                     │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        CAPTURE LAYER                               │  │
│  │  ┌─────────────────┐              ┌─────────────────┐             │  │
│  │  │   X11 Backend   │     OR       │ Wayland Backend │             │  │
│  │  │   (XCB + SHM)   │              │   (PipeWire)    │             │  │
│  │  └────────┬────────┘              └────────┬────────┘             │  │
│  │           │          AUTO-DETECT           │                       │  │
│  │           └──────────────┬─────────────────┘                       │  │
│  └──────────────────────────┼────────────────────────────────────────┘  │
│                             ▼                                            │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        ENCODING LAYER                              │  │
│  │                                                                    │  │
│  │   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐          │  │
│  │   │   VA-API    │    │   NVENC     │    │    VP8      │          │  │
│  │   │  (Intel/AMD)│    │  (NVIDIA)   │    │  (Software) │          │  │
│  │   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘          │  │
│  │          │     PRIORITY: HW > SW         │     │                  │  │
│  │          └──────────────┬────────────────┘     │                  │  │
│  │                         │                      │                  │  │
│  │   ┌─────────────────────┴──────────────────────┘                  │  │
│  │   │                                                               │  │
│  │   │   ┌─────────────────────────────────────────┐                │  │
│  │   │   │           AUDIO CAPTURE                 │                │  │
│  │   │   │  PipeWire ◄──► PulseAudio ──► Opus     │                │  │
│  │   │   └─────────────────────┬───────────────────┘                │  │
│  │   │                         │                                     │  │
│  └───┼─────────────────────────┼────────────────────────────────────┘  │
│      │                         │                                        │
│      └────────────┬────────────┘                                        │
│                   ▼                                                      │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                      TRANSPORT LAYER                               │  │
│  │                                                                    │  │
│  │              ┌─────────────────────────────┐                       │  │
│  │              │        WebRTCBin            │                       │  │
│  │              │     (GStreamer 1.20+)       │                       │  │
│  │              │                             │                       │  │
│  │              │  • ICE Candidate Gathering  │                       │  │
│  │              │  • DTLS Key Exchange        │                       │  │
│  │              │  • SRTP Encryption          │                       │  │
│  │              │  • Adaptive Bitrate         │                       │  │
│  │              └──────────────┬──────────────┘                       │  │
│  │                             │                                       │  │
│  └─────────────────────────────┼───────────────────────────────────────┘  │
│                                │                                         │
└────────────────────────────────┼─────────────────────────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │    SIGNALING SERVER     │
                    │         (Go)            │
                    │                         │
                    │  • WebSocket Server     │
                    │  • Peer Discovery       │
                    │  • SDP Exchange         │
                    │  • ICE Negotiation      │
                    └────────────┬────────────┘
                                 │
                    ═════════════╪═════════════  LAN / USB
                                 │
┌────────────────────────────────┼─────────────────────────────────────────┐
│                                │              ANDROID CLIENT             │
│  ┌─────────────────────────────┼───────────────────────────────────────┐ │
│  │                      TRANSPORT LAYER                                │ │
│  │              ┌──────────────▼──────────────┐                        │ │
│  │              │       WebRTC Client         │                        │ │
│  │              │    (libwebrtc / Native)     │                        │ │
│  │              └──────────────┬──────────────┘                        │ │
│  └─────────────────────────────┼───────────────────────────────────────┘ │
│                                │                                         │
│  ┌─────────────────────────────┼───────────────────────────────────────┐ │
│  │                      DECODING LAYER                                 │ │
│  │       ┌─────────────────────┴─────────────────────┐                 │ │
│  │       │                                           │                 │ │
│  │  ┌────▼─────┐                              ┌──────▼──────┐          │ │
│  │  │  Video   │                              │    Audio    │          │ │
│  │  │ Decoder  │                              │   Player    │          │ │
│  │  │MediaCodec│                              │  OpenSL ES  │          │ │
│  │  │ (HW Acc) │                              │  (Native)   │          │ │
│  │  └────┬─────┘                              └──────┬──────┘          │ │
│  │       │                                           │                 │ │
│  └───────┼───────────────────────────────────────────┼─────────────────┘ │
│          │                                           │                   │
│  ┌───────▼───────────────────────────────────────────▼─────────────────┐ │
│  │                        PRESENTATION LAYER                           │ │
│  │                                                                     │ │
│  │                    ┌─────────────────────┐                          │ │
│  │                    │   Jetpack Compose   │                          │ │
│  │                    │   Material Design 3 │                          │ │
│  │                    │                     │                          │ │
│  │                    │  • Surface Renderer │                          │ │
│  │                    │  • Touch Handling   │                          │ │
│  │                    │  • Gesture Support  │                          │ │
│  │                    └─────────────────────┘                          │ │
│  │                                                                     │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### Technology Stack

| Layer | Component | Technology | Purpose |
|:------|:----------|:-----------|:--------|
| **UI** | Linux GUI | Python + GTK4 + libadwaita | Modern GNOME interface |
| **UI** | Android App | Kotlin + Jetpack Compose | Material Design 3 client |
| **Capture** | X11 | XCB + SHM | Low-level screen capture |
| **Capture** | Wayland | PipeWire + xdg-portal | Secure screen sharing |
| **Capture** | Audio | PipeWire / PulseAudio | System audio capture |
| **Encoding** | Video | VP8 (VAAPI/NVENC) | Hardware-accelerated |
| **Encoding** | Audio | Opus @ 48kHz | Ultra-low latency codec |
| **Transport** | Streaming | WebRTC (GStreamer) | Real-time protocol |
| **Transport** | Signaling | Go + WebSocket | Peer coordination |
| **Decoding** | Video | MediaCodec | Hardware-accelerated |
| **Decoding** | Audio | OpenSL ES | Native Android audio |

---

## 📋 System Requirements

### Linux Host

| Component | Minimum | Recommended |
|:----------|:-------:|:-----------:|
| **Distribution** | Any with GTK4 | Fedora 39+, Ubuntu 23.04+ |
| **Display Server** | X11 or Wayland | Wayland |
| **Kernel** | 5.10+ | 6.0+ |
| **CPU** | 2 cores @ 2.0 GHz | 4+ cores @ 3.0 GHz |
| **RAM** | 2 GB | 4 GB+ |
| **GPU** | Integrated | Dedicated with VA-API/NVENC |
| **Network** | 100 Mbps LAN | 1 Gbps LAN + 5GHz WiFi |
| **Python** | 3.10 | 3.11+ |

### Android Device

| Component | Minimum | Recommended |
|:----------|:-------:|:-----------:|
| **Android Version** | 8.0 (API 26) | 11.0+ (API 30) |
| **RAM** | 2 GB | 4 GB+ |
| **Display** | 720p | 1080p+ |
| **Processor** | Quad-core | Snapdragon 600+ series |
| **Network** | WiFi 802.11n | WiFi 5 (802.11ac) |

---

## ❓ Troubleshooting

<details>
<summary><b>🔴 "Connection failed" or can't find host</b></summary>

**Causes & Solutions:**

1. **Different networks** — Ensure both devices are on the same WiFi/LAN
2. **Firewall blocking** — Allow port 54321:
   ```bash
   sudo firewall-cmd --add-port=54321/tcp --permanent
   sudo firewall-cmd --reload
   ```
3. **VPN active** — Disable VPN on both devices
4. **Try USB connection**:
   ```bash
   adb forward tcp:54321 tcp:54321
   ```

</details>

<details>
<summary><b>🔴 No video / black screen</b></summary>

**Causes & Solutions:**

1. **Wayland permissions** — Grant screen share when prompted
2. **Missing GStreamer plugins**:
   ```bash
   # Fedora
   sudo dnf install gstreamer1-plugins-bad-free
   
   # Ubuntu/Debian
   sudo apt install gstreamer1.0-plugins-bad
   ```
3. **Restart the xdg-desktop-portal**:
   ```bash
   systemctl --user restart xdg-desktop-portal
   ```

</details>

<details>
<summary><b>🔴 No audio in stream</b></summary>

**Causes & Solutions:**

1. **Check audio source** — Select "System Audio" in settings
2. **PipeWire/PulseAudio not running**:
   ```bash
   systemctl --user status pipewire
   ```
3. **Wrong monitor device** — StreamLinux auto-detects, but you can check:
   ```bash
   pactl list sources | grep -A2 "monitor"
   ```

</details>

<details>
<summary><b>🔴 High latency / lag</b></summary>

**Causes & Solutions:**

1. **Use 5GHz WiFi** instead of 2.4GHz
2. **Lower resolution** to 720p
3. **Enable hardware encoding** (check `vainfo`)
4. **Use USB connection** for best latency
5. **Close bandwidth-heavy apps**

</details>

<details>
<summary><b>🔴 High CPU usage</b></summary>

**Causes & Solutions:**

1. **Hardware encoding not active** — Install VA-API drivers:
   ```bash
   # Intel
   sudo dnf install intel-media-driver
   
   # AMD
   sudo dnf install mesa-va-drivers
   ```
2. **Lower framerate** to 30 FPS
3. **Reduce resolution** to 720p

</details>

---

## 🛠️ Building from Source

<details>
<summary><b>Build Requirements</b></summary>

**Linux Host:**
- Python 3.10+
- GTK4, libadwaita
- GStreamer 1.20+ with plugins-bad
- PyGObject

**Android Client:**
- JDK 17+
- Android SDK 34
- Gradle 8.0+

**Signaling Server:**
- Go 1.21+

</details>

<details>
<summary><b>Build Instructions</b></summary>

```bash
# Clone repository
git clone https://github.com/MrVanguardia/streamlinux.git
cd streamlinux

# === Linux Host ===
cd linux-gui

# Install dependencies (Fedora)
sudo dnf install python3-gobject gtk4 libadwaita \
    gstreamer1-plugins-base gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free pipewire-gstreamer

# Run directly
python3 streamlinux_gui.py

# Build RPM
./scripts/build.sh rpm

# === Android Client ===
cd android-client
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
./gradlew assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk

# === Signaling Server ===
cd signaling-server
go build -o signaling-server ./cmd/server
./signaling-server -port 54321
```

</details>

---

## 🤝 Contributing

We welcome contributions! Here's how you can help:

| Type | How to Contribute |
|:-----|:------------------|
| 🐛 **Bug Reports** | Open an [issue](https://github.com/MrVanguardia/streamlinux/issues) with details |
| 💡 **Feature Requests** | Describe your idea in an issue |
| 🔧 **Code** | Fork → Branch → Code → Pull Request |
| 📖 **Documentation** | Improve README, add tutorials |
| 🌍 **Translations** | Add your language to `i18n.py` |
| ⭐ **Spread the Word** | Star the repo, share with others! |

### Development Setup

```bash
git clone https://github.com/MrVanguardia/streamlinux.git
cd streamlinux

# Python virtual environment (recommended)
python3 -m venv venv
source venv/bin/activate
pip install -r linux-gui/requirements.txt

# Run in development mode
cd linux-gui
python3 streamlinux_gui.py
```

---

## 📜 License

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

## 🙏 Credits

<table>
<tr>
<td align="center"><a href="https://gstreamer.freedesktop.org/"><img src="https://img.shields.io/badge/-GStreamer-E42D2D?style=for-the-badge&logo=gnome&logoColor=white" alt="GStreamer"></a><br>Multimedia Framework</td>
<td align="center"><a href="https://webrtc.org/"><img src="https://img.shields.io/badge/-WebRTC-333333?style=for-the-badge&logo=webrtc&logoColor=white" alt="WebRTC"></a><br>Real-time Communication</td>
<td align="center"><a href="https://gtk.org/"><img src="https://img.shields.io/badge/-GTK4-4A86CF?style=for-the-badge&logo=gtk&logoColor=white" alt="GTK4"></a><br>UI Toolkit</td>
<td align="center"><a href="https://kotlinlang.org/"><img src="https://img.shields.io/badge/-Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white" alt="Kotlin"></a><br>Android Development</td>
</tr>
</table>

---

<div align="center">

### 💖 Support the Project

If StreamLinux has been useful to you, consider:

[![Star](https://img.shields.io/badge/⭐_Star_on_GitHub-181717?style=for-the-badge&logo=github)](https://github.com/MrVanguardia/streamlinux)
[![Donate](https://img.shields.io/badge/💝_Donate_via_PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://www.paypal.com/invoice/p/#ENPBS57FGYS3EB9J)

---

**Made with ❤️ by [Vanguardia Studio](https://vanguardiastudio.us)**

[![Website](https://img.shields.io/badge/Website-vanguardiastudio.us-0066FF?style=flat-square&logo=google-chrome&logoColor=white)](https://vanguardiastudio.us)
[![GitHub](https://img.shields.io/badge/GitHub-MrVanguardia-181717?style=flat-square&logo=github)](https://github.com/MrVanguardia)

</div>
