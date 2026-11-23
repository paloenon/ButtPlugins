#pragma once
#include <vector>
#include "Core/Theory.h"

namespace cb
{
struct VoicingParams
{
    int inversion = 0;    // 0..3
    float spread = 0.0f;  // 0..1
    int octave = 0;       // -2..+2
};

// Ascending, strictly increasing MIDI notes, all in [0,127].
std::vector<int> voiceChord (const Chord&, const VoicingParams&);
} // namespace cb
