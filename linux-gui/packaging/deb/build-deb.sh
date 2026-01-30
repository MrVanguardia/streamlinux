#!/bin/bash
# StreamLinux DEB Package Builder
# For Ubuntu 22.04+, Linux Mint 21+, Debian 12+

set -e

VERSION="0.2.0"
RELEASE="1"
ARCH="amd64"
PKG_NAME="streamlinux"
PKG_DIR="${PKG_NAME}_${VERSION}-${RELEASE}_${ARCH}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LINUX_GUI="${PROJECT_ROOT}"
SIGNALING_SERVER="$(cd "${SCRIPT_DIR}/../../../signaling-server" && pwd)"

echo "=== StreamLinux DEB Package Builder ==="
echo "Version: ${VERSION}-${RELEASE}"
echo "Architecture: ${ARCH}"

# Build signaling server
echo ""
echo "[1/4] Building signaling server..."
cd "${SIGNALING_SERVER}"
go build -ldflags="-s -w" -o signaling-server ./cmd/server/
echo "      Done"

# Create package structure
echo ""
echo "[2/4] Creating package structure..."
BUILD_DIR="${SCRIPT_DIR}/build"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/${PKG_DIR}"
cd "${BUILD_DIR}/${PKG_DIR}"

# Create directories
mkdir -p DEBIAN
mkdir -p usr/bin
mkdir -p usr/share/streamlinux
mkdir -p usr/lib/streamlinux
mkdir -p usr/share/applications
mkdir -p usr/share/icons/hicolor/scalable/apps
mkdir -p usr/share/metainfo

# Copy files
echo ""
echo "[3/4] Copying files..."

# Python files
cp "${LINUX_GUI}/streamlinux_gui.py" usr/share/streamlinux/
cp "${LINUX_GUI}/webrtc_streamer.py" usr/share/streamlinux/
cp "${LINUX_GUI}/portal_screencast.py" usr/share/streamlinux/
cp "${LINUX_GUI}/security.py" usr/share/streamlinux/
cp "${LINUX_GUI}/usb_manager.py" usr/share/streamlinux/
cp "${LINUX_GUI}/i18n.py" usr/share/streamlinux/
cp "${LINUX_GUI}/message_crypto.py" usr/share/streamlinux/

# Signaling server
cp "${SIGNALING_SERVER}/signaling-server" usr/lib/streamlinux/

# Desktop and icons
cp "${LINUX_GUI}/data/com.streamlinux.host.desktop" usr/share/applications/
cp "${LINUX_GUI}/data/icons/streamlinux.svg" usr/share/icons/hicolor/scalable/apps/
cp "${LINUX_GUI}/data/com.streamlinux.host.metainfo.xml" usr/share/metainfo/

# Create launcher script
cat > usr/bin/streamlinux << 'LAUNCHER'
#!/bin/bash
cd /usr/share/streamlinux
exec python3 streamlinux_gui.py "$@"
LAUNCHER
chmod +x usr/bin/streamlinux

# Create control file
cat > DEBIAN/control << CONTROL
Package: ${PKG_NAME}
Version: ${VERSION}-${RELEASE}
Section: video
Priority: optional
Architecture: ${ARCH}
Depends: python3 (>= 3.10), python3-gi, python3-gi-cairo, gir1.2-gtk-4.0, gir1.2-adw-1, gstreamer1.0-plugins-bad, gstreamer1.0-plugins-good, gstreamer1.0-plugins-base, gstreamer1.0-tools, gstreamer1.0-pipewire, pipewire, libqrencode4
Recommends: gstreamer1.0-vaapi
Maintainer: Vanguardia Studio <contact@vanguardiastudio.us>
Homepage: https://github.com/MrVanguardia/streamlinux
Description: Stream your Linux desktop to Android
 StreamLinux enables real-time screen and audio streaming from
 Linux systems to Android devices over local network (WiFi) or
 USB connection using WebRTC technology.
 .
 Features:
  - Low-latency WebRTC streaming
  - VP8 video + Opus audio codecs
  - WiFi and USB connection modes
  - QR code authentication
  - End-to-end encryption (DTLS-SRTP)
  - No external servers required
CONTROL

# Create postinst script
cat > DEBIAN/postinst << 'POSTINST'
#!/bin/bash
set -e

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
fi

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

echo ""
echo "StreamLinux installed successfully!"
echo "Launch from Applications menu or run: streamlinux"
echo ""

exit 0
POSTINST
chmod +x DEBIAN/postinst

# Create postrm script
cat > DEBIAN/postrm << 'POSTRM'
#!/bin/bash
set -e

if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    # Update icon cache
    if command -v gtk-update-icon-cache &> /dev/null; then
        gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    fi
    
    # Update desktop database
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database /usr/share/applications 2>/dev/null || true
    fi
fi

exit 0
POSTRM
chmod +x DEBIAN/postrm

# Set permissions
find . -type d -exec chmod 755 {} \;
find . -type f -exec chmod 644 {} \;
chmod 755 usr/bin/streamlinux
chmod 755 usr/lib/streamlinux/signaling-server
chmod 755 DEBIAN/postinst
chmod 755 DEBIAN/postrm

# Build package
echo ""
echo "[4/4] Building DEB package..."
cd "${BUILD_DIR}"
dpkg-deb --build --root-owner-group "${PKG_DIR}"

# Move to releases
RELEASES_DIR="${SCRIPT_DIR}/../../../releases/deb"
mkdir -p "${RELEASES_DIR}"
mv "${PKG_DIR}.deb" "${RELEASES_DIR}/${PKG_NAME}-${VERSION}-${RELEASE}.ubuntu_amd64.deb"

echo ""
echo "=== Build Complete ==="
echo "Package: ${RELEASES_DIR}/${PKG_NAME}-${VERSION}-${RELEASE}.ubuntu_amd64.deb"
echo ""
echo "Install with:"
echo "  sudo apt install ./${PKG_NAME}-${VERSION}-${RELEASE}.ubuntu_amd64.deb"
echo ""

# Cleanup
rm -rf "${BUILD_DIR}"
