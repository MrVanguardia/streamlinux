<p align="center">
  <img src="linux-gui/data/icons/streamlinux.svg" alt="StreamLinux Logo" width="150" height="150">
</p>

<h1 align="center">🖥️ StreamLinux</h1>

<p align="center">
  <strong>Stream your Linux desktop to Android devices with ultra-low latency using WebRTC</strong>
</p>

<p align="center">
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-downloads">Downloads</a> •
  <a href="#-features">Features</a> •
  <a href="#-screenshots">Screenshots</a> •
  <a href="#-faq">FAQ</a> •
  <a href="#-contributing">Contributing</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.1.2-blue.svg?style=for-the-badge" alt="Version">
  <img src="https://img.shields.io/badge/license-MIT-green.svg?style=for-the-badge" alt="License">
  <img src="https://img.shields.io/github/stars/MrVanguardia/streamlinux?style=for-the-badge&color=gold" alt="Stars">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black" alt="Linux">
  <img src="https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white" alt="Android">
  <img src="https://img.shields.io/badge/WebRTC-333333?style=for-the-badge&logo=webrtc&logoColor=white" alt="WebRTC">
</p>

---

## 🎯 What is StreamLinux?

**StreamLinux** is an open-source application that lets you stream your Linux desktop screen and audio to any Android device over your local network. Perfect for:

- 📺 **Extended Display** - Use your tablet as a second monitor
- 🎮 **Gaming** - Stream games to your mobile device
- 📱 **Remote Work** - Access your desktop from anywhere in your home
- 🎥 **Presentations** - Share your screen wirelessly
- 🔧 **Server Management** - Monitor your Linux server from your phone

### ⚡ Key Highlights

| Feature | Description |
|---------|-------------|
| 🚀 **Ultra-Low Latency** | WebRTC-based streaming for real-time performance |
| 🎨 **Hardware Acceleration** | VAAPI/NVENC encoding for smooth, efficient streaming |
| 🔊 **System Audio** | Stream what you hear with high-quality Opus codec |
| 🖥️ **Wayland & X11** | Full support for both display servers |
| 📱 **Easy Connection** | Scan a QR code and you're connected! |
| 🌍 **Multi-Language** | English and Spanish interface |

---

## 📥 Downloads

### Latest Release: v1.1.2

| Platform | Download | Size | Notes |
|:--------:|:---------|:----:|:------|
| 🐧 **Fedora/RHEL** | [📦 streamlinux-1.1.2-1.fc43.x86_64.rpm](https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-1.fc43.x86_64.rpm) | 2.6 MB | Recommended for Fedora 39+ |
| 🍃 **Universal Linux** | [📦 streamlinux-1.1.2-linux-universal.tar.gz](https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-linux-universal.tar.gz) | 5.8 MB | Ubuntu, Mint, Debian, Arch... |
| 📱 **Android** | [📦 StreamLinux-1.1.2-android.apk](https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/StreamLinux-1.1.2-android.apk) | 32 MB | Android 8.0+ (API 26) |

> 💡 **Tip:** The universal installer automatically detects your distribution and installs the required dependencies.

---

## 🚀 Quick Start

### Step 1: Install on Linux

<details open>
<summary><strong>🐧 Fedora / RHEL / CentOS / Rocky Linux</strong></summary>

```bash
# Download
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-1.fc43.x86_64.rpm

# Install
sudo rpm -ivh streamlinux-1.1.2-1.fc43.x86_64.rpm

# Launch
streamlinux-gui
```

</details>

<details>
<summary><strong>🍃 Ubuntu / Linux Mint / Debian / Pop!_OS</strong></summary>

```bash
# Download
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-linux-universal.tar.gz

# Extract
tar -xzvf streamlinux-1.1.2-linux-universal.tar.gz
cd streamlinux-1.1.2

# Install (will prompt for dependencies)
./install.sh

# Launch
streamlinux-gui
```

</details>

<details>
<summary><strong>🔵 Arch Linux / Manjaro</strong></summary>

```bash
# Download universal installer
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.1.2/streamlinux-1.1.2-linux-universal.tar.gz

# Extract and install
tar -xzvf streamlinux-1.1.2-linux-universal.tar.gz
cd streamlinux-1.1.2
./install.sh
```

</details>

### Step 2: Install on Android

1. Download the APK from [releases](https://github.com/MrVanguardia/streamlinux/releases/latest)
2. Enable **"Install from unknown sources"** in Settings → Security
3. Install the APK
4. Open **StreamLinux** app

### Step 3: Connect!

1. 🖥️ Launch **StreamLinux** on your Linux machine
2. 📱 Open the **StreamLinux** app on your Android device
3. 📷 Tap **"Scan QR Code"** and scan the code on your Linux screen
4. ✨ Enjoy your stream!

---

## ✨ Features

<table>
<tr>
<td width="50%">

### 🖥️ Linux Host Features

| Feature | Status |
|---------|:------:|
| X11 Screen Capture | ✅ |
| Wayland Screen Capture | ✅ |
| Hardware Encoding (VAAPI) | ✅ |
| Hardware Encoding (NVENC) | ✅ |
| Software Encoding (VP8) | ✅ |
| System Audio Capture | ✅ |
| Microphone Capture | ✅ |
| Multi-Monitor Support | ✅ |
| Custom Bitrate | ✅ |
| Quality Presets | ✅ |
| QR Code Connection | ✅ |
| USB/ADB Connection | ✅ |

</td>
<td width="50%">

### 📱 Android Client Features

| Feature | Status |
|---------|:------:|
| Hardware Video Decoding | ✅ |
| Low-Latency Playback | ✅ |
| QR Code Scanner | ✅ |
| LAN Auto-Discovery | ✅ |
| Full-Screen Mode | ✅ |
| Immersive Mode | ✅ |
| Keep Screen On | ✅ |
| Connection History | ✅ |
| Multi-Language (EN/ES) | ✅ |
| Touch Pass-through | 🔜 |
| Keyboard Input | 🔜 |
| Mouse Support | 🔜 |

</td>
</tr>
</table>

---

## 🏗️ Architecture

StreamLinux uses a modern, efficient architecture optimized for low latency:

```
┌────────────────────────────────────────────────────────────────────┐
│                         📺 LINUX HOST                              │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌─────────────┐    ┌─────────────┐    ┌──────────────────────┐   │
│  │   Display   │    │    Audio    │    │      Encoder         │   │
│  │   Capture   │    │   Capture   │    │  ┌────────────────┐  │   │
│  │ ┌─────────┐ │    │ ┌─────────┐ │    │  │ Video: VP8     │  │   │
│  │ │  X11    │ │    │ │PipeWire │ │    │  │ VAAPI/NVENC    │  │   │
│  │ │ XCB/SHM │ │    │ └─────────┘ │    │  └────────────────┘  │   │
│  │ └─────────┘ │    │ ┌─────────┐ │    │  ┌────────────────┐  │   │
│  │ ┌─────────┐ │    │ │PulseAudio│ │   │  │ Audio: Opus    │  │   │
│  │ │ Wayland │ │    │ └─────────┘ │    │  │ 48kHz Stereo   │  │   │
│  │ │PipeWire │ │    └──────┬──────┘    │  └────────────────┘  │   │
│  │ └─────────┘ │           │           └──────────┬───────────┘   │
│  └──────┬──────┘           │                      │               │
│         │                  │                      │               │
│         └──────────────────┼──────────────────────┘               │
│                            │                                       │
│                    ┌───────▼───────┐                               │
│                    │   WebRTCBin   │                               │
│                    │  (GStreamer)  │                               │
│                    └───────┬───────┘                               │
│                            │                                       │
└────────────────────────────┼───────────────────────────────────────┘
                             │
                    ┌────────▼────────┐
                    │ Signaling Server│
                    │     (Go)        │
                    │   WebSocket     │
                    └────────┬────────┘
                             │
                     ════════╪════════  Network (LAN/USB)
                             │
┌────────────────────────────┼───────────────────────────────────────┐
│                            │          📱 ANDROID CLIENT            │
├────────────────────────────┼───────────────────────────────────────┤
│                    ┌───────▼───────┐                               │
│                    │   WebRTC      │                               │
│                    │   Client      │                               │
│                    └───────┬───────┘                               │
│                            │                                       │
│         ┌──────────────────┼──────────────────────┐               │
│         │                  │                      │               │
│  ┌──────▼──────┐    ┌──────▼──────┐    ┌─────────▼─────────┐     │
│  │   Video     │    │    Audio    │    │       UI          │     │
│  │  Decoder    │    │   Player    │    │  Jetpack Compose  │     │
│  │ MediaCodec  │    │  OpenSL ES  │    │  Material Design  │     │
│  └─────────────┘    └─────────────┘    └───────────────────┘     │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### Technology Stack

| Component | Technology | Why? |
|-----------|------------|------|
| **Linux GUI** | Python 3, GTK4, libadwaita | Native GNOME integration, GStreamer bindings |
| **Screen Capture** | XCB, PipeWire, xdg-portal | Best performance on each display server |
| **Video Streaming** | GStreamer, WebRTC | Industry-standard, hardware acceleration |
| **Audio Codec** | Opus | Low latency, high quality at low bitrates |
| **Signaling Server** | Go | Efficient concurrent WebSocket handling |
| **Android Client** | Kotlin, Jetpack Compose | Modern Android development, Material 3 |

---

## 📋 System Requirements

### Linux Host

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **OS** | Any Linux with GTK4 | Fedora 39+, Ubuntu 22.04+ |
| **Display Server** | X11 or Wayland | Wayland (better security) |
| **CPU** | Dual-core 2GHz | Quad-core 3GHz+ |
| **RAM** | 2 GB | 4 GB+ |
| **GPU** | Any | Intel/AMD/NVIDIA with VAAPI |
| **Python** | 3.10+ | 3.11+ |

### Android Device

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **Android** | 8.0 (API 26) | 11.0+ (API 30) |
| **RAM** | 2 GB | 4 GB+ |
| **Display** | 720p | 1080p+ |
| **Network** | WiFi 802.11n | WiFi 5 (802.11ac) |

---

## ❓ FAQ

<details>
<summary><strong>🔴 The stream is laggy or choppy</strong></summary>

1. **Lower the resolution** in StreamLinux settings (try 720p instead of 1080p)
2. **Check your WiFi** - 5GHz WiFi is much faster than 2.4GHz
3. **Use USB connection** for the lowest latency
4. **Enable hardware encoding** if you have a compatible GPU

</details>

<details>
<summary><strong>🔴 No audio in the stream</strong></summary>

1. Make sure **PipeWire** or **PulseAudio** is running
2. Check that audio is enabled in StreamLinux settings
3. Try selecting "System Audio" as the audio source
4. On Wayland, ensure you've granted screen capture permissions

</details>

<details>
<summary><strong>🔴 Can't connect to host</strong></summary>

1. Ensure both devices are on the **same network**
2. Check if your firewall allows port **54321** (signaling server)
3. Try connecting via **QR code** instead of manual entry
4. For USB: Run `adb forward tcp:54321 tcp:54321`

</details>

<details>
<summary><strong>🔴 Screen capture doesn't work on Wayland</strong></summary>

1. StreamLinux uses **xdg-desktop-portal** for Wayland capture
2. A permission dialog should appear - make sure to allow it
3. If using GNOME, you need to share your entire screen
4. Some custom Wayland compositors may not be fully supported

</details>

<details>
<summary><strong>🟢 How do I enable hardware encoding?</strong></summary>

Hardware encoding is automatically detected. To verify:

- **Intel/AMD**: Install `intel-media-driver` or `mesa-va-drivers`
- **NVIDIA**: Install proprietary drivers with NVENC support
- Check with: `vainfo` (VAAPI) or `nvidia-smi` (NVENC)

</details>

---

## 🛠️ Building from Source

<details>
<summary><strong>Build Linux Host</strong></summary>

```bash
# Clone the repository
git clone https://github.com/MrVanguardia/streamlinux.git
cd streamlinux

# Install dependencies (Fedora)
sudo dnf install python3 python3-gobject gtk4 libadwaita \
    gstreamer1-plugins-base gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free pipewire-gstreamer

# Install dependencies (Ubuntu/Debian)
sudo apt install python3 python3-gi gir1.2-gtk-4.0 \
    gir1.2-adw-1 gir1.2-gst-plugins-bad-1.0 \
    gstreamer1.0-plugins-good gstreamer1.0-pipewire

# Run directly
cd linux-gui
python3 streamlinux_gui.py

# Or build packages
./scripts/build.sh rpm      # For Fedora/RHEL
./scripts/build.sh tarball  # Universal tarball
```

</details>

<details>
<summary><strong>Build Android Client</strong></summary>

```bash
# Prerequisites: Android Studio or JDK 17+

cd android-client
./gradlew assembleDebug

# APK will be in app/build/outputs/apk/debug/
```

</details>

<details>
<summary><strong>Build Signaling Server</strong></summary>

```bash
# Prerequisites: Go 1.21+

cd signaling-server
go build -o signaling-server ./cmd/server

# Run the server
./signaling-server -port 54321
```

</details>

---

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. 🐛 **Report bugs** - Open an [issue](https://github.com/MrVanguardia/streamlinux/issues)
2. 💡 **Suggest features** - We'd love to hear your ideas
3. 🔧 **Submit PRs** - Fork, code, and submit a pull request
4. 📖 **Improve docs** - Help make this README even better
5. 🌍 **Translate** - Add support for your language

### Development Setup

```bash
# Clone with all submodules
git clone --recursive https://github.com/MrVanguardia/streamlinux.git

# Create a virtual environment (optional but recommended)
python3 -m venv venv
source venv/bin/activate

# Install development dependencies
pip install -r linux-gui/requirements.txt
```

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **GStreamer** - Amazing multimedia framework
- **WebRTC** - Making real-time communication possible
- **GTK4 & libadwaita** - Beautiful, modern UI toolkit
- **Jetpack Compose** - Declarative Android UI
- **The open-source community** - For all the amazing tools

---

## 📬 Contact

<p align="center">
  <a href="https://vanguardiastudio.us">
    <img src="https://img.shields.io/badge/Website-vanguardiastudio.us-blue?style=for-the-badge&logo=google-chrome&logoColor=white" alt="Website">
  </a>
  <a href="https://github.com/MrVanguardia">
    <img src="https://img.shields.io/badge/GitHub-MrVanguardia-181717?style=for-the-badge&logo=github&logoColor=white" alt="GitHub">
  </a>
</p>

<p align="center">
  <strong>Made with ❤️ by <a href="https://vanguardiastudio.us">Vanguardia Studio</a></strong>
</p>

<p align="center">
  If you find this project useful, please consider giving it a ⭐!
</p>
