# ButtPlugins

Audio Unit plugins for Logic Pro on macOS, built with [JUCE](https://juce.com).

| Plugin | What it does |
| --- | --- |
| [Cosmic Chords](CosmicChords/README.md) | MIDI FX chord generator. Audition chords in your key, build a progression, pick from 200 library progressions, play them as chords, rhythms or arpeggios, and drag the result into Logic as MIDI. |
| [Buttcrusher](Buttcrusher/README.md) | Lo-fi bitcrusher effect. |

Website and download: https://cosmicchords.vercel.app (source in [`website/`](website/)).

## Build

Requirements: macOS 11 or later, Xcode with command line tools, CMake 3.22 or later.
JUCE and Catch2 are fetched automatically by CMake.

```bash
cd CosmicChords
./build_mac.sh
```

The script builds a universal (Apple Silicon + Intel) Audio Unit, copies it to
`~/Library/Audio/Plug-Ins/Components` and validates it with `auval`.
Cosmic Chords' unit tests run with `ctest` from `CosmicChords/build-mac`.

## Licence

GNU Affero General Public License v3.0, the same open-source licence JUCE is offered
under. See [LICENSE](LICENSE).
