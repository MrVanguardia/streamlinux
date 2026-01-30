#!/bin/bash
#
# StreamLinux RPM Build Script
# Version: 0.2.0-alpha
#
# Prerequisites:
#   sudo dnf install rpm-build rpmdevtools python3-devel desktop-file-utils libappstream-glib golang
#

set -e

VERSION="0.2.0"
RELEASE="alpha"
PACKAGE_NAME="streamlinux"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       StreamLinux RPM Builder v${VERSION}-${RELEASE}              ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if running on Fedora/RHEL
if ! command -v rpmbuild &> /dev/null; then
    echo -e "${RED}Error: rpmbuild not found. Install with:${NC}"
    echo "  sudo dnf install rpm-build rpmdevtools"
    exit 1
fi

# Setup RPM build environment
echo -e "${YELLOW}[1/6] Setting up RPM build environment...${NC}"
rpmdev-setuptree 2>/dev/null || true

RPM_BUILD_DIR="$HOME/rpmbuild"
SOURCES_DIR="$RPM_BUILD_DIR/SOURCES"
SPECS_DIR="$RPM_BUILD_DIR/SPECS"
RPMS_DIR="$RPM_BUILD_DIR/RPMS"

# Build signaling server if needed
echo -e "${YELLOW}[2/6] Building signaling server...${NC}"
SIGNALING_DIR="$PROJECT_ROOT/signaling-server"
SIGNALING_BIN="$SIGNALING_DIR/signaling-server"

if [ ! -f "$SIGNALING_BIN" ] || [ "$1" == "--rebuild" ]; then
    echo "  Compiling Go signaling server..."
    cd "$SIGNALING_DIR"
    
    # Check Go version
    if ! command -v go &> /dev/null; then
        echo -e "${RED}Error: Go not found. Install with: sudo dnf install golang${NC}"
        exit 1
    fi
    
    GO_VERSION=$(go version | awk '{print $3}' | sed 's/go//')
    echo "  Using Go version: $GO_VERSION"
    
    # Build with optimizations
    CGO_ENABLED=0 go build -ldflags="-s -w -X main.Version=${VERSION}" -o signaling-server ./cmd/server/
    
    echo -e "${GREEN}  ✓ Signaling server built successfully${NC}"
else
    echo -e "${GREEN}  ✓ Signaling server already exists${NC}"
fi

# Create source tarball
echo -e "${YELLOW}[3/6] Creating source tarball...${NC}"
TARBALL_NAME="${PACKAGE_NAME}-${VERSION}"
TARBALL_DIR=$(mktemp -d)
TARBALL_PATH="$TARBALL_DIR/$TARBALL_NAME"

mkdir -p "$TARBALL_PATH"
mkdir -p "$TARBALL_PATH/data/icons"

# Copy Python files
cp "$PROJECT_ROOT/linux-gui/streamlinux_gui.py" "$TARBALL_PATH/"
cp "$PROJECT_ROOT/linux-gui/webrtc_streamer.py" "$TARBALL_PATH/"
cp "$PROJECT_ROOT/linux-gui/i18n.py" "$TARBALL_PATH/"
cp "$PROJECT_ROOT/linux-gui/portal_screencast.py" "$TARBALL_PATH/"
cp "$PROJECT_ROOT/linux-gui/usb_manager.py" "$TARBALL_PATH/"
cp "$PROJECT_ROOT/linux-gui/security.py" "$TARBALL_PATH/"

# Copy data files
cp "$PROJECT_ROOT/linux-gui/data/com.streamlinux.host.desktop" "$TARBALL_PATH/data/"
cp "$PROJECT_ROOT/linux-gui/data/com.streamlinux.host.metainfo.xml" "$TARBALL_PATH/data/"
cp "$PROJECT_ROOT/linux-gui/data/icons/streamlinux.svg" "$TARBALL_PATH/data/icons/" 2>/dev/null || \
    echo '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><circle cx="32" cy="32" r="30" fill="#3584e4"/><path d="M20 24l24 8-24 8z" fill="white"/></svg>' > "$TARBALL_PATH/data/icons/streamlinux.svg"

# Copy signaling server binary
cp "$SIGNALING_BIN" "$TARBALL_PATH/"

# Copy docs
cp "$PROJECT_ROOT/LICENSE" "$TARBALL_PATH/" 2>/dev/null || echo "MIT License" > "$TARBALL_PATH/LICENSE"
cp "$PROJECT_ROOT/README.md" "$TARBALL_PATH/" 2>/dev/null || echo "# StreamLinux" > "$TARBALL_PATH/README.md"

# Create tarball
cd "$TARBALL_DIR"
tar -czf "${TARBALL_NAME}.tar.gz" "$TARBALL_NAME"
cp "${TARBALL_NAME}.tar.gz" "$SOURCES_DIR/"

echo -e "${GREEN}  ✓ Source tarball created: ${TARBALL_NAME}.tar.gz${NC}"

# Copy spec file
echo -e "${YELLOW}[4/6] Copying spec file...${NC}"
cp "$SCRIPT_DIR/streamlinux.spec" "$SPECS_DIR/"
echo -e "${GREEN}  ✓ Spec file copied${NC}"

# Build RPM
echo -e "${YELLOW}[5/6] Building RPM package...${NC}"
cd "$SPECS_DIR"

rpmbuild -ba streamlinux.spec 2>&1 | tee /tmp/rpmbuild.log

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo -e "${RED}Error: RPM build failed. Check /tmp/rpmbuild.log${NC}"
    exit 1
fi

# Find and copy the built RPM
echo -e "${YELLOW}[6/6] Finalizing...${NC}"
OUTPUT_DIR="$PROJECT_ROOT/releases/rpm"
mkdir -p "$OUTPUT_DIR"

ARCH=$(uname -m)
RPM_FILE=$(find "$RPMS_DIR" -name "${PACKAGE_NAME}-${VERSION}*.rpm" -type f | head -1)

if [ -n "$RPM_FILE" ]; then
    cp "$RPM_FILE" "$OUTPUT_DIR/"
    RPM_BASENAME=$(basename "$RPM_FILE")
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    BUILD SUCCESSFUL!                     ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  ${BLUE}RPM Package:${NC} $OUTPUT_DIR/$RPM_BASENAME"
    echo ""
    echo -e "  ${YELLOW}To install:${NC}"
    echo "    sudo dnf install $OUTPUT_DIR/$RPM_BASENAME"
    echo ""
    echo -e "  ${YELLOW}To test (without installing):${NC}"
    echo "    rpm -qilp $OUTPUT_DIR/$RPM_BASENAME"
    echo ""
else
    echo -e "${RED}Error: RPM file not found after build${NC}"
    exit 1
fi

# Cleanup
rm -rf "$TARBALL_DIR"

echo -e "${GREEN}Done!${NC}"
