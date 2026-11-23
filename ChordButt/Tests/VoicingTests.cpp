#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "Core/Voicing.h"

using namespace cb;
using V = std::vector<int>;

TEST_CASE ("Voicing: close position, inversion, spread, slash")
{
    REQUIRE (voiceChord ({ 0, Quality::Maj }, {}) == V { 48, 52, 55 });
    REQUIRE (voiceChord ({ 0, Quality::Maj }, { 1, 0.0f, 0 }) == V { 52, 55, 60 });
    REQUIRE (voiceChord ({ 0, Quality::Maj }, { 5, 0.0f, 0 }) == V { 55, 60, 64 });  // 5 mod 3 = 2
    REQUIRE (voiceChord ({ 9, Quality::Min7 }, { 0, 0.4f, 0 }) == V { 52, 57, 60, 67 });
    REQUIRE (voiceChord ({ 0, Quality::Maj, 7 }, {}) == V { 43, 48, 52, 55 });
    REQUIRE (voiceChord ({ 0, Quality::Maj }, { 0, 0.7f, 0 }) == V { 36, 40, 48, 55 });
}

TEST_CASE ("Voicing: all notes valid for every quality/root/param")
{
    const float spreads[] { 0.0f, 0.5f, 1.0f };
    const int octs[] { -2, 2 };
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
