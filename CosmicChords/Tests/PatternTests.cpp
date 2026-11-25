#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include "Core/Patterns.h"

using namespace cb;
using Catch::Approx;

namespace
{
std::vector<double> starts (const std::vector<NoteEvent>& e)
{
    std::vector<double> s;
    for (auto& x : e) s.push_back (x.startBeat);
    return s;
}
std::vector<int> notes (const std::vector<NoteEvent>& e, size_t n = 0)
{
    std::vector<int> s;
    for (auto& x : e) { if (n && s.size() >= n) break; s.push_back (x.note); }
    return s;
}
void requireStarts (const std::vector<NoteEvent>& e, std::vector<double> expect)
{
    auto s = starts (e);
    REQUIRE (s.size() == expect.size());
    for (size_t i = 0; i < s.size(); ++i)
        REQUIRE (s[i] == Approx (expect[i]).margin (1e-9));
}
StyleParams arp (ArpPattern p)
{
    StyleParams sp; sp.mode = PlayMode::Arp; sp.arp = p; return sp;
}
StyleParams rhythm (RhythmPattern p)
{
    StyleParams sp; sp.mode = PlayMode::Rhythm; sp.rhythm = p; return sp;
}
const std::vector<VoicedSlot> one4 { { { 60 }, 4.0 } };
const std::vector<VoicedSlot> bar4 { { { 60, 64, 67, 72 }, 4.0 } };
}

TEST_CASE ("Patterns: held two slots")
{
    auto e = generateEvents ({ { { 48, 52, 55 }, 4.0 }, { { 53, 57, 60 }, 4.0 } }, StyleParams {});
    REQUIRE (e.size() == 6);
    REQUIRE (e[0].startBeat == Approx (0.0));
    REQUIRE (e[0].lengthBeats == Approx (4.0));
    REQUIRE (e[3].startBeat == Approx (4.0));
    REQUIRE (e[3].note == 53);
}

TEST_CASE ("Patterns: stabs starts")
{
    requireStarts (generateEvents ({ { { 60 }, 4.0 } }, rhythm (RhythmPattern::Stabs)), { 0, 1.5, 3 });
    auto e = generateEvents ({ { { 60 }, 4.0 } }, rhythm (RhythmPattern::Stabs));
    REQUIRE (e[0].lengthBeats == Approx (0.4));
}

TEST_CASE ("Patterns: stabs repeat per bar, half-bar uses first half")
{
    requireStarts (generateEvents ({ { { 60 }, 8.0 } }, rhythm (RhythmPattern::Stabs)), { 0, 1.5, 3, 4, 5.5, 7 });
    requireStarts (generateEvents ({ { { 60 }, 2.0 } }, rhythm (RhythmPattern::Stabs)), { 0, 1.5 });
}

TEST_CASE ("Patterns: offbeat and charleston")
{
    requireStarts (generateEvents (one4, rhythm (RhythmPattern::Offbeat)), { 0.5, 1.5, 2.5, 3.5 });
    auto e = generateEvents ({ { { 60 }, 4.0 } }, rhythm (RhythmPattern::Charleston));
    requireStarts (e, { 0, 1.5 });
    REQUIRE (e[0].lengthBeats == Approx (0.8));
    REQUIRE (e[1].lengthBeats == Approx (0.4));
}

TEST_CASE ("Patterns: push anticipates next slot, last wraps")
{
    auto e = generateEvents ({ { { 48 }, 4.0 }, { { 55 }, 4.0 } }, rhythm (RhythmPattern::Push));
    REQUIRE (e.size() == 16);
    REQUIRE (e[7].startBeat == Approx (3.5));
    REQUIRE (e[7].note == 55);
    REQUIRE (e[15].startBeat == Approx (7.5));
    REQUIRE (e[15].note == 48);
    REQUIRE (e[7].startBeat + e[7].lengthBeats <= 4.0 + 1e-9);
}

TEST_CASE ("Patterns: cascade")
{
    auto e = generateEvents (bar4, arp (ArpPattern::Cascade));
    REQUIRE (notes (e, 6) == std::vector<int> { 72, 67, 64, 67, 64, 60 });
    REQUIRE (e.size() == 16);
    REQUIRE (e[1].startBeat == Approx (0.25));
    REQUIRE (e[0].lengthBeats == Approx (0.2));
}

TEST_CASE ("Patterns: pendulum")
{
    auto e = generateEvents ({ { { 60, 61, 62, 63, 64 }, 4.0 } }, arp (ArpPattern::Pendulum));
    REQUIRE (notes (e, 5) == std::vector<int> { 62, 63, 61, 64, 60 });
}

TEST_CASE ("Patterns: broken thirds")
{
    auto e = generateEvents ({ { { 60, 61, 62, 63, 64 }, 4.0 } }, arp (ArpPattern::BrokenThirds));
    REQUIRE (notes (e, 14) == std::vector<int> { 60, 62, 61, 63, 62, 64, 64, 62, 63, 61, 62, 60, 60, 62 });
}

TEST_CASE ("Patterns: octave leap")
{
    auto e = generateEvents (bar4, arp (ArpPattern::OctaveLeap));
    REQUIRE (notes (e, 4) == std::vector<int> { 60, 72, 64, 76 });
}

TEST_CASE ("Patterns: gallop")
{
    auto e = generateEvents (bar4, arp (ArpPattern::Gallop));
    auto s = starts (e);
    REQUIRE (s[0] == Approx (0.0));
    REQUIRE (s[1] == Approx (0.5));
    REQUIRE (s[2] == Approx (0.75));
    REQUIRE (s[3] == Approx (1.0));
}

TEST_CASE ("Patterns: syncopated 16ths")
{
    auto e = generateEvents (bar4, arp (ArpPattern::Syncopated16));
    requireStarts (e, { 0, 0.75, 1.5, 2.25, 3.0, 3.5 });
    REQUIRE (e[0].velocity == 112);
    REQUIRE (e[2].velocity == 112);
    REQUIRE (e[1].velocity == 100);
    REQUIRE (e[0].note == 60);
    REQUIRE (e[1].note == 64);
    REQUIRE (e[2].note == 60);
    REQUIRE (e[3].note == 67);
}

TEST_CASE ("Patterns: strum")
{
    auto sp = arp (ArpPattern::Strum);
    auto e = generateEvents ({ { { 60, 64, 67 }, 4.0 } }, sp);
    REQUIRE (e.size() == 6);
    REQUIRE (e[0].startBeat == Approx (0.0));
    REQUIRE (e[1].startBeat == Approx (0.03));
    REQUIRE (e[2].startBeat == Approx (0.06));
    REQUIRE (e[0].note == 60);
    // beat-2 strum goes high -> low
    REQUIRE (e[3].note == 67);
    REQUIRE (e[3].startBeat == Approx (2.0));
    REQUIRE (e[5].note == 60);
    REQUIRE (e[0].startBeat + e[0].lengthBeats == Approx (2.0));
}

TEST_CASE ("Patterns: swing")
{
    StyleParams sp = rhythm (RhythmPattern::Pulse8);
    sp.swing = 0.5f;
    auto e = generateEvents (one4, sp);
    REQUIRE (e[1].startBeat == Approx (0.625));
    REQUIRE (e[2].startBeat == Approx (1.0));

    StyleParams a = arp (ArpPattern::Cascade);
    a.rate = Rate::Eighth;
    a.swing = 0.5f;
    auto ea = generateEvents (one4, a);
    REQUIRE (ea[1].startBeat == Approx (0.625));

    a.rate = Rate::EighthTriplet;
    REQUIRE (generateEvents (one4, a)[1].startBeat == Approx (1.0 / 3.0));
}

TEST_CASE ("Patterns: humanize deterministic")
{
    StyleParams sp = arp (ArpPattern::Cascade);
    sp.humanize = 0.8f;
    sp.seed = 7;
    auto a = generateEvents (bar4, sp);
    auto b = generateEvents (bar4, sp);
    REQUIRE (starts (a) == starts (b));
    REQUIRE (a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) REQUIRE (a[i].velocity == b[i].velocity);
    sp.seed = 8;
    REQUIRE (starts (generateEvents (bar4, sp)) != starts (a));

    sp.humanize = 0.0f;
    auto c = generateEvents (bar4, sp);
    sp.seed = 99;
    REQUIRE (starts (generateEvents (bar4, sp)) == starts (c));
}

TEST_CASE ("Patterns: bounds and range invariants")
{
    std::vector<VoicedSlot> slots { { { 100, 112, 124, 127 }, 4.0 }, { { 90, 120 }, 2.0 }, { { 60, 72 }, 8.0 } };
    for (int m = 0; m < 3; ++m)
        for (int p = 0; p < 7; ++p)
            for (float hum : { 0.0f, 1.0f })
            {
                StyleParams sp;
                sp.mode = (PlayMode) m;
                sp.rhythm = (RhythmPattern) (p % 5);
                sp.arp = (ArpPattern) p;
                sp.arpOctaves = 3;
                sp.humanize = hum;
                sp.swing = 0.6f;
                sp.rate = Rate::Sixteenth;
                auto e = generateEvents (slots, sp);
                REQUIRE_FALSE (e.empty());
                for (size_t i = 0; i < e.size(); ++i)
                {
                    REQUIRE (e[i].note <= 127);
                    REQUIRE (e[i].note >= 0);
                    REQUIRE (e[i].velocity >= 1);
                    REQUIRE (e[i].velocity <= 127);
                    REQUIRE (e[i].lengthBeats > 0.0);
                    REQUIRE (e[i].startBeat >= 0.0);
                    REQUIRE (e[i].startBeat + e[i].lengthBeats <= 14.0 + 1e-9);
                    if (i) REQUIRE (e[i - 1].startBeat <= e[i].startBeat);
                }
            }
}

TEST_CASE ("Patterns: no event ends after its slot")
{
    std::vector<VoicedSlot> slots { { { 60, 64 }, 2.0 }, { { 62, 65 }, 4.0 } };
    for (int p = 0; p < 7; ++p)
    {
        StyleParams sp = arp ((ArpPattern) p);
        sp.humanize = 1.0f;
        for (auto& ev : generateEvents (slots, sp))
        {
            const double end = ev.startBeat + ev.lengthBeats;
            if (ev.startBeat < 2.0) REQUIRE (end <= 2.0 + 1e-9);
            else REQUIRE (end <= 6.0 + 1e-9);
        }
    }
    StyleParams push = rhythm (RhythmPattern::Push);
    push.humanize = 1.0f;
    for (auto& ev : generateEvents (slots, push))
    {
        const double end = ev.startBeat + ev.lengthBeats;
        if (ev.startBeat < 2.0) REQUIRE (end <= 2.0 + 1e-9);
        else REQUIRE (end <= 6.0 + 1e-9);
    }
}

TEST_CASE ("Patterns: names")
{
    REQUIRE (rhythmName (RhythmPattern::Charleston) == "Charleston");
    REQUIRE (rhythmName (RhythmPattern::Push) == "Push");
    REQUIRE (arpName (ArpPattern::BrokenThirds) == "Broken Thirds");
    REQUIRE (arpName (ArpPattern::Syncopated16) == "Syncopated 16ths");
    REQUIRE (rateName (Rate::SixteenthTriplet) == "1/16T");
    REQUIRE (rateName (Rate::Quarter) == "1/4");
}
