#!/bin/bash
# uninstall.sh
# official VSS uninstaller for Linux and macOS

VSS_DIR="$HOME/.vss"
if [ -d "$VSS_DIR" ]; then
    rm -rf "$VSS_DIR"
    echo -e "\033[1;32mVSS installation files deleted.\033[0m"
else
    echo "VSS is not installed."
fi

echo "To finish uninstalling, please remove the path '$VSS_DIR/bin' from your shell profile (e.g. .bashrc or .zshrc)."
