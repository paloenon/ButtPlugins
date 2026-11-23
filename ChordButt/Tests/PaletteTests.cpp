#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "Core/Palette.h"

using namespace cb;

namespace
{
std::vector<std::string> names (const std::vector<PadChord>& p, int from, int to, int tonic)
{
    std::vector<std::string> n;
    for (int i = from; i < to; ++i)
        n.push_back (chordName (resolve (p[(size_t) i].chord, tonic)));
    return n;
}
using V = std::vector<std::string>;
}

TEST_CASE ("Palette: major diatonic triads")
{
    REQUIRE (names (buildPalette (Scale::Major, 0), 0, 7, 0) == V { "C", "Dm", "Em", "F", "G", "Am", "B\xC2\xB0" });
}

TEST_CASE ("Palette: major diatonic sevenths")
{
    REQUIRE (names (buildPalette (Scale::Major, 1), 0, 7, 0)
             == V { "Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "B\xC3\xB8" "7" });
}

TEST_CASE ("Palette: minor diatonic sevenths")
{
    REQUIRE (names (buildPalette (Scale::Minor, 1), 0, 7, 9)
             == V { "Am7", "B\xC3\xB8" "7", "Cmaj7", "Dm7", "Em7", "Fmaj7", "G7" });
}

TEST_CASE ("Palette: harmonic minor V is dominant seventh")
{
    REQUIRE (buildPalette (Scale::HarmonicMinor, 1)[4].chord.quality == Quality::Dom7);
}

TEST_CASE ("Palette: always 14 pads")
{
    for (auto s : { Scale::Major, Scale::Minor, Scale::Dorian, Scale::Phrygian, Scale::Lydian, Scale::Mixolydian,
                    Scale::HarmonicMinor, Scale::MelodicMinor })
        for (int l = 0; l <= 3; ++l)
        {
            auto p = buildPalette (s, l);
            REQUIRE (p.size() == 14);
            for (int i = 0; i < 7; ++i)
                REQUIRE (p[(size_t) i].category == "diatonic");
        }
}

TEST_CASE ("Palette: major colour row")
{
    REQUIRE (names (buildPalette (Scale::Major, 0), 7, 14, 0) == V { "Csus2", "G7sus4", "Fm", "Ab", "Bb", "D7", "E7" });
}

TEST_CASE ("Palette: minor colour row")
{
    auto p = buildPalette (Scale::Minor, 0);
    REQUIRE (names (p, 7, 14, 0) == V { "Csus2", "G7", "F", "C#maj7", "C", "C7", "Gsus4" });
    REQUIRE (p[10].chord.roman == "bII");
    REQUIRE (p[12].chord.roman == "V/iv");
}

TEST_CASE ("Palette: roman numerals")
{
    auto p = buildPalette (Scale::Major, 0);
    REQUIRE (p[0].chord.roman == "I");
    REQUIRE (p[1].chord.roman == "ii");
    REQUIRE (p[6].chord.roman == "vii\xC2\xB0");
    REQUIRE (buildPalette (Scale::HarmonicMinor, 0)[2].chord.roman == "III+");
}

TEST_CASE ("Palette: complexity levels")
{
    REQUIRE (applyComplexity ({ 0, Quality::Maj }, Scale::Major, 2).quality == Quality::Maj9);
    REQUIRE (applyComplexity ({ 0, Quality::Maj }, Scale::Major, 3).quality == Quality::Maj13);
    REQUIRE (applyComplexity ({ 2, Quality::Min }, Scale::Major, 3).quality == Quality::Min11);
    REQUIRE (applyComplexity ({ 7, Quality::Dom7 }, Scale::Major, 1).quality == Quality::Dom7);
    REQUIRE (applyComplexity ({ 7, Quality::Dom7 }, Scale::Major, 3).quality == Quality::Dom13);
    REQUIRE (applyComplexity ({ 4, Quality::Min }, Scale::Major, 3).quality == Quality::Min7);
    REQUIRE (applyComplexity ({ 11, Quality::Dim }, Scale::Major, 3).quality == Quality::HalfDim7);
    REQUIRE (applyComplexity ({ 3, Quality::Aug }, Scale::HarmonicMinor, 3).quality == Quality::Aug);
    REQUIRE (applyComplexity ({ 0, Quality::Maj }, Scale::Major, 0).quality == Quality::Maj);
    REQUIRE (applyComplexity ({ 0, Quality::Sus2 }, Scale::Major, 3).quality == Quality::Sus2);
    REQUIRE (applyComplexity ({ 7, Quality::Sus4 }, Scale::Major, 1).quality == Quality::Dom7sus4);
}

TEST_CASE ("Palette: never a minor ninth unless explicit Dom7b9")
{
    for (auto s : { Scale::Major, Scale::Minor, Scale::Dorian, Scale::Phrygian, Scale::Lydian, Scale::Mixolydian,
                    Scale::HarmonicMinor, Scale::MelodicMinor })
        for (int l = 0; l <= 3; ++l)
            for (const auto& pad : buildPalette (s, l))
            {
                REQUIRE (pad.chord.quality != Quality::Dom7b9);
                const auto& iv = qualityIntervals (pad.chord.quality);
                for (int a : iv)
                    for (int b : iv)
                        REQUIRE (b - a != 13);
            }
}

TEST_CASE ("Palette: fix round 1 regressions")
{
    REQUIRE (buildPalette (Scale::HarmonicMinor, 1)[1].chord.quality == Quality::HalfDim7);
    REQUIRE (buildPalette (Scale::HarmonicMinor, 1)[0].chord.quality == Quality::MinMaj7);
    REQUIRE (applyComplexity ({ 10, Quality::Dom7 }, Scale::Major, 2).quality == Quality::Dom9);
    auto p = buildPalette (Scale::Major, 1);
    REQUIRE (p[9].chord.quality == Quality::Min7);
    REQUIRE (chordName (resolve (p[9].chord, 0)) == "Fm7");
}
