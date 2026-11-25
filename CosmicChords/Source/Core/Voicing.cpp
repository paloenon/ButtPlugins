#include "Core/Voicing.h"
#include <algorithm>
#include <cmath>

namespace cb
{
namespace
{
int pcOf (int n) { return ((n % 12) + 12) % 12; }

// Highest note below `limit` with the given pitch class (may be negative; clamped later).
int belowWithPc (int pc, int limit)
{
    int n = limit - 1;
    while (pcOf (n) != pc)
        --n;
    return n;
}

void invert (std::vector<int>& notes, int inversion)
{
    std::sort (notes.begin(), notes.end());
    const int n = (int) notes.size();
    if (n == 0)
        return;
    const int inv = ((inversion % n) + n) % n;
    for (int i = 0; i < inv; ++i)
        notes[(size_t) i] += 12;
    std::sort (notes.begin(), notes.end());
}

void clampToMidi (std::vector<int>& notes)
{
    std::sort (notes.begin(), notes.end());
    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    while (notes.front() < 0)
        for (auto& x : notes) x += 12;
    while (notes.back() > 127 && notes.front() >= 12)
        for (auto& x : notes) x -= 12;
    while (! notes.empty() && notes.back() > 127)
        notes.pop_back();
}

std::vector<int> closeVoicing (const Chord& chord, const VoicingParams& p)
{
    const int root = 48 + chord.rootPc + 12 * p.octave;
    std::vector<int> notes;
    for (int iv : qualityIntervals (chord.quality))
        notes.push_back (root + iv);
    invert (notes, p.inversion);

    if (chord.bassPc >= 0)
        notes.push_back (belowWithPc (pcOf (chord.bassPc), notes.front()));
    return notes;
}

std::vector<int> spreadVoicing (const Chord& chord, const VoicingParams& p, int level)
{
    const auto& intervals = qualityIntervals (chord.quality);
    const bool slash = chord.bassPc >= 0;
    const bool dropRoot = ! slash && intervals.size() >= 4;

    // Upper chord: around middle C so the bass has room below it.
    const int rootPc = pcOf (chord.rootPc);
    const int ref = (rootPc < 6 ? 60 : 48) + rootPc + 12 * p.octave;
    std::vector<int> upper;
    for (int iv : intervals)
        if (! (dropRoot && iv == 0))
            upper.push_back (ref + iv);
    invert (upper, p.inversion);

    if (level >= 2)   // open: raise every second note of the close upper chord by an octave
    {
        for (size_t i = 1; i < upper.size(); i += 2)
            upper[i] += 12;
        std::sort (upper.begin(), upper.end());
    }

    const int bassPc = slash ? pcOf (chord.bassPc) : rootPc;
    int bass = belowWithPc (bassPc, upper.front() - 7);   // at least a fifth below the upper chord
    if (level >= 3)
        bass -= 12;

    std::vector<int> notes { bass };
    if (level >= 4)
    {
        const bool hasFifth = std::find (intervals.begin(), intervals.end(), 7) != intervals.end();
        notes.push_back (bass + (hasFifth && ! slash ? 7 : 12));
    }
    notes.insert (notes.end(), upper.begin(), upper.end());
    return notes;
}
} // namespace

int spreadLevel (float spread)
{
    return std::clamp ((int) std::lround (std::clamp (spread, 0.0f, 1.0f) * 4.0f), 0, 4);
}

std::string spreadLevelName (int level)
{
    static const char* const names[] { "Close", "Bass", "Open", "Deep", "Wide" };
    return names[std::clamp (level, 0, 4)];
}

std::vector<int> voiceChord (const Chord& chord, const VoicingParams& p)
{
    const int level = spreadLevel (p.spread);
    auto notes = level == 0 ? closeVoicing (chord, p) : spreadVoicing (chord, p, level);
    clampToMidi (notes);
    return notes;
}
} // namespace cb
