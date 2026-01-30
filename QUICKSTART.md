# 🚀 StreamLinux - Quick Start Guide

Get StreamLinux up and running in 5 minutes!

## Prerequisites Check

Before starting, verify you have:

### Linux Host
- Ubuntu 20.04+ / Fedora 35+ / Arch Linux
- Internet connection
- ~500MB free disk space

### Android Device
- Android 8.0 (Oreo) or newer
- WiFi or USB cable

## 🏁 Installation (5 minutes)

### Step 1: Download StreamLinux

```bash
git clone https://github.com/yourusername/streamlinux.git
cd streamlinux
```

### Step 2: Install Dependencies

**One-line install:**
```bash
./install-deps.sh
```

This installs:
- Go 1.21+ (signaling server)
- GCC/CMake (C++ compilation)
- FFmpeg libraries (video encoding)
- Python GTK4 (GUI)
- ADB tools (USB connection)

**Manual check:**
```bash
go version      # Should show 1.21+
gcc --version   # Should show 9.0+
python3 --version # Should show 3.10+
```

### Step 3: Build Everything

```bash
./build.sh --all
```

This builds:
- ✅ Signaling server (Go)
- ✅ Linux host binary (C++)
- ✅ Android APK (Kotlin)

**Build time:** ~3-5 minutes (first time)

### Step 4: Start Linux GUI

```bash
cd linux-gui
python3 streamlinux_gui.py
```

You should see:
- StreamLinux window opens
- "Server Stopped" status (red)
- Empty QR code area

### Step 5: Start Server

In the GUI:
1. Click **"Start Server"** button
2. Wait ~2 seconds
3. Status turns green: **"Server Running"**
4. QR code appears

**Server is now running on port 8443!**

### Step 6: Install Android App

**Option A: From Computer (ADB)**
```bash
# Enable USB debugging on Android first!
adb devices  # Verify device is connected
cd android-client
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

**Option B: Transfer APK**
```bash
# Transfer APK to phone
# Navigate to Downloads and tap to install
```

### Step 7: Connect and Stream!

**On Android:**
1. Open **StreamLinux** app
2. Go to **"Discovered"** tab
3. Wait 2-5 seconds for host to appear
4. Tap your computer name
5. **Video starts streaming!** 🎉

## 🔌 Connection Methods

### WiFi (Recommended)
- ✅ Auto-discovery via mDNS
- ✅ TLS encryption
- ✅ Works anywhere on same network

**Both devices must be on the SAME WiFi network!**

### USB (Lowest Latency)
1. Enable **USB Debugging** on Android
2. Connect USB cable
3. Run: `adb forward tcp:8080 tcp:8080`
4. In app, go to **"USB"** tab
5. Tap localhost entry

## ⚙️ First-Time Configuration

### On Linux (GUI Settings Tab)

**Video Quality:**
- Resolution: `1920x1080` (recommended)
- FPS: `30` (smooth)
- Bitrate: `5000 kbps` (high quality)

**Audio:**
- ✅ Enable Audio Streaming
- Bitrate: `128 kbps`

**Network:**
- Port: `8443` (default)
- ✅ Use TLS

### On Android (Settings Screen)

**Performance:**
- ✅ Hardware Decoding (better performance)
- Codec: `H264`

**Convenience:**
- ✅ Keep Screen On (prevents dimming)
- Auto Connect: Optional

## 🧪 Testing Your Setup

### 1. Test Server

```bash
# In terminal:
curl http://localhost:8080/health
# Should return: {"status":"ok"}

# Check QR code:
curl http://localhost:8080/qr
# Should return JSON with connection info
```

### 2. Test Network Discovery

**On Linux:**
```bash
# Check mDNS broadcasting
avahi-browse -rt _streamlinux._tcp
# Should show your service
```

**On Android:**
- Open app
- "Discovered" tab should show your PC within 5 seconds

### 3. Test Video Streaming

1. Connect from Android
2. You should see:
   - "Connecting..." (1-2 seconds)
   - "Waiting for video..." (1-2 seconds)
   - **Your desktop appears!**

**Expected latency:** 50-100ms on LAN

## 🔧 Quick Troubleshooting

### "No hosts found" on Android

**Solution:**
```bash
# Check firewall
sudo ufw allow 8443/tcp
sudo ufw allow 5353/udp

# Fedora/RHEL:
sudo firewall-cmd --add-port=8443/tcp --permanent
sudo firewall-cmd --add-port=5353/udp --permanent
sudo firewall-cmd --reload
```

### Server won't start

**Solution:**
```bash
# Check port availability
sudo lsof -i :8443

# Kill if occupied
sudo kill <PID>

# Restart server from GUI
```

### Black screen on Android

**Solutions:**
1. Check Linux host has display running
2. Try different resolution (Settings → 1280x720)
3. Disable hardware accel temporarily
4. Check logs: `journalctl -u streamlinux-server`

### USB connection fails

**Solution:**
```bash
# Verify ADB
adb devices
# Should list your device

# Set up port forwarding
adb forward tcp:8080 tcp:8080

# Restart StreamLinux app
adb shell am force-stop com.streamlinux.client
adb shell am start -n com.streamlinux.client/.MainActivity
```

## 📱 Android Permissions

When you first open the app, grant:
- ✅ **Network access** (required)
- ✅ **Local network discovery** (for auto-discovery)

Optional:
- Camera (for QR code scanning - future feature)

## 🎯 Performance Tips

### For Best Quality
- WiFi 5GHz network
- Resolution: 1920x1080
- Bitrate: 8000 kbps
- FPS: 60

### For Low Latency
- USB connection
- Resolution: 1280x720
- Preset: ultrafast
- FPS: 30

### For Battery Saving
- Resolution: 854x480
- Bitrate: 2000 kbps
- FPS: 24
- Disable hardware decoding

## 🎬 Demo Scenarios

### Presentation Mode
1. Connect laptop to projector
2. Stream to Android phone/tablet
3. Walk around room with controls

### Gaming Stream
1. Set to 60 FPS, low latency
2. Use USB for minimal lag
3. Stream gameplay to phone

### Remote Monitoring
1. Leave Linux PC running
2. Check from phone anywhere on LAN
3. Monitor processes, terminals, etc.

## 📚 Next Steps

Once you have it working:

1. **Explore Settings**
   - Try different resolutions
   - Adjust bitrate for your network
   - Test audio streaming

2. **Read Full Documentation**
   - [README.md](README.md) - Complete features
   - [INSTALL.md](INSTALL.md) - Advanced installation
   - [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) - Technical details

3. **Customize**
   - Edit `default.config.json`
   - Set up systemd service for auto-start
   - Create custom presets

## 🆘 Getting Help

**Something not working?**

1. Check logs:
   ```bash
   # Linux server logs
   journalctl -u streamlinux-server -f
   
   # Android logs
   adb logcat | grep StreamLinux
   ```

2. Verify versions:
   ```bash
   go version  # 1.21+
   gcc --version  # 9.0+
   python3 --version  # 3.10+
   ```

3. Open GitHub issue:
   - https://github.com/yourusername/streamlinux/issues
   - Include: OS version, error messages, logs

## ✅ Success Checklist

You're ready when you can:
- [x] Start Linux GUI
- [x] Server shows green "Running" status
- [x] QR code is visible
- [x] Android app discovers host automatically
- [x] Video streams with < 100ms latency
- [x] Audio works (if enabled)

## 🎉 You're Done!

StreamLinux is now running. Enjoy wireless screen streaming!

**Happy Streaming!** 🚀

---

**Total setup time:** ~5 minutes  
**Difficulty:** Easy  
**Support:** [GitHub Issues](https://github.com/yourusername/streamlinux/issues)
