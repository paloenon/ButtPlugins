#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "Core/Voicing.h"

using namespace cb;
using V = std::vector<int>;

TEST_CASE ("Voicing: close position, inversion, slash")
{
    REQUIRE (voiceChord ({ 0, Quality::Maj }, {}) == V { 48, 52, 55 });
    REQUIRE (voiceChord ({ 0, Quality::Maj }, { 1, 0.0f, 0 }) == V { 52, 55, 60 });
    REQUIRE (voiceChord ({ 0, Quality::Maj }, { 5, 0.0f, 0 }) == V { 55, 60, 64 });  // 5 mod 3 = 2
    REQUIRE (voiceChord ({ 0, Quality::Maj, 7 }, {}) == V { 43, 48, 52, 55 });
}

TEST_CASE ("Voicing: spread levels put a low root under a widening chord")
{
    auto at = [] (Chord c, float s, int inv = 0) { return voiceChord (c, { inv, s, 0 }); };

    // spreadLevel snaps 0..1 to the five named levels.
    REQUIRE (spreadLevel (0.0f) == 0);
    REQUIRE (spreadLevel (0.24f) == 1);
    REQUIRE (spreadLevel (0.5f) == 2);
    REQUIRE (spreadLevel (0.75f) == 3);
    REQUIRE (spreadLevel (1.0f) == 4);
    REQUIRE (spreadLevelName (0) == "Close");
    REQUIRE (spreadLevelName (4) == "Wide");

    // Cmaj7: the root moves to the bass, the upper chord (E G B) opens up step by step.
    REQUIRE (at ({ 0, Quality::Maj7 }, 0.0f)  == V { 48, 52, 55, 59 });          // Close
    REQUIRE (at ({ 0, Quality::Maj7 }, 0.25f) == V { 48, 64, 67, 71 });          // Bass
    REQUIRE (at ({ 0, Quality::Maj7 }, 0.5f)  == V { 48, 64, 71, 79 });          // Open
    REQUIRE (at ({ 0, Quality::Maj7 }, 0.75f) == V { 36, 64, 71, 79 });          // Deep
    REQUIRE (at ({ 0, Quality::Maj7 }, 1.0f)  == V { 36, 43, 64, 71, 79 });      // Wide: + fifth over the bass

    // Triads keep the root in the upper chord as well.
    REQUIRE (at ({ 0, Quality::Maj }, 0.25f) == V { 48, 60, 64, 67 });
    REQUIRE (at ({ 0, Quality::Maj }, 0.5f)  == V { 48, 60, 67, 76 });

    // Am7 (root pc 9 sits low: upper chord starts around C4, bass on A).
    REQUIRE (at ({ 9, Quality::Min7 }, 0.25f) == V { 45, 60, 64, 67 });
    REQUIRE (at ({ 9, Quality::Min7 }, 0.75f) == V { 33, 60, 67, 76 });
    REQUIRE (at ({ 9, Quality::Min7 }, 1.0f)  == V { 33, 40, 60, 67, 76 });

    // Inversion rotates the upper chord; the bass stays on the root.
    REQUIRE (at ({ 0, Quality::Maj7 }, 0.25f, 1) == V { 48, 67, 71, 76 });

    // Slash chord: the slash note is the bass, the root stays in the upper chord.
    REQUIRE (at ({ 0, Quality::Maj, 7 }, 0.25f) == V { 43, 60, 64, 67 });

    // The bass is always the lowest note and at least a fifth below the upper chord.
    for (int root = 0; root < 12; ++root)
        for (int lvl = 1; lvl <= 4; ++lvl)
        {
            auto v = at ({ root, Quality::Min7 }, (float) lvl / 4.0f);
            REQUIRE (v.front() % 12 == root);
            const int firstUpper = lvl == 4 ? v[2] : v[1];
            REQUIRE (firstUpper - v.front() >= 7);
        }
}

TEST_CASE ("Voicing: all notes valid for every quality/root/param")
{
    const float spreads[] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    const int octs[] { -2, 0, 2 };
    for (int q = (int) Quality::Maj; q <= (int) Quality::Min13; ++q)
        for (int root = 0; root < 12; ++root)
            for (int inv = 0; inv <= 3; ++inv)
                for (float s : spreads)
                    for (int o : octs)
                        for (int bass : { -1, 5 })
                        {
                            auto v = voiceChord ({ root, (Quality) q, bass }, { inv, s, o });
                            REQUIRE_FALSE (v.empty());
                            for (size_t i = 0; i < v.size(); ++i)
                            {
                                REQUIRE (v[i] >= 0);
                                REQUIRE (v[i] <= 127);
                                if (i > 0) REQUIRE (v[i] > v[i - 1]);
                            }
                        }
}

TEST_CASE ("Voicing: octave clamp keeps extreme settings in range")
{
    auto v = voiceChord ({ 11, Quality::Dom13 }, { 3, 1.0f, 9 });
    REQUIRE (v.back() <= 127);
    auto w = voiceChord ({ 0, Quality::Maj }, { 0, 1.0f, -9 });
    REQUIRE (w.front() >= 0);
}
