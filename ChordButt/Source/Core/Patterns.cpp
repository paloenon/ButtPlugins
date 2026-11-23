#include "Core/Patterns.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace cb
{
namespace
{
constexpr int baseVel = 100;
constexpr int accentVel = 112;
constexpr double eps = 1e-9;

double rateBeats (Rate r)
{
    switch (r)
    {
        case Rate::Quarter:         return 1.0;
        case Rate::Eighth:          return 0.5;
        case Rate::EighthTriplet:   return 1.0 / 3.0;
        case Rate::Sixteenth:       return 0.25;
        case Rate::SixteenthTriplet:return 1.0 / 6.0;
    }
    return 0.25;
}

struct RawEvent { double start; double length; int note; int velocity; double limit; size_t slot; };

using Raw = std::vector<RawEvent>;

void add (Raw& out, size_t slot, double slotLen, double start, double len, int note, int vel)
{
    if (note < 0 || note > 127 || len <= 0.0) return;
    out.push_back ({ start, len, note, vel, slotLen, slot });
}

std::vector<int> sortedNotes (const VoicedSlot& s)
{
    std::vector<int> n;
    for (int x : s.notes) if (x >= 0 && x <= 127) n.push_back (x);
    std::sort (n.begin(), n.end());
    return n;
}

std::vector<int> arpPool (const std::vector<int>& base, int octaves)
{
    std::vector<int> pool;
    for (int o = 0; o < std::clamp (octaves, 1, 3); ++o)
        for (int n : base)
            if (n + 12 * o <= 127) pool.push_back (n + 12 * o);
    std::sort (pool.begin(), pool.end());
    return pool;
}

// ---- held / rhythm -------------------------------------------------------
struct Hit { double pos; double len; };

std::vector<Hit> barHits (RhythmPattern p)
{
    switch (p)
    {
        case RhythmPattern::Stabs:      return { { 0, 0.5 }, { 1.5, 0.5 }, { 3, 0.5 } };
        case RhythmPattern::Offbeat:    return { { 0.5, 0.5 }, { 1.5, 0.5 }, { 2.5, 0.5 }, { 3.5, 0.5 } };
        case RhythmPattern::Charleston: return { { 0, 1 }, { 1.5, 0.5 } };
        case RhythmPattern::Pulse8:
        case RhythmPattern::Push:
            break;
    }
    std::vector<Hit> h;
    for (int i = 0; i < 8; ++i) h.push_back ({ i * 0.5, 0.5 });
    return h;
}

double swingShift (int stepIndex, double step, float swing)
{
    return (stepIndex % 2 == 1) ? (double) swing * step * 0.5 : 0.0;
}

void genRhythm (Raw& out, const std::vector<VoicedSlot>& slots, size_t si, const StyleParams& sp)
{
    const auto& slot = slots[si];
    const double len = slot.lengthBeats;
    const auto hits = barHits (sp.rhythm);
    const auto own = sortedNotes (slot);
    const bool push = sp.rhythm == RhythmPattern::Push;
    const auto next = sortedNotes (slots[(si + 1) % slots.size()]);

    for (int bar = 0; bar * 4.0 < len - eps; ++bar)
        for (auto& h : hits)
        {
            const double pos = bar * 4.0 + h.pos;
            if (pos >= len - eps) continue;
            const int eighth = (int) std::lround (h.pos / 0.5);
            const double start = pos + swingShift (eighth, 0.5, sp.swing);
            const bool last = push && pos + 0.5 >= len - eps;
            for (int n : last ? next : own)
                add (out, si, len, start, h.len * sp.gate, n, baseVel);
        }
}

// ---- arpeggio sequences (return a cycle of pool indices) ----------------
std::vector<int> cascadeCycle (int n)
{
    std::vector<int> c;
    if (n < 3) { for (int i = n - 1; i >= 0; --i) c.push_back (i); return c; }
    for (int s = n - 1; s >= 2; --s) { c.push_back (s); c.push_back (s - 1); c.push_back (s - 2); }
    return c;
}

std::vector<int> pendulumCycle (int n)
{
    const int m = n / 2;
    std::vector<int> f { m };
    for (int d = 1; (int) f.size() < n; ++d)
    {
        if (m + d < n) f.push_back (m + d);
        if (m - d >= 0) f.push_back (m - d);
    }
    std::vector<int> c = f;
    // back inwards without repeating the turning note nor the first note of the next cycle
    for (int i = n - 2; i >= 1; --i) c.push_back (f[(size_t) i]);
    return c;
}

std::vector<int> thirdsCycle (int n)
{
    std::vector<int> c;
    if (n < 3) { for (int i = 0; i < n; ++i) c.push_back (i); return c; }
    for (int i = 0; i + 2 < n; ++i) { c.push_back (i); c.push_back (i + 2); }
    for (int i = n - 1; i >= 2; --i) { c.push_back (i); c.push_back (i - 2); }
    return c;
}

void genStepArp (Raw& out, size_t si, double len, const std::vector<int>& cycleNotes, const StyleParams& sp)
{
    if (cycleNotes.empty()) return;
    const double step = rateBeats (sp.rate);
    const bool swingable = sp.rate == Rate::Eighth || sp.rate == Rate::Sixteenth;
    for (int k = 0; k * step < len - eps; ++k)
    {
        double start = k * step;
        if (swingable) start += swingShift (k, step, sp.swing);
        add (out, si, len, start, step * sp.gate, cycleNotes[(size_t) k % cycleNotes.size()], baseVel);
    }
}

void genGallop (Raw& out, size_t si, double len, const std::vector<int>& pool, const StyleParams& sp)
{
    if (pool.empty()) return;
    const double durs[3] { 0.5, 0.25, 0.25 };
    double t = 0.0;
    for (int k = 0; t < len - eps; ++k)
    {
        const double d = durs[k % 3];
        add (out, si, len, t, d * sp.gate, pool[(size_t) k % pool.size()], baseVel);
        t += d;
    }
}

void genSync16 (Raw& out, size_t si, double len, const std::vector<int>& pool, const StyleParams& sp)
{
    if (pool.empty()) return;
    const int pos16[6] { 0, 3, 6, 9, 12, 14 };
    const size_t upper = pool.size() - 1;
    int k = 0;
    for (int bar = 0; bar * 4.0 < len - eps; ++bar)
        for (int j = 0; j < 6; ++j, ++k)
        {
            const double start = bar * 4.0 + pos16[j] * 0.25;
            if (start >= len - eps) continue;
            const double nextPos = (j < 5 ? pos16[j + 1] : 16) * 0.25;
            const double gap = nextPos - pos16[j] * 0.25;
            const int note = (k % 2 == 0 || upper == 0) ? pool[0] : pool[1 + (size_t) (k / 2) % upper];
            add (out, si, len, start, gap * sp.gate, note, (pos16[j] == 0 || pos16[j] == 6) ? accentVel : baseVel);
        }
}

void genStrum (Raw& out, size_t si, double len, const std::vector<int>& notes)
{
    std::vector<double> strums;
    for (int bar = 0; bar * 4.0 < len - eps; ++bar)
        for (double b : { 0.0, 2.0 })
            if (bar * 4.0 + b < len - eps) strums.push_back (bar * 4.0 + b);

    for (size_t s = 0; s < strums.size(); ++s)
    {
        const double next = s + 1 < strums.size() ? strums[s + 1] : len;
        const bool down = std::fmod (strums[s], 4.0) < 1.0;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const int note = down ? notes[i] : notes[notes.size() - 1 - i];
            const double start = strums[s] + 0.03 * (double) i;
            add (out, si, len, start, next - start, note, baseVel);
        }
    }
}

void genArp (Raw& out, size_t si, double len, const VoicedSlot& slot, const StyleParams& sp)
{
    const auto base = sortedNotes (slot);
    if (base.empty()) return;
    const auto pool = arpPool (base, sp.arpOctaves);
    const int n = (int) pool.size();

    auto mapped = [&] (const std::vector<int>& idx)
    {
        std::vector<int> v;
        for (int i : idx) v.push_back (pool[(size_t) i]);
        return v;
    };

    switch (sp.arp)
    {
        case ArpPattern::Cascade:      genStepArp (out, si, len, mapped (cascadeCycle (n)), sp); break;
        case ArpPattern::Pendulum:     genStepArp (out, si, len, mapped (pendulumCycle (n)), sp); break;
        case ArpPattern::BrokenThirds: genStepArp (out, si, len, mapped (thirdsCycle (n)), sp); break;
        case ArpPattern::OctaveLeap:
        {
            std::vector<int> c;
            for (int p : base) { c.push_back (p); c.push_back (p + 12 <= 127 ? p + 12 : p); }
            genStepArp (out, si, len, c, sp);
            break;
        }
        case ArpPattern::Gallop:       genGallop (out, si, len, pool, sp); break;
        case ArpPattern::Syncopated16: genSync16 (out, si, len, pool, sp); break;
        case ArpPattern::Strum:        genStrum (out, si, len, base); break;
    }
}
} // namespace

std::vector<NoteEvent> generateEvents (const std::vector<VoicedSlot>& slots, const StyleParams& sp)
{
    Raw raw;
    std::vector<double> offsets;
    double offset = 0.0;
    for (size_t si = 0; si < slots.size(); ++si)
    {
        offsets.push_back (offset);
        const auto& slot = slots[si];
        const double len = slot.lengthBeats;
        if (len > 0.0)
        {
            switch (sp.mode)
            {
                case PlayMode::Held:
                    for (int n : sortedNotes (slot)) add (raw, si, len, 0.0, len, n, baseVel);
                    break;
                case PlayMode::Rhythm: genRhythm (raw, slots, si, sp); break;
                case PlayMode::Arp:    genArp (raw, si, len, slot, sp); break;
            }
        }
        offset += len;
    }

    std::mt19937 rng (sp.seed);
    const double h = (double) sp.humanize;
    std::vector<NoteEvent> out;
    for (auto& e : raw)
    {
        double start = e.start;
        int vel = e.velocity;
        if (h > 0.0)
        {
            std::uniform_real_distribution<double> ds (-0.03 * h, 0.03 * h), dv (-15.0 * h, 15.0 * h);
            start += ds (rng);
            vel = (int) std::lround ((double) vel + dv (rng));
            start = std::clamp (start, 0.0, e.limit - 1e-6);
        }
        double length = std::min (e.length, e.limit - start);
        if (length <= 0.0) continue;
        out.push_back ({ offsets[e.slot] + start, length, e.note, std::clamp (vel, 1, 127) });
    }
    std::stable_sort (out.begin(), out.end(), [] (const NoteEvent& a, const NoteEvent& b)
    {
        if (a.startBeat < b.startBeat) return true;
        if (b.startBeat < a.startBeat) return false;
        return a.note < b.note;
    });
    return out;
}

std::string rhythmName (RhythmPattern p)
{
    switch (p)
    {
        case RhythmPattern::Stabs:      return "Stabs";
        case RhythmPattern::Offbeat:    return "Offbeat";
        case RhythmPattern::Pulse8:     return "Pulse 8ths";
        case RhythmPattern::Charleston: return "Charleston";
        case RhythmPattern::Push:       return "Push";
    }
    return {};
}

std::string arpName (ArpPattern p)
{
    switch (p)
    {
        case ArpPattern::Cascade:      return "Cascade";
        case ArpPattern::Pendulum:     return "Pendulum";
        case ArpPattern::BrokenThirds: return "Broken Thirds";
        case ArpPattern::OctaveLeap:   return "Octave Leap";
        case ArpPattern::Gallop:       return "Gallop";
        case ArpPattern::Syncopated16: return "Syncopated 16ths";
        case ArpPattern::Strum:        return "Strum";
    }
    return {};
}

std::string rateName (Rate r)
{
    switch (r)
    {
        case Rate::Quarter:          return "1/4";
        case Rate::Eighth:           return "1/8";
        case Rate::EighthTriplet:    return "1/8T";
        case Rate::Sixteenth:        return "1/16";
        case Rate::SixteenthTriplet: return "1/16T";
    }
    return {};
}
} // namespace cb
