#include <catch2/catch_test_macros.hpp>
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
        REQUIRE (before[i].startBeat == after[i].startBeat);
        REQUIRE (before[i].lengthBeats == after[i].lengthBeats);
        REQUIRE (before[i].note == after[i].note);
        REQUIRE (before[i].velocity == after[i].velocity);
    }
}

TEST_CASE ("PluginState: tolerant of missing strip and seed")
{
    juce::ValueTree t ("CHORDBUTT");
    ParamValues p;
    Strip s;
    s.append (slot (0, Quality::Maj));
    uint32_t seed = 7;
    readState (t, p, s, seed);
    REQUIRE (s.slots().empty());
    REQUIRE (seed == 1u);
    REQUIRE (p.root == 9);
}
