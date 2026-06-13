#!/bin/bash
set -e

# MEGA-PI Bare-Metal Release Packager Script

echo "=== Packaging MEGA-PI Release ==="

# Define paths
ROOT_DIR="$(pwd)"
EMU_DIR="$ROOT_DIR/emulator"
RELEASE_DIR="$ROOT_DIR/release_temp"

# Get version from argument, VERSION file, git tag/hash, or fallback
if [ -n "$1" ]; then
    RAW_VERSION="$1"
elif [ -f "$ROOT_DIR/VERSION" ]; then
    RAW_VERSION="$(cat "$ROOT_DIR/VERSION" | xargs)"
else
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        RAW_VERSION=$(git describe --tags --always --dirty 2>/dev/null || git rev-parse --short HEAD 2>/dev/null)
    fi
    if [ -z "$RAW_VERSION" ]; then
        RAW_VERSION="1.0"
    fi
fi

# Ensure version has a leading 'v' prefix
if [ "${RAW_VERSION:0:1}" = "v" ]; then
    VERSION="$RAW_VERSION"
else
    VERSION="v$RAW_VERSION"
fi

echo "Version selected: $VERSION"

ZIP_NAME="$ROOT_DIR/mega-pi-release-$VERSION.zip"

# Ensure we are in the root directory
if [ ! -d "$EMU_DIR" ]; then
    echo "Error: Please run this script from the project root directory."
    exit 1
fi

# Clean up any existing zip or temp dir
rm -f "$ZIP_NAME"
rm -rf "$RELEASE_DIR"

# Create release structure
mkdir -p "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/roms"
mkdir -p "$RELEASE_DIR/bios"

# Copy bootloader and firmware files
echo "Copying firmware and bootloader configuration..."
if [ -d "$EMU_DIR/boot" ]; then
    cp -r "$EMU_DIR/boot/"* "$RELEASE_DIR/"
else
    echo "Error: Boot files directory $EMU_DIR/boot not found."
    exit 1
fi

# Copy compiled kernel image(s)
echo "Copying compiled kernel images..."
KERNEL_COPIED=0
for img in "$EMU_DIR"/*.img; do
    if [ -f "$img" ]; then
        echo "Found: $(basename "$img")"
        cp "$img" "$RELEASE_DIR/"
        KERNEL_COPIED=1
    fi
done

if [ "$KERNEL_COPIED" -eq 0 ]; then
    echo "Warning: No compiled kernel images (*.img) found in $EMU_DIR."
    echo "Please compile the emulator first using: make -C emulator"
fi

# Zip the release package
echo "Creating zip archive $ZIP_NAME..."
cd "$RELEASE_DIR"
zip -r "$ZIP_NAME" ./* > /dev/null
cd "$ROOT_DIR"

# Clean up
rm -rf "$RELEASE_DIR"

echo "=== Release Package Created Successfully ==="
echo "Zip File: mega-pi-release-$VERSION.zip"
echo "Instructions: Extract all contents of the zip file directly to the root of your FAT32-formatted SD card."
