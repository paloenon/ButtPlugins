#!/bin/bash
# Builds the Buttcrusher Audio Unit and installs it for Logic Pro.
# Usage:  cd into this folder, then run:  ./build_mac.sh
set -e
cd "$(dirname "$0")"

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake is not installed."
    echo "Install it with Homebrew:   brew install cmake"
    echo "or download it from:        https://cmake.org/download/"
    exit 1
fi

echo "==> Configuring (downloads JUCE the first time, takes a minute)..."
cmake -B build-mac -G Xcode

echo "==> Building the Audio Unit..."
cmake --build build-mac --config Release --target Buttcrusher_AU

echo "==> Refreshing the macOS plug-in cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "==> Validating with Apple's auval..."
if auval -v aufx Bcrs Mypl >/tmp/buttcrusher_auval.log 2>&1; then
    echo "    auval: PASSED"
else
    echo "    auval reported a problem. See /tmp/buttcrusher_auval.log"
fi

echo ""
echo "Done! Installed to ~/Library/Audio/Plug-Ins/Components/Buttcrusher.component"
echo "Open Logic Pro -> Audio FX slot -> Audio Units -> MyPlugins -> Buttcrusher"
