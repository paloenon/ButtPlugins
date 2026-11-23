#include "Core/Theory.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace cb
{
namespace
{
int mod12 (int v) { return ((v % 12) + 12) % 12; }

struct QualityInfo
{
    Quality q;
    const char* suffix;
    std::vector<int> intervals;
};

const std::vector<QualityInfo>& qualityTable()
{
    static const std::vector<QualityInfo> t {
        { Quality::Maj,      "",                       { 0, 4, 7 } },
        { Quality::Min,      "m",                      { 0, 3, 7 } },
        { Quality::Dim,      "\xC2\xB0",               { 0, 3, 6 } },
        { Quality::Aug,      "+",                      { 0, 4, 8 } },
        { Quality::Sus2,     "sus2",                   { 0, 2, 7 } },
        { Quality::Sus4,     "sus4",                   { 0, 5, 7 } },
        { Quality::Maj6,     "6",                      { 0, 4, 7, 9 } },
        { Quality::Min6,     "m6",                     { 0, 3, 7, 9 } },
        { Quality::Add9,     "add9",                   { 0, 4, 7, 14 } },
        { Quality::Maj7,     "maj7",                   { 0, 4, 7, 11 } },
        { Quality::Dom7,     "7",                      { 0, 4, 7, 10 } },
        { Quality::Min7,     "m7",                     { 0, 3, 7, 10 } },
        { Quality::HalfDim7, "\xC3\xB8" "7",           { 0, 3, 6, 10 } },
        { Quality::Dim7,     "\xC2\xB0" "7",           { 0, 3, 6, 9 } },
        { Quality::MinMaj7,  "m(maj7)",                { 0, 3, 7, 11 } },
        { Quality::Dom7sus4, "7sus4",                  { 0, 5, 7, 10 } },
        { Quality::Dom7b9,   "7\xE2\x99\xAD" "9",      { 0, 4, 7, 10, 13 } },
        { Quality::Maj9,     "maj9",                   { 0, 4, 7, 11, 14 } },
        { Quality::Dom9,     "9",                      { 0, 4, 7, 10, 14 } },
        { Quality::Min9,     "m9",                     { 0, 3, 7, 10, 14 } },
        { Quality::Dom11,    "11",                     { 0, 4, 7, 10, 14, 17 } },
        { Quality::Min11,    "m11",                    { 0, 3, 7, 10, 14, 17 } },
        { Quality::Maj13,    "maj13",                  { 0, 4, 7, 11, 14, 21 } },
        { Quality::Dom13,    "13",                     { 0, 4, 7, 10, 14, 21 } },
        { Quality::Min13,    "m13",                    { 0, 3, 7, 10, 14, 21 } },
    };
    return t;
}

const QualityInfo& info (Quality q)
{
    for (const auto& e : qualityTable())
        if (e.q == q)
            return e;
    return qualityTable().front();
}
} // namespace

const std::array<int, 7>& scaleIntervals (Scale s)
{
    static const std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 }, minor { 0, 2, 3, 5, 7, 8, 10 },
        dorian { 0, 2, 3, 5, 7, 9, 10 }, phrygian { 0, 1, 3, 5, 7, 8, 10 }, lydian { 0, 2, 4, 6, 7, 9, 11 },
        mixo { 0, 2, 4, 5, 7, 9, 10 }, harm { 0, 2, 3, 5, 7, 8, 11 }, mel { 0, 2, 3, 5, 7, 9, 11 };
    switch (s)
    {
        case Scale::Major:         return major;
        case Scale::Minor:         return minor;
        case Scale::Dorian:        return dorian;
        case Scale::Phrygian:      return phrygian;
        case Scale::Lydian:        return lydian;
        case Scale::Mixolydian:    return mixo;
        case Scale::HarmonicMinor: return harm;
        case Scale::MelodicMinor:  return mel;
    }
    return major;
}

std::string scaleName (Scale s)
{
    switch (s)
    {
        case Scale::Major:         return "Major";
        case Scale::Minor:         return "Minor";
        case Scale::Dorian:        return "Dorian";
        case Scale::Phrygian:      return "Phrygian";
        case Scale::Lydian:        return "Lydian";
        case Scale::Mixolydian:    return "Mixolydian";
        case Scale::HarmonicMinor: return "Harmonic Minor";
        case Scale::MelodicMinor:  return "Melodic Minor";
    }
    return "Major";
}

bool isMinorFamily (Scale s)
{
    return s == Scale::Minor || s == Scale::Dorian || s == Scale::Phrygian || s == Scale::HarmonicMinor
           || s == Scale::MelodicMinor;
}

const std::vector<int>& qualityIntervals (Quality q) { return info (q).intervals; }

std::string qualitySuffix (Quality q) { return info (q).suffix; }

std::optional<Quality> parseQuality (const std::string& s)
{
    for (const auto& e : qualityTable())
        if (s == e.suffix)
            return e.q;

    static const std::vector<std::pair<const char*, Quality>> aliases {
        { "maj", Quality::Maj },       { "dim", Quality::Dim },       { "aug", Quality::Aug },
        { "hdim7", Quality::HalfDim7 }, { "dim7", Quality::Dim7 },     { "mmaj7", Quality::MinMaj7 },
        { "7b9", Quality::Dom7b9 },    { "min", Quality::Min },       { "mi", Quality::Min },
        { "M7", Quality::Maj7 },       { "min7", Quality::Min7 },     { "m7b5", Quality::HalfDim7 },
    };
    for (const auto& a : aliases)
        if (s == a.first)
            return a.second;
    return std::nullopt;
}

std::string noteName (int pc)
{
    static const char* names[12] { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[mod12 (pc)];
}

Chord resolve (const RelChord& r, int tonicPc)
{
    Chord c { mod12 (r.rootOffset + tonicPc), r.quality, -1 };
    if (r.bassOffset >= 0)
        c.bassPc = mod12 (r.bassOffset + tonicPc);
    return c;
}

std::string chordName (const Chord& c)
{
    auto s = noteName (c.rootPc) + qualitySuffix (c.quality);
    if (c.bassPc >= 0)
        s += "/" + noteName (c.bassPc);
    return s;
}

std::optional<int> romanToOffset (const std::string& str)
{
    size_t i = 0;
    int shift = 0;
    while (i < str.size())
    {
        if (str[i] == 'b') { --shift; ++i; }
        else if (str[i] == '#') { ++shift; ++i; }
        else if (str.compare (i, 3, "\xE2\x99\xAD") == 0) { --shift; i += 3; }
        else if (str.compare (i, 3, "\xE2\x99\xAF") == 0) { ++shift; i += 3; }
        else break;
    }

    std::string numeral;
    while (i < str.size() && std::strchr ("ivIV", str[i]) != nullptr && str[i] != '\0')
        numeral += (char) std::tolower ((unsigned char) str[i++]);

    // Anything after the numeral (quality markers, extensions) is ignored.
    static const std::vector<std::pair<const char*, int>> degrees {
        { "i", 0 }, { "ii", 2 }, { "iii", 4 }, { "iv", 5 }, { "v", 7 }, { "vi", 9 }, { "vii", 11 } };
    for (const auto& d : degrees)
        if (numeral == d.first)
            return mod12 (d.second + shift);
    return std::nullopt;
}
} // namespace cb
