#include "Core/Library.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

namespace cb
{
namespace
{
[[noreturn]] void fail (const std::string& who, const std::string& what)
{
    throw std::runtime_error ("library entry '" + who + "': " + what);
}

bool startsWith (const std::string& s, size_t pos, const char* prefix)
{
    return s.compare (pos, std::char_traits<char>::length (prefix), prefix) == 0;
}

// Strict: optional single b/#, exactly one of I..VII (all upper or all lower case),
// then optionally one marker among ° o + ø. romanToOffset() alone ignores trailing text.
bool isStrictRoman (const std::string& s)
{
    size_t i = 0;
    if (i < s.size() && (s[i] == 'b' || s[i] == '#'))
        ++i;

    static const char* numerals[] { "VII", "III", "II", "IV", "VI", "I", "V" };  // longest first
    bool matched = false;
    for (const char* n : numerals)
    {
        const size_t len = std::char_traits<char>::length (n);
        if (i + len > s.size())
            continue;
        auto part = s.substr (i, len);
        std::string lower = part;
        for (auto& c : lower) c = (char) std::tolower ((unsigned char) c);
        std::string upper = part;
        for (auto& c : upper) c = (char) std::toupper ((unsigned char) c);
        if (upper == n && (part == upper || part == lower))
        {
            i += len;
            matched = true;
            break;
        }
    }
    if (! matched)
        return false;

    if (i == s.size())
        return true;
    for (const char* marker : { "\xC2\xB0", "o", "+", "\xC3\xB8" })   // ° o + ø
        if (startsWith (s, i, marker) && i + std::char_traits<char>::length (marker) == s.size())
            return true;
    return false;
}

int degreeOffset (const std::string& who, const std::string& roman, const char* field)
{
    if (! isStrictRoman (roman))
        fail (who, std::string ("invalid roman numeral in \"") + field + "\": \"" + roman + "\"");
    auto off = romanToOffset (roman);
    if (! off)
        fail (who, std::string ("invalid roman numeral in \"") + field + "\": \"" + roman + "\"");
    return *off;
}

std::string requireString (const juce::var& obj, const char* key, const std::string& who)
{
    auto v = obj.getProperty (key, juce::var());
    if (! v.isString() || v.toString().isEmpty())
        fail (who, std::string ("missing or empty \"") + key + "\"");
    return v.toString().toStdString();
}

std::string lowerCase (const std::string& s)
{
    return juce::String::fromUTF8 (s.c_str()).toLowerCase().toStdString();
}

LibraryEntry parseEntry (const juce::var& obj, int index)
{
    std::string who = "#" + std::to_string (index);
    if (! obj.isObject())
        fail (who, "not an object");

    auto idVar = obj.getProperty ("id", juce::var());
    if (idVar.isString() && idVar.toString().isNotEmpty())
        who = idVar.toString().toStdString();

    LibraryEntry e;
    e.id = requireString (obj, "id", who);
    e.name = requireString (obj, "name", who);
    e.genre = requireString (obj, "genre", who);
    if (std::find (Library::genres().begin(), Library::genres().end(), e.genre) == Library::genres().end())
        fail (who, "unknown genre \"" + e.genre + "\"");

    auto moods = obj.getProperty ("moods", juce::var());
    if (! moods.isArray() || moods.size() == 0)
        fail (who, "\"moods\" must be a non-empty array");
    for (const auto& m : *moods.getArray())
    {
        auto mood = m.toString().toStdString();
        if (! m.isString() || std::find (Library::moods().begin(), Library::moods().end(), mood) == Library::moods().end())
            fail (who, "unknown mood \"" + mood + "\"");
        if (std::find (e.moods.begin(), e.moods.end(), mood) != e.moods.end())
            fail (who, "duplicate mood \"" + mood + "\"");
        e.moods.push_back (mood);
    }

    const auto scaleStr = requireString (obj, "scale", who);
    bool scaleFound = false;
    for (int s = 0; s <= (int) Scale::MelodicMinor; ++s)
        if (scaleName ((Scale) s) == scaleStr)
        {
            e.scale = (Scale) s;
            scaleFound = true;
        }
    if (! scaleFound)
        fail (who, "unknown scale \"" + scaleStr + "\"");

    auto chords = obj.getProperty ("chords", juce::var());
    if (! chords.isArray() || chords.size() < 2 || chords.size() > 8)
        fail (who, "\"chords\" must be an array of 2 to 8 chords");
    for (const auto& c : *chords.getArray())
    {
        if (! c.isObject())
            fail (who, "chord is not an object");
        auto r = c.getProperty ("r", juce::var());
        auto q = c.getProperty ("q", juce::var());
        if (! r.isString())
            fail (who, "chord without \"r\"");
        if (! q.isString())
            fail (who, "chord without \"q\"");

        RelChord rc;
        rc.roman = r.toString().toStdString();
        rc.rootOffset = degreeOffset (who, rc.roman, "r");
        auto quality = parseQuality (q.toString().toStdString());
        if (! quality)
            fail (who, "unknown quality \"" + q.toString().toStdString() + "\"");
        rc.quality = *quality;

        auto bass = c.getProperty ("bass", juce::var());
        if (! bass.isVoid())
        {
            if (! bass.isString())
                fail (who, "\"bass\" must be a string");
            rc.bassOffset = degreeOffset (who, bass.toString().toStdString(), "bass");
        }
        e.chords.push_back (rc);
    }

    auto bars = obj.getProperty ("bars", juce::var());
    if (bars.isVoid())
    {
        e.lengthsBars.assign (e.chords.size(), 1.0);
    }
    else
    {
        if (! bars.isArray() || bars.size() != (int) e.chords.size())
            fail (who, "\"bars\" must have one length per chord");
        for (const auto& b : *bars.getArray())
        {
            const double len = (b.isInt() || b.isInt64() || b.isDouble()) ? (double) b : -1.0;
            if (len != 0.5 && len != 1.0 && len != 2.0)
                fail (who, "chord length must be 0.5, 1 or 2 bars");
            e.lengthsBars.push_back (len);
        }
    }

    // Optional top line: one array per chord of [semitones from the tonic, start beat in the chord, length in beats].
    auto top = obj.getProperty ("topline", juce::var());
    if (! top.isVoid())
    {
        if (! top.isArray() || top.size() != (int) e.chords.size())
            fail (who, "\"topline\" must have one note list per chord");
        auto number = [] (const juce::var& v, double fallback)
        {
            return (v.isInt() || v.isInt64() || v.isDouble()) ? (double) v : fallback;
        };
        for (int i = 0; i < top.size(); ++i)
        {
            const auto& notes = top[i];
            if (! notes.isArray())
                fail (who, "topline entry " + std::to_string (i + 1) + " is not an array");
            const double chordBeats = e.lengthsBars[(size_t) i] * 4.0;
            std::vector<MelodyNote> line;
            for (const auto& n : *notes.getArray())
            {
                if (! n.isArray() || n.size() < 3 || n.size() > 4)
                    fail (who, "topline note must be [semis, start, length] or [semis, start, length, velocity]");
                MelodyNote m;
                const double semis = number (n[0], 1000.0);
                m.startBeat = number (n[1], -1.0);
                m.lengthBeats = number (n[2], -1.0);
                m.velocity = n.size() > 3 ? (int) number (n[3], -1.0) : 100;
                if (semis != std::floor (semis) || semis < -24 || semis > 36)
                    fail (who, "topline semitones must be whole numbers from -24 to 36");
                m.semis = (int) semis;
                if (m.startBeat < 0.0 || m.lengthBeats <= 0.0 || m.startBeat + m.lengthBeats > chordBeats + 1.0e-9)
                    fail (who, "topline note in chord " + std::to_string (i + 1) + " does not fit inside the chord");
                if (m.velocity < 1 || m.velocity > 127)
                    fail (who, "topline velocity must be 1 to 127");
                line.push_back (m);
            }
            e.melody.push_back (std::move (line));
        }
    }
    return e;
}
} // namespace

Library Library::fromJson (const juce::String& json)
{
    juce::var root;
    auto result = juce::JSON::parse (json, root);
    if (result.failed())
        throw std::runtime_error ("library: invalid JSON: " + result.getErrorMessage().toStdString());
    if (! root.isArray())
        throw std::runtime_error ("library: top level must be an array");

    Library lib;
    std::set<std::string> ids, names;
    int index = 0;
    for (const auto& item : *root.getArray())
    {
        auto e = parseEntry (item, index++);
        if (! ids.insert (e.id).second)
            fail (e.id, "duplicate id");
        if (! names.insert (e.name).second)
            fail (e.id, "duplicate name \"" + e.name + "\"");
        lib.items.push_back (std::move (e));
    }
    return lib;
}

std::vector<const LibraryEntry*> Library::filter (const std::string& genre, const std::string& mood,
                                                  const std::string& search, int tonicPc) const
{
    const auto needle = lowerCase (search);
    std::vector<const LibraryEntry*> out;
    for (const auto& e : items)
    {
        if (! genre.empty() && e.genre != genre)
            continue;
        if (! mood.empty() && std::find (e.moods.begin(), e.moods.end(), mood) == e.moods.end())
            continue;
        if (! needle.empty())
        {
            bool hit = lowerCase (e.name).find (needle) != std::string::npos;
            for (size_t i = 0; ! hit && i < e.chords.size(); ++i)
                hit = lowerCase (chordNameInKey (e.chords[i], tonicPc, e.scale)).find (needle) != std::string::npos;
            if (! hit)
                continue;
        }
        out.push_back (&e);
    }
    return out;
}

const std::vector<std::string>& Library::genres()
{
    static const std::vector<std::string> g { "Cosmic Pack", "Pop", "Hip-Hop/Trap", "Lo-Fi", "R&B/Neo-Soul",
                                              "House/EDM", "Cinematic", "Jazz", "Rock/Indie" };
    return g;
}

const std::vector<std::string>& Library::moods()
{
    static const std::vector<std::string> m { "Happy", "Melancholic", "Dark", "Dreamy",
                                              "Epic", "Romantic", "Tense", "Chill" };
    return m;
}

Strip toStrip (const LibraryEntry& e)
{
    Strip s;
    for (size_t i = 0; i < e.chords.size(); ++i)
        s.append ({ e.chords[i], i < e.lengthsBars.size() ? e.lengthsBars[i] : 1.0, std::nullopt,
                    i < e.melody.size() ? e.melody[i] : std::vector<MelodyNote> {} });
    return s;
}

void applyLibraryEntry (const LibraryEntry& e, ParamValues& p, Strip& strip)
{
    p.scale = e.scale;
    strip = toStrip (e);
}
} // namespace cb
