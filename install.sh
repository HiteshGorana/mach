#!/bin/sh
# Mach Installer
# Usage: curl -fsSL https://raw.githubusercontent.com/HiteshGorana/mach/main/install.sh | sh

set -e

REPO="HiteshGorana/mach"
INSTALL_DIR="$HOME/.mach/bin"

# Detect OS
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$OS" in
    linux)
        PLATFORM="linux"
        ;;
    darwin)
        PLATFORM="macos"
        ;;
    mingw*|msys*|cygwin*)
        PLATFORM="windows"
        ;;
    *)
        echo "Unsupported OS: $OS"
        exit 1
        ;;
esac

case "$ARCH" in
    x86_64|amd64)
        ARCH="x86_64"
        ;;
    arm64|aarch64)
        ARCH="arm64"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Build filename
if [ "$PLATFORM" = "windows" ]; then
    FILENAME="mach-windows-x86_64.exe"
    INSTALL_DIR="$HOME/bin"
else
    FILENAME="mach-${PLATFORM}-${ARCH}"
fi

echo "⚡ Installing Mach..."
echo "   Platform: $PLATFORM"
echo "   Architecture: $ARCH"

# Get latest release URL
LATEST=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" | grep "browser_download_url.*$FILENAME" | cut -d '"' -f 4)

if [ -z "$LATEST" ]; then
    echo "❌ Could not find release for $FILENAME"
    echo "   Please check: https://github.com/$REPO/releases"
    exit 1
fi

# Ensure install directory exists and is actually a directory
if [ -e "$INSTALL_DIR" ] && [ ! -d "$INSTALL_DIR" ]; then
    echo "❌ $INSTALL_DIR exists but is not a directory. Please remove it and try again."
    exit 1
fi

mkdir -p "$INSTALL_DIR"

if [ ! -w "$INSTALL_DIR" ]; then
    echo "❌ Cannot write to $INSTALL_DIR. Check your permissions."
    exit 1
fi

echo "   Downloading: $FILENAME"

# Download to a temporary file first
TMP_FILE=$(mktemp "/tmp/mach.XXXXXX")
if ! curl -fsSL "$LATEST" -o "$TMP_FILE"; then
    echo "❌ Download failed."
    rm -f "$TMP_FILE"
    exit 1
fi

# Install
if [ "$PLATFORM" = "windows" ]; then
    mv "$TMP_FILE" "$INSTALL_DIR/mach.exe"
    echo "✅ Installed to $INSTALL_DIR/mach.exe"
else
    mv "$TMP_FILE" "$INSTALL_DIR/mach"
    chmod +x "$INSTALL_DIR/mach"
    echo "✅ Installed to $INSTALL_DIR/mach"
fi

# Verify
if command -v mach >/dev/null 2>&1; then
    echo ""
    mach version
    echo ""
    echo "🚀 Run 'mach http://example.com' to get started!"
else
    echo ""
    echo "⚠️  Add $INSTALL_DIR to your PATH:"
    echo "   export PATH=\"\$PATH:$INSTALL_DIR\""
fi
