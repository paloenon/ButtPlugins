# Chord Generator (MIDI FX for Logic Pro) – Design

Date: 2026-11-23 · Status: draft for approval · Working name: **Cosmic Chords** (placeholder)

## 1. Intent and why

The user makes music in Logic Pro and wants to stop spending time hunting for or
hand-building chords. The plugin lets them pick a key, hear fitting chords and
ready-made progressions through **their own instrument sound**, assemble a
progression, and **drag it as MIDI into a Logic track**.

Success: open the plugin, pick key + a genre/mood, and within a minute have a
progression in the track that sounds good with the actual sound of the song.

## 2. Goals

- Key + scale selection; a large palette of chords that fit the key.
- A library of ready-made progressions, filterable by genre and mood, with search.
- A chord strip where the progression is assembled and edited.
- Drag-and-drop of the strip (or one chord) as a standard MIDI file into Logic.
- Optional playing styles: rhythm patterns and especially **non-basic arpeggios**.
- Preview of everything through the instrument on the same track.
- Modern, restrained, high-end look; glowing accents on active elements; soft,
  fluid animations that blend into each other.
- All UI text in **English**.

## 3. Non-goals

- No built-in sound engine/synth (preview goes through the user's instrument).
- No writing directly into Logic regions (not possible for a plugin; drag-and-drop instead).
- No VST3/AAX focus; AU in Logic is the target (VST3/Standalone may build as a by-product but are not tested goals).
- No audio processing, no chord detection from audio/MIDI input.
- No user accounts, online preset downloads or cloud features.
- No playing along with Logic's transport/playhead.
- No time signatures other than 4/4 in v1.

## 4. Anti-goals (would be a failure even if it works)

- Looks childish, toy-like or rainbow-coloured. (The first mockups were rejected for exactly this.)
- Arpeggios that are only up/down/up-down.
- What you hear in the preview differs from what ends up in the dragged MIDI.
- Library progressions that sound wrong or are wrongly transposed in some keys.
- Janky, stuttering or abrupt UI transitions.

## 5. Constraints

- macOS, Logic Pro, Audio Unit **MIDI FX** (`kAudioUnitType_MIDIProcessor`, `aumi`).
- Same toolchain as the existing Buttcrusher project in this folder: JUCE 9.0.6 via
  CMake FetchContent, Xcode generator, universal binary arm64+x86_64, macOS 11.0
  deployment target, `COPY_PLUGIN_AFTER_BUILD`, plus a `build_mac.sh` that builds,
  refreshes `AudioComponentRegistrar` and runs `auval` (must PASS).
- Manufacturer code `Mypl`, company "MyPlugins" (same as Buttcrusher). Plugin code: a new
  4-char code with exactly one uppercase letter, e.g. `Chbt` (builder's call).
- Project lives in its own folder next to `Buttcrusher/` in `/Users/oliverzimmer/Documents/ButtPlugins/`.

## 6. Approaches considered

| Approach | Trade-off | Decision |
|---|---|---|
| **A. MIDI FX plugin** in front of the user's instrument | Hear chords in the real song sound; useless without an instrument behind it | **Chosen** (user picked it) |
| B. Instrument plugin with built-in preview sound | Works anywhere incl. standalone; never your real sound, extra track, extra synth work | Rejected |
| A+B combined | Much bigger for little gain | Rejected |

How chords reach the track: **drag-and-drop of a MIDI file** (user's choice) rather
than "Record MIDI to Track Here".

UI layout: three layouts were mocked (pad grid / library browser / both). User picked
**pad grid as main view + library as a slide-over drawer** (see §8).

## 7. Functional design (decided with the user)

### 7.1 Key and chords
- Root (C … B) and scale: Major, Minor, Dorian, Phrygian, Lydian, Mixolydian,
  Harmonic Minor, Melodic Minor.
- Chord pads show chords fitting the key: diatonic triads, 7ths, 9ths, 11ths, 13ths;
  sus2/sus4, add9, 6 chords; borrowed chords from the parallel key; secondary
  dominants. Each pad shows chord name and Roman-numeral function.
- Knobs: **Voicing/Inversion**, **Spread** (close ↔ open voicing), **Octave**,
  **Complexity** (simple triads ↔ richer extensions).
- **Knob scope (user decision):** the knobs (and all playing-style settings in §7.4)
  apply to the **whole progression** live — turning a knob re-voices every chord in the
  strip and is heard immediately. When a single strip slot is selected, that chord can
  additionally get its own **Voicing/Inversion override**; everything else stays global.
  A strip slot therefore stores: chord (root degree/quality), length, optional
  voicing/inversion override. Complexity acts on chords as generated, also on library
  progressions (it may add extensions on top of the stored quality).
- Pad interaction *(my call)*: click = play the chord and append it to the strip; if a
  strip slot is selected, click replaces that slot instead; ⌥-click = audition only.

### 7.2 Library
- Drawer opened by a LIBRARY button; slides over the pad area with a soft blur/slide
  animation. The chord strip stays visible.
- Filters: Genre and Mood (combinable), plus text search.
- Each entry: chord names in the current key, a short evocative name, Roman numerals,
  a ▶ preview button (plays through the instrument).
- Click loads the progression into the strip, **transposed to the selected key**.
- ~200 progressions, ~8 genres × ~8 moods. Progressions are stored key-independently
  (scale degrees + chord quality), so they transpose correctly.
- Scale handling *(my call)*: each progression is tagged with the scale/mode it was
  written in; degrees count from the tonic of that mode. Loading a progression keeps
  the selected **root** and switches the **scale** to the progression's tag, so it
  always sounds as intended in every key. The list shows all progressions regardless
  of the current scale.

### 7.3 Chord strip and drag
- Strip at the bottom holds the progression; each chord's length is ½, 1 or 2 bars
  (default 1 bar). Chords can be replaced via the pads, reordered and removed.
- **DRAG MIDI** drags the whole progression as a `.mid` file into Logic; a single
  chord can also be dragged from its strip slot.
- Timing follows the project tempo (host BPM; 120 BPM fallback if unavailable).
- **4/4 only** *(my call)*; other time signatures are out of scope for v1.

### 7.3a Preview and MIDI routing
- **Preview only (user decision):** a Play/Stop button (with loop) plays the strip
  at the host tempo, **independent of Logic's transport**. The plugin does not follow
  Logic's playhead and does not play along with the song.
- Pad clicks and library ▶ also preview through MIDI out.
- Incoming MIDI from the user's keyboard is **passed through unchanged** to the
  instrument *(my call)*.

### 7.4 Playing styles (optional, default = held chords)
- Mode: **Chords (held)** / **Rhythm** / **Arpeggio**.
- Rhythm patterns, e.g. Stabs, Offbeat, plus a few others.
- Arpeggio variants that are not basic, e.g. Cascade (falling waves), Pendulum
  (swings around the middle note), Broken Thirds, Octave Leap, Gallop (long-short-short),
  Syncopated 16ths, Strum (guitar-like spread).
- Controls: Rate, Gate, Swing, Octave range, Humanize.
- **What you hear is exactly what gets dragged** (same note generator for preview and export;
  Humanize uses a fixed seed per generation so preview and export match).

### 7.5 Look and feel
- Restrained, premium studio-hardware feel: near-black/dark grey matte surfaces,
  fine thin lines, one clean typeface, **a single accent colour** that glows only where
  something is active (active knob ring, playing chord, drag button). Accent colour
  to be picked later; builder proposes one.
- Knobs with a glowing value ring.
- Soft, fluid animations everywhere (drawer slide, pad highlight when playing,
  strip changes, mode switches) that blend into each other — no hard cuts.

## 8. Layout reference

```
┌───────────────────────────────────────────────────────────┐
│ COSMIC CHORDS          [KEY A] [SCALE Minor]      [LIBRARY]    │
├───────────────────────────────────────────────────────────┤
│  [Am9] [Bø7] [Cmaj7] [Dm7] [Em7] [Fmaj9] [G6]              │  ← chord pads
│  [Asus2] [E7] [Fm] [B♭maj7] [Dadd9] [C/G] [G7sus4]          │    (library drawer
│        (Voicing) (Spread) (Octave) (Complexity)            │     slides over here)
│  Mode: Chords | Rhythm | Arp   + pattern/arp controls       │
├───────────────────────────────────────────────────────────┤
│ [▶] [ Am9 ][ Fmaj9 ][ Cmaj7 ][ G6 ]              [DRAG MIDI]│  ← strip, always visible
└───────────────────────────────────────────────────────────┘
```
Mockups (colours rejected, layout accepted):
`.superpowers/brainstorm/9836-1795442391/content/layout-a-library-v2.html`.

## 9. Left to the builder

- Plugin code, exact window size, accent colour proposal, typeface.
- Exact chord-palette layout/count on the pads and how Complexity maps to extensions.
- Exact rhythm pattern list and arpeggio algorithms (within §7.4 and the anti-goals).
- Library content authoring (must be musically correct in every key).
- Data format for library and state persistence.
