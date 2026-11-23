#include "Core/Palette.h"
#include <algorithm>

namespace cb
{
namespace
{
int mod12 (int x) { return ((x % 12) + 12) % 12; }

bool inScale (Scale s, int pc)
{
    for (int i : scaleIntervals (s))
        if (i == mod12 (pc))
            return true;
    return false;
}

bool hasTone (const std::vector<int>& t, int v) { return std::find (t.begin(), t.end(), v) != t.end(); }

std::optional<Quality> matchQuality (std::vector<int> tones)
{
    std::sort (tones.begin(), tones.end());
    static const Quality all[] { Quality::Maj, Quality::Min, Quality::Dim, Quality::Aug, Quality::Sus2,
        Quality::Sus4, Quality::Maj6, Quality::Min6, Quality::Add9, Quality::Maj7, Quality::Dom7, Quality::Min7,
        Quality::HalfDim7, Quality::Dim7, Quality::MinMaj7, Quality::Dom7sus4, Quality::Dom7b9, Quality::Maj9,
        Quality::Dom9, Quality::Min9, Quality::Dom11, Quality::Min11, Quality::Maj13, Quality::Dom13,
        Quality::Min13 };
    for (auto q : all)
    {
        auto iv = qualityIntervals (q);
        std::sort (iv.begin(), iv.end());
        if (iv == tones)
            return q;
    }
    return std::nullopt;
}

bool formsMinorNinth (const std::vector<int>& tones, int t)
{
    for (int e : tones)
        if (t > e && (t - e) % 12 == 1)
            return true;
    return false;
}
} // namespace

RelChord applyComplexity (const RelChord& in, Scale scale, int level)
{
    RelChord out = in;
    if (level <= 0)
        return out;

    if (in.quality == Quality::Sus4)
    {
        if (inScale (scale, in.rootOffset + 10))
            out.quality = Quality::Dom7sus4;
        return out;
    }
    if (in.quality == Quality::Sus2 || in.quality == Quality::Dom7sus4)
        return out;

    auto tones = qualityIntervals (in.quality);
    auto highest = [&] { return *std::max_element (tones.begin(), tones.end()); };

    for (int step = 1; step <= std::min (level, 3); ++step)
    {
        std::vector<int> candidates;
        if (step == 1)
        {
            if (hasTone (tones, 9) || hasTone (tones, 10) || hasTone (tones, 11))
                continue;
            int fifth = hasTone (tones, 6) ? 6 : hasTone (tones, 8) ? 8 : 7;
            candidates = { fifth + 3, fifth + 4 };
        }
        else if (step == 2)
        {
            if (hasTone (tones, 13) || hasTone (tones, 14))
                continue;
            candidates = { highest() + 3, highest() + 4 };
        }
        else
        {
            if (hasTone (tones, 17) || hasTone (tones, 18) || hasTone (tones, 20) || hasTone (tones, 21))
                continue;
            if (hasTone (tones, 3))
                candidates = { highest() + 3, highest() + 4 };
            else
                candidates = { 21 };
        }

        int chosen = -1;
        if (candidates.size() == 1)
        {
            // Direct 13th for major-third chords.
            if (inScale (scale, in.rootOffset + candidates[0]) && ! formsMinorNinth (tones, candidates[0]))
                chosen = candidates[0];
        }
        else
        {
            const int h = step == 1 ? candidates[0] - 3 : highest();
            if (inScale (scale, in.rootOffset + h))
            {
                // Next third = the scale tone two scale steps above h.
                const auto& sc = scaleIntervals (scale);
                for (size_t i = 0; i < sc.size(); ++i)
                    if (sc[i] == mod12 (in.rootOffset + h))
                    {
                        int d = mod12 (sc[(i + 2) % 7] - sc[i]);
                        if ((d == 3 || d == 4) && ! formsMinorNinth (tones, h + d))
                            chosen = h + d;
                        break;
                    }
            }
            else
            {
                for (int c : candidates)
                    if (inScale (scale, in.rootOffset + c) && ! formsMinorNinth (tones, c))
                    {
                        chosen = c;
                        break;
                    }
            }
        }
        if (chosen < 0)
            break;
        if (step == 1 && chosen == 11 && hasTone (tones, 3) && ! hasTone (tones, 6) && in.rootOffset != 0)
            chosen = 10; // no MinMaj7 away from the tonic

        auto next = tones;
        next.push_back (chosen);
        auto q = matchQuality (next);
        if (! q)
            break;
        tones = next;
        out.quality = *q;
    }
    return out;
}

std::vector<PadChord> buildPalette (Scale scale, int complexity)
{
    std::vector<PadChord> pads;
    const auto& sc = scaleIntervals (scale);
    static const char* numerals[7] { "I", "II", "III", "IV", "V", "VI", "VII" };

    for (int d = 0; d < 7; ++d)
    {
        int root = sc[(size_t) d];
        int third = mod12 (sc[(size_t) ((d + 2) % 7)] - root);
        int fifth = mod12 (sc[(size_t) ((d + 4) % 7)] - root);
        Quality q = Quality::Maj;
        std::string suffix;
        if (third == 3 && fifth == 7)      q = Quality::Min;
        else if (third == 3 && fifth == 6) { q = Quality::Dim; suffix = "\xC2\xB0"; }
        else if (third == 4 && fifth == 8) { q = Quality::Aug; suffix = "+"; }

        std::string roman = numerals[d];
        if (q == Quality::Min || q == Quality::Dim)
            std::transform (roman.begin(), roman.end(), roman.begin(), [] (unsigned char c) { return (char) std::tolower (c); });

        RelChord tri { root, q, -1, roman + suffix };
        pads.push_back ({ applyComplexity (tri, scale, complexity), "diatonic" });
    }

    auto add = [&] (int root, Quality q, const char* roman, const char* cat)
    {
        RelChord c { root, q, -1, roman };
        pads.push_back ({ applyComplexity (c, scale, complexity), cat });
    };

    if (! isMinorFamily (scale))
    {
        add (0, Quality::Sus2, "Isus2", "sus");
        add (7, Quality::Dom7sus4, "Vsus4", "sus");
        add (5, Quality::Min, "iv", "borrowed");
        add (8, Quality::Maj, "bVI", "borrowed");
        add (10, Quality::Maj, "bVII", "borrowed");
        add (2, Quality::Dom7, "V/V", "secondary");
        add (4, Quality::Dom7, "V/vi", "secondary");
    }
    else
    {
        add (0, Quality::Sus2, "isus2", "sus");
        add (7, Quality::Dom7, "V7", "borrowed");
        add (5, Quality::Maj, "IV", "borrowed");
        add (1, Quality::Maj7, "bII", "colour");
        // Picardy I: stays a major triad (only add9 from level 2) so it never turns into the same
        // dominant seventh as the V/iv pad next to it.
        {
            RelChord picardy { 0, Quality::Maj, -1, "I" };
            if (complexity >= 2 && inScale (scale, 2))
                picardy.quality = Quality::Add9;
            pads.push_back ({ picardy, "borrowed" });
        }
        add (0, Quality::Dom7, "V/iv", "secondary");
        add (7, Quality::Sus4, "vsus4", "sus");
    }
    return pads;
}
} // namespace cb
