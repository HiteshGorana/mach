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

echo "   Downloading: $LATEST"

# Create install dir if needed
mkdir -p "$INSTALL_DIR"

# Download and install
if [ "$PLATFORM" = "windows" ]; then
    curl -fsSL "$LATEST" -o "$INSTALL_DIR/mach.exe"
    echo "✅ Installed to $INSTALL_DIR/mach.exe"
else
    curl -fsSL "$LATEST" -o "$INSTALL_DIR/mach"
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
