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
    items[(size_t) index].melody.clear();   // the top line was written for the old chord
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

bool Strip::hasMelody() const
{
    for (auto& s : items)
        if (! s.melody.empty())
            return true;
    return false;
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
        for (auto& m : s.melody)
        {
            juce::ValueTree note ("NOTE");
            note.setProperty ("semis", m.semis, nullptr);
            note.setProperty ("start", m.startBeat, nullptr);
            note.setProperty ("length", m.lengthBeats, nullptr);
            note.setProperty ("velocity", m.velocity, nullptr);
            n.appendChild (note, nullptr);
        }
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
        s.chord.rootOffset = juce::jlimit (0, 11, (int) n.getProperty ("rootOffset", 0));
        int q = (int) n.getProperty ("quality", 0);
        q = juce::jlimit (0, (int) Quality::Min13, q);
        s.chord.quality = (Quality) q;
        s.chord.bassOffset = juce::jlimit (-1, 11, (int) n.getProperty ("bassOffset", -1));
        s.chord.roman = n.getProperty ("roman", "").toString().toStdString();
        s.lengthBars = snapLength ((double) n.getProperty ("lengthBars", 1.0));
        if (n.hasProperty ("inversionOverride"))
            s.inversionOverride = juce::jlimit (0, 3, (int) n.getProperty ("inversionOverride"));
        for (int k = 0; k < n.getNumChildren(); ++k)
        {
            auto note = n.getChild (k);
            if (! note.hasType ("NOTE")) continue;
            MelodyNote m;
            m.semis = juce::jlimit (-36, 48, (int) note.getProperty ("semis", 0));
            m.startBeat = juce::jmax (0.0, (double) note.getProperty ("start", 0.0));
            m.lengthBeats = juce::jmax (0.01, (double) note.getProperty ("length", 1.0));
            m.velocity = juce::jlimit (1, 127, (int) note.getProperty ("velocity", 100));
            s.melody.push_back (m);
        }
        if (! strip.append (s)) break;
    }
    return strip;
}

int melodyReference (int tonicPc)
{
    tonicPc = ((tonicPc % 12) + 12) % 12;
    return (tonicPc < 6 ? 72 : 60) + tonicPc;
}

std::vector<NoteEvent> melodyEvents (const Strip& strip, int tonicPc, int octave)
{
    std::vector<NoteEvent> out;
    const int ref = melodyReference (tonicPc) + 12 * octave;
    double slotStart = 0.0;
    for (auto& s : strip.slots())
    {
        const double slotBeats = s.lengthBars * 4.0;
        for (auto& m : s.melody)
        {
            if (m.startBeat >= slotBeats - 1.0e-6)
                continue;   // the chord got shorter than this note's start
            const double len = std::min (m.lengthBeats, slotBeats - m.startBeat);
            int note = ref + m.semis;
            while (note > 127) note -= 12;
            while (note < 0) note += 12;
            out.push_back ({ slotStart + m.startBeat, len, note, m.velocity });
        }
        slotStart += slotBeats;
    }
    return out;
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
