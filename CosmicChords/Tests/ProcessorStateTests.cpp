#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "Core/PluginState.h"

using namespace cb;

static StripSlot slot (int root, Quality q, double len = 1.0)
{
    StripSlot s;
    s.chord = { root, q, -1, "" };
    s.lengthBars = len;
    return s;
}

TEST_CASE ("PluginState: round trip through XML string")
{
    ParamValues p;
    p.root = 2;
    p.scale = Scale::Dorian;
    p.style.mode = PlayMode::Arp;
    p.style.arp = ArpPattern::Pendulum;
    p.style.humanize = 0.5f;
    p.voicing.inversion = 2;
    p.voicing.spread = 0.25f;
    p.voicing.octave = -1;
    p.complexity = 2;

    Strip strip;
    strip.append (slot (0, Quality::Min));
    strip.append (slot (5, Quality::Maj, 2.0));
    strip.append (slot (7, Quality::Dom7, 0.5));
    strip.setInversionOverride (1, 3);

    const uint32_t seed = 42;
    auto xml = makeState (p, strip, seed).toXmlString();

    auto parsed = juce::ValueTree::fromXml (xml);
    REQUIRE (parsed.isValid());

    ParamValues q;
    Strip strip2;
    uint32_t seed2 = 0;
    readState (parsed, q, strip2, seed2);

    REQUIRE (q.root == 2);
    REQUIRE (q.scale == Scale::Dorian);
    REQUIRE (q.style.mode == PlayMode::Arp);
    REQUIRE (q.style.arp == ArpPattern::Pendulum);
    REQUIRE (q.style.humanize == 0.5f);
    REQUIRE (q.voicing.inversion == 2);
    REQUIRE (q.voicing.spread == 0.25f);
    REQUIRE (q.voicing.octave == -1);
    REQUIRE (q.complexity == 2);
    REQUIRE (seed2 == 42u);
    REQUIRE (strip2.slots().size() == 3);
    REQUIRE (strip2.slots()[1].inversionOverride == std::optional<int> (3));
    REQUIRE (strip2.toValueTree().isEquivalentTo (strip.toValueTree()));

    auto before = eventsFor (p, strip, seed);
    auto after = eventsFor (q, strip2, seed2);
    REQUIRE_FALSE (before.empty());
    REQUIRE (before.size() == after.size());
    for (size_t i = 0; i < before.size(); ++i)
    {
        REQUIRE (juce::exactlyEqual (before[i].startBeat, after[i].startBeat));
        REQUIRE (juce::exactlyEqual (before[i].lengthBeats, after[i].lengthBeats));
        REQUIRE (before[i].note == after[i].note);
        REQUIRE (before[i].velocity == after[i].velocity);
    }
}

TEST_CASE ("PluginState: tolerant of missing strip and seed")
{
    juce::ValueTree t ("COSMICCHORDS");
    ParamValues p;
    Strip s;
    s.append (slot (0, Quality::Maj));
    uint32_t seed = 7;
    readState (t, p, s, seed);
    REQUIRE (s.slots().empty());
    REQUIRE (seed == 1u);
    REQUIRE (p.root == 9);
}

TEST_CASE ("State: the top line switch is saved and controls the melody in the events")
{
    Strip s;
    s.append ({ RelChord { 0, Quality::Maj, -1, "I" }, 1.0, std::nullopt, { { 12, 0.0, 4.0, 100 } } });
    ParamValues p;
    p.root = 0;
    p.scale = Scale::Major;

    auto hasMelody = [] (const std::vector<NoteEvent>& ev)
    {
        return std::any_of (ev.begin(), ev.end(), [] (const NoteEvent& e) { return e.note == 84; });
    };
    REQUIRE (hasMelody (eventsFor (p, s, 1)));
    p.topLine = false;
    REQUIRE_FALSE (hasMelody (eventsFor (p, s, 1)));

    ParamValues back;
    Strip strip;
    uint32_t seed = 0;
    readState (makeState (p, s, 7), back, strip, seed);
    REQUIRE_FALSE (back.topLine);
    REQUIRE (strip.slots()[0].melody == s.slots()[0].melody);

    auto events = eventsFor (ParamValues {}, s, 1);
    REQUIRE (std::is_sorted (events.begin(), events.end(), [] (const NoteEvent& a, const NoteEvent& b)
    {
        return a.startBeat != b.startBeat ? a.startBeat < b.startBeat : a.note < b.note;
    }));
}
