#include "Core/Voicing.h"
#include <algorithm>

namespace cb
{
namespace
{
// Highest note below `limit` with the given pitch class (may be negative; clamped later).
int belowWithPc (int pc, int limit)
{
    int n = limit - 1;
    while (((n % 12) + 12) % 12 != pc)
        --n;
    return n;
}
} // namespace

std::vector<int> voiceChord (const Chord& chord, const VoicingParams& p)
{
    const int root = 48 + chord.rootPc + 12 * p.octave;
    std::vector<int> notes;
    for (int iv : qualityIntervals (chord.quality))
        notes.push_back (root + iv);
    std::sort (notes.begin(), notes.end());

    const int n = (int) notes.size();
    const int inv = ((p.inversion % n) + n) % n;
    for (int i = 0; i < inv; ++i)
        notes[(size_t) i] += 12;
    std::sort (notes.begin(), notes.end());

    if (p.spread >= 0.33f && n >= 2)
        notes[(size_t) n - 2] -= 12;
    std::sort (notes.begin(), notes.end());

    if (p.spread >= 0.66f)
        notes.push_back (belowWithPc (((chord.rootPc % 12) + 12) % 12, notes.front()));

    if (chord.bassPc >= 0)
    {
        std::sort (notes.begin(), notes.end());
        notes.push_back (belowWithPc (chord.bassPc % 12, notes.front()));
    }

    std::sort (notes.begin(), notes.end());
    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());

    while (notes.front() < 0)
        for (auto& x : notes) x += 12;
    while (notes.back() > 127 && notes.front() >= 12)
        for (auto& x : notes) x -= 12;
    while (! notes.empty() && notes.back() > 127)
        notes.pop_back();
    return notes;
}
} // namespace cb
