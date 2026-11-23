#pragma once
#include <optional>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>
#include "Core/Patterns.h"
#include "Core/Theory.h"
#include "Core/Voicing.h"

namespace cb
{
struct StripSlot
{
    RelChord chord;
    double lengthBars = 1.0;
    std::optional<int> inversionOverride;
};

class Strip
{
public:
    static constexpr int maxSlots = 16;

    const std::vector<StripSlot>& slots() const { return items; }
    bool append (const StripSlot&);             // false when full
    void replace (int index, const RelChord&);  // keeps length, clears override
    void remove (int index);
    void move (int from, int to);
    void setLength (int index, double bars);    // snaps to nearest of 0.5, 1, 2
    void setInversionOverride (int index, std::optional<int>);
    void clear();
    double totalBeats() const;
    juce::ValueTree toValueTree() const;        // type "STRIP", children "SLOT"
    static Strip fromValueTree (const juce::ValueTree&);

private:
    bool valid (int index) const { return index >= 0 && index < (int) items.size(); }
    std::vector<StripSlot> items;
};

std::vector<VoicedSlot> voiceStrip (const Strip&, int tonicPc, Scale, int complexity, const VoicingParams&);
} // namespace cb
