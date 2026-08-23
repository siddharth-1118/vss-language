#!/bin/sh
# VSS Linux/macOS Installer Script
set -e

INSTALL_DIR="$HOME/.vss"
BIN_DIR="$INSTALL_DIR/bin"
TMP_DIR="/tmp/vss-install"

echo "[*] Detecting OS and architecture..."
OS="$(uname -s)"
ARCH="$(uname -m)"
ASSET_NAME=""

case "$OS" in
    Linux)
        if [ "$ARCH" = "x86_64" ]; then
            ASSET_NAME="vss-linux-x64.tar.gz"
        else
            echo "[x] Unsupported architecture for Linux: $ARCH"
            exit 1
        fi
        ;;
    Darwin)
        if [ "$ARCH" = "arm64" ]; then
            ASSET_NAME="vss-macos-arm64.tar.gz"
        elif [ "$ARCH" = "x86_64" ]; then
            ASSET_NAME="vss-macos-x64.tar.gz"
        else
            echo "[x] Unsupported architecture for macOS: $ARCH"
            exit 1
        fi
        ;;
    *)
        echo "[x] Unsupported Operating System: $OS"
        exit 1
        ;;
esac

DOWNLOAD_URL="https://github.com/siddharth-1118/vss-language/releases/latest/download/$ASSET_NAME"

echo "[*] Downloading VSS ($ASSET_NAME)..."
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"
curl -sSfL "$DOWNLOAD_URL" -o "$TMP_DIR/vss.tar.gz"

echo "[*] Extracting VSS to $BIN_DIR..."
mkdir -p "$BIN_DIR"
tar -xzf "$TMP_DIR/vss.tar.gz" -C "$BIN_DIR"
rm -rf "$TMP_DIR"

add_to_profile() {
    PROFILE_FILE="$1"
    if [ -f "$PROFILE_FILE" ]; then
        if ! grep -q "$BIN_DIR" "$PROFILE_FILE"; then
            echo "export PATH=\"\$PATH:$BIN_DIR\"" >> "$PROFILE_FILE"
            echo "[*] Added VSS to $PROFILE_FILE"
        fi
    fi
}

echo "[*] Updating shell profiles..."
add_to_profile "$HOME/.bashrc"
add_to_profile "$HOME/.zshrc"
add_to_profile "$HOME/.profile"

echo ""
echo "[+] VSS Installed Successfully!"
echo "----------------------------------------"
echo "Installed at: $BIN_DIR"
echo ""
echo "To start using VSS, open a new terminal session or run:"
echo "  export PATH=\"\$PATH:$BIN_DIR\""
echo "  vss help"
