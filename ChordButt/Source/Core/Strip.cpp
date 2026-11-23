#include "Core/Strip.h"
#include <algorithm>
#include <cmath>
#include "Core/Palette.h"

namespace cb
{
namespace
{
double snapLength (double bars)
{
    static const double options[] { 0.5, 1.0, 2.0 };
    double best = options[0];
    for (double o : options)
        if (std::abs (o - bars) < std::abs (best - bars))
            best = o;
    return best;
}
} // namespace

bool Strip::append (const StripSlot& s)
{
    if ((int) items.size() >= maxSlots)
        return false;
    items.push_back (s);
    items.back().lengthBars = snapLength (items.back().lengthBars);
    return true;
}

void Strip::replace (int index, const RelChord& c)
{
    if (! valid (index)) return;
    items[(size_t) index].chord = c;
    items[(size_t) index].inversionOverride.reset();
}

void Strip::remove (int index)
{
    if (! valid (index)) return;
    items.erase (items.begin() + index);
}

void Strip::move (int from, int to)
{
    if (! valid (from) || ! valid (to) || from == to) return;
    auto s = items[(size_t) from];
    items.erase (items.begin() + from);
    items.insert (items.begin() + to, s);
}

void Strip::setLength (int index, double bars)
{
    if (! valid (index)) return;
    items[(size_t) index].lengthBars = snapLength (bars);
}

void Strip::setInversionOverride (int index, std::optional<int> inv)
{
    if (! valid (index)) return;
    items[(size_t) index].inversionOverride = inv;
}

void Strip::clear() { items.clear(); }

double Strip::totalBeats() const
{
    double bars = 0.0;
    for (auto& s : items) bars += s.lengthBars;
    return bars * 4.0;
}

juce::ValueTree Strip::toValueTree() const
{
    juce::ValueTree t ("STRIP");
    for (auto& s : items)
    {
        juce::ValueTree n ("SLOT");
        n.setProperty ("rootOffset", s.chord.rootOffset, nullptr);
        n.setProperty ("quality", (int) s.chord.quality, nullptr);
        n.setProperty ("bassOffset", s.chord.bassOffset, nullptr);
        n.setProperty ("roman", juce::String (s.chord.roman), nullptr);
        n.setProperty ("lengthBars", s.lengthBars, nullptr);
        if (s.inversionOverride)
            n.setProperty ("inversionOverride", *s.inversionOverride, nullptr);
        t.appendChild (n, nullptr);
    }
    return t;
}

Strip Strip::fromValueTree (const juce::ValueTree& t)
{
    Strip strip;
    if (! t.isValid()) return strip;
    for (int i = 0; i < t.getNumChildren(); ++i)
    {
        auto n = t.getChild (i);
        if (! n.hasType ("SLOT")) continue;
        StripSlot s;
        s.chord.rootOffset = (int) n.getProperty ("rootOffset", 0);
        int q = (int) n.getProperty ("quality", 0);
        q = juce::jlimit (0, (int) Quality::Min13, q);
        s.chord.quality = (Quality) q;
        s.chord.bassOffset = (int) n.getProperty ("bassOffset", -1);
        s.chord.roman = n.getProperty ("roman", "").toString().toStdString();
        s.lengthBars = snapLength ((double) n.getProperty ("lengthBars", 1.0));
        if (n.hasProperty ("inversionOverride"))
            s.inversionOverride = (int) n.getProperty ("inversionOverride");
        if (! strip.append (s)) break;
    }
    return strip;
}

std::vector<VoicedSlot> voiceStrip (const Strip& strip, int tonicPc, Scale scale, int complexity, const VoicingParams& params)
{
    std::vector<VoicedSlot> out;
    for (auto& s : strip.slots())
    {
        auto rel = applyComplexity (s.chord, scale, complexity);
        auto vp = params;
        if (s.inversionOverride) vp.inversion = *s.inversionOverride;
        out.push_back ({ voiceChord (resolve (rel, tonicPc), vp), s.lengthBars * 4.0 });
    }
    return out;
}
} // namespace cb
