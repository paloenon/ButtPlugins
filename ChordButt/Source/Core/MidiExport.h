#pragma once
#include <optional>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Core/Patterns.h"

namespace cb
{
constexpr int midiExportPPQ = 960;

// 960 PPQ, 1 track, channel 1.
juce::MidiFile buildMidiFile (const std::vector<NoteEvent>&, double bpm, std::optional<double> lengthBeats = std::nullopt);

// Writes <tmp>/ChordButt/<baseName>.mid (sanitised, overwrites) and returns the file.
juce::File writeTempMidi (const std::vector<NoteEvent>&, double bpm, const juce::String& baseName,
                          std::optional<double> lengthBeats = std::nullopt);

// hostBpm if > 0, else 120.
double effectiveBpm (std::optional<double> hostBpm);
}
