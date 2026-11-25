#pragma once
#include <optional>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>
#include "Core/Patterns.h"
#include "Core/Theory.h"
#include "Core/Voicing.h"

namespace cb
{
// One note of a chord's top line (melody). `semis` counts from the tonic in the melody register
// (see melodyReference), `startBeat` from the start of the chord.
struct MelodyNote
{
    int semis = 0;
    double startBeat = 0.0;
    double lengthBeats = 1.0;
    int velocity = 100;

    bool operator== (const MelodyNote& o) const
    {
        return semis == o.semis && startBeat == o.startBeat && lengthBeats == o.lengthBeats && velocity == o.velocity;
    }
};

struct StripSlot
{
    RelChord chord;
    double lengthBars = 1.0;
    std::optional<int> inversionOverride;
    std::vector<MelodyNote> melody;   // optional top line that belongs to this chord
};

class Strip
{
public:
    static constexpr int maxSlots = 16;

    const std::vector<StripSlot>& slots() const { return items; }
    bool append (const StripSlot&);             // false when full
    void replace (int index, const RelChord&);  // keeps length, clears override and top line
    void remove (int index);
    void move (int from, int to);
    void setLength (int index, double bars);    // snaps to nearest of 0.5, 1, 2
    void setInversionOverride (int index, std::optional<int>);
    void clear();
    double totalBeats() const;
    bool hasMelody() const;
    juce::ValueTree toValueTree() const;        // type "STRIP", children "SLOT"
    static Strip fromValueTree (const juce::ValueTree&);

private:
    bool valid (int index) const { return index >= 0 && index < (int) items.size(); }
    std::vector<StripSlot> items;
};

// MIDI note of melody semitone 0: one octave above the chord voicing reference, so the top line
// sits above the chords in every key (A -> A4 = 69, C -> C5 = 72, F# -> F#4 = 66).
int melodyReference (int tonicPc);

// The strip's top lines as note events (notes are cut at the end of their chord), shifted by `octave`.
std::vector<NoteEvent> melodyEvents (const Strip&, int tonicPc, int octave);

std::vector<VoicedSlot> voiceStrip (const Strip&, int tonicPc, Scale, int complexity, const VoicingParams&);
} // namespace cb
