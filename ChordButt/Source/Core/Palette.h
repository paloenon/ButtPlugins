#pragma once
#include <string>
#include <vector>
#include "Core/Theory.h"

namespace cb
{
struct PadChord
{
    RelChord chord;
    std::string category; // "diatonic" | "sus" | "borrowed" | "secondary" | "colour"
};

// Always 14 pads: [0..6] diatonic row, [7..13] colour row. complexity 0..3.
std::vector<PadChord> buildPalette (Scale, int complexity);

// Adds stacked thirds (7th, 9th, 11th/13th) while they stay in the scale. Never removes tones. level 0..3.
RelChord applyComplexity (const RelChord&, Scale, int level);
} // namespace cb
