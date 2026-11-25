# Cosmic Chords (Chord Generator MIDI FX) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Logic Pro Audio Unit MIDI FX plugin that offers key-fitting chords and a genre/mood progression library, previews them through the track's instrument, and exports the progression (held, rhythmic or arpeggiated) as a draggable MIDI file.

**Architecture:** A pure, unit-tested core library (`ChordCore`: theory, palette, voicing, patterns, strip model, MIDI export, library, preview engine) with no GUI dependency, linked into a thin JUCE plugin (processor = parameters + state + preview engine; editor = custom-drawn components animated by one `VBlankAttachment`). Preview and export call the same `generateEvents()` so what you hear is what you drag.

**Tech Stack:** C++17, JUCE 9.0.6 (CMake FetchContent), Catch2 v3.7.1 (FetchContent) for tests, CMake ≥ 3.22 with Xcode generator, macOS AU (`aumi`).

**Spec:** `docs/superpowers/specs/2026-11-23-chord-generator-design.md`

## Global Constraints

- Plugin type: AU MIDI FX, `AU_MAIN_TYPE kAudioUnitType_MIDIProcessor`, `IS_MIDI_EFFECT TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`, no audio buses. Formats: `AU Standalone` (Standalone only for visual UI checks).
- Codes: company `"MyPlugins"`, manufacturer `Mypl`, plugin code `Chbt`, product name `"Cosmic Chords"`, bundle id `com.myplugins.cosmicchords`. auval id: `aumi Cosc Mypl` — must PASS.
- Toolchain copied from `Buttcrusher/CMakeLists.txt`: JUCE `9.0.6` via FetchContent, `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"`, `CMAKE_OSX_DEPLOYMENT_TARGET "11.0"`, `COPY_PLUGIN_AFTER_BUILD TRUE`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`.
- Project folder: `/Users/oliverzimmer/Documents/ButtPlugins/CosmicChords/`. All paths below are relative to it unless they start with `docs/`.
- All UI text in English.
- Timing: 4/4 only; 1 bar = 4 beats; chord lengths only 0.5, 1 or 2 bars; host BPM, fallback 120 when unavailable or ≤ 0.
- Strip holds at most 16 chords.
- Visual: near-black matte surfaces, thin lines, one accent colour that glows only on active elements. Tokens (in `Source/UI/Theme.h`): bg `0xff0b0c0f`, surface `0xff14161b`, surface2 `0xff1a1d23`, line `0xff262a32`, text `0xffe6e7ea`, dim `0xff8a8f98`, accent `0xffe8b04b` (amber; the only saturated colour anywhere). No other saturated colours.
- Every visual state change animates (exponential smoothing, time constant 0.12 s); no instant jumps of position, opacity or glow.
- What is previewed must equal what is exported: both use `cb::generateEvents()` on the same inputs.

## Review Focus

1. **Changing the key with chords in the strip** — the strip transposes with the key (it stores offsets from the tonic, not absolute notes). Test in Task 6.
2. **Extreme knob settings** (Octave +2, Spread max, 13th chords, 3 arp octaves) — every note stays within MIDI 0–127. Tests in Tasks 4 and 5.
3. **Dragging with an empty strip / host gives no tempo** — no drag starts, nothing crashes; tempo falls back to 120. Tests in Tasks 7 and 10.
4. **Stopping, editing the strip, or bypassing while preview plays** — no hanging notes; every sounding note gets a note-off. Tests in Task 8.
5. **Reopening a Logic project** — key, style, strip, per-chord overrides and humanize seed are restored, so the export is identical. Test in Task 9.

---

## File Structure

```
CosmicChords/
  CMakeLists.txt            plugin target + ChordCore static lib + ChordCoreTests
  build_mac.sh              build AU, refresh registrar, auval aumi Cosc Mypl
  README.md
  Resources/library.json    ~200 progressions (embedded as BinaryData)
  Source/Core/              (no juce_gui; links juce_core + juce_audio_basics only)
    Theory.h/.cpp           Scale, Quality, RelChord, names, roman numerals
    Palette.h/.cpp          14 pad chords per key, complexity
    Voicing.h/.cpp          RelChord -> MIDI notes
    Patterns.h/.cpp         held / rhythm / arp event generation, humanize
    Strip.h/.cpp            strip model, ValueTree (de)serialisation, voiceStrip()
    MidiExport.h/.cpp       NoteEvents -> .mid file
    Library.h/.cpp          JSON loader, filter, toStrip()
    PreviewEngine.h/.cpp    real-time playback into a MidiBuffer
    Anim.h                  smoothing helper used by the UI
  Source/Plugin/
    PluginProcessor.h/.cpp  params, state, preview engine, MIDI passthrough
    PluginEditor.h/.cpp     layout + VBlank animation driver
  Source/UI/
    Theme.h                 colour tokens, fonts
    ChordLookAndFeel.h/.cpp knobs, buttons, combo boxes
    HeaderBar.h/.cpp        logo, key, scale, LIBRARY button
    PadGrid.h/.cpp          14 pads + 4 knobs
    StylePanel.h/.cpp       mode + pattern + rate/gate/swing/octaves/humanize
    ChordStrip.h/.cpp       slots, selection, length, reorder, remove, play, drag
    LibraryDrawer.h/.cpp    genre/mood/search list, preview, load
  Tests/                    Catch2 test files, one per Core file
```

---

### Task 1: Project scaffold, empty MIDI FX that passes auval

**Files:**
- Create: `/Users/oliverzimmer/Documents/ButtPlugins/.gitignore` (`build-mac/`, `.superpowers/`, `.DS_Store`), `CMakeLists.txt`, `build_mac.sh`, `Source/Plugin/PluginProcessor.h/.cpp`, `Source/Plugin/PluginEditor.h/.cpp`, `Source/Core/Theory.cpp` (empty placeholder so the lib builds), `Tests/SmokeTests.cpp`

**Interfaces:**
- Produces: CMake variable `CHORD_CORE_SOURCES` (glob of `Source/Core/*.cpp`), added with `target_sources` to both the plugin target `Cosmic Chords` and the test target. Test target: `juce_add_console_app(ChordCoreTests)` with `CHORD_CORE_SOURCES` + `Tests/*.cpp`, linking `juce::juce_core juce::juce_audio_basics Catch2::Catch2WithMain` (no `ChordCore` static lib: JUCE module targets are INTERFACE source libs and must not be linked into two binaries via a shared static lib), registered with `catch_discover_tests`. Include dir `Source` on both. Class `CosmicChordsProcessor`, `CosmicChordsEditor` (900×560).

- [ ] **Step 1:** `git init` in `/Users/oliverzimmer/Documents/ButtPlugins`, add `.gitignore`.
- [ ] **Step 2:** Write `CMakeLists.txt` per Global Constraints and the Interfaces above; Catch2 via FetchContent tag `v3.7.1`; `enable_testing()` + `include(Catch)`.
- [ ] **Step 3:** `Tests/SmokeTests.cpp`: `TEST_CASE("smoke") { REQUIRE(1 + 1 == 2); }`.
- [ ] **Step 4:** Processor: `BusesProperties()` with no buses, `isMidiEffect() == true`, `acceptsMidi/producesMidi == true`, `processBlock` leaves MIDI untouched (passthrough). Editor paints `Theme` bg only.
- [ ] **Step 5:** `build_mac.sh` = Buttcrusher's script with target `CosmicChords_AU`, log `/tmp/cosmicchords_auval.log`, `auval -v aumi Cosc Mypl`, and exit code 1 when auval fails.
- [ ] **Step 6:** Run `cmake -B build-mac -G Xcode && cmake --build build-mac --config Debug --target ChordCoreTests && ctest --test-dir build-mac -C Debug` → `100% tests passed`. Run `./build_mac.sh` → `auval: PASSED`, `~/Library/Audio/Plug-Ins/Components/CosmicChords.component` exists.
- [ ] **Step 7:** Commit `chore: scaffold Cosmic Chords MIDI FX`.

---

### Task 2: Theory

**Files:** Create `Source/Core/Theory.h/.cpp`, `Tests/TheoryTests.cpp`

**Interfaces (namespace `cb`):**
```cpp
enum class Scale { Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, HarmonicMinor, MelodicMinor };
const std::array<int,7>& scaleIntervals (Scale);
std::string scaleName (Scale);                       // "Major", "Minor", "Dorian", ..., "Harmonic Minor", "Melodic Minor"
bool isMinorFamily (Scale);                          // Minor, Dorian, Phrygian, HarmonicMinor, MelodicMinor
enum class Quality { Maj, Min, Dim, Aug, Sus2, Sus4, Maj6, Min6, Add9, Maj7, Dom7, Min7, HalfDim7, Dim7, MinMaj7,
                     Dom7sus4, Dom7b9, Maj9, Dom9, Min9, Dom11, Min11, Maj13, Dom13, Min13 };
const std::vector<int>& qualityIntervals (Quality);  // semitones from root, ascending
std::string qualitySuffix (Quality);                 // see Step 1
std::optional<Quality> parseQuality (const std::string&);
std::string noteName (int pc);                       // C C# D Eb E F F# G Ab A Bb B
struct RelChord { int rootOffset; Quality quality; int bassOffset = -1; std::string roman;
                  bool operator== (const RelChord&) const; };
struct Chord { int rootPc; Quality quality; int bassPc = -1; };
Chord resolve (const RelChord&, int tonicPc);        // (offset + tonic) mod 12
std::string chordName (const Chord&);                // root + suffix [+ "/" + bass]
std::optional<int> romanToOffset (const std::string&); // accidentals relative to the MAJOR scale
```

- [ ] **Step 1: Failing tests** in `TheoryTests.cpp`:
  - `scaleIntervals(Scale::Dorian) == {0,2,3,5,7,9,10}`, `HarmonicMinor == {0,2,3,5,7,8,11}`, `MelodicMinor == {0,2,3,5,7,9,11}`.
  - `qualityIntervals(Maj9) == {0,4,7,11,14}`, `Dom7b9 == {0,4,7,10,13}`, `HalfDim7 == {0,3,6,10}`, `Min11 == {0,3,7,10,14,17}`, `Dom13 == {0,4,7,10,14,21}`.
  - Suffixes: Maj `""`, Min `"m"`, Dim `"°"`, Aug `"+"`, HalfDim7 `"ø7"`, Dim7 `"°7"`, MinMaj7 `"m(maj7)"`, Dom7b9 `"7♭9"`, Dom7 `"7"`; `parseQuality` round-trips every suffix and accepts ASCII aliases `"dim" "aug" "hdim7" "dim7" "mmaj7" "7b9" "maj"`.
  - `chordName(resolve({9,Min9},0)) == "Am9"`, `chordName({0,Maj,7}) == "C/G"`, `chordName({10,Maj7}) == "Bbmaj7"`.
  - `romanToOffset("bVI") == 8`, `"iv" == 5`, `"V" == 7`, `"#iv" == 6`, `"bII" == 1`, `"viio" == 11`, `"x"` → `nullopt`.
- [ ] **Step 2:** Run `ctest --test-dir build-mac -C Debug -R Theory` → FAIL (does not compile).
- [ ] **Step 3:** Implement. Note names use `♭`-free ASCII (`Bb`, `Eb`) for reliable fonts; only `qualitySuffix` uses `° ø ♭`.
- [ ] **Step 4:** Run tests → PASS.
- [ ] **Step 5:** Commit `feat(core): music theory primitives`.

---

### Task 3: Palette and complexity

**Files:** Create `Source/Core/Palette.h/.cpp`, `Tests/PaletteTests.cpp`

**Interfaces:**
- Consumes: Task 2 types.
- Produces:
```cpp
struct PadChord { RelChord chord; std::string category; };  // "diatonic" | "sus" | "borrowed" | "secondary" | "colour"
std::vector<PadChord> buildPalette (Scale, int complexity);   // always 14; [0..6] diatonic row, [7..13] colour row
RelChord applyComplexity (const RelChord&, Scale, int level); // level 0..3
```

**Rules:**
- Diatonic row: stack thirds from each scale degree within the scale; level 0 = triad, 1 = 7th, 2 = 9th, 3 = 11th (minor/half-dim) or 13th (major/dominant). Quality chosen by matching the stacked intervals to a `Quality`; if no exact match at a level, use the highest level that matches. Roman numerals: case by triad (upper = major/aug, lower = minor/dim), suffix `°`/`+`.
- Colour row, major family (Major, Lydian, Mixolydian): `Isus2, Vsus4→Dom7sus4, iv (Min), bVI (Maj), bVII (Maj), II7 (Dom7, roman "V/V"), III7 (Dom7, roman "V/vi")`.
- Colour row, minor family: `isus2, V7 (Dom7), IV (Maj), bII (Maj7, roman "bII"), I (Maj, roman "I"), I7 (Dom7, roman "V/iv"), vsus4 (Sus4)`.
- `applyComplexity` on a non-diatonic or library chord: never removes tones; adds stacked thirds above the chord's highest tertian tone only when the added pitch class is in the scale *relative to the chord root* and is not a minor 9th above any existing chord tone; stops at `level`.

- [ ] **Step 1: Failing tests:**
  - `buildPalette(Major,0)` diatonic names (resolved at tonic 0) == `C Dm Em F G Am B°`; `buildPalette(Major,1)` == `Cmaj7 Dm7 Em7 Fmaj7 G7 Am7 Bø7`; `buildPalette(Minor,1)[0..6]` resolved at 9 == `Am7 Bø7 Cmaj7 Dm7 Em7 Fmaj7 G7`.
  - `buildPalette(HarmonicMinor,1)[4]` quality == `Dom7`; `buildPalette(Major,0).size() == 14` for every scale and level 0..3.
  - Colour row Major at tonic 0 names == `Csus2 G7sus4 Fm Ab Bb D7 E7`.
  - `applyComplexity({0,Maj}, Major, 2)` == `Maj9`; `applyComplexity({7,Dom7}, Major, 1)` unchanged (already a 7th); never contains a minor-9th interval except an explicit `Dom7b9`.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): key palette with complexity`.

---

### Task 4: Voicing

**Files:** Create `Source/Core/Voicing.h/.cpp`, `Tests/VoicingTests.cpp`

**Interfaces:**
```cpp
struct VoicingParams { int inversion = 0; float spread = 0.0f; int octave = 0; };  // inversion 0..3, spread 0..1, octave -2..+2
std::vector<int> voiceChord (const Chord&, const VoicingParams&);                  // ascending MIDI notes, all in [0,127]
```

**Algorithm:** root at `48 + rootPc + 12*octave`; add `qualityIntervals` (close position; intervals > 12 kept as written). Inversion `k`: move the lowest `k mod n` notes up one octave. Spread `≥ 0.33`: drop the second-highest note one octave (drop-2); `≥ 0.66`: additionally add the root one octave below the lowest note. Slash bass: add `bassPc` in the octave below the lowest note. Finally shift the whole chord by octaves until all notes lie in 0–127 (if it spans > 127 semitones, drop top notes).

- [ ] **Step 1: Failing tests:** `voiceChord({0,Maj},{}) == {48,52,55}`; inversion 1 → `{52,55,60}`; `{9,Min7}` spread 0.4 → `{57,60,64,67}` with 64 dropped → `{52,57,60,67}`; `{0,Maj,7}` → `{43,48,52,55}`; **Review Focus 2:** for every Quality, every root, inversion 0..3, spread {0,0.5,1}, octave {-2,+2}: all notes in [0,127], sorted, non-empty.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): chord voicing`.

---

### Task 5: Patterns (held, rhythm, arpeggio, humanize)

**Files:** Create `Source/Core/Patterns.h/.cpp`, `Tests/PatternTests.cpp`

**Interfaces:**
```cpp
enum class PlayMode { Held, Rhythm, Arp };
enum class RhythmPattern { Stabs, Offbeat, Pulse8, Charleston, Push };
enum class ArpPattern { Cascade, Pendulum, BrokenThirds, OctaveLeap, Gallop, Syncopated16, Strum };
enum class Rate { Quarter, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet };  // 1, 1/2, 1/3, 1/4, 1/6 beats
struct StyleParams { PlayMode mode = PlayMode::Held; RhythmPattern rhythm = RhythmPattern::Stabs;
                     ArpPattern arp = ArpPattern::Cascade; Rate rate = Rate::Sixteenth; float gate = 0.8f;
                     float swing = 0.0f; int arpOctaves = 1; float humanize = 0.0f; uint32_t seed = 1; };
struct NoteEvent { double startBeat; double lengthBeats; int note; int velocity; };
struct VoicedSlot { std::vector<int> notes; double lengthBeats; };
std::vector<NoteEvent> generateEvents (const std::vector<VoicedSlot>&, const StyleParams&); // sorted by (startBeat, note)
std::string rhythmName (RhythmPattern); std::string arpName (ArpPattern); std::string rateName (Rate);
```

**Definitions** (slot-local beats; events never extend past the slot end; base velocity 100, accents 112):
- Held: every note from 0 to slot length.
- Rhythm (4/4 grid, gate scales length): Stabs = 8th hits at 0, 1.5, 3; Offbeat = 8th hits at 0.5, 1.5, 2.5, 3.5; Pulse8 = every 8th; Charleston = hits at 0 (len 1) and 1.5 (len 0.5); Push = like Pulse8 but the last 8th of the slot plays the *next* slot's notes (last slot: wraps to first). Pattern repeats per bar; half-bar slots use the first half.
- Arp pool = slot notes repeated over `arpOctaves` octaves (ascending). Step length = rate; note length = step × gate. Sequences over pool index (n = pool size):
  - Cascade: falling groups of 3: `n-1,n-2,n-3, n-2,n-3,n-4, …` wrapping to the top when hitting 0.
  - Pendulum: from the middle `m = n/2` outwards: `m, m+1, m-1, m+2, m-2, …` then back inwards, repeat.
  - BrokenThirds: `0,2,1,3,2,4,…` up to the top, then mirrored down.
  - OctaveLeap: each chord note followed by the same note +12: `p0, p0+12, p1, p1+12, …` (ignores arpOctaves beyond 1, clamps to 127).
  - Gallop: ascending notes with rhythm long-short-short = 8th,16th,16th repeating (ignores rate).
  - Syncopated16: 16th grid, hits at 0,3,6,9,12,14 per bar (velocity 112 on 0 and 6), notes alternate lowest / ascending upper notes.
  - Strum: at beats 0 and 2 of each bar, all chord notes start staggered by 0.03 beats (down = low→high on beat 0, up = high→low on beat 2), each held to the next strum.
- Swing: shifts every odd step (Eighth: odd 8ths; Sixteenth: odd 16ths; Rhythm patterns: odd 8ths) later by `swing × step × 0.5`.
- Humanize: `std::mt19937 rng(seed)`; start ±`0.03 × humanize` beats (clamped ≥ slot start), velocity ±`15 × humanize`, clamp 1..127. Same seed → identical output.

- [ ] **Step 1: Failing tests:** Held with two slots `{48,52,55}`×4 beats and `{53,57,60}`×4 → 6 events, second chord at 4.0; Stabs one bar → starts `{0,1.5,3}`; Cascade pool `{60,64,67,72}` first 6 notes `72,67,64,67,64,60`; Pendulum pool of 5 starts `index 2,3,1,4,0`; OctaveLeap first 4 `60,72,64,76`; Gallop starts `0,0.5,0.75,1.0`; Syncopated16 starts per bar `0,0.75,1.5,2.25,3.0,3.5`; Strum beat-0 starts `0,0.03,0.06`; swing 0.5 Eighth Pulse8 second hit at `0.625`; humanize with seed 7 twice → identical vectors, seed 8 → different; **Review Focus 2:** arpOctaves 3 + octave-high chords → all notes ≤ 127; no event ends after its slot (except Push's anticipation, which ends at its slot end).
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): rhythm and arpeggio generation`.

---

### Task 6: Strip model

**Files:** Create `Source/Core/Strip.h/.cpp`, `Tests/StripTests.cpp`

**Interfaces:**
- Consumes: Tasks 2–5.
- Produces:
```cpp
struct StripSlot { RelChord chord; double lengthBars = 1.0; std::optional<int> inversionOverride; };
class Strip {
public:
    static constexpr int maxSlots = 16;
    const std::vector<StripSlot>& slots() const;
    bool append (const StripSlot&);           // false when full
    void replace (int index, const RelChord&); // keeps length, clears override
    void remove (int index);
    void move (int from, int to);
    void setLength (int index, double bars);   // snaps to nearest of 0.5, 1, 2
    void setInversionOverride (int index, std::optional<int>);
    void clear();
    double totalBeats() const;
    juce::ValueTree toValueTree() const;       // type "STRIP", children "SLOT"
    static Strip fromValueTree (const juce::ValueTree&);
};
std::vector<VoicedSlot> voiceStrip (const Strip&, int tonicPc, Scale, int complexity, const VoicingParams&);
// per slot: applyComplexity(chord, scale, complexity) -> resolve(tonic) -> voiceChord(params, inversion = override or global)
```

- [ ] **Step 1: Failing tests:** append stops at 16; `setLength(0, 1.4)` → 1.0, `0.6` → 0.5, `3` → 2; `move(0,2)` order; `replace` clears override; ValueTree round-trip equals original (incl. override, bass, roman); `voiceStrip` uses override instead of global inversion for that slot only; **Review Focus 1:** same strip voiced at tonic 0 and tonic 2 → every note differs by exactly +2 (mod octave shifts from clamping excluded by using mid-range chords).
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): chord strip model`.

---

### Task 7: MIDI export

**Files:** Create `Source/Core/MidiExport.h/.cpp`, `Tests/MidiExportTests.cpp`

**Interfaces:**
```cpp
juce::MidiFile buildMidiFile (const std::vector<NoteEvent>&, double bpm);   // 960 PPQ, 1 track, ch 1
juce::File writeTempMidi (const std::vector<NoteEvent>&, double bpm, const juce::String& baseName);
// writes <tmp>/CosmicChords/<baseName>.mid (baseName sanitised, e.g. "Am9-Fmaj9-Cmaj7-G6"), overwrites, returns the file
double effectiveBpm (std::optional<double> hostBpm);  // value if > 0 else 120
```
Track contents: tempo meta at tick 0, time signature 4/4 at tick 0, note on/off pairs (note-off before note-on at equal ticks), end-of-track after the last note-off.

- [ ] **Step 1: Failing tests:** one event `{0,4,60,100}` at 90 BPM → note-on tick 0, note-off tick 3840, tempo meta = 90 BPM, time sig 4/4; written file reads back via `juce::MidiFile::readFrom` with same events; `effectiveBpm(nullopt) == 120`, `effectiveBpm(0) == 120`, `effectiveBpm(140) == 140`; **Review Focus 3:** empty event vector → valid file with only meta events (UI never drags it, but export must not crash).
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): MIDI file export`.

---

### Task 8: Preview engine

**Files:** Create `Source/Core/PreviewEngine.h/.cpp`, `Tests/PreviewEngineTests.cpp`

**Interfaces:**
```cpp
class PreviewEngine {
public:
    using Sequence = std::vector<NoteEvent>;
    void prepare (double sampleRate);
    // message thread:
    void setSequence (std::shared_ptr<const Sequence>, double lengthBeats, bool loop); // swaps at next block; playhead kept (mod length)
    void play();  void stop();
    void auditionOn (const std::vector<int>& notes);  void auditionOff();
    bool isPlaying() const;  double playheadBeat() const;   // atomics, for UI
    // audio thread:
    void process (juce::MidiBuffer& midi, int numSamples, std::optional<double> hostBpm);
};
```
Thread safety: sequence via `std::atomic_store/atomic_load` on `shared_ptr`; commands (play, stop, auditionOn/Off) via a 64-entry `juce::AbstractFifo` of fixed-size command structs (max 16 notes). Audio thread keeps a 128-bit "sounding" set; channel 1. Uses `effectiveBpm`.

- [ ] **Step 1: Failing tests** (sampleRate 48000, BPM 120 → 1 beat = 24000 samples): play + sequence `{0,1,60,100}` → note-on at sample 0, note-off at sample 24000 (across blocks of 512); loop=true with length 2 beats → second note-on at 48000; **Review Focus 4:** `stop()` while a note sounds → note-off in the next block for every sounding note; `setSequence` with a different sequence while playing → notes from the old sequence that are sounding get note-offs; auditionOn `{60,64}` then auditionOff → two ons then two offs; incoming MIDI already in the buffer is preserved (passthrough).
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): realtime preview engine`.

---

### Task 9: Processor — parameters, state, MIDI

**Files:** Modify `Source/Plugin/PluginProcessor.h/.cpp`; Create `Source/Core/PluginState.h/.cpp`, `Tests/ProcessorStateTests.cpp`

State logic lives in Core so it is testable without instantiating the plugin: `juce::ValueTree makeState (const ParamValues&, const Strip&, uint32_t seed)` / `void readState (const juce::ValueTree&, ParamValues&, Strip&, uint32_t& seed)` where `struct ParamValues { int root; Scale scale; VoicingParams voicing; int complexity; StyleParams style; }`; the processor maps APVTS ↔ `ParamValues` and stores the tree as a child of its APVTS state. `currentEvents()` is a free function too: `std::vector<NoteEvent> eventsFor (const ParamValues&, const Strip&, uint32_t seed)`.

**Interfaces:**
- Consumes: Tasks 2–8.
- Produces (used by UI tasks):
```cpp
class CosmicChordsProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState apvts;
    cb::Strip& strip();  void stripChanged();      // message thread; rebuilds preview sequence
    std::vector<cb::PadChord> currentPalette() const;
    std::vector<cb::VoicedSlot> voicedStrip() const;
    std::vector<cb::NoteEvent> currentEvents() const;  // generateEvents(voicedStrip(), currentStyle())
    cb::StyleParams currentStyle() const;
    int tonic() const; cb::Scale scale() const;
    double hostBpm() const;                          // effectiveBpm of last seen host tempo
    cb::PreviewEngine& preview();
    void newHumanizeSeed();
    // added in Task 10, not this task:
    //   const cb::Library& library() const;  void loadLibraryEntry (const cb::LibraryEntry&);
    juce::ChangeBroadcaster stateBroadcaster;        // fires when strip/params change
};
```
Parameter IDs / ranges (all `ParameterID{id,1}`): `root` choice C…B (default A=9), `scale` choice of `scaleName` (default Minor), `inversion` int 0–3, `spread` float 0–1, `octave` int −2–2, `complexity` int 0–3 (default 1), `mode` choice Chords/Rhythm/Arp, `rhythm` choice `rhythmName`, `arp` choice `arpName`, `rate` choice `rateName` (default 1/16), `gate` 0.1–1 (0.8), `swing` 0–0.6, `arpOctaves` int 1–3, `humanize` 0–1. Non-parameter state in the APVTS tree: child `STRIP`, property `seed`.
Parameter or strip changes (via `AudioProcessorValueTreeState::Listener` + `stripChanged()`) push a new sequence to the preview engine (`currentEvents()`, `strip.totalBeats()`, loop = true). `processBlock`: read `getPlayHead()->getPosition()->getBpm()`, keep incoming MIDI, call `preview.process`. `releaseResources` and bypass (`processBlockBypassed`) send note-offs for all sounding notes.

- [ ] **Step 1: Failing tests:** **Review Focus 5:** `ParamValues` (root 2, Dorian, mode Arp, arp Pendulum, humanize 0.5), strip of 3 slots with one override, seed 42 → `makeState` → serialise to XML string and parse back → `readState` → equal values, equivalent strip tree, and `eventsFor(...)` identical before/after. Passthrough is covered by Task 8's test.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS; `./build_mac.sh` → auval PASSED.
- [ ] **Step 5:** Commit `feat(plugin): parameters, state and preview wiring`.

---

### Task 10: Progression library

**Files:** Create `Resources/library.json`, `Source/Core/Library.h/.cpp`, `Tests/LibraryTests.cpp`; Modify `CMakeLists.txt` (`juce_add_binary_data(CosmicChordsData SOURCES Resources/library.json)`, linked into plugin and tests), `Source/Plugin/PluginProcessor.cpp` (`library()`, `loadLibraryEntry`)

**Interfaces:**
```cpp
struct LibraryEntry { std::string id, name, genre; std::vector<std::string> moods; Scale scale;
                      std::vector<RelChord> chords; std::vector<double> lengthsBars; };
class Library {
public:
    static Library fromJson (const juce::String&);        // throws std::runtime_error naming the bad entry id
    const std::vector<LibraryEntry>& entries() const;
    std::vector<const LibraryEntry*> filter (const std::string& genre, const std::string& mood,
                                             const std::string& search) const; // "" = any; search matches name or chord names (case-insensitive)
    static const std::vector<std::string>& genres();  // Pop, Hip-Hop/Trap, Lo-Fi, R&B/Neo-Soul, House/EDM, Cinematic, Jazz, Rock/Indie
    static const std::vector<std::string>& moods();   // Happy, Melancholic, Dark, Dreamy, Epic, Romantic, Tense, Chill
};
Strip toStrip (const LibraryEntry&);   // RelChords are tonic-relative already
```
JSON schema: `[{"id":"lofi-001","name":"Rainy Window","genre":"Lo-Fi","moods":["Melancholic","Chill"],"scale":"Minor","chords":[{"r":"i","q":"m9"},{"r":"bVI","q":"maj9"},{"r":"bIII","q":"maj7"},{"r":"bVII","q":"6"}],"bars":[1,1,1,1]}]` — `r` parsed with `romanToOffset`, `q` with `parseQuality`, optional `"bass":"V"` for slash chords; `bars` optional (default all 1).

**Authoring rules:** ≥ 200 entries; every genre ≥ 20 entries; every mood ≥ 15 entries; 2–8 chords each; unique ids and names; harmonically idiomatic for the genre (e.g. Jazz uses ii–V–I and extensions, Trap/Dark uses minor/phrygian with ♭II/♭VI, House uses minor 7th/9th loops, Pop uses I–V–vi–IV family); names short and evocative, English.

- [ ] **Step 1: Failing tests:** parses `library.json` from BinaryData; size ≥ 200; per-genre ≥ 20, per-mood ≥ 15; every entry's genre/moods are in the fixed lists; ids unique; every entry → `toStrip` → `voiceStrip` at all 12 tonics with complexity 0..3 → non-empty, notes in 0–127; `filter("Lo-Fi","Melancholic","")` all match both; `filter("","","am9")` finds entries whose resolved names (tonic A) contain "Am9"; malformed JSON entry → exception mentioning its id. Processor: `loadLibraryEntry` sets `scale` param to the entry's scale and leaves `root` unchanged.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement loader, then author `library.json` genre by genre. **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `feat(core): progression library`.

---

### Task 11: UI foundation — theme, animation, look-and-feel, header

**Files:** Create `Source/Core/Anim.h`, `Tests/AnimTests.cpp`, `Source/UI/Theme.h`, `Source/UI/ChordLookAndFeel.h/.cpp`, `Source/UI/HeaderBar.h/.cpp`; Modify `Source/Plugin/PluginEditor.h/.cpp`

**Interfaces:**
```cpp
// Anim.h (header-only, no juce_gui)
struct Anim { float value = 0, target = 0; void snap (float v); void setTarget (float t);
              bool tick (float dtSeconds);   // value += (target-value)*(1-exp(-dt/0.12)); returns true while |target-value| > 0.001, then snaps
};
// Theme.h: namespace Theme { colours from Global Constraints; juce::Font title(), label(), value(); }
// ChordLookAndFeel: drawRotarySlider = thin 2px track in `line`, value arc in accent with soft glow
//   (glow alpha driven by an Anim on hover/drag via slider properties "glow"), matte surface2 body, 2px text pointer.
// HeaderBar(CosmicChordsProcessor&): "COSMIC CHORDS" wordmark (text colour, letter-spaced, accent only on the hover underline),
//   ComboBoxes for root & scale (ComboBoxAttachment), TextButton "LIBRARY" (toggle, accent fill when open, animated).
//   std::function<void(bool)> onLibraryToggled;
// CosmicChordsEditor: owns a juce::VBlankAttachment that calls tickAnimations(dt) on all child components implementing
//   struct Animated { virtual bool tickAnimations (float dt) = 0; }  and repaints only those returning true.
```

- [ ] **Step 1: Failing tests (`AnimTests.cpp`):** after `setTarget(1)` and ticks totalling 0.12 s value ≈ 0.632 (±0.01); after 1 s value == 1 and `tick` returns false; `snap` sets both.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement `Anim`, Theme, LookAndFeel, HeaderBar, editor animation driver; editor 900×560, header 56 px.
- [ ] **Step 4:** Tests PASS; build `CosmicChords_Standalone`, launch it, take a screenshot; check: only one saturated colour visible, knob glow fades in/out smoothly on hover (record two screenshots 60 ms apart to see an intermediate state).
- [ ] **Step 5:** Commit `feat(ui): theme, animation driver, header`.

---

### Task 12: Pad grid, style panel, chord strip with drag

**Files:** Create `Source/UI/PadGrid.h/.cpp`, `Source/UI/StylePanel.h/.cpp`, `Source/UI/ChordStrip.h/.cpp`; Modify `Source/Plugin/PluginEditor.cpp`

**Interfaces:**
- Consumes: processor API (Task 9), `Anim`, Theme.
- Behaviour (spec §7.1, §7.3, §7.3a, §7.4):
  - **PadGrid:** 2×7 pads from `currentPalette()`, each showing `chordName` (resolved) and `roman`. Mouse down: `preview().auditionOn(voiceChord(...))`; mouse up: `auditionOff()`, then — unless ⌥ held — if a strip slot is selected `strip().replace(sel, chord)` else `strip().append({chord})`; then `stripChanged()`. Pad highlight (accent border + inner glow) animates in on press and out over 0.25 s. Below: four knobs Voicing (`inversion`), Spread, Octave, Complexity with SliderAttachments. When a slot is selected, the Voicing knob edits that slot's `inversionOverride` (label changes to "VOICING · CHORD 3" with a small reset "×" that clears the override).
  - **StylePanel:** segmented control Chords / Rhythm / Arp (accent underline slides between segments); Rhythm shows pattern selector + Gate + Swing; Arp shows pattern selector + Rate + Gate + Swing + Octaves; all modes show Humanize and a "re-roll" button (`newHumanizeSeed()`). Controls fade/slide when the mode changes.
  - **ChordStrip:** slots sized proportional to length; click selects (click again deselects); length chip cycles ½ → 1 → 2; "×" removes; horizontal drag inside the strip reorders (slots animate to new positions). Play/Stop button toggles `preview().play()/stop()`; a thin accent playhead line follows `playheadBeat()`. **DRAG MIDI** button and dragging a single slot out of the plugin window start an external file drag: implement via `juce::DragAndDropContainer` on the editor with `shouldDropFilesWhenDraggingExternally` returning `writeTempMidi(events, hostBpm(), name)` — whole strip for the button, that slot only (events of that slot, shifted to beat 0) for a slot. **Review Focus 3:** empty strip → DRAG MIDI is disabled (dimmed), no drag starts, Play disabled; strip shows a hint "Click pads or open the library to add chords".
  - Strip append when full (16): pad press still auditions, slot not added, strip flashes its border once.
- [ ] **Step 1:** Implement PadGrid + knobs, rebuild Standalone, screenshot: pads show `Am7 Bø7 Cmaj7 Dm7 Em7 Fmaj7 G7` row 1 for default A Minor, complexity 1.
- [ ] **Step 2:** Implement StylePanel; screenshot each mode.
- [ ] **Step 3:** Implement ChordStrip incl. drag; rebuild AU via `./build_mac.sh` → auval PASSED.
- [ ] **Step 4: Manual check in Logic** (list for the user in the hand-off): load on a software-instrument track's MIDI FX slot; pad click sounds through the instrument; strip Play loops at project tempo; DRAG MIDI onto a track creates a region with the expected chords and length; Arp mode export matches what was heard; ⌥-click doesn't add; empty strip can't drag.
- [ ] **Step 5:** Commit `feat(ui): pads, style panel, chord strip with MIDI drag`.

---

### Task 13: Library drawer, README, final verification

**Files:** Create `Source/UI/LibraryDrawer.h/.cpp`, `README.md`; Modify `Source/Plugin/PluginEditor.cpp`

**Behaviour:** Drawer covers the pad + style area (not header, not strip). Open/close animates `Anim` 0→1: translate from 24 px below + opacity 0 → in place + opacity 1, background behind it dims and blurs (render a cached snapshot of the pad area with `juce::ImageConvolutionKernel` blur once per open, then cross-fade). Left column: "GENRE" and "MOOD" lists (single-select each, click again to clear; selected item gets accent text + thin accent bar). Top: search `TextEditor` ("Search progressions…") and count label ("Lo-Fi · Melancholic · 17"). List (`juce::ListBox` with custom rows): ▶ preview button (sets preview sequence to the entry's events at current tonic/style, plays once, not looping; ▶ becomes ■ while playing), chord names resolved at current tonic, name, roman numerals. Row click → `loadLibraryEntry` → drawer stays open, row gets accent outline, strip animates in the new slots. Esc or LIBRARY button closes.

- [ ] **Step 1:** Implement drawer; Standalone screenshots: closed, mid-animation, open with Lo-Fi + Melancholic filter.
- [ ] **Step 2:** README: what it is, build (`./build_mac.sh`), load in Logic (MIDI FX slot → Audio Units → MyPlugins → Cosmic Chords), drag workflow, Plug-in Manager rescan hint.
- [ ] **Step 3: Full verification:** `ctest --test-dir build-mac -C Debug` → 100% passed; `./build_mac.sh` → `auval: PASSED`; `lipo -archs ~/Library/Audio/Plug-Ins/Components/CosmicChords.component/Contents/MacOS/Cosmic Chords` → `x86_64 arm64`; no warnings from `Source/` in the Release build log.
- [ ] **Step 4:** Commit `feat(ui): progression library drawer; docs`.
