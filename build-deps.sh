#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
OS="$(uname -s)"

case "$OS" in
    Darwin)
        exec bash "$ROOT_DIR/scripts/build/build-deps-macos.sh"
        ;;
    Linux)
        exec bash "$ROOT_DIR/scripts/build/build-deps-linux.sh"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        exec bash "$ROOT_DIR/scripts/build/build-deps-windows.sh"
        ;;
    *)
        echo "Unsupported OS: $OS"
        exit 1
        ;;
esac
