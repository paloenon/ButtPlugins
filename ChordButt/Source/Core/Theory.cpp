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

namespace
{
constexpr int naturalPc[7] { 0, 2, 4, 5, 7, 9, 11 };   // C D E F G A B
constexpr char letters[] = "CDEFGAB";

int letterOfNatural (int pc)
{
    for (int i = 0; i < 7; ++i)
        if (naturalPc[i] == pc)
            return i;
    return -1;
}

// Offset of the scale's parent major key above its tonic.
int parentMajorOffset (Scale s)
{
    switch (s)
    {
        case Scale::Major:         return 0;
        case Scale::Minor:
        case Scale::HarmonicMinor:
        case Scale::MelodicMinor:  return 3;
        case Scale::Dorian:        return 10;
        case Scale::Phrygian:      return 8;
        case Scale::Lydian:        return 7;
        case Scale::Mixolydian:    return 5;
    }
    return 0;
}

bool isSharpKey (int tonicPc, Scale s)
{
    switch (mod12 (tonicPc + parentMajorOffset (s)))
    {
        case 7: case 2: case 9: case 4: case 11: case 6: return true;
        default: return false;
    }
}

// Letter + one accidental for `pc` on `letter`; empty when that needs a double accidental or would be
// one of the awkward enharmonics E#, B#, Cb, Fb.
std::string spellOn (int letter, int pc)
{
    letter = ((letter % 7) + 7) % 7;
    int acc = mod12 (pc - naturalPc[letter]);
    if (acc > 6)
        acc -= 12;
    if (acc < -1 || acc > 1)
        return {};
    const char l = letters[letter];
    if ((acc == 1 && (l == 'E' || l == 'B')) || (acc == -1 && (l == 'C' || l == 'F')))
        return {};
    return std::string (1, l) + (acc == 1 ? "#" : acc == -1 ? "b" : "");
}

std::string keySideName (int pc, bool sharps)
{
    static const char* sharpNames[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[12] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return (sharps ? sharpNames : flatNames)[mod12 (pc)];
}

int tonicLetter (int tonicPc, Scale s)
{
    tonicPc = mod12 (tonicPc);
    const int natural = letterOfNatural (tonicPc);
    if (natural >= 0)
        return natural;
    return isSharpKey (tonicPc, s) ? letterOfNatural (tonicPc - 1) : letterOfNatural (tonicPc + 1);
}

int letterOf (const std::string& name) { return (int) (std::strchr (letters, name[0]) - letters); }
// Spelling of a slash bass that is a chord tone, from the root's letter; empty when not a chord tone
// or not spellable with one accidental.
std::string chordToneBass (const Chord& c, int rootLetter)
{
    const int interval = mod12 (c.bassPc - c.rootPc);
    for (int tone : qualityIntervals (c.quality))
    {
        if (mod12 (tone) != interval)
            continue;
        // Letter steps of a chord tone above the root (3rds, 5ths, 7ths; 2/9, 4/11, 6/13).
        int steps = 0;
        switch (tone)
        {
            case 0: steps = 0; break;
            case 2: case 14: case 13: case 15: steps = 1; break;
            case 3: case 4: steps = 2; break;
            case 5: case 17: case 18: steps = 3; break;
            case 6: case 7: case 8: steps = 4; break;
            case 9: case 21: steps = (c.quality == Quality::Dim7) ? 6 : 5; break;
            case 10: case 11: steps = 6; break;
            default: return {};
        }
        return spellOn (rootLetter + steps, c.bassPc);
    }
    return {};
}
} // namespace

std::string noteNameInKey (int pc, int tonicPc, Scale s)
{
    // Letter steps above the tonic for each semitone degree: 1 b2 2 b3 3 4 #4 5 b6 6 b7 7.
    static const int degreeLetter[12] { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
    const int degree = mod12 (pc - tonicPc);
    auto name = spellOn (tonicLetter (tonicPc, s) + degreeLetter[degree], pc);
    return name.empty() ? keySideName (pc, isSharpKey (tonicPc, s)) : name;
}

std::string chordNameInKey (const Chord& c, int tonicPc, Scale s)
{
    const auto root = noteNameInKey (c.rootPc, tonicPc, s);
    auto name = root + qualitySuffix (c.quality);
    if (c.bassPc < 0)
        return name;

    std::string bass = chordToneBass (c, letterOf (root));
    if (bass.empty())
        bass = noteNameInKey (c.bassPc, tonicPc, s);
    return name + "/" + bass;
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

namespace
{
struct ParsedRoman { int shift; int degree; };   // degree 0..6

std::optional<ParsedRoman> parseRoman (const std::string& str)
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
    static const char* numerals[7] { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    for (int d = 0; d < 7; ++d)
        if (numeral == numerals[d])
            return ParsedRoman { shift, d };
    return std::nullopt;
}
} // namespace

std::optional<int> romanToOffset (const std::string& str)
{
    static const int majorDegrees[7] { 0, 2, 4, 5, 7, 9, 11 };
    if (auto p = parseRoman (str))
        return mod12 (majorDegrees[p->degree] + p->shift);
    return std::nullopt;
}

std::string chordNameInKey (const RelChord& r, int tonicPc, Scale s)
{
    const auto c = resolve (r, tonicPc);
    auto name = chordNameInKey (c, tonicPc, s);
    const auto parsed = parseRoman (r.roman);
    if (! parsed || romanToOffset (r.roman) != mod12 (r.rootOffset))
        return name;   // no numeral, or it names something else (e.g. "V/iv")

    const auto root = spellOn (tonicLetter (tonicPc, s) + parsed->degree, c.rootPc);
    if (root.empty())
        return name;
    const auto oldRoot = noteNameInKey (c.rootPc, tonicPc, s);
    if (root == oldRoot)
        return name;

    // Re-spell with the numeral's root; a chord-tone slash bass follows the new root letter.
    auto out = root + qualitySuffix (c.quality);
    if (c.bassPc >= 0)
    {
        const auto slash = name.substr (name.find ('/') + 1);
        const auto relative = chordToneBass (c, letterOf (root));
        out += "/" + (relative.empty() ? slash : relative);
    }
    return out;
}

} // namespace cb
