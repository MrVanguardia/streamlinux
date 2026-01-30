#!/bin/bash
# StreamLinux Arch Linux Package Builder
# Creates a package using makepkg

set -e

VERSION="0.2.0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

echo "=== StreamLinux Arch Linux Package Builder ==="
echo "Version: ${VERSION}"

# Check if we're on Arch
if ! command -v makepkg &> /dev/null; then
    echo "Error: makepkg not found. This script requires Arch Linux or derivatives."
    echo ""
    echo "For manual installation on Arch:"
    echo "  1. Copy PKGBUILD to a build directory"
    echo "  2. Run: makepkg -si"
    exit 1
fi

# Create build directory
BUILD_DIR="${SCRIPT_DIR}/build"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Create source tarball
echo ""
echo "[1/3] Creating source tarball..."
cd "${PROJECT_ROOT}"
TARBALL="streamlinux-${VERSION}.tar.gz"
tar --transform "s,^,streamlinux-${VERSION}-alpha/," \
    -czf "${BUILD_DIR}/${TARBALL}" \
    linux-gui/*.py \
    linux-gui/data/ \
    signaling-server/cmd/ \
    signaling-server/internal/ \
    signaling-server/go.mod \
    signaling-server/go.sum \
    LICENSE \
    README.md

# Copy PKGBUILD
echo ""
echo "[2/3] Preparing PKGBUILD..."
cp "${SCRIPT_DIR}/PKGBUILD" "${BUILD_DIR}/"

# Update sha256sum
cd "${BUILD_DIR}"
SHA256=$(sha256sum "${TARBALL}" | cut -d' ' -f1)
sed -i "s/sha256sums=('SKIP')/sha256sums=('${SHA256}')/" PKGBUILD
sed -i "s|source=(.*)|source=(\"${TARBALL}\")|" PKGBUILD

# Build package
echo ""
echo "[3/3] Building package..."
makepkg -f

# Move to releases
RELEASES_DIR="${PROJECT_ROOT}/releases/arch"
mkdir -p "${RELEASES_DIR}"
mv streamlinux-*.pkg.tar.zst "${RELEASES_DIR}/" 2>/dev/null || true

echo ""
echo "=== Build Complete ==="
echo "Package: ${RELEASES_DIR}/streamlinux-${VERSION}-1-x86_64.pkg.tar.zst"
echo ""
echo "Install with:"
echo "  sudo pacman -U streamlinux-${VERSION}-1-x86_64.pkg.tar.zst"
echo ""

# Cleanup
rm -rf "${BUILD_DIR}"
