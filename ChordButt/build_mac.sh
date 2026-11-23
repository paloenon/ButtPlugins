#!/bin/bash
# Builds the ChordButt Audio Unit (MIDI FX) and installs it for Logic Pro.
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
cmake -B build-mac -G Xcode ${CHORDBUTT_CMAKE_ARGS}

echo "==> Building the Audio Unit..."
cmake --build build-mac --config Release --target ChordButt_AU

echo "==> Refreshing the macOS plug-in cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "==> Validating with Apple's auval..."
if auval -v aumi Chbt Mypl >/tmp/chordbutt_auval.log 2>&1; then
    echo "    auval: PASSED"
else
    echo "    auval reported a problem. See /tmp/chordbutt_auval.log"
    exit 1
fi

echo ""
echo "Done! Installed to ~/Library/Audio/Plug-Ins/Components/ChordButt.component"
echo "Open Logic Pro -> MIDI FX slot -> Audio Units -> MyPlugins -> ChordButt"
