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
        DISTRO_LIKE=$ID_LIKE
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        DISTRO=$DISTRIB_ID
    else
        DISTRO=$(uname -s)
    fi
    echo "$DISTRO"
}

# Get package manager
get_package_manager() {
    if command -v dnf &>/dev/null; then
        echo "dnf"
    elif command -v apt &>/dev/null; then
        echo "apt"
    elif command -v pacman &>/dev/null; then
        echo "pacman"
    elif command -v zypper &>/dev/null; then
        echo "zypper"
    elif command -v apk &>/dev/null; then
        echo "apk"
    else
        echo "unknown"
    fi
}

# Install dependencies based on distribution
install_dependencies() {
    local distro=$(detect_distro)
    local pkg_mgr=$(get_package_manager)
    print_info "Detected distribution: $distro (package manager: $pkg_mgr)"
    
    case "$pkg_mgr" in
        dnf)
            print_status "Installing dependencies for Fedora/RHEL..."
            sudo dnf install -y \
                python3 python3-pip python3-gobject gtk4 libadwaita \
                gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free \
                gstreamer1-plugin-libav pipewire pipewire-gstreamer \
                python3-pillow python3-cairo cairo-gobject-devel \
                gobject-introspection-devel 2>/dev/null || true
            sudo dnf install -y python3-qrcode python3-websocket-client 2>/dev/null || true
            ;;
        apt)
            print_status "Installing dependencies for Debian/Ubuntu..."
            sudo apt update
            sudo apt install -y \
                python3 python3-pip python3-gi python3-gi-cairo \
                gir1.2-gtk-4.0 gir1.2-adw-1 \
                gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
                gstreamer1.0-libav gstreamer1.0-pipewire \
                python3-pil python3-cairo libcairo2-dev \
                libgirepository1.0-dev 2>/dev/null || true
            sudo apt install -y python3-qrcode python3-websocket 2>/dev/null || true
            ;;
        pacman)
            print_status "Installing dependencies for Arch Linux..."
            sudo pacman -Sy --noconfirm \
                python python-pip python-gobject gtk4 libadwaita \
                gst-plugins-base gst-plugins-good gst-plugins-bad gst-libav \
                pipewire gst-plugin-pipewire \
                python-pillow python-cairo cairo gobject-introspection 2>/dev/null || true
            sudo pacman -Sy --noconfirm python-qrcode python-websocket-client 2>/dev/null || true
            ;;
        zypper)
            print_status "Installing dependencies for openSUSE..."
            sudo zypper install -y \
                python3 python3-pip python3-gobject gtk4 libadwaita \
                gstreamer-plugins-base gstreamer-plugins-good gstreamer-plugins-bad \
                gstreamer-plugins-libav pipewire \
                python3-Pillow python3-cairo cairo-devel gobject-introspection-devel 2>/dev/null || true
            sudo zypper install -y python3-qrcode python3-websocket-client 2>/dev/null || true
            ;;
        apk)
            print_status "Installing dependencies for Alpine Linux..."
            sudo apk add --no-cache \
                python3 py3-pip py3-gobject3 gtk4.0 libadwaita \
                gst-plugins-base gst-plugins-good gst-plugins-bad gst-libav \
                pipewire py3-pillow py3-cairo cairo-dev gobject-introspection-dev 2>/dev/null || true
            ;;
        *)
            print_warning "Unknown package manager"
            print_info "Please install manually: python3, gtk4, libadwaita, gstreamer, pipewire"
            ;;
    esac
    
    # Install Python packages via pip as fallback
    print_status "Installing Python packages via pip (fallback)..."
    pip3 install --user --break-system-packages pillow qrcode websocket-client PyGObject 2>/dev/null || \
    pip3 install --user pillow qrcode websocket-client PyGObject 2>/dev/null || true
}

# Verify Python dependencies
verify_dependencies() {
    print_info "Verifying Python dependencies..."
    local missing=()
    
    python3 -c "import gi; gi.require_version('Gtk', '4.0'); from gi.repository import Gtk" 2>/dev/null || missing+=("GTK4")
    python3 -c "import gi; gi.require_version('Adw', '1'); from gi.repository import Adw" 2>/dev/null || missing+=("libadwaita")
    python3 -c "import gi; gi.require_version('Gst', '1.0'); from gi.repository import Gst" 2>/dev/null || missing+=("GStreamer")
    python3 -c "from PIL import Image" 2>/dev/null || missing+=("Pillow")
    python3 -c "import qrcode" 2>/dev/null || missing+=("qrcode")
    python3 -c "import websocket" 2>/dev/null || missing+=("websocket-client")
    
    if [ ${#missing[@]} -gt 0 ]; then
        print_warning "Missing dependencies: ${missing[*]}"
        print_info "Attempting to install missing packages via pip..."
        pip3 install --user --break-system-packages pillow qrcode websocket-client 2>/dev/null || \
        pip3 install --user pillow qrcode websocket-client 2>/dev/null || true
        return 1
    else
        print_status "All Python dependencies are satisfied!"
        return 0
    fi
}

# Install StreamLinux
install_streamlinux() {
    print_status "Installing StreamLinux..."
    
    # Create directories
    sudo mkdir -p /usr/local/lib/streamlinux
    sudo mkdir -p /usr/local/lib/signaling-server
    sudo mkdir -p /usr/share/applications
    sudo mkdir -p /usr/share/icons/hicolor/scalable/apps
    sudo mkdir -p /usr/share/metainfo
    
    # Install Python modules
    sudo cp "$SCRIPT_DIR/streamlinux_gui.py" /usr/local/lib/streamlinux/
    sudo cp "$SCRIPT_DIR/webrtc_streamer.py" /usr/local/lib/streamlinux/
    
    # Install optional modules if present
    [ -f "$SCRIPT_DIR/portal_screencast.py" ] && sudo cp "$SCRIPT_DIR/portal_screencast.py" /usr/local/lib/streamlinux/
    [ -f "$SCRIPT_DIR/usb_manager.py" ] && sudo cp "$SCRIPT_DIR/usb_manager.py" /usr/local/lib/streamlinux/
    
    # Create __init__.py for proper module loading
    sudo touch /usr/local/lib/streamlinux/__init__.py
    
    # Install signaling server if present
    SIGNALING_SERVER="$SCRIPT_DIR/../signaling-server/signaling-server"
    if [ -f "$SIGNALING_SERVER" ]; then
        print_status "Installing signaling server..."
        sudo cp "$SIGNALING_SERVER" /usr/local/lib/signaling-server/
        sudo chmod +x /usr/local/lib/signaling-server/signaling-server
    else
        print_warning "Signaling server binary not found"
    fi
    
    # Create launcher script with better error handling
    sudo tee /usr/local/bin/streamlinux > /dev/null << 'LAUNCHER_EOF'
#!/bin/bash
# StreamLinux Launcher
export PATH="$HOME/.local/bin:$PATH"
export PYTHONPATH="$HOME/.local/lib/python3/site-packages:$PYTHONPATH"
cd /usr/local/lib/streamlinux || exit 1

check_deps() {
    python3 -c "
import sys
try:
    import gi
    gi.require_version('Gtk', '4.0')
    gi.require_version('Adw', '1')
    gi.require_version('Gst', '1.0')
    from gi.repository import Gtk, Adw, Gst
    from PIL import Image
    import qrcode
    import websocket
except ImportError as e:
    print(f'Missing dependency: {e}', file=sys.stderr)
    sys.exit(1)
" 2>/dev/null
    return $?
}

if ! check_deps; then
    if command -v zenity &>/dev/null; then
        zenity --error --title="StreamLinux" --text="Missing dependencies.\n\nRun: pip3 install --user pillow qrcode websocket-client"
    elif command -v kdialog &>/dev/null; then
        kdialog --error "Missing dependencies.\n\nRun: pip3 install --user pillow qrcode websocket-client"
    else
        echo "ERROR: Missing dependencies. Run:"
        echo "  pip3 install --user pillow qrcode websocket-client"
    fi
    exit 1
fi

exec python3 streamlinux_gui.py "$@"
LAUNCHER_EOF
    sudo chmod +x /usr/local/bin/streamlinux
    
    # Create symlink for backwards compatibility
    sudo ln -sf /usr/local/bin/streamlinux /usr/local/bin/streamlinux-gui
    
    # Install desktop file with absolute path
    sudo tee /usr/share/applications/com.streamlinux.host.desktop > /dev/null << 'DESKTOP_EOF'
[Desktop Entry]
Name=StreamLinux
Comment=Stream your Linux screen to Android devices
GenericName=Screen Streaming
Exec=/usr/local/bin/streamlinux
Icon=com.streamlinux.host
Terminal=false
Type=Application
Categories=Network;Video;AudioVideo;Utility;
Keywords=stream;screen;share;android;webrtc;
StartupWMClass=streamlinux
StartupNotify=true
DESKTOP_EOF
    
    # Install icon
    if [ -f "$SCRIPT_DIR/data/icons/streamlinux.svg" ]; then
        sudo cp "$SCRIPT_DIR/data/icons/streamlinux.svg" /usr/share/icons/hicolor/scalable/apps/com.streamlinux.host.svg
    fi
    
    # Install metainfo if present
    if [ -f "$SCRIPT_DIR/data/com.streamlinux.host.metainfo.xml" ]; then
        sudo cp "$SCRIPT_DIR/data/com.streamlinux.host.metainfo.xml" /usr/share/metainfo/
    fi
    
    # Update caches
    print_status "Updating system caches..."
    sudo gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    
    print_status "StreamLinux installed successfully!"
    echo ""
    print_info "Start from app menu or run: streamlinux"
}

# Uninstall StreamLinux
uninstall_streamlinux() {
    print_status "Uninstalling StreamLinux..."
    
    sudo rm -f /usr/local/bin/streamlinux
    sudo rm -f /usr/local/bin/streamlinux-gui
    sudo rm -rf /usr/local/lib/streamlinux
    sudo rm -rf /usr/local/lib/signaling-server
    sudo rm -f /usr/share/applications/com.streamlinux.host.desktop
    sudo rm -f /usr/share/icons/hicolor/scalable/apps/com.streamlinux.host.svg
    sudo rm -f /usr/share/icons/hicolor/scalable/apps/streamlinux.svg
    sudo rm -f /usr/share/metainfo/com.streamlinux.host.metainfo.xml
    
    sudo gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    
    print_status "StreamLinux uninstalled successfully!"
}

# Diagnose issues
diagnose() {
    print_banner
    print_info "Running diagnostics..."
    echo ""
    
    echo "Python version:"
    python3 --version
    echo ""
    
    echo "Checking GTK4..."
    python3 -c "import gi; gi.require_version('Gtk', '4.0'); from gi.repository import Gtk; print('  GTK4: OK')" 2>/dev/null || echo "  GTK4: MISSING"
    
    echo "Checking libadwaita..."
    python3 -c "import gi; gi.require_version('Adw', '1'); from gi.repository import Adw; print('  libadwaita: OK')" 2>/dev/null || echo "  libadwaita: MISSING"
    
    echo "Checking GStreamer..."
    python3 -c "import gi; gi.require_version('Gst', '1.0'); from gi.repository import Gst; Gst.init(None); print('  GStreamer: OK')" 2>/dev/null || echo "  GStreamer: MISSING"
    
    echo "Checking Pillow..."
    python3 -c "from PIL import Image; print('  Pillow: OK')" 2>/dev/null || echo "  Pillow: MISSING"
    
    echo "Checking qrcode..."
    python3 -c "import qrcode; print('  qrcode: OK')" 2>/dev/null || echo "  qrcode: MISSING"
    
    echo "Checking websocket-client..."
    python3 -c "import websocket; print('  websocket-client: OK')" 2>/dev/null || echo "  websocket-client: MISSING"
    
    echo ""
    echo "Checking installation..."
    [ -f /usr/local/bin/streamlinux ] && echo "  Launcher: OK" || echo "  Launcher: MISSING"
    [ -f /usr/local/lib/streamlinux/streamlinux_gui.py ] && echo "  Main script: OK" || echo "  Main script: MISSING"
    [ -f /usr/local/lib/signaling-server/signaling-server ] && echo "  Signaling server: OK" || echo "  Signaling server: MISSING"
    [ -f /usr/share/applications/com.streamlinux.host.desktop ] && echo "  Desktop file: OK" || echo "  Desktop file: MISSING"
    
    echo ""
    print_info "To fix missing packages: pip3 install --user pillow qrcode websocket-client"
}

# Show help
show_help() {
    echo "StreamLinux Universal Installer v${VERSION}"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  install     Install StreamLinux (default)"
    echo "  uninstall   Uninstall StreamLinux"
    echo "  diagnose    Check dependencies and installation"
    echo "  deps        Install dependencies only"
    echo "  help        Show this help"
}

# Main
main() {
    print_banner
    
    case "${1:-install}" in
        install)
            install_dependencies
            verify_dependencies || true
            install_streamlinux
            ;;
        uninstall)
            uninstall_streamlinux
            ;;
        diagnose|diag|check)
            diagnose
            ;;
        deps|dependencies)
            install_dependencies
            verify_dependencies
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
