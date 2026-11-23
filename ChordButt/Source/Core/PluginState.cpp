#include "Core/PluginState.h"
#include <algorithm>

namespace cb
{
namespace
{
template <typename E> E toEnum (const juce::var& v, E fallback, int maxValue)
{
    if (v.isVoid()) return fallback;
    return static_cast<E> (std::clamp ((int) v, 0, maxValue));
}
}

juce::ValueTree makeState (const ParamValues& p, const Strip& strip, uint32_t seed)
{
    juce::ValueTree t ("CHORDBUTT");
    t.setProperty ("root", p.root, nullptr);
    t.setProperty ("scale", (int) p.scale, nullptr);
    t.setProperty ("inversion", p.voicing.inversion, nullptr);
    t.setProperty ("spread", (double) p.voicing.spread, nullptr);
    t.setProperty ("octave", p.voicing.octave, nullptr);
    t.setProperty ("complexity", p.complexity, nullptr);
    t.setProperty ("mode", (int) p.style.mode, nullptr);
    t.setProperty ("rhythm", (int) p.style.rhythm, nullptr);
    t.setProperty ("arp", (int) p.style.arp, nullptr);
    t.setProperty ("rate", (int) p.style.rate, nullptr);
    t.setProperty ("gate", (double) p.style.gate, nullptr);
    t.setProperty ("swing", (double) p.style.swing, nullptr);
    t.setProperty ("arpOctaves", p.style.arpOctaves, nullptr);
    t.setProperty ("humanize", (double) p.style.humanize, nullptr);
    t.setProperty ("seed", (juce::int64) seed, nullptr);
    t.appendChild (strip.toValueTree(), nullptr);
    return t;
}

void readState (const juce::ValueTree& t, ParamValues& p, Strip& strip, uint32_t& seed)
{
    auto num = [&] (const char* id, int fallback) { auto v = t.getProperty (id); return v.isVoid() ? fallback : (int) v; };
    auto flt = [&] (const char* id, float fallback) { auto v = t.getProperty (id); return v.isVoid() ? fallback : (float) (double) v; };

    p.root = std::clamp (num ("root", p.root), 0, 11);
    p.scale = toEnum (t.getProperty ("scale"), p.scale, (int) Scale::MelodicMinor);
    p.voicing.inversion = std::clamp (num ("inversion", p.voicing.inversion), 0, 3);
    p.voicing.spread = std::clamp (flt ("spread", p.voicing.spread), 0.0f, 1.0f);
    p.voicing.octave = std::clamp (num ("octave", p.voicing.octave), -2, 2);
    p.complexity = std::clamp (num ("complexity", p.complexity), 0, 3);
    p.style.mode = toEnum (t.getProperty ("mode"), p.style.mode, (int) PlayMode::Arp);
    p.style.rhythm = toEnum (t.getProperty ("rhythm"), p.style.rhythm, (int) RhythmPattern::Push);
    p.style.arp = toEnum (t.getProperty ("arp"), p.style.arp, (int) ArpPattern::Strum);
    p.style.rate = toEnum (t.getProperty ("rate"), p.style.rate, (int) Rate::SixteenthTriplet);
    p.style.gate = std::clamp (flt ("gate", p.style.gate), 0.1f, 1.0f);
    p.style.swing = std::clamp (flt ("swing", p.style.swing), 0.0f, 0.6f);
    p.style.arpOctaves = std::clamp (num ("arpOctaves", p.style.arpOctaves), 1, 3);
    p.style.humanize = std::clamp (flt ("humanize", p.style.humanize), 0.0f, 1.0f);

    auto s = t.getProperty ("seed");
    seed = s.isVoid() ? 1u : (uint32_t) (juce::int64) s;

    auto child = t.getChildWithName ("STRIP");
    strip = child.isValid() ? Strip::fromValueTree (child) : Strip();
}

std::vector<NoteEvent> eventsFor (const ParamValues& p, const Strip& strip, uint32_t seed)
{
    StyleParams style = p.style;
    style.seed = seed;
    return generateEvents (voiceStrip (strip, p.root, p.scale, p.complexity, p.voicing), style);
}
} // namespace cb
