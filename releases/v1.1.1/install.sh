#!/bin/bash
#
# StreamLinux Universal Installer v1.1.1
# ======================================
# This script installs StreamLinux on various Linux distributions.
#
# ⚠️  WARNING: This installer is EXPERIMENTAL and has only been tested on:
#     - Linux Mint 21/22
#     - Fedora 39/40
#
#     It may work on other distributions but is NOT guaranteed.
#     Please report issues at: https://github.com/MrVanguardia/streamlinux/issues
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

VERSION="1.1.1"
GITHUB_URL="https://github.com/MrVanguardia/streamlinux"
RELEASE_URL="$GITHUB_URL/releases/download/v$VERSION"

# Installation paths
INSTALL_DIR="/usr/share/streamlinux"
BIN_DIR="/usr/bin"
DESKTOP_DIR="/usr/share/applications"
ICON_DIR="/usr/share/icons/hicolor/scalable/apps"
SIGNALING_DIR="/usr/local/lib/signaling-server"

print_banner() {
    echo -e "${CYAN}"
    echo "╔═══════════════════════════════════════════════════════════════╗"
    echo "║                                                               ║"
    echo "║   ${BOLD}███████╗████████╗██████╗ ███████╗ █████╗ ███╗   ███╗${NC}${CYAN}        ║"
    echo "║   ${BOLD}██╔════╝╚══██╔══╝██╔══██╗██╔════╝██╔══██╗████╗ ████║${NC}${CYAN}        ║"
    echo "║   ${BOLD}███████╗   ██║   ██████╔╝█████╗  ███████║██╔████╔██║${NC}${CYAN}        ║"
    echo "║   ${BOLD}╚════██║   ██║   ██╔══██╗██╔══╝  ██╔══██║██║╚██╔╝██║${NC}${CYAN}        ║"
    echo "║   ${BOLD}███████║   ██║   ██║  ██║███████╗██║  ██║██║ ╚═╝ ██║${NC}${CYAN}        ║"
    echo "║   ${BOLD}╚══════╝   ╚═╝   ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝${NC}${CYAN}        ║"
    echo "║                          ${BOLD}LINUX${NC}${CYAN}                              ║"
    echo "║                                                               ║"
    echo "║                  Universal Installer v$VERSION                   ║"
    echo "║                                                               ║"
    echo "╚═══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_warning() {
    echo -e "${YELLOW}${BOLD}⚠️  WARNING${NC}"
    echo -e "${YELLOW}This installer is ${BOLD}EXPERIMENTAL${NC}${YELLOW} and has only been tested on:${NC}"
    echo -e "${YELLOW}  • Linux Mint 21/22${NC}"
    echo -e "${YELLOW}  • Fedora 39/40${NC}"
    echo ""
    echo -e "${YELLOW}It may work on other distributions but is NOT guaranteed.${NC}"
    echo -e "${YELLOW}Please report issues at: ${BLUE}$GITHUB_URL/issues${NC}"
    echo ""
}

print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[i]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_step() {
    echo -e "\n${PURPLE}▶${NC} ${BOLD}$1${NC}"
}

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_LIKE=$ID_LIKE
        DISTRO_VERSION=$VERSION_ID
        DISTRO_NAME=$PRETTY_NAME
    else
        DISTRO="unknown"
        DISTRO_NAME="Unknown Distribution"
    fi
    
    print_info "Detected: $DISTRO_NAME"
}

detect_package_manager() {
    if command -v dnf &> /dev/null; then
        PKG_MANAGER="dnf"
        PKG_INSTALL="sudo dnf install -y"
    elif command -v apt &> /dev/null; then
        PKG_MANAGER="apt"
        PKG_INSTALL="sudo apt install -y"
    elif command -v pacman &> /dev/null; then
        PKG_MANAGER="pacman"
        PKG_INSTALL="sudo pacman -S --noconfirm"
    elif command -v zypper &> /dev/null; then
        PKG_MANAGER="zypper"
        PKG_INSTALL="sudo zypper install -y"
    else
        print_error "No supported package manager found!"
        exit 1
    fi
    
    print_info "Package manager: $PKG_MANAGER"
}

check_requirements() {
    print_step "Checking system requirements..."
    
    # Check if running as root
    if [ "$EUID" -eq 0 ]; then
        print_error "Please do not run this script as root. It will ask for sudo when needed."
        exit 1
    fi
    
    # Check for required commands
    local required_cmds=("curl" "wget")
    for cmd in "${required_cmds[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            print_info "Installing $cmd..."
            $PKG_INSTALL $cmd
        fi
    done
    
    print_status "System requirements met"
}

install_dependencies() {
    print_step "Installing dependencies..."
    
    case $PKG_MANAGER in
        apt)
            print_info "Updating package lists..."
            sudo apt update
            
            print_info "Installing Python and GTK4 dependencies..."
            $PKG_INSTALL \
                python3 \
                python3-pip \
                python3-gi \
                python3-gi-cairo \
                gir1.2-gtk-4.0 \
                gir1.2-adw-1 \
                libadwaita-1-0 \
                gstreamer1.0-tools \
                gstreamer1.0-plugins-base \
                gstreamer1.0-plugins-good \
                gstreamer1.0-plugins-bad \
                pipewire \
                xdg-desktop-portal \
                xdg-desktop-portal-gtk
            ;;
        dnf)
            print_info "Installing Python and GTK4 dependencies..."
            $PKG_INSTALL \
                python3 \
                python3-pip \
                python3-gobject \
                gtk4 \
                libadwaita \
                gstreamer1-plugins-base \
                gstreamer1-plugins-good \
                gstreamer1-plugins-bad-free \
                pipewire \
                xdg-desktop-portal \
                xdg-desktop-portal-gtk \
                xdg-desktop-portal-gnome
            ;;
        pacman)
            print_info "Installing Python and GTK4 dependencies..."
            $PKG_INSTALL \
                python \
                python-pip \
                python-gobject \
                gtk4 \
                libadwaita \
                gstreamer \
                gst-plugins-base \
                gst-plugins-good \
                gst-plugins-bad \
                pipewire \
                xdg-desktop-portal \
                xdg-desktop-portal-gtk
            ;;
        zypper)
            print_info "Installing Python and GTK4 dependencies..."
            $PKG_INSTALL \
                python3 \
                python3-pip \
                python3-gobject \
                gtk4 \
                libadwaita-1 \
                gstreamer-plugins-base \
                gstreamer-plugins-good \
                pipewire \
                xdg-desktop-portal \
                xdg-desktop-portal-gtk
            ;;
    esac
    
    # Install Python packages
    print_info "Installing Python packages..."
    pip3 install --user qrcode pillow websocket-client 2>/dev/null || \
    pip3 install --break-system-packages --user qrcode pillow websocket-client 2>/dev/null || \
    print_info "Python packages may already be installed or require manual installation"
    
    print_status "Dependencies installed"
}

download_files() {
    print_step "Downloading StreamLinux files..."
    
    # Create temp directory
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    # Download main Python files from GitHub
    local files=("streamlinux_gui.py" "webrtc_streamer.py" "portal_screencast.py" "security.py" "i18n.py" "usb_manager.py")
    
    for file in "${files[@]}"; do
        print_info "Downloading $file..."
        curl -sL "$GITHUB_URL/raw/main/linux-gui/$file" -o "$file" || {
            print_error "Failed to download $file"
            exit 1
        }
    done
    
    # Download signaling server
    print_info "Downloading signaling server..."
    curl -sL "$GITHUB_URL/raw/main/signaling-server/signaling-server" -o "signaling-server" 2>/dev/null || \
    print_info "Signaling server binary not available, will need to be built manually"
    
    # Download icon and desktop file
    print_info "Downloading assets..."
    curl -sL "$GITHUB_URL/raw/main/linux-gui/data/icons/streamlinux.svg" -o "streamlinux.svg"
    curl -sL "$GITHUB_URL/raw/main/linux-gui/data/com.streamlinux.host.desktop" -o "com.streamlinux.host.desktop"
    
    print_status "Files downloaded"
}

install_files() {
    print_step "Installing StreamLinux..."
    
    # Create directories
    sudo mkdir -p "$INSTALL_DIR"
    sudo mkdir -p "$ICON_DIR"
    sudo mkdir -p "$SIGNALING_DIR"
    
    # Install Python files
    print_info "Installing application files..."
    sudo cp streamlinux_gui.py "$BIN_DIR/streamlinux-gui"
    sudo chmod +x "$BIN_DIR/streamlinux-gui"
    
    sudo cp webrtc_streamer.py "$INSTALL_DIR/"
    sudo cp portal_screencast.py "$INSTALL_DIR/"
    sudo cp security.py "$INSTALL_DIR/"
    sudo cp i18n.py "$INSTALL_DIR/"
    sudo cp usb_manager.py "$INSTALL_DIR/"
    
    # Install signaling server if available
    if [ -f "signaling-server" ] && [ -s "signaling-server" ]; then
        print_info "Installing signaling server..."
        sudo cp signaling-server "$SIGNALING_DIR/"
        sudo chmod +x "$SIGNALING_DIR/signaling-server"
    fi
    
    # Install icon and desktop file
    print_info "Installing desktop integration..."
    sudo cp streamlinux.svg "$ICON_DIR/"
    sudo cp com.streamlinux.host.desktop "$DESKTOP_DIR/"
    
    # Update caches
    sudo gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    sudo update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
    
    # Cleanup
    cd /
    rm -rf "$TEMP_DIR"
    
    print_status "StreamLinux installed successfully!"
}

print_success() {
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                               ║${NC}"
    echo -e "${GREEN}║   ${BOLD}✅ StreamLinux v$VERSION installed successfully!${NC}${GREEN}            ║${NC}"
    echo -e "${GREEN}║                                                               ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${BOLD}To start StreamLinux:${NC}"
    echo -e "  • From terminal: ${CYAN}streamlinux-gui${NC}"
    echo -e "  • From applications menu: Search for ${CYAN}StreamLinux${NC}"
    echo ""
    echo -e "${BOLD}Android app:${NC}"
    echo -e "  Download from: ${BLUE}$GITHUB_URL/releases${NC}"
    echo ""
    echo -e "${YELLOW}Note: You may need to log out and log back in for the${NC}"
    echo -e "${YELLOW}application to appear in your applications menu.${NC}"
    echo ""
}

uninstall() {
    print_step "Uninstalling StreamLinux..."
    
    sudo rm -f "$BIN_DIR/streamlinux-gui"
    sudo rm -rf "$INSTALL_DIR"
    sudo rm -rf "$SIGNALING_DIR"
    sudo rm -f "$DESKTOP_DIR/com.streamlinux.host.desktop"
    sudo rm -f "$ICON_DIR/streamlinux.svg"
    
    sudo gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    sudo update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
    
    print_status "StreamLinux uninstalled successfully!"
}

# Main
main() {
    clear
    print_banner
    print_warning
    
    echo -e "${BOLD}What would you like to do?${NC}"
    echo "  1) Install StreamLinux"
    echo "  2) Uninstall StreamLinux"
    echo "  3) Exit"
    echo ""
    read -p "Enter your choice [1-3]: " choice
    
    case $choice in
        1)
            detect_distro
            detect_package_manager
            check_requirements
            install_dependencies
            download_files
            install_files
            print_success
            ;;
        2)
            uninstall
            ;;
        3)
            echo "Goodbye!"
            exit 0
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
}

# Check for --uninstall flag
if [ "$1" == "--uninstall" ] || [ "$1" == "-u" ]; then
    uninstall
    exit 0
fi

main
