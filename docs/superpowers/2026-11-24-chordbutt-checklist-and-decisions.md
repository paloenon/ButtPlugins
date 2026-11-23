# ChordButt — Logic test checklist and build decisions

Kept from the build process (2026-11-23/24) so it survives cleanup of the scratch workspace.

## Manual Logic checklist (not runnable here)
1. Bypass ChordButt mid-loop (strip ▶ playing) → all notes stop; un-bypass → loop resumes (held chords resume at the playhead).
2. Remove ChordButt mid-loop → check for a hanging note (README troubleshooting applies if so).
3. Close the plugin window mid-loop → loop stops, no hanging notes.
4. Turn Voicing / Spread / Octave / Complexity, change length, reorder, RE-ROLL, replace a slot via pad while a held loop plays → change heard immediately, common notes sustain without retrigger, no gaps, no hanging notes.
5. Drag the whole strip (DRAG MIDI) and a single ½-bar slot into Logic → region lengths equal the strip / slot length; loop the region and compare with the preview.
6. Type in the library search box (keys must go to the search field, not trigger Logic key commands).
7. Save the project, close, reopen → strip, key, scale, knobs and seed restored.
8. Two ChordButt instances on two tracks → independent strips and previews.
9. 44.1 kHz vs 96 kHz project → preview timing identical in beats.
10. Tempo 60 and 180 BPM → preview and export follow the host tempo.
11. Track selected vs not selected while Logic is stopped → selected: previews sound, no hint; not selected: hint "Select the track in Logic to hear previews" appears, ▶ returns to ▶ after ~0.5 s.
12. Strip loop playing, press Logic's Play → preview stops; press strip ▶ during song playback → it plays.
13. Press a pad, drag off it, release → no chord added; press and release on the pad → the auditioned chord is added, even if a knob changed the palette in between.
14. Load a library entry → the scale change is one undoable step / automation gesture.

## Decisions made during the build (rulings)

- Ruling R1: Task 1 implementer skips git init/.gitignore — already done — no cost if wrong.
- Ruling R2: ChordCoreTests (and Core code) may additionally link juce::juce_data_structures (and juce_events if required) — ValueTree lives there; plan's module list was incomplete — cost if wrong: none, only a link line.
- Ruling R3: Work happens on branch feat/chordbutt in the repo itself (no separate worktree) — the repo was created fresh for this plan, nothing else lives on it — cost if wrong: trivial.
- Ruling R4 (Task 3): single applyComplexity for diatonic + colour rows; "in scale" = (rootOffset+interval) mod 12 in scaleIntervals; level 3 = 11th for minor-third chords, 13th (skip 11) for major-third; stop at minor-9th or no Quality match; Sus4→Dom7sus4 if b7 in scale — plan text was ambiguous — cost if wrong: palette voicings differ, easy to retune.
- Ruling R5 (amends R4): if the chord's current highest tertian tone is in the scale, the next third is the scale tone two scale steps above it (accepted only if 3 or 4 semitones away); otherwise candidate method, dropping minor-9th candidates BEFORE choosing. Extension never yields MinMaj7 unless rootOffset == 0; in that case use the minor 7th (Fm7) — spec anti-goal "progressions that sound wrong" — cost if wrong: retune.
- Ruling R6 (Task 5): 14 detail rulings in task-5-rulings.md (per-slot reset, pool, algorithm edge cases, swing scope, humanize draw order) — plan underspecified — cost if wrong: retune pattern feel.
- Ruling R8 (Task 8): 9 rulings in task-8-rulings.md (play from 0, one-shot mode, loop-wrap ordering, setSequence offs, two sounding sets, RT safety, allNotesOff + processBypassed added for Task 9) — plan gaps — cost if wrong: engine rework.
- Ruling R9: fix via per-pitch refcount across both sources (note-on on an already-sounding pitch emits off then on at same sample; note-off emitted only when count reaches 0); keep retired sequences until audio thread acknowledges generation; drain FIFO before atomic_load (M1); include M4 (size 65+ / keep audition+transport state robust). Others deferred. — retrigger keeps strict on/off pairing — cost if wrong: small.
- Ruling R10 (Task 9): rulings in task-9-rulings.md (listener→atomic dirty + 30Hz Timer rebuild; bypass/release wiring; labels; state layout; broadcaster) — plan gaps; listener may run on audio thread — cost if wrong: small.
- Ruling R11 (Tasks 11-13): visual verification via a ChordButtSnapshot console tool rendering the editor to PNG (createComponentSnapshot) with deterministic advanceAnimations(dt); replaces 'launch Standalone + screenshot' — avoids screen-recording permission, lets reviewers see output — cost if wrong: one extra dev target.
- Ruling R12 (Task 12): task-12-rulings.md (layout may grow to 900x640, drawerArea(), voicing override detach/reattach, drag via DragAndDropContainer w/ existsAsFile check, alt-click, playhead polling, refresh on state/params, carried Task-11 polish, snapshot scenarios) — cost if wrong: UI rework.
- Ruling R13: fix I1 + minors 1 (FadeText for value/INV/length/×), 2 (DRAG MIDI dim at rest, accent on hover/drag — spec: accent only on active), 4 (comment), 7 (SHAPE spacing). Accept minor 3 (single-slot humanize) — export of one chord is its own generation; pad fade tau stays global (constraint wins over brief's 0.25 s). Deferred: ghost choice, file size.
- Ruling R14 (Task 13): task-13-rulings.md — drawer over drawerArea, one-shot preview restores strip sequence via stripChanged, blur snapshot once, key-aware spelling (noteNameInKey) + Picardy I exempt from complexity pulled into this task (user-visible) — cost if wrong: small.
- Ruling R15: I1 via engine generation tracking (any other generation = preview over); I2 via spelling from the roman numeral (letter from degree, accidental from b/#) when roman present. Modal duplicates: colour pad whose resolved name equals an earlier pad is replaced by the first unused fallback [II7 (V/V), bVI, bVII, bIII, iv6 (Min6), bII, IV, v] — test: no duplicate names in any scale, levels 0-3. Enharmonic fallbacks accepted (readability > double sharps). Row names ignoring complexity accepted.
- Ruling R16 (amends R8 #4): on sequence swap, diff sounding vs new-active-at-playhead: keep common pitches, off old-only, on new-only at sample 0. Fix wave = I1, I2 (+ stop when host transport starts playing), I3 (README + heartbeat hint), I4 checklist, gesture wrap, end-of-track at strip length, pad release outside cancels commit, Strip clamping, processBypassed acks generation. Atomic ptr instead of atomic_load: include if contained. Preview clipping at loop end: accepted (loop must wrap).

## Known minor items left for later

- Task 3: minor (deferred): Dorian IV / Mixolydian bVII colour pads duplicate diatonic pads (plan-mandated lists).
- Task 4: minor (deferred): octave-clamp path untested (needs octave ±9 test); rootPc used raw at Voicing.cpp:38 vs %12 at :55; spread thresholds/order not pinned by tests.
- Task 5: minor (deferred): pendulum wrap untested; slot-end test lacks Held/rhythm/swing/gate loop; clamp UB if slot <1e-6 beats (unreachable); arp pool may contain duplicate notes when chord spans an octave (audible repeats); strum+humanize lengths.
- Task 6: minor (deferred): no test global inversion applies w/o override (nonzero global); voiceStrip never tested with complexity>0; out-of-range mutator no-op tests missing.
- Task 7: minor (deferred): round-trip test lacks note-off/tempo asserts; redundant sort + misleading comments MidiExport.cpp:62-73; writeTempMidi silently returns non-existent file on failure (Task 12 drag must check existsAsFile — carry); tempo not clamped for absurd BPM; sanitised name not asserted; empty-name test deletes shared ChordButt.mid.
- Task 5: minor (deferred, from Task 8 review): Strum+humanize keeps length after start shift → same-pitch overlaps in exported MIDI.
- Task 8: minor (deferred): fallback release flags can cancel a newer command when FIFO full; retired list grows while only processBypassed runs; post-swap overlapping same-pitch early cut; M2 flag flicker; M3 bypass applies Play; M5 libc++ atomic_load mutex; M6 sub-sample loop drift.
- Task 9: minor (deferred): empty strip 1-beat loop; no processor-level tests (paramValues mapping, dirty/timer, garbage state); startTimerHz assumes message manager; double host notify on state load; block-size fix not tried in real host (user Logic check).
- Task 10: minor (deferred): loadLibraryEntry scale change lacks begin/endChangeGesture; cm9 search test vacuous if empty; duplicate-name path untested; unknown JSON keys ignored; name stems repeat (Midnight x5, Golden x4); pop-014 Tense tag stretch.
- Theory: minor (deferred, user-visible): noteName flat-only spelling shows E/G# as "E/Ab", vii°7 in A harm. minor as "Abdim7" — consider fixing in final wave (key-aware spelling).
- Task 12: minor (deferred): ChordStrip.cpp 853 lines (split PlayButton/DragButton); SHAPE panel ~100px empty below knobs; LabelledKnob polls formatValue per frame; ghost choice on external shrink.
- Task 13: minor (deferred): LibraryDrawer whitespace; any rebuildPreview (automation, reseed) ends a library preview; weak sweep tests; preview-end restore needs vblank.
