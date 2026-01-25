#!/bin/bash
# StreamLinux Universal Installer
# Works on Fedora, Ubuntu, Debian, Arch, openSUSE, and other Linux distributions

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="1.0.0"

print_banner() {
    echo -e "${BLUE}"
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║                                                           ║"
    echo "║   🖥️  StreamLinux v${VERSION} - Universal Installer        ║"
    echo "║   Stream your Linux screen to Android                     ║"
    echo "║                                                           ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_status() { echo -e "${GREEN}[✓]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[!]${NC} $1"; }
print_error() { echo -e "${RED}[✗]${NC} $1"; }
print_info() { echo -e "${BLUE}[i]${NC} $1"; }

# Detect distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_FAMILY=$ID_LIKE
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        DISTRO=$DISTRIB_ID
    else
        DISTRO=$(uname -s)
    fi
    echo "$DISTRO"
}

# Install dependencies based on distribution
install_dependencies() {
    local distro=$(detect_distro)
    print_info "Detected distribution: $distro"
    
    case "$distro" in
        fedora|rhel|centos|rocky|alma)
            print_status "Installing dependencies for Fedora/RHEL..."
            sudo dnf install -y python3 python3-pip python3-gobject gtk4 libadwaita \
                gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free \
                gstreamer1-plugin-libav pipewire pipewire-gstreamer \
                python3-pillow python3-qrcode python3-websocket-client 2>/dev/null || true
            ;;
        ubuntu|debian|linuxmint|pop|elementary|zorin)
            print_status "Installing dependencies for Debian/Ubuntu..."
            sudo apt update
            sudo apt install -y python3 python3-pip python3-gi python3-gi-cairo \
                gir1.2-gtk-4.0 gir1.2-adw-1 \
                gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
                gstreamer1.0-libav gstreamer1.0-pipewire \
                python3-pil python3-qrcode python3-websocket 2>/dev/null || true
            ;;
        arch|manjaro|endeavouros|garuda)
            print_status "Installing dependencies for Arch Linux..."
            sudo pacman -Sy --noconfirm python python-pip python-gobject gtk4 libadwaita \
                gst-plugins-base gst-plugins-good gst-plugins-bad gst-libav \
                pipewire gst-plugin-pipewire \
                python-pillow python-qrcode python-websocket-client 2>/dev/null || true
            ;;
        opensuse*|suse*)
            print_status "Installing dependencies for openSUSE..."
            sudo zypper install -y python3 python3-pip python3-gobject gtk4 libadwaita \
                gstreamer-plugins-base gstreamer-plugins-good gstreamer-plugins-bad \
                gstreamer-plugins-libav pipewire \
                python3-Pillow python3-qrcode python3-websocket-client 2>/dev/null || true
            ;;
        *)
            print_warning "Unknown distribution: $distro"
            print_info "Please install manually: python3, gtk4, libadwaita, gstreamer, pipewire"
            print_info "Python packages: pillow, qrcode, websocket-client"
            ;;
    esac
    
    # Install Python packages via pip as fallback
    print_status "Installing Python packages..."
    pip3 install --user pillow qrcode websocket-client 2>/dev/null || true
}

# Install StreamLinux
install_streamlinux() {
    print_status "Installing StreamLinux..."
    
    # Create directories
    sudo mkdir -p /usr/local/lib/streamlinux
    sudo mkdir -p /usr/local/lib/signaling-server
    sudo mkdir -p /usr/share/applications
    sudo mkdir -p /usr/share/icons/hicolor/scalable/apps
    
    # Install Python modules
    sudo cp "$SCRIPT_DIR/streamlinux_gui.py" /usr/local/lib/streamlinux/
    sudo cp "$SCRIPT_DIR/webrtc_streamer.py" /usr/local/lib/streamlinux/
    
    # Install optional modules if present
    [ -f "$SCRIPT_DIR/portal_screencast.py" ] && sudo cp "$SCRIPT_DIR/portal_screencast.py" /usr/local/lib/streamlinux/
    [ -f "$SCRIPT_DIR/usb_manager.py" ] && sudo cp "$SCRIPT_DIR/usb_manager.py" /usr/local/lib/streamlinux/
    
    # Install signaling server if present
    SIGNALING_SERVER="$SCRIPT_DIR/../signaling-server/signaling-server"
    if [ -f "$SIGNALING_SERVER" ]; then
        print_status "Installing signaling server..."
        sudo cp "$SIGNALING_SERVER" /usr/local/lib/signaling-server/
        sudo chmod +x /usr/local/lib/signaling-server/signaling-server
    else
        print_warning "Signaling server not found. Build it with: cd signaling-server && go build -o signaling-server ./cmd/server"
    fi
    
    # Create launcher script
    sudo tee /usr/local/bin/streamlinux-gui > /dev/null << 'EOF'
#!/bin/bash
cd /usr/local/lib/streamlinux
exec python3 streamlinux_gui.py "$@"
EOF
    sudo chmod +x /usr/local/bin/streamlinux-gui
    
    # Install desktop file
    sudo cp "$SCRIPT_DIR/data/com.streamlinux.host.desktop" /usr/share/applications/
    
    # Install icon
    sudo cp "$SCRIPT_DIR/data/icons/streamlinux.svg" /usr/share/icons/hicolor/scalable/apps/
    
    # Update caches
    sudo gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    
    print_status "StreamLinux installed successfully!"
}

# Uninstall StreamLinux
uninstall_streamlinux() {
    print_status "Uninstalling StreamLinux..."
    
    sudo rm -f /usr/local/bin/streamlinux-gui
    sudo rm -rf /usr/local/lib/streamlinux
    sudo rm -rf /usr/local/lib/signaling-server
    sudo rm -f /usr/share/applications/com.streamlinux.host.desktop
    sudo rm -f /usr/share/icons/hicolor/scalable/apps/streamlinux.svg
    
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    sudo gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
    
    print_status "StreamLinux uninstalled successfully!"
}

# Main
print_banner

case "${1:-install}" in
    install)
        install_dependencies
        install_streamlinux
        echo ""
        print_status "Installation complete!"
        print_info "Run with: streamlinux-gui"
        print_info "Or find 'StreamLinux' in your application menu"
        ;;
    uninstall)
        uninstall_streamlinux
        ;;
    deps)
        install_dependencies
        print_status "Dependencies installed!"
        ;;
    *)
        echo "Usage: $0 [install|uninstall|deps]"
        echo ""
        echo "Commands:"
        echo "  install    Install StreamLinux and dependencies (default)"
        echo "  uninstall  Remove StreamLinux"
        echo "  deps       Install only dependencies"
        ;;
esac
