#include <catch2/catch_test_macros.hpp>
#include "Core/Strip.h"

using namespace cb;

static StripSlot slot (int root, Quality q, double len = 1.0)
{
    StripSlot s;
    s.chord = { root, q, -1, "" };
    s.lengthBars = len;
    return s;
}

TEST_CASE ("Strip: append stops at 16")
{
    Strip s;
    for (int i = 0; i < 16; ++i) REQUIRE (s.append (slot (i % 12, Quality::Maj)));
    REQUIRE_FALSE (s.append (slot (0, Quality::Maj)));
    REQUIRE (s.slots().size() == 16);
}

TEST_CASE ("Strip: length snapping, totalBeats")
{
    Strip s;
    s.append (slot (0, Quality::Maj));
    s.setLength (0, 1.4); REQUIRE (s.slots()[0].lengthBars == 1.0);
    s.setLength (0, 0.6); REQUIRE (s.slots()[0].lengthBars == 0.5);
    s.setLength (0, 3.0); REQUIRE (s.slots()[0].lengthBars == 2.0);
    s.append (slot (7, Quality::Maj, 1.0));
    REQUIRE (s.totalBeats() == 12.0);
}

TEST_CASE ("Strip: move, remove, replace")
{
    Strip s;
    s.append (slot (0, Quality::Maj));
    s.append (slot (2, Quality::Min));
    s.append (slot (4, Quality::Min));
    s.move (0, 2);
    REQUIRE (s.slots()[0].chord.rootOffset == 2);
    REQUIRE (s.slots()[1].chord.rootOffset == 4);
    REQUIRE (s.slots()[2].chord.rootOffset == 0);
    s.remove (1);
    REQUIRE (s.slots().size() == 2);

    s.setLength (0, 2.0);
    s.setInversionOverride (0, 2);
    s.replace (0, { 9, Quality::Min7, -1, "vi7" });
    REQUIRE (s.slots()[0].chord.rootOffset == 9);
    REQUIRE (s.slots()[0].lengthBars == 2.0);
    REQUIRE_FALSE (s.slots()[0].inversionOverride.has_value());
}

TEST_CASE ("Strip: ValueTree round-trip")
{
    Strip s;
    StripSlot a; a.chord = { 7, Quality::Dom7b9, 11, "V7b9/B" }; a.lengthBars = 2.0; a.inversionOverride = 3;
    StripSlot b; b.chord = { 0, Quality::Maj13, -1, "I" }; b.lengthBars = 0.5;
    s.append (a); s.append (b);
    auto r = Strip::fromValueTree (s.toValueTree());
    REQUIRE (r.slots().size() == 2);
    for (size_t i = 0; i < 2; ++i)
    {
        REQUIRE (r.slots()[i].chord == s.slots()[i].chord);
        REQUIRE (juce::exactlyEqual (r.slots()[i].lengthBars, s.slots()[i].lengthBars));
        REQUIRE (r.slots()[i].inversionOverride == s.slots()[i].inversionOverride);
    }
    REQUIRE (Strip::fromValueTree ({}).slots().empty());
    REQUIRE (Strip::fromValueTree (juce::ValueTree ("STRIP")).slots().empty());

    auto big = s.toValueTree();
    for (int i = 0; i < 20; ++i) big.appendChild (juce::ValueTree ("SLOT"), nullptr);
    REQUIRE (Strip::fromValueTree (big).slots().size() == 16);
}

TEST_CASE ("Strip: voiceStrip override applies to its slot only")
{
    Strip s;
    s.append (slot (0, Quality::Maj));
    s.append (slot (0, Quality::Maj));
    s.setInversionOverride (1, 1);
    auto v = voiceStrip (s, 0, Scale::Major, 0, {});
    REQUIRE (v[0].notes == std::vector<int> { 48, 52, 55 });
    REQUIRE (v[1].notes == std::vector<int> { 52, 55, 60 });
    REQUIRE (v[0].lengthBeats == 4.0);
}

TEST_CASE ("Strip: voiceStrip transposes by exactly the tonic difference")
{
    Strip s;
    s.append (slot (0, Quality::Maj));
    s.append (slot (5, Quality::Maj, 2.0));
    s.append (slot (7, Quality::Dom7, 0.5));
    s.append (slot (9, Quality::Min, 1.0));
    auto a = voiceStrip (s, 0, Scale::Major, 0, {});
    auto b = voiceStrip (s, 2, Scale::Major, 0, {});
    REQUIRE (a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i)
    {
        REQUIRE (juce::exactlyEqual (a[i].lengthBeats, b[i].lengthBeats));
        REQUIRE (a[i].notes.size() == b[i].notes.size());
        for (size_t j = 0; j < a[i].notes.size(); ++j)
            REQUIRE (b[i].notes[j] - a[i].notes[j] == 2);
    }
}

TEST_CASE ("Strip: fromValueTree clamps chord offsets and the inversion override")
{
    juce::ValueTree t ("STRIP");
    auto addSlot = [&] (int root, int bass, std::optional<int> inv)
    {
        juce::ValueTree n ("SLOT");
        n.setProperty ("rootOffset", root, nullptr);
        n.setProperty ("quality", 0, nullptr);
        n.setProperty ("bassOffset", bass, nullptr);
        n.setProperty ("lengthBars", 1.0, nullptr);
        if (inv)
            n.setProperty ("inversionOverride", *inv, nullptr);
        t.appendChild (n, nullptr);
    };
    addSlot (14, 40, 9);
    addSlot (-3, -7, -2);
    addSlot (5, 7, 2);

    auto s = Strip::fromValueTree (t);
    REQUIRE (s.slots().size() == 3);
    REQUIRE (s.slots()[0].chord.rootOffset == 11);
    REQUIRE (s.slots()[0].chord.bassOffset == 11);
    REQUIRE (s.slots()[0].inversionOverride == 3);
    REQUIRE (s.slots()[1].chord.rootOffset == 0);
    REQUIRE (s.slots()[1].chord.bassOffset == -1);
    REQUIRE (s.slots()[1].inversionOverride == 0);
    REQUIRE (s.slots()[2].chord.rootOffset == 5);
    REQUIRE (s.slots()[2].chord.bassOffset == 7);
    REQUIRE (s.slots()[2].inversionOverride == 2);
}

TEST_CASE ("Strip: top lines survive the value tree and follow their chord")
{
    Strip s;
    s.append ({ RelChord { 0, Quality::Maj7, -1, "I" }, 1.0, std::nullopt, { { 4, 0.0, 2.0, 100 }, { 7, 2.0, 2.0, 90 } } });
    s.append ({ RelChord { 5, Quality::Maj7, -1, "IV" }, 0.5, std::nullopt, { { 9, 0.0, 1.0, 100 } } });
    REQUIRE (s.hasMelody());

    auto back = Strip::fromValueTree (s.toValueTree());
    REQUIRE (back.slots()[0].melody == s.slots()[0].melody);
    REQUIRE (back.slots()[1].melody == s.slots()[1].melody);

    s.move (1, 0);
    REQUIRE (s.slots()[0].melody == std::vector<MelodyNote> { { 9, 0.0, 1.0, 100 } });

    s.replace (0, RelChord { 7, Quality::Dom7, -1, "V" });
    REQUIRE (s.slots()[0].melody.empty());   // written for the old chord
}

TEST_CASE ("Strip: melody events sit above the chords, follow the key and stop at the chord end")
{
    REQUIRE (melodyReference (9) == 69);   // A4
    REQUIRE (melodyReference (0) == 72);   // C5
    REQUIRE (melodyReference (6) == 66);   // F#4

    Strip s;
    s.append ({ RelChord { 0, Quality::Maj, -1, "I" }, 1.0, std::nullopt, { { 0, 0.0, 1.0, 100 }, { 4, 3.0, 4.0, 80 } } });
    s.append ({ RelChord { 7, Quality::Maj, -1, "V" }, 0.5, std::nullopt, { { 2, 0.0, 1.0, 100 }, { 7, 2.5, 1.0, 100 } } });

    auto ev = melodyEvents (s, 9, 0);   // A major
    REQUIRE (ev.size() == 3);           // the note at beat 2.5 of a half-bar chord is dropped
    REQUIRE (ev[0].note == 69);
    REQUIRE (ev[1].note == 73);
    REQUIRE (ev[1].startBeat == 3.0);
    REQUIRE (ev[1].lengthBeats == 1.0); // cut at the end of the chord
    REQUIRE (ev[1].velocity == 80);
    REQUIRE (ev[2].note == 71);
    REQUIRE (ev[2].startBeat == 4.0);

    REQUIRE (melodyEvents (s, 9, -1)[0].note == 57);
}
