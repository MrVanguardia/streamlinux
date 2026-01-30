#!/bin/bash
# StreamLinux Universal Linux Installer
# Works on: Ubuntu, Linux Mint, Debian, Fedora, Arch, openSUSE, and others
# Version: 0.2.0-alpha

set -e

VERSION="0.2.0"
INSTALL_DIR="/opt/streamlinux"
BIN_LINK="/usr/local/bin/streamlinux"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_banner() {
    echo ""
    echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║${NC}              ${GREEN}StreamLinux Installer v${VERSION}${NC}              ${BLUE}║${NC}"
    echo -e "${BLUE}║${NC}        Stream your Linux desktop to Android           ${BLUE}║${NC}"
    echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: This installer must be run as root${NC}"
        echo "Please run: sudo $0"
        exit 1
    fi
}

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_LIKE=$ID_LIKE
        DISTRO_VERSION=$VERSION_ID
    else
        DISTRO="unknown"
    fi
    
    echo -e "${BLUE}Detected:${NC} $PRETTY_NAME"
}

install_dependencies() {
    echo ""
    echo -e "${YELLOW}[1/5] Installing dependencies...${NC}"
    
    case "$DISTRO" in
        ubuntu|linuxmint|debian|pop|elementary|zorin)
            apt-get update -qq
            apt-get install -y -qq \
                python3 python3-gi python3-gi-cairo \
                gir1.2-gtk-4.0 gir1.2-adw-1 \
                gstreamer1.0-plugins-bad gstreamer1.0-plugins-good \
                gstreamer1.0-plugins-base gstreamer1.0-tools \
                gstreamer1.0-pipewire pipewire \
                libqrencode4 wget curl
            # Optional VAAPI
            apt-get install -y -qq gstreamer1.0-vaapi 2>/dev/null || true
            ;;
        fedora|rhel|centos|rocky|almalinux)
            dnf install -y -q \
                python3 python3-gobject gtk4 libadwaita \
                gstreamer1-plugins-bad-free gstreamer1-plugins-good \
                gstreamer1-plugins-base gstreamer1-tools \
                pipewire-gstreamer pipewire \
                qrencode wget curl
            dnf install -y -q gstreamer1-vaapi 2>/dev/null || true
            ;;
        arch|manjaro|endeavouros|garuda)
            pacman -Sy --noconfirm --needed \
                python python-gobject gtk4 libadwaita \
                gst-plugins-bad gst-plugins-good gst-plugins-base gstreamer \
                pipewire pipewire-gstreamer \
                qrencode wget curl
            pacman -S --noconfirm --needed gst-plugin-va 2>/dev/null || true
            ;;
        opensuse*|suse)
            zypper install -y \
                python3 python3-gobject gtk4 libadwaita \
                gstreamer-plugins-bad gstreamer-plugins-good \
                gstreamer-plugins-base gstreamer \
                pipewire \
                libqrencode4 wget curl
            ;;
        *)
            echo -e "${YELLOW}Warning: Unknown distribution. Attempting generic install...${NC}"
            echo "You may need to install dependencies manually:"
            echo "  - Python 3.10+"
            echo "  - GTK4, libadwaita"
            echo "  - GStreamer (plugins-bad, plugins-good, plugins-base)"
            echo "  - PipeWire"
            echo ""
            read -p "Continue anyway? [y/N] " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                exit 1
            fi
            ;;
    esac
    
    echo -e "${GREEN}      Dependencies installed${NC}"
}

download_files() {
    echo ""
    echo -e "${YELLOW}[2/5] Downloading StreamLinux...${NC}"
    
    DOWNLOAD_URL="https://github.com/MrVanguardia/streamlinux/archive/refs/tags/v${VERSION}-alpha.tar.gz"
    TEMP_DIR=$(mktemp -d)
    
    cd "$TEMP_DIR"
    
    if command -v wget &> /dev/null; then
        wget -q --show-progress -O streamlinux.tar.gz "$DOWNLOAD_URL"
    elif command -v curl &> /dev/null; then
        curl -L -o streamlinux.tar.gz "$DOWNLOAD_URL"
    else
        echo -e "${RED}Error: wget or curl required${NC}"
        exit 1
    fi
    
    tar -xzf streamlinux.tar.gz
    
    echo -e "${GREEN}      Download complete${NC}"
}

install_files() {
    echo ""
    echo -e "${YELLOW}[3/5] Installing files...${NC}"
    
    # Remove old installation
    rm -rf "$INSTALL_DIR"
    mkdir -p "$INSTALL_DIR"
    mkdir -p "$INSTALL_DIR/bin"
    
    # Find extracted directory
    SRC_DIR=$(find "$TEMP_DIR" -maxdepth 1 -type d -name "streamlinux-*" | head -1)
    
    # Copy Python files
    cp "$SRC_DIR/linux-gui/"*.py "$INSTALL_DIR/"
    
    # Copy data files
    cp -r "$SRC_DIR/linux-gui/data" "$INSTALL_DIR/"
    
    # Download pre-built signaling server binary
    echo "      Downloading signaling server..."
    SIGNALING_URL="https://github.com/MrVanguardia/streamlinux/releases/download/v${VERSION}-alpha/signaling-server-linux-amd64"
    
    # Try to download pre-built binary, fallback to building
    if wget -q -O "$INSTALL_DIR/bin/signaling-server" "$SIGNALING_URL" 2>/dev/null; then
        chmod +x "$INSTALL_DIR/bin/signaling-server"
    else
        echo "      Building signaling server from source..."
        if command -v go &> /dev/null; then
            cd "$SRC_DIR/signaling-server"
            go build -ldflags="-s -w" -o "$INSTALL_DIR/bin/signaling-server" ./cmd/server/
        else
            echo -e "${YELLOW}Warning: Go not installed. Installing minimal Go...${NC}"
            # Install Go temporarily
            case "$DISTRO" in
                ubuntu|linuxmint|debian|pop) apt-get install -y -qq golang-go ;;
                fedora|rhel|centos) dnf install -y -q golang ;;
                arch|manjaro) pacman -S --noconfirm go ;;
                opensuse*) zypper install -y go ;;
            esac
            cd "$SRC_DIR/signaling-server"
            go build -ldflags="-s -w" -o "$INSTALL_DIR/bin/signaling-server" ./cmd/server/
        fi
    fi
    
    echo -e "${GREEN}      Files installed${NC}"
}

create_launcher() {
    echo ""
    echo -e "${YELLOW}[4/5] Creating launcher...${NC}"
    
    # Create launcher script
    cat > "$BIN_LINK" << 'EOF'
#!/bin/bash
cd /opt/streamlinux
exec python3 streamlinux_gui.py "$@"
EOF
    chmod +x "$BIN_LINK"
    
    # Create desktop file
    cat > /usr/share/applications/com.streamlinux.host.desktop << EOF
[Desktop Entry]
Name=StreamLinux
Comment=Stream your Linux desktop to Android
Exec=streamlinux
Icon=/opt/streamlinux/data/icons/streamlinux.svg
Terminal=false
Type=Application
Categories=AudioVideo;Video;Network;
Keywords=stream;screen;share;android;webrtc;
StartupNotify=true
EOF
    
    # Update icon cache
    if command -v gtk-update-icon-cache &> /dev/null; then
        gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    fi
    
    # Update desktop database
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database /usr/share/applications 2>/dev/null || true
    fi
    
    echo -e "${GREEN}      Launcher created${NC}"
}

cleanup() {
    echo ""
    echo -e "${YELLOW}[5/5] Cleaning up...${NC}"
    rm -rf "$TEMP_DIR"
    echo -e "${GREEN}      Done${NC}"
}

print_success() {
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${NC}           StreamLinux installed successfully!            ${GREEN}║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "Launch from:"
    echo -e "  ${BLUE}•${NC} Applications menu → StreamLinux"
    echo -e "  ${BLUE}•${NC} Terminal: ${YELLOW}streamlinux${NC}"
    echo ""
    echo -e "${YELLOW}Note: This is alpha software. Report issues at:${NC}"
    echo -e "  https://github.com/MrVanguardia/streamlinux/issues"
    echo ""
}

uninstall() {
    echo -e "${YELLOW}Uninstalling StreamLinux...${NC}"
    rm -rf "$INSTALL_DIR"
    rm -f "$BIN_LINK"
    rm -f /usr/share/applications/com.streamlinux.host.desktop
    echo -e "${GREEN}StreamLinux uninstalled${NC}"
    exit 0
}

# Main
case "${1:-}" in
    --uninstall|-u)
        check_root
        uninstall
        ;;
    --help|-h)
        echo "StreamLinux Universal Installer"
        echo ""
        echo "Usage: sudo $0 [option]"
        echo ""
        echo "Options:"
        echo "  (none)        Install StreamLinux"
        echo "  --uninstall   Remove StreamLinux"
        echo "  --help        Show this help"
        exit 0
        ;;
    *)
        print_banner
        check_root
        detect_distro
        install_dependencies
        download_files
        install_files
        create_launcher
        cleanup
        print_success
        ;;
esac
