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

std::string noteName (int pc);   // fixed spelling (C# Eb F# Ab Bb), kept for parameter names / back-compat

// Key-aware spelling. The tonic is spelled with sharps when the scale's parent major key is a sharp key
// (G D A E B F#), otherwise with flats; every other note is spelled from its degree above the tonic
// (b2 b3 3 4 #4 5 b6 6 b7 7), so the leading tone of A minor is G#, the IV of F major Bb and the
// iii of E major G#. Spellings needing a double accidental, or E#/B#/Cb/Fb, fall back to the
// key-side sharp/flat name.
std::string noteNameInKey (int pc, int tonicPc, Scale);

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
// chordName with key-aware spelling; a slash bass that is a chord tone is spelled from the chord root
// (E/G#, D/F#, Ab/C).
std::string chordNameInKey (const Chord&, int tonicPc, Scale);

// Accidentals are relative to the MAJOR scale degree. Trailing quality markers are ignored.
std::optional<int> romanToOffset (const std::string&);
} // namespace cb
