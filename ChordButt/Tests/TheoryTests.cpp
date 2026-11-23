#include <catch2/catch_test_macros.hpp>
#include "Core/Theory.h"

using namespace cb;

namespace
{
const std::vector<Quality> allQualities {
    Quality::Maj, Quality::Min, Quality::Dim, Quality::Aug, Quality::Sus2, Quality::Sus4, Quality::Maj6,
    Quality::Min6, Quality::Add9, Quality::Maj7, Quality::Dom7, Quality::Min7, Quality::HalfDim7,
    Quality::Dim7, Quality::MinMaj7, Quality::Dom7sus4, Quality::Dom7b9, Quality::Maj9, Quality::Dom9,
    Quality::Min9, Quality::Dom11, Quality::Min11, Quality::Maj13, Quality::Dom13, Quality::Min13 };
}

TEST_CASE ("Theory: scale intervals")
{
    using A = std::array<int, 7>;
    REQUIRE (scaleIntervals (Scale::Dorian) == A { 0, 2, 3, 5, 7, 9, 10 });
    REQUIRE (scaleIntervals (Scale::HarmonicMinor) == A { 0, 2, 3, 5, 7, 8, 11 });
    REQUIRE (scaleIntervals (Scale::MelodicMinor) == A { 0, 2, 3, 5, 7, 9, 11 });
    REQUIRE (scaleIntervals (Scale::Major) == A { 0, 2, 4, 5, 7, 9, 11 });
    REQUIRE (scaleName (Scale::HarmonicMinor) == "Harmonic Minor");
    REQUIRE (scaleName (Scale::Dorian) == "Dorian");
    REQUIRE (isMinorFamily (Scale::Dorian));
    REQUIRE_FALSE (isMinorFamily (Scale::Lydian));
    REQUIRE_FALSE (isMinorFamily (Scale::Major));
}

TEST_CASE ("Theory: quality intervals")
{
    using V = std::vector<int>;
    REQUIRE (qualityIntervals (Quality::Maj9) == V { 0, 4, 7, 11, 14 });
    REQUIRE (qualityIntervals (Quality::Dom7b9) == V { 0, 4, 7, 10, 13 });
    REQUIRE (qualityIntervals (Quality::HalfDim7) == V { 0, 3, 6, 10 });
    REQUIRE (qualityIntervals (Quality::Min11) == V { 0, 3, 7, 10, 14, 17 });
    REQUIRE (qualityIntervals (Quality::Dom13) == V { 0, 4, 7, 10, 14, 21 });
    for (auto q : allQualities)
    {
        const auto& iv = qualityIntervals (q);
        REQUIRE (iv.size() >= 3);
        REQUIRE (iv.front() == 0);
        for (size_t i = 1; i < iv.size(); ++i)
            REQUIRE (iv[i] > iv[i - 1]);
    }
}

TEST_CASE ("Theory: suffixes and parseQuality")
{
    REQUIRE (qualitySuffix (Quality::Maj) == "");
    REQUIRE (qualitySuffix (Quality::Min) == "m");
    REQUIRE (qualitySuffix (Quality::Dim) == "\xC2\xB0");
    REQUIRE (qualitySuffix (Quality::Aug) == "+");
    REQUIRE (qualitySuffix (Quality::HalfDim7) == "\xC3\xB8" "7");
    REQUIRE (qualitySuffix (Quality::Dim7) == "\xC2\xB0" "7");
    REQUIRE (qualitySuffix (Quality::MinMaj7) == "m(maj7)");
    REQUIRE (qualitySuffix (Quality::Dom7b9) == "7\xE2\x99\xAD" "9");
    REQUIRE (qualitySuffix (Quality::Dom7) == "7");

    for (auto q : allQualities)
    {
        auto p = parseQuality (qualitySuffix (q));
        REQUIRE (p.has_value());
        REQUIRE (*p == q);
    }

    REQUIRE (parseQuality ("dim") == Quality::Dim);
    REQUIRE (parseQuality ("aug") == Quality::Aug);
    REQUIRE (parseQuality ("hdim7") == Quality::HalfDim7);
    REQUIRE (parseQuality ("dim7") == Quality::Dim7);
    REQUIRE (parseQuality ("mmaj7") == Quality::MinMaj7);
    REQUIRE (parseQuality ("7b9") == Quality::Dom7b9);
    REQUIRE (parseQuality ("maj") == Quality::Maj);
    REQUIRE (parseQuality ("") == Quality::Maj);
    REQUIRE (parseQuality ("m") == Quality::Min);
    REQUIRE_FALSE (parseQuality ("zzz").has_value());
}

TEST_CASE ("Theory: chord names")
{
    REQUIRE (chordName (resolve ({ 9, Quality::Min9, -1, {} }, 0)) == "Am9");
    REQUIRE (chordName (Chord { 0, Quality::Maj, 7 }) == "C/G");
    REQUIRE (chordName (Chord { 10, Quality::Maj7 }) == "Bbmaj7");
    REQUIRE (noteName (3) == "Eb");
    REQUIRE (noteName (-1) == "B");
    REQUIRE (noteName (13) == "C#");
    auto c = resolve ({ 7, Quality::Dom7, 2, "V7" }, 5);
    REQUIRE (c.rootPc == 0);
    REQUIRE (c.bassPc == 7);
    REQUIRE (resolve ({ 7, Quality::Maj, -1, {} }, 5).bassPc == -1);
}

TEST_CASE ("Theory: romanToOffset")
{
    REQUIRE (romanToOffset ("bVI") == 8);
    REQUIRE (romanToOffset ("iv") == 5);
    REQUIRE (romanToOffset ("V") == 7);
    REQUIRE (romanToOffset ("#iv") == 6);
    REQUIRE (romanToOffset ("bII") == 1);
    REQUIRE (romanToOffset ("viio") == 11);
    REQUIRE (romanToOffset ("I") == 0);
    REQUIRE (romanToOffset ("V7") == 7);
    REQUIRE (romanToOffset ("ii\xC3\xB8" "7") == 2);
    REQUIRE (romanToOffset ("\xE2\x99\xAD" "VII") == 10);
    REQUIRE (romanToOffset ("\xE2\x99\xAF" "iv") == 6);
    REQUIRE_FALSE (romanToOffset ("x").has_value());
    REQUIRE_FALSE (romanToOffset ("").has_value());
}

TEST_CASE ("Theory: key-aware spelling")
{
    // A minor: leading tone with a sharp.
    REQUIRE (chordNameInKey (Chord { 4, Quality::Maj, 8 }, 9, Scale::Minor) == "E/G#");
    REQUIRE (chordNameInKey (Chord { 8, Quality::Dim7 }, 9, Scale::HarmonicMinor) == "G#\xC2\xB0" "7");
    REQUIRE (noteNameInKey (8, 9, Scale::Minor) == "G#");
    // F major: flat side.
    REQUIRE (chordNameInKey (Chord { 10, Quality::Maj }, 5, Scale::Major) == "Bb");
    // E major: sharp side.
    REQUIRE (chordNameInKey (Chord { 8, Quality::Min }, 4, Scale::Major) == "G#m");
    // D minor: raised seventh.
    REQUIRE (chordNameInKey (Chord { 1, Quality::Dim }, 2, Scale::Minor) == "C#\xC2\xB0");

    // Tonic spelling follows the key side; flat keys and borrowed chords stay flat.
    REQUIRE (noteNameInKey (1, 1, Scale::Major) == "Db");
    REQUIRE (noteNameInKey (1, 1, Scale::Minor) == "C#");       // C# minor (parent E major)
    REQUIRE (noteNameInKey (6, 6, Scale::Major) == "F#");
    REQUIRE (noteNameInKey (10, 10, Scale::Minor) == "Bb");      // Bb minor (parent Db major)
    REQUIRE (chordNameInKey (Chord { 1, Quality::Maj7 }, 0, Scale::Minor) == "Dbmaj7");   // bII in C minor
    REQUIRE (chordNameInKey (Chord { 8, Quality::Maj }, 0, Scale::Major) == "Ab");        // bVI in C major
    REQUIRE (chordNameInKey (Chord { 10, Quality::Maj }, 7, Scale::Major) == "Bb");       // bIII in G major
    REQUIRE (chordNameInKey (Chord { 3, Quality::Dom7 }, 0, Scale::Minor) == "Eb7");

    // Slash basses that are chord tones are spelled from the chord root.
    REQUIRE (chordNameInKey (Chord { 2, Quality::Maj, 6 }, 0, Scale::Major) == "D/F#");
    REQUIRE (chordNameInKey (Chord { 8, Quality::Maj, 0 }, 0, Scale::Minor) == "Ab/C");
    REQUIRE (chordNameInKey (Chord { 4, Quality::Maj, 8 }, 0, Scale::Major) == "E/G#");    // V/vi in C major
    REQUIRE (chordNameInKey (Chord { 0, Quality::Maj, 2 }, 0, Scale::Major) == "C/D");     // non-chord-tone bass

    // Unchanged for plain keys and back-compat noteName.
    REQUIRE (chordNameInKey (Chord { 9, Quality::Min7 }, 9, Scale::Minor) == "Am7");
    REQUIRE (noteName (8) == "Ab");

    // Every pc in every key gets a single-accidental (or natural) name.
    for (int tonic = 0; tonic < 12; ++tonic)
        for (auto s : { Scale::Major, Scale::Minor, Scale::Dorian, Scale::Phrygian, Scale::Lydian, Scale::Mixolydian,
                        Scale::HarmonicMinor, Scale::MelodicMinor })
            for (int pc = 0; pc < 12; ++pc)
            {
                auto n = noteNameInKey (pc, tonic, s);
                REQUIRE ((n.size() == 1 || n.size() == 2));
                REQUIRE (std::string ("CDEFGAB").find (n[0]) != std::string::npos);
            }
}
