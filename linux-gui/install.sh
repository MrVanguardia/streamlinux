#!/usr/bin/env bash
# StreamLinux Universal Installer v1.0.1
# Works on Fedora, Ubuntu, Debian, Linux Mint, Arch, openSUSE, and other Linux distributions

# Don't exit on error - we handle errors ourselves
set +e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
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

# Detect distribution and version
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO_ID="$ID"
        DISTRO_ID_LIKE="$ID_LIKE"
        DISTRO_VERSION="$VERSION_ID"
        DISTRO_NAME="$PRETTY_NAME"
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        DISTRO_ID="$DISTRIB_ID"
        DISTRO_VERSION="$DISTRIB_RELEASE"
        DISTRO_NAME="$DISTRIB_DESCRIPTION"
    else
        DISTRO_ID="unknown"
    fi
    echo "$DISTRO_ID"
}

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_error "This script must be run with sudo"
        echo "  Run: sudo bash install.sh"
        exit 1
    fi
}

# Get the real user (not root)
get_real_user() {
    if [ -n "$SUDO_USER" ]; then
        echo "$SUDO_USER"
    else
        echo "$USER"
    fi
}

# Install dependencies based on distribution
install_dependencies() {
    local distro=$(detect_distro)
    print_info "Detected: $DISTRO_NAME ($distro)"
    
    # Determine package manager and install
    if command -v dnf &>/dev/null; then
        print_status "Installing dependencies (Fedora/RHEL)..."
        dnf install -y \
            python3 python3-pip python3-gobject gtk4 libadwaita \
            gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free \
            gstreamer1-plugin-libav pipewire pipewire-gstreamer \
            python3-pillow python3-cairo zenity 2>/dev/null || true
        dnf install -y python3-qrcode python3-websocket-client 2>/dev/null || true
        
    elif command -v apt-get &>/dev/null; then
        print_status "Installing dependencies (Debian/Ubuntu/Mint)..."
        
        # Update package list
        apt-get update -qq 2>/dev/null || true
        
        # Core packages - try different names for compatibility
        apt-get install -y python3 python3-pip python3-venv 2>/dev/null || true
        
        # GTK4 and libadwaita - these are critical
        apt-get install -y python3-gi python3-gi-cairo gir1.2-gtk-4.0 2>/dev/null || true
        
        # libadwaita - different package names in different versions
        apt-get install -y gir1.2-adw-1 2>/dev/null || \
        apt-get install -y libadwaita-1-0 gir1.2-adw-1 2>/dev/null || true
        
        # GStreamer - including WebRTC support
        apt-get install -y \
            gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
            gstreamer1.0-plugins-bad gstreamer1.0-libav \
            gstreamer1.0-pipewire gstreamer1.0-nice \
            gir1.2-gst-plugins-bad-1.0 gir1.2-nice-0.1 2>/dev/null || true
        
        # GStreamer introspection for WebRTC (different package names)
        apt-get install -y libgstreamer-plugins-bad1.0-dev 2>/dev/null || true
        
        # Python packages - try apt first, then pip
        apt-get install -y python3-pil python3-pillow 2>/dev/null || true
        apt-get install -y python3-qrcode 2>/dev/null || true
        apt-get install -y python3-websocket 2>/dev/null || true
        
        # Dialog tools
        apt-get install -y zenity 2>/dev/null || apt-get install -y kdialog 2>/dev/null || true
        
    elif command -v pacman &>/dev/null; then
        print_status "Installing dependencies (Arch)..."
        pacman -Sy --noconfirm \
            python python-pip python-gobject gtk4 libadwaita \
            gst-plugins-base gst-plugins-good gst-plugins-bad gst-libav \
            pipewire gst-plugin-pipewire \
            python-pillow python-cairo zenity 2>/dev/null || true
        pacman -Sy --noconfirm python-qrcode python-websocket-client 2>/dev/null || true
        
    elif command -v zypper &>/dev/null; then
        print_status "Installing dependencies (openSUSE)..."
        zypper install -y \
            python3 python3-pip python3-gobject gtk4 libadwaita \
            gstreamer-plugins-base gstreamer-plugins-good gstreamer-plugins-bad \
            gstreamer-plugins-libav pipewire \
            python3-Pillow python3-cairo zenity 2>/dev/null || true
        zypper install -y python3-qrcode python3-websocket-client 2>/dev/null || true
        
    else
        print_warning "Unknown package manager - please install dependencies manually"
    fi
    
    # Install Python packages via pip as fallback (run as real user)
    local real_user=$(get_real_user)
    print_status "Installing Python packages via pip..."
    
    # Try with --break-system-packages first (newer pip), then without
    su - "$real_user" -c "pip3 install --user --break-system-packages pillow qrcode websocket-client 2>/dev/null" || \
    su - "$real_user" -c "pip3 install --user pillow qrcode websocket-client 2>/dev/null" || true
}

# Verify dependencies and show what's missing
verify_dependencies() {
    print_info "Verifying dependencies..."
    local all_ok=true
    
    # Check GTK4
    if python3 -c "import gi; gi.require_version('Gtk', '4.0'); from gi.repository import Gtk" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} GTK4"
    else
        echo -e "  ${RED}✗${NC} GTK4 - Install: gir1.2-gtk-4.0 (apt) or python3-gobject gtk4 (dnf)"
        all_ok=false
    fi
    
    # Check libadwaita
    if python3 -c "import gi; gi.require_version('Adw', '1'); from gi.repository import Adw" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} libadwaita"
    else
        echo -e "  ${RED}✗${NC} libadwaita - Install: gir1.2-adw-1 (apt) or libadwaita (dnf)"
        all_ok=false
    fi
    
    # Check GStreamer
    if python3 -c "import gi; gi.require_version('Gst', '1.0'); from gi.repository import Gst" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} GStreamer"
    else
        echo -e "  ${RED}✗${NC} GStreamer - Install: gstreamer1.0-plugins-base"
        all_ok=false
    fi
    
    # Check PIL/Pillow
    if python3 -c "from PIL import Image" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} Pillow"
    else
        echo -e "  ${YELLOW}!${NC} Pillow - Will install via pip"
    fi
    
    # Check qrcode
    if python3 -c "import qrcode" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} qrcode"
    else
        echo -e "  ${YELLOW}!${NC} qrcode - Will install via pip"
    fi
    
    # Check websocket
    if python3 -c "import websocket" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} websocket-client"
    else
        echo -e "  ${YELLOW}!${NC} websocket-client - Will install via pip"
    fi
    
    if [ "$all_ok" = false ]; then
        print_error "Critical dependencies missing!"
        print_info "On Ubuntu/Mint 22.04+, run:"
        echo "  sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1"
        print_info "On older Ubuntu/Mint, you may need to enable a PPA or use a newer version"
        return 1
    fi
    
    return 0
}

# Install StreamLinux
install_streamlinux() {
    print_status "Installing StreamLinux files..."
    
    local real_user=$(get_real_user)
    local real_home=$(eval echo ~$real_user)
    
    # Create directories
    mkdir -p /usr/local/lib/streamlinux
    mkdir -p /usr/local/lib/signaling-server
    mkdir -p /usr/share/applications
    mkdir -p /usr/share/icons/hicolor/scalable/apps
    mkdir -p /usr/share/metainfo
    
    # Install Python modules
    cp "$SCRIPT_DIR/streamlinux_gui.py" /usr/local/lib/streamlinux/
    cp "$SCRIPT_DIR/webrtc_streamer.py" /usr/local/lib/streamlinux/
    
    # Install optional modules if present
    [ -f "$SCRIPT_DIR/portal_screencast.py" ] && cp "$SCRIPT_DIR/portal_screencast.py" /usr/local/lib/streamlinux/
    [ -f "$SCRIPT_DIR/usb_manager.py" ] && cp "$SCRIPT_DIR/usb_manager.py" /usr/local/lib/streamlinux/
    
    # Create __init__.py
    touch /usr/local/lib/streamlinux/__init__.py
    
    # Install signaling server if present (check multiple locations)
    local signaling_found=false
    for sig_path in "$SCRIPT_DIR/signaling-server" "$SCRIPT_DIR/../signaling-server/signaling-server"; do
        if [ -f "$sig_path" ]; then
            print_status "Installing signaling server..."
            cp "$sig_path" /usr/local/lib/signaling-server/
            chmod +x /usr/local/lib/signaling-server/signaling-server
            signaling_found=true
            break
        fi
    done
    [ "$signaling_found" = false ] && print_warning "Signaling server not found in package"
    
    # Create improved launcher script
    cat > /usr/local/bin/streamlinux << 'LAUNCHER_EOF'
#!/usr/bin/env bash
# StreamLinux Launcher

# Add user pip packages to path
export PATH="$HOME/.local/bin:$PATH"

# Add common pip library paths
for pyver in python3.12 python3.11 python3.10 python3.9 python3; do
    [ -d "$HOME/.local/lib/$pyver/site-packages" ] && \
        export PYTHONPATH="$HOME/.local/lib/$pyver/site-packages:$PYTHONPATH"
done

# Change to app directory
cd /usr/local/lib/streamlinux || {
    echo "ERROR: StreamLinux not installed properly"
    exit 1
}

# Function to check dependencies
check_dependencies() {
    python3 << 'PYCHECK'
import sys
missing = []

try:
    import gi
except ImportError:
    missing.append("PyGObject (python3-gi)")
    print("Missing: PyGObject", file=sys.stderr)
    sys.exit(1)

try:
    gi.require_version('Gtk', '4.0')
    from gi.repository import Gtk
except:
    missing.append("GTK4 (gir1.2-gtk-4.0)")

try:
    gi.require_version('Adw', '1')
    from gi.repository import Adw
except:
    missing.append("libadwaita (gir1.2-adw-1)")

try:
    gi.require_version('Gst', '1.0')
    from gi.repository import Gst
except:
    missing.append("GStreamer (gstreamer1.0-plugins-base)")

try:
    from PIL import Image
except ImportError:
    missing.append("Pillow")

try:
    import qrcode
except ImportError:
    missing.append("qrcode")

try:
    import websocket
except ImportError:
    missing.append("websocket-client")

if missing:
    print("MISSING:" + ",".join(missing), file=sys.stderr)
    sys.exit(1)
sys.exit(0)
PYCHECK
}

# Check dependencies
dep_result=$(check_dependencies 2>&1)
dep_exit=$?

if [ $dep_exit -ne 0 ]; then
    # Extract missing packages
    missing_list=$(echo "$dep_result" | grep "^MISSING:" | sed 's/MISSING://')
    
    error_msg="StreamLinux cannot start.\n\nMissing dependencies:\n$missing_list\n\n"
    error_msg+="To fix, run:\n"
    error_msg+="  pip3 install --user pillow qrcode websocket-client\n\n"
    error_msg+="For GTK4/libadwaita on Ubuntu/Mint:\n"
    error_msg+="  sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1"
    
    # Show error dialog
    if command -v zenity &>/dev/null; then
        zenity --error --title="StreamLinux - Error" --text="$error_msg" --width=400 2>/dev/null
    elif command -v kdialog &>/dev/null; then
        kdialog --error "$error_msg" 2>/dev/null
    elif command -v notify-send &>/dev/null; then
        notify-send -u critical "StreamLinux Error" "Missing dependencies. Run 'streamlinux' in terminal for details."
    fi
    
    # Also print to terminal
    echo ""
    echo "=========================================="
    echo "  StreamLinux - Missing Dependencies"
    echo "=========================================="
    echo ""
    echo "Missing: $missing_list"
    echo ""
    echo "To fix Python packages:"
    echo "  pip3 install --user pillow qrcode websocket-client"
    echo ""
    echo "For GTK4/libadwaita on Ubuntu/Mint:"
    echo "  sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1"
    echo ""
    echo "For GTK4/libadwaita on Fedora:"
    echo "  sudo dnf install python3-gobject gtk4 libadwaita"
    echo ""
    exit 1
fi

# All good - launch the application
exec python3 streamlinux_gui.py "$@"
LAUNCHER_EOF
    chmod +x /usr/local/bin/streamlinux
    
    # Create symlink
    ln -sf /usr/local/bin/streamlinux /usr/local/bin/streamlinux-gui
    
    # Install desktop file
    cat > /usr/share/applications/com.streamlinux.host.desktop << 'DESKTOP_EOF'
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
    [ -f "$SCRIPT_DIR/data/icons/streamlinux.svg" ] && \
        cp "$SCRIPT_DIR/data/icons/streamlinux.svg" /usr/share/icons/hicolor/scalable/apps/com.streamlinux.host.svg
    
    # Install metainfo
    [ -f "$SCRIPT_DIR/data/com.streamlinux.host.metainfo.xml" ] && \
        cp "$SCRIPT_DIR/data/com.streamlinux.host.metainfo.xml" /usr/share/metainfo/
    
    # Update caches
    gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    update-desktop-database /usr/share/applications 2>/dev/null || true
    
    print_status "StreamLinux installed successfully!"
    echo ""
    print_info "To start StreamLinux:"
    echo "  • From application menu: look for 'StreamLinux'"
    echo "  • From terminal: streamlinux"
    echo ""
    
    # Test if it will work
    print_info "Testing installation..."
    if su - "$real_user" -c "cd /usr/local/lib/streamlinux && python3 -c \"
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
print('OK')
\" 2>/dev/null" | grep -q "OK"; then
        print_status "Installation test passed! StreamLinux should work."
    else
        print_warning "GTK4/libadwaita test failed"
        print_info "StreamLinux may not start. Check the errors above."
    fi
}

# Uninstall
uninstall_streamlinux() {
    print_status "Uninstalling StreamLinux..."
    
    rm -f /usr/local/bin/streamlinux
    rm -f /usr/local/bin/streamlinux-gui
    rm -rf /usr/local/lib/streamlinux
    rm -rf /usr/local/lib/signaling-server
    rm -f /usr/share/applications/com.streamlinux.host.desktop
    rm -f /usr/share/icons/hicolor/scalable/apps/com.streamlinux.host.svg
    rm -f /usr/share/metainfo/com.streamlinux.host.metainfo.xml
    
    gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    update-desktop-database /usr/share/applications 2>/dev/null || true
    
    print_status "StreamLinux uninstalled!"
}

# Diagnose issues
diagnose() {
    print_banner
    print_info "Running diagnostics..."
    echo ""
    
    # System info
    echo -e "${CYAN}System Information:${NC}"
    detect_distro >/dev/null
    echo "  Distribution: $DISTRO_NAME"
    echo "  Python: $(python3 --version 2>&1)"
    echo ""
    
    # Dependencies check
    echo -e "${CYAN}Dependencies:${NC}"
    verify_dependencies
    echo ""
    
    # Installation check
    echo -e "${CYAN}Installation:${NC}"
    [ -f /usr/local/bin/streamlinux ] && echo -e "  ${GREEN}✓${NC} Launcher installed" || echo -e "  ${RED}✗${NC} Launcher missing"
    [ -f /usr/local/lib/streamlinux/streamlinux_gui.py ] && echo -e "  ${GREEN}✓${NC} Main script installed" || echo -e "  ${RED}✗${NC} Main script missing"
    [ -f /usr/local/lib/signaling-server/signaling-server ] && echo -e "  ${GREEN}✓${NC} Signaling server installed" || echo -e "  ${YELLOW}!${NC} Signaling server missing"
    [ -f /usr/share/applications/com.streamlinux.host.desktop ] && echo -e "  ${GREEN}✓${NC} Desktop file installed" || echo -e "  ${RED}✗${NC} Desktop file missing"
    echo ""
    
    # Fix suggestions
    echo -e "${CYAN}Quick fixes:${NC}"
    echo "  pip3 install --user pillow qrcode websocket-client"
    echo ""
    
    if command -v apt-get &>/dev/null; then
        echo "  # For Ubuntu/Mint/Debian:"
        echo "  sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1"
    elif command -v dnf &>/dev/null; then
        echo "  # For Fedora:"
        echo "  sudo dnf install python3-gobject gtk4 libadwaita"
    elif command -v pacman &>/dev/null; then
        echo "  # For Arch:"
        echo "  sudo pacman -S python-gobject gtk4 libadwaita"
    fi
}

# Show help
show_help() {
    echo "StreamLinux Universal Installer v${VERSION}"
    echo ""
    echo "Usage: sudo bash $0 [command]"
    echo ""
    echo "Commands:"
    echo "  install     Install StreamLinux (default)"
    echo "  uninstall   Remove StreamLinux"
    echo "  diagnose    Check system and dependencies"
    echo "  deps        Install dependencies only"
    echo "  help        Show this help"
    echo ""
    echo "Examples:"
    echo "  sudo bash install.sh          # Install"
    echo "  sudo bash install.sh diagnose # Check issues"
    echo ""
}

# Main
main() {
    print_banner
    
    case "${1:-install}" in
        install)
            check_root
            install_dependencies
            echo ""
            verify_dependencies || true
            echo ""
            install_streamlinux
            ;;
        uninstall)
            check_root
            uninstall_streamlinux
            ;;
        diagnose|diag|check|test)
            diagnose
            ;;
        deps|dependencies)
            check_root
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
