#pragma once
#include <string>
#include <vector>
#include "Core/Theory.h"

namespace cb
{
struct VoicingParams
{
    int inversion = 0;    // 0..3
    float spread = 0.0f;  // 0..1, snapped to five levels (see spreadLevel)
    int octave = 0;       // -2..+2
};

// Spread levels (the knob has five detents at 0, .25, .5, .75, 1):
//   0 Close — close position, inversion applied (slash bass below)
//   1 Bass  — root (or slash note) moves to a bass note at least a fifth below the upper chord
//   2 Open  — as Bass, and the upper chord opens up (every second note raised an octave)
//   3 Deep  — as Open, with the bass a further octave down
//   4 Wide  — as Deep, plus the fifth above the bass (left-hand root-fifth)
// For chords with four or more tones the root is taken out of the upper chord once it is in the
// bass (it would only thicken the middle); triads keep it.
int spreadLevel (float spread);
std::string spreadLevelName (int level);

// Ascending, strictly increasing MIDI notes, all in [0,127].
std::vector<int> voiceChord (const Chord&, const VoicingParams&);
} // namespace cb
