<p align="center">
  <img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux Logo" width="120" height="120">
</p>

<h1 align="center">StreamLinux</h1>

<p align="center">
  <strong>🖥️ Stream your Linux screen to Android devices with ultra-low latency</strong>
</p>

<p align="center">
  <a href="#-installation">Installation</a> •
  <a href="#-features">Features</a> •
  <a href="#-usage">Usage</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-contributing">Contributing</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.1.1-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Android-orange.svg" alt="Platform">
  <img src="https://img.shields.io/badge/python-3.10+-yellow.svg" alt="Python">
  <img src="https://img.shields.io/badge/kotlin-1.9+-purple.svg" alt="Kotlin">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/⚠️_EXPERIMENTAL-This_is_a_beta_version-red.svg" alt="Experimental">
</p>

---

## 📦 Installation

### Quick Install (Recommended)

<details>
<summary><strong>🐧 Fedora / RHEL / CentOS (RPM)</strong></summary>

```bash
# Download and install the RPM package
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.1/streamlinux-1.1.1-1.fc43.noarch.rpm
sudo rpm -ivh streamlinux-1.1.1-1.fc43.noarch.rpm
```

**To uninstall:**
```bash
sudo rpm -e streamlinux
```

</details>

<details>
<summary><strong>🍃 Linux Mint / Ubuntu / Debian (Universal Installer)</strong></summary>

```bash
# Download and run the universal installer
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.1/install.sh
chmod +x install.sh
./install.sh
```

> ⚠️ **Note:** The universal installer is **EXPERIMENTAL** and has only been tested on **Linux Mint 21/22** and **Fedora 39/40**. It may work on other distributions but is not guaranteed. Please [report issues](https://github.com/MrVanguardia/streamlinux/issues)!

**To uninstall:**
```bash
./install.sh --uninstall
```

</details>

<details>
<summary><strong>📱 Android App</strong></summary>

1. Download the APK from the [releases page](https://github.com/MrVanguardia/streamlinux/releases/latest)
2. On your Android device, enable **"Install from unknown sources"** in Settings
3. Open the downloaded APK and install it
4. Launch **StreamLinux** and scan the QR code from your Linux host

**Direct download:**
```
https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.1/StreamLinux-1.1.1-android.apk
```

</details>

---

## ✨ Features

<table>
<tr>
<td width="50%">

### 🖥️ Linux Host
- **Screen Capture**
  - X11 (XCB/SHM) support
  - Wayland (xdg-desktop-portal/PipeWire)
  - Auto-detection of display server
  
- **Video Encoding**
  - Hardware acceleration (VAAPI, NVENC)
  - Software fallback (H.264)
  - Adaptive bitrate

- **Audio Streaming**
  - PipeWire & PulseAudio support
  - System audio capture
  - Opus codec (low latency)

</td>
<td width="50%">

### 📱 Android Client
- **Video Decoding**
  - Hardware acceleration (MediaCodec)
  - Smooth playback
  
- **Connection**
  - QR code scanning
  - LAN auto-discovery
  - USB connection (ADB port forwarding)
  
- **Features**
  - Full-screen mode
  - Touch pass-through (coming soon)
  - Multi-language (EN/ES)

</td>
</tr>
</table>

### 🌐 Multi-Language Architecture

| Component | Language | Why? |
|-----------|----------|------|
| **Linux GUI** | Python + GTK4 | Native desktop integration, GStreamer access, libadwaita UI |
| **Android Client** | Kotlin | Official Android language, native MediaCodec access |
| **Signaling Server** | Go | Excellent for concurrent WebSocket connections, single binary |

---

## 🚀 Usage

### 1. Start StreamLinux on Linux

```bash
# Launch from terminal
streamlinux-gui

# Or from your applications menu, search for "StreamLinux"
```

### 2. Connect from Android

1. Open the **StreamLinux** app on your Android device
2. Make sure both devices are on the **same network**
3. **Scan the QR code** displayed on your Linux host
4. Enjoy your stream! 🎉

### Connection Options

| Method | Description |
|--------|-------------|
| **📶 WiFi** | Both devices on the same local network |
| **🔌 USB** | Connect via ADB port forwarding (lower latency) |
| **📷 QR Code** | Quick connection by scanning |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         LINUX HOST                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ Display      │  │ Audio        │  │ Encoder              │   │
│  │ Backend      │  │ Capture      │  │ (H.264/Opus)         │   │
│  │ (X11/Wayland)│  │ (PipeWire/   │  │ HW: VAAPI/NVENC      │   │
│  │              │  │  PulseAudio) │  │ SW: libx264          │   │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘   │
│         │                 │                      │               │
│         └────────────┬────┴──────────────────────┘               │
│                      │                                           │
│              ┌───────▼────────┐                                  │
│              │ WebRTC Transport│                                  │
│              └───────┬────────┘                                  │
└──────────────────────┼───────────────────────────────────────────┘
                       │ DTLS/SRTP
              ┌────────▼────────┐
              │ Signaling Server │
              │   (WebSocket)    │
              └────────┬────────┘
                       │
┌──────────────────────┼───────────────────────────────────────────┐
│              ┌───────▼────────┐          ANDROID CLIENT          │
│              │ WebRTC Client   │                                  │
│              └───────┬────────┘                                  │
│         ┌────────────┴────────────┐                              │
│  ┌──────▼───────┐         ┌───────▼──────┐                       │
│  │ Video Decoder │         │ Audio Decoder│                       │
│  │ (MediaCodec)  │         │ (Opus)       │                       │
│  └──────────────┘         └──────────────┘                       │
└──────────────────────────────────────────────────────────────────┘
```

---

## 📋 Requirements

### Linux Host

| Requirement | Version |
|-------------|---------|
| Python | 3.10+ |
| GTK | 4.0+ |
| libadwaita | 1.0+ |
| GStreamer | 1.20+ |
| PipeWire | 0.3+ |

### Android Client

| Requirement | Version |
|-------------|---------|
| Android | 7.0+ (API 24) |
| Architecture | arm64-v8a, armeabi-v7a |

---

## 🔧 Building from Source

<details>
<summary><strong>Linux Host (Python GUI)</strong></summary>

```bash
# Install dependencies (Fedora)
sudo dnf install python3 python3-gobject gtk4 libadwaita

# Install Python packages
pip install qrcode pillow websocket-client

# Run directly
cd linux-gui
python3 streamlinux_gui.py
```

</details>

<details>
<summary><strong>Linux Host (C++ Native - Optional)</strong></summary>

```bash
cd linux-host
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

</details>

<details>
<summary><strong>Android Client</strong></summary>

```bash
cd android-client
./gradlew assembleDebug
# APK will be in app/build/outputs/apk/debug/
```

</details>

<details>
<summary><strong>Signaling Server</strong></summary>

```bash
cd signaling-server
go build -o signaling-server ./cmd/server
```

</details>

---

## 🐛 Troubleshooting

<details>
<summary><strong>Screen capture not working on Wayland</strong></summary>

Make sure xdg-desktop-portal is running:
```bash
systemctl --user restart xdg-desktop-portal xdg-desktop-portal-gnome
```

</details>

<details>
<summary><strong>No audio capture</strong></summary>

Check PipeWire is running:
```bash
systemctl --user status pipewire
pw-cli list-objects | grep -i audio
```

</details>

<details>
<summary><strong>Connection issues</strong></summary>

1. Ensure both devices are on the same network
2. Check firewall allows port 54321
3. Try USB connection as alternative

</details>

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 💖 Support

If you find this project useful, please consider:
- ⭐ Starring the repository
- 🐛 Reporting bugs
- 💡 Suggesting features
- 🤝 Contributing code

---

<p align="center">
  Made with ❤️ by <a href="https://github.com/MrVanguardia">MrVanguardia</a>
</p>

<p align="center">
  <a href="https://paypal.me/mrvanguardia">
    <img src="https://img.shields.io/badge/Donate-PayPal-blue.svg" alt="Donate">
  </a>
</p>
