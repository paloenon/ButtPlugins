#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cb
{
enum class Scale { Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, HarmonicMinor, MelodicMinor };

const std::array<int, 7>& scaleIntervals (Scale);
std::string scaleName (Scale);
bool isMinorFamily (Scale);

enum class Quality { Maj, Min, Dim, Aug, Sus2, Sus4, Maj6, Min6, Add9, Maj7, Dom7, Min7, HalfDim7, Dim7, MinMaj7,
                     Dom7sus4, Dom7b9, Maj9, Dom9, Min9, Dom11, Min11, Maj13, Dom13, Min13 };

const std::vector<int>& qualityIntervals (Quality);
std::string qualitySuffix (Quality);
std::optional<Quality> parseQuality (const std::string&);

std::string noteName (int pc);

struct RelChord
{
    int rootOffset;
    Quality quality;
    int bassOffset = -1;
    std::string roman;

    bool operator== (const RelChord& o) const
    {
        return rootOffset == o.rootOffset && quality == o.quality && bassOffset == o.bassOffset && roman == o.roman;
    }
};

struct Chord
{
    int rootPc;
    Quality quality;
    int bassPc = -1;
};

Chord resolve (const RelChord&, int tonicPc);
std::string chordName (const Chord&);

// Accidentals are relative to the MAJOR scale degree. Trailing quality markers are ignored.
std::optional<int> romanToOffset (const std::string&);
} // namespace cb
