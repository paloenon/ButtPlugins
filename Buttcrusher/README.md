# Buttcrusher – Audio Unit for Logic Pro

A lo-fi bitcrusher effect built with JUCE (C++).

## Controls

| Knob   | What it does |
|--------|--------------|
| DRIVE  | Boosts the input (0–24 dB) before crushing. Pushing it adds hard digital clipping. |
| BITS   | Bit depth, 16 → 1. Lower = grainier, noisier. Sweeps smoothly, so it's fun to automate. |
| RATE   | Sample-rate reduction (1/1 → 1/50). Higher = more aliasing, "metallic" and robotic. |
| TONE   | Low-pass filter on the crushed sound, 20 kHz → 500 Hz, to tame the harsh highs. |
| MIX    | Dry/wet blend. Lower it for parallel crushing. |
| OUTPUT | Final level, −24 to +12 dB. |

## Build it (one time)

1. Install CMake if you don't have it: `brew install cmake` (or get it from cmake.org).
2. Open Terminal, go into this folder and run:

   ```
   chmod +x build_mac.sh
   ./build_mac.sh
   ```

   The script downloads JUCE, builds the plugin, installs it into
   `~/Library/Audio/Plug-Ins/Components/` and checks it with Apple's `auval`.

3. Open Logic Pro. On a channel, click an empty Audio FX slot →
   **Audio Units → MyPlugins → Buttcrusher**.

If Logic doesn't show it: Logic Pro → Settings → Plug-in Manager →
find Buttcrusher → **Reset & Rescan Selection**.

## Working in Xcode

After the first build, open `build-mac/Buttcrusher.xcodeproj` in Xcode to edit
and rebuild. Pick the `Buttcrusher_AU` scheme (or `Buttcrusher_Standalone` to test
the effect as its own app without Logic).

## Where the code lives

- `Source/PluginProcessor.cpp` – the audio processing (the actual bitcrusher)
- `Source/PluginEditor.cpp` – the interface (knobs, colours, layout)
- `CMakeLists.txt` – project setup (plugin name, IDs, formats)
