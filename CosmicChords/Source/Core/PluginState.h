#pragma once
#include <cstdint>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>
#include "Core/Patterns.h"
#include "Core/Strip.h"
#include "Core/Theory.h"
#include "Core/Voicing.h"

namespace cb
{
struct ParamValues
{
    int root = 9;
    Scale scale = Scale::Minor;
    VoicingParams voicing;
    int complexity = 1;
    StyleParams style;   // style.seed is not used; the seed is passed separately
    bool topLine = true; // play / export the strip's top lines
};

// Tree of type "COSMICCHORDS": one property per parameter, child "STRIP", property "seed" (int64).
juce::ValueTree makeState (const ParamValues&, const Strip&, uint32_t seed);

// Tolerant: missing properties keep the passed-in/default values, missing STRIP gives an empty strip,
// missing seed gives 1.
void readState (const juce::ValueTree&, ParamValues&, Strip&, uint32_t& seed);

// Appends `extra` and re-sorts by (startBeat, note), the order generateEvents produces.
void appendSorted (std::vector<NoteEvent>& events, const std::vector<NoteEvent>& extra);

// generateEvents (voiceStrip (...), style with seed) plus the top lines when p.topLine
// -- the single source for preview and export.
std::vector<NoteEvent> eventsFor (const ParamValues&, const Strip&, uint32_t seed);
} // namespace cb
