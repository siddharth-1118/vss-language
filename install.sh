#!/bin/bash
# install.sh
# official VSS installer for Linux and macOS

VSS_DIR="$HOME/.vss"
BIN_DIR="$VSS_DIR/bin"
mkdir -p "$BIN_DIR"

# Copy compiled compiler to install dir
if [ -f "vss/vss" ]; then
    cp "vss/vss" "$BIN_DIR/vss"
    chmod +x "$BIN_DIR/vss"
    echo "Compiled binary copied to $BIN_DIR/vss."
else
    echo "Error: Compiled binary 'vss/vss' not found. Did you run make?"
    exit 1
fi

# Detect shell profile to add PATH
SHELL_PROFILE=""
if [ -n "$bash" ]; then
    SHELL_PROFILE="$HOME/.bashrc"
elif [ -f "$HOME/.zshrc" ]; then
    SHELL_PROFILE="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    SHELL_PROFILE="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile" ]; then
    SHELL_PROFILE="$HOME/.bash_profile"
elif [ -f "$HOME/.profile" ]; then
    SHELL_PROFILE="$HOME/.profile"
fi

if [ -n "$SHELL_PROFILE" ]; then
    if ! grep -q "$BIN_DIR" "$SHELL_PROFILE"; then
        echo 'export PATH="$PATH:'"$BIN_DIR"'"' >> "$SHELL_PROFILE"
        echo -e "\033[1;32mVSS has been successfully installed globally!\033[0m"
        echo "Please run 'source $SHELL_PROFILE' or restart your terminal to activate the command."
    else
        echo "VSS is already configured in $SHELL_PROFILE PATH."
    fi
else
    echo -e "\033[1;33mWarning:\033[0m Shell profile not found. Please manually add the following to your PATH:"
    echo "  export PATH=\"\$PATH:$BIN_DIR\""
fi
