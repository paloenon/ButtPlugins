#pragma once
#include <string>
#include <vector>
#include <juce_core/juce_core.h>
#include "Core/PluginState.h"
#include "Core/Strip.h"
#include "Core/Theory.h"

namespace cb
{
// One hand-authored progression. Chords are tonic-relative (degrees count from the tonic of `scale`,
// accidentals relative to the major scale), so the entry plays correctly in every key.
struct LibraryEntry
{
    std::string id, name, genre;
    std::vector<std::string> moods;
    Scale scale = Scale::Major;
    std::vector<RelChord> chords;
    std::vector<double> lengthsBars;
};

class Library
{
public:
    // Throws std::runtime_error naming the offending entry id (or index when the id is missing).
    static Library fromJson (const juce::String&);

    const std::vector<LibraryEntry>& entries() const { return items; }

    // "" = any. `search` matches the entry name or its chord names resolved at `tonicPc`
    // (case-insensitive). tonicPc defaults to A (9), the plugin's default root.
    std::vector<const LibraryEntry*> filter (const std::string& genre, const std::string& mood,
                                             const std::string& search, int tonicPc = 9) const;

    static const std::vector<std::string>& genres();
    static const std::vector<std::string>& moods();

private:
    std::vector<LibraryEntry> items;
};

Strip toStrip (const LibraryEntry&);

// What loading an entry does to the model: scale := entry scale, root unchanged, strip := toStrip (entry).
void applyLibraryEntry (const LibraryEntry&, ParamValues&, Strip&);
} // namespace cb
