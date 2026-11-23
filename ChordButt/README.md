# ChordButt – chord generator MIDI FX for Logic Pro

ChordButt is an Audio Unit **MIDI FX** plugin built with JUCE (C++). It sits in front
of any software instrument. You click chords from the current key, arrange them into a
progression, or pick one from a library of about 200. You hear everything through the
instrument on that track, and you drag the result into Logic as a MIDI region.

## What's inside

- **Key & scale**: in the header, pick a root and a scale (Major, Minor, Dorian, Phrygian,
  Lydian, Mixolydian, Harmonic Minor or Melodic Minor). Chord names are spelled
  for the key, so A minor shows E/G♯ and F major shows B♭.
- **Pads**: 14 chords for the key.
  - Top row: the diatonic chords.
  - Bottom row: sus, borrowed and secondary-dominant colour chords.
  - Press a pad to hear the chord. Release it to add the chord to the strip, or to replace
    the selected slot. Hold ⌥ while releasing to only listen.
- **Shape**: these knobs set the voicing of the chords.
  - **Voicing** sets the inversion. While a strip slot is selected, it sets that one
    chord's inversion only.
  - **Spread** opens the voicing up.
  - **Octave** moves the chords up or down.
  - **Complexity** adds extensions: triads, 7ths, 9ths, and 11ths/13ths.
- **Style**: how the progression is played.
  - **Chords**: held chords.
  - **Rhythm**: pattern, gate and swing.
  - **Arp**: pattern, rate, gate, swing and octaves.
  - **Humanize** adds timing and velocity variation. **RE-ROLL** makes a new variation.
- **Strip**: your progression, up to 16 chords, each ½, 1 or 2 bars long.
  - Click a slot to select it, and click again to deselect.
  - Click the length chip to change the length.
  - **×** removes a slot. Drag sideways to reorder.
  - **▶** plays the progression in a loop at the host tempo, or 120 BPM when there is no
    host tempo.
- **Library**: a drawer that slides over the pads.
  - Filter by genre and mood, or search by name or chord, e.g. "Am9".
  - **▶** previews a progression once, in your key and with your current Shape and Style
    settings.
  - Click a row to load it into the strip. Loading sets the scale to the progression's
    scale and keeps your root.
  - **Esc** or **LIBRARY** closes the drawer.

The preview and the exported MIDI come from the same generator, so what you hear is
exactly what you drag out.

## Build it (one time)

1. Install CMake if you don't have it: `brew install cmake` (or get it from cmake.org).
2. Open Terminal, go into this folder and run:

   ```
   chmod +x build_mac.sh
   ./build_mac.sh
   ```

   The script downloads JUCE, builds a universal (Apple Silicon + Intel) Audio Unit and
   installs it into `~/Library/Audio/Plug-Ins/Components/ChordButt.component`. Then it
   checks the plugin with Apple's `auval`. The script should end with `auval: PASSED`.

## Load it in Logic Pro

1. Create a **Software Instrument** track and load an instrument, e.g. a piano or a pad.
2. On that channel strip, click the empty **MIDI FX** slot above the instrument, then
   choose **Audio Units → MyPlugins → ChordButt**.
3. Open the plugin window and click pads. You hear the chords through the instrument.
   Logic doesn't need to be playing.

If Logic doesn't list ChordButt: go to **Logic Pro → Settings → Plug-in Manager**, find
ChordButt (manufacturer MyPlugins) and click **Reset & Rescan Selection**. If it still
doesn't show up, quit Logic, run `./build_mac.sh` again and reopen Logic.

## Drag the progression into Logic

- **Whole progression**: grab **DRAG MIDI** at the right of the strip. Drop it on a track
  in Logic's Tracks area, either on the instrument track or a new one. Logic creates a MIDI
  region with the progression as you hear it (Style, Shape and Humanize included).
- **One chord**: drag a slot out of the plugin window and drop it onto a track.
- The file uses the host tempo. When Logic hasn't reported a tempo yet, it uses 120 BPM.

## Working on the code

- `Source/Core`: the music logic (theory, palette, voicing, patterns, strip, MIDI export,
  preview engine, library), with no GUI.
- `Source/Plugin`: the processor and the editor.
- `Source/UI`: the components.
- `Resources/library.json`: the progression library.
- Run the tests:

  ```
  cmake --build build-mac --config Debug --target ChordCoreTests
  ctest --test-dir build-mac -C Debug
  ```

- Visual check without Logic: `cmake --build build-mac --config Debug --target ChordButtSnapshot`,
  then run `build-mac/ChordButtSnapshot_artefacts/Debug/ChordButtSnapshot <outDir> [scenario…]`.
  It renders the editor to PNGs.
- Open `build-mac/ChordButt.xcodeproj` in Xcode for editing. The `ChordButt_Standalone`
  scheme runs the UI as its own app. It only shows the UI: you can't hear anything without
  an instrument after it.
