#!/bin/bash
# StreamLinux - Build script for creating distribution packages

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VERSION="1.0.0"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Create build directory
mkdir -p "$BUILD_DIR"

build_tarball() {
    print_status "Creating source tarball..."
    
    TARBALL_DIR="$BUILD_DIR/streamlinux-$VERSION"
    mkdir -p "$TARBALL_DIR"
    
    # Copy files
    cp "$PROJECT_ROOT/streamlinux_gui.py" "$TARBALL_DIR/"
    cp "$PROJECT_ROOT/webrtc_streamer.py" "$TARBALL_DIR/" 2>/dev/null || true
    cp "$PROJECT_ROOT/requirements.txt" "$TARBALL_DIR/"
    cp -r "$PROJECT_ROOT/data" "$TARBALL_DIR/"
    
    # Copy license and readme from parent
    if [ -f "$PROJECT_ROOT/../LICENSE" ]; then
        cp "$PROJECT_ROOT/../LICENSE" "$TARBALL_DIR/"
    fi
    if [ -f "$PROJECT_ROOT/../README.md" ]; then
        cp "$PROJECT_ROOT/../README.md" "$TARBALL_DIR/"
    fi
    
    # Create tarball
    cd "$BUILD_DIR"
    tar -czvf "streamlinux-$VERSION.tar.gz" "streamlinux-$VERSION"
    rm -rf "$TARBALL_DIR"
    
    print_status "Created: $BUILD_DIR/streamlinux-$VERSION.tar.gz"
}

build_rpm() {
    print_status "Building RPM package..."
    
    if ! command -v rpmbuild &> /dev/null; then
        print_error "rpmbuild not found. Install with: sudo dnf install rpm-build"
        return 1
    fi
    
    # Use /tmp for RPM build to avoid spaces in path
    RPM_BUILD_ROOT="/tmp/streamlinux-rpmbuild-$$"
    mkdir -p "$RPM_BUILD_ROOT"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
    
    # Cleanup function
    cleanup_rpm() {
        rm -rf "$RPM_BUILD_ROOT"
    }
    trap cleanup_rpm EXIT
    
    # Copy spec file
    cp "$PROJECT_ROOT/packaging/rpm/streamlinux.spec" "$RPM_BUILD_ROOT/SPECS/"
    
    # Build tarball if not exists
    if [ ! -f "$BUILD_DIR/streamlinux-$VERSION.tar.gz" ]; then
        build_tarball
    fi
    
    # Copy tarball to SOURCES
    cp "$BUILD_DIR/streamlinux-$VERSION.tar.gz" "$RPM_BUILD_ROOT/SOURCES/"
    
    # Build RPM
    rpmbuild --define "_topdir $RPM_BUILD_ROOT" -ba "$RPM_BUILD_ROOT/SPECS/streamlinux.spec"
    
    # Copy result
    cp "$RPM_BUILD_ROOT/RPMS/noarch/"*.rpm "$BUILD_DIR/" 2>/dev/null || true
    
    print_status "RPM package built successfully!"
    ls -la "$BUILD_DIR/"*.rpm 2>/dev/null || print_warning "No RPM files found"
    
    # Reset trap and cleanup
    trap - EXIT
    rm -rf "$RPM_BUILD_ROOT"
}

build_flatpak() {
    print_status "Building Flatpak package..."
    
    if ! command -v flatpak-builder &> /dev/null; then
        print_error "flatpak-builder not found. Install with: sudo dnf install flatpak-builder"
        return 1
    fi
    
    FLATPAK_BUILD_DIR="$BUILD_DIR/flatpak-build"
    FLATPAK_REPO="$BUILD_DIR/flatpak-repo"
    
    # Install runtime if needed
    flatpak install -y flathub org.gnome.Platform//46 org.gnome.Sdk//46 2>/dev/null || true
    
    # Build
    cd "$PROJECT_ROOT"
    flatpak-builder --force-clean --repo="$FLATPAK_REPO" "$FLATPAK_BUILD_DIR" \
        "$PROJECT_ROOT/packaging/flatpak/com.streamlinux.host.yaml"
    
    # Create bundle
    flatpak build-bundle "$FLATPAK_REPO" "$BUILD_DIR/streamlinux-$VERSION.flatpak" \
        com.streamlinux.host
    
    print_status "Created: $BUILD_DIR/streamlinux-$VERSION.flatpak"
}

build_deb() {
    print_status "Building DEB package..."
    
    if ! command -v dpkg-deb &> /dev/null; then
        print_error "dpkg-deb not found. Install with: sudo apt install dpkg"
        return 1
    fi
    
    DEB_BUILD_DIR="$BUILD_DIR/streamlinux_${VERSION}_all"
    mkdir -p "$DEB_BUILD_DIR/DEBIAN"
    mkdir -p "$DEB_BUILD_DIR/usr/bin"
    mkdir -p "$DEB_BUILD_DIR/usr/share/applications"
    mkdir -p "$DEB_BUILD_DIR/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "$DEB_BUILD_DIR/usr/share/metainfo"
    
    # Create control file
    cat > "$DEB_BUILD_DIR/DEBIAN/control" << EOF
Package: streamlinux
Version: $VERSION
Section: video
Priority: optional
Architecture: all
Depends: python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1, python3-pil, python3-qrcode
Maintainer: StreamLinux Team <contact@streamlinux.dev>
Description: Stream your Linux screen to Android devices
 StreamLinux allows you to stream your Linux desktop screen and audio
 to Android devices over your local network with ultra-low latency
 using WebRTC technology.
 .
 Features include hardware-accelerated encoding, QR code connection,
 and support for both X11 and Wayland.
EOF
    
    # Copy files
    cp "$PROJECT_ROOT/streamlinux_gui.py" "$DEB_BUILD_DIR/usr/bin/streamlinux-gui"
    chmod +x "$DEB_BUILD_DIR/usr/bin/streamlinux-gui"
    cp "$PROJECT_ROOT/data/com.streamlinux.host.desktop" "$DEB_BUILD_DIR/usr/share/applications/"
    cp "$PROJECT_ROOT/data/icons/streamlinux.svg" "$DEB_BUILD_DIR/usr/share/icons/hicolor/scalable/apps/"
    cp "$PROJECT_ROOT/data/com.streamlinux.host.metainfo.xml" "$DEB_BUILD_DIR/usr/share/metainfo/"
    
    # Build deb
    dpkg-deb --build "$DEB_BUILD_DIR"
    mv "$BUILD_DIR/streamlinux_${VERSION}_all.deb" "$BUILD_DIR/"
    rm -rf "$DEB_BUILD_DIR"
    
    print_status "Created: $BUILD_DIR/streamlinux_${VERSION}_all.deb"
}

install_local() {
    print_status "Installing locally..."
    
    # Create lib directory for Python modules
    sudo mkdir -p /usr/local/lib/streamlinux
    sudo mkdir -p /usr/local/lib/signaling-server
    
    # Install Python modules
    sudo install -D -m 644 "$PROJECT_ROOT/streamlinux_gui.py" /usr/local/lib/streamlinux/streamlinux_gui.py
    sudo install -D -m 644 "$PROJECT_ROOT/webrtc_streamer.py" /usr/local/lib/streamlinux/webrtc_streamer.py
    sudo install -D -m 644 "$PROJECT_ROOT/portal_screencast.py" /usr/local/lib/streamlinux/portal_screencast.py
    sudo install -D -m 644 "$PROJECT_ROOT/usb_manager.py" /usr/local/lib/streamlinux/usb_manager.py
    
    # Install signaling server if available
    SIGNALING_SERVER="$(dirname "$PROJECT_ROOT")/signaling-server/signaling-server"
    if [ -f "$SIGNALING_SERVER" ]; then
        print_status "Installing signaling server..."
        sudo install -D -m 755 "$SIGNALING_SERVER" /usr/local/lib/signaling-server/signaling-server
    else
        print_warning "Signaling server binary not found at: $SIGNALING_SERVER"
        print_warning "Build it with: cd signaling-server && go build -o signaling-server ./cmd/server"
    fi
    
    # Create launcher script
    sudo tee /usr/local/bin/streamlinux-gui > /dev/null << 'LAUNCHER'
#!/bin/bash
cd /usr/local/lib/streamlinux
exec python3 streamlinux_gui.py "$@"
LAUNCHER
    sudo chmod 755 /usr/local/bin/streamlinux-gui
    
    # Install desktop file
    sudo install -D -m 644 "$PROJECT_ROOT/data/com.streamlinux.host.desktop" \
        /usr/share/applications/com.streamlinux.host.desktop
    
    # Install icon
    sudo install -D -m 644 "$PROJECT_ROOT/data/icons/streamlinux.svg" \
        /usr/share/icons/hicolor/scalable/apps/streamlinux.svg
    
    # Update icon cache
    sudo gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
    
    # Update desktop database
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    
    print_status "StreamLinux installed successfully!"
    print_status "Run with: streamlinux-gui"
}

uninstall_local() {
    print_status "Uninstalling..."
    
    sudo rm -f /usr/local/bin/streamlinux-gui
    sudo rm -rf /usr/local/lib/streamlinux
    sudo rm -rf /usr/local/lib/signaling-server
    sudo rm -f /usr/share/applications/com.streamlinux.host.desktop
    sudo rm -f /usr/share/icons/hicolor/scalable/apps/streamlinux.svg
    
    # Update caches
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    sudo gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
    
    print_status "StreamLinux uninstalled successfully!"
}

show_help() {
    echo "StreamLinux Build Script"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  tarball     Create source tarball"
    echo "  rpm         Build RPM package (Fedora/RHEL)"
    echo "  deb         Build DEB package (Debian/Ubuntu)"
    echo "  flatpak     Build Flatpak package"
    echo "  install     Install locally to /usr/local"
    echo "  uninstall   Uninstall from /usr/local"
    echo "  all         Build all packages"
    echo "  clean       Clean build directory"
    echo "  help        Show this help"
}

clean() {
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    print_status "Clean complete!"
}

# Main
case "${1:-help}" in
    tarball)
        build_tarball
        ;;
    rpm)
        build_rpm
        ;;
    deb)
        build_deb
        ;;
    flatpak)
        build_flatpak
        ;;
    install)
        install_local
        ;;
    uninstall)
        uninstall_local
        ;;
    all)
        build_tarball
        build_rpm
        build_deb
        ;;
    clean)
        clean
        ;;
    help|*)
        show_help
        ;;
esac
