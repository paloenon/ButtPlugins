#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include "BinaryData.h"
#include "Core/Library.h"

using namespace cb;

static const Library& bundled()
{
    static const Library lib = Library::fromJson (juce::String::fromUTF8 (BinaryData::library_json,
                                                                         BinaryData::library_jsonSize));
    return lib;
}

static bool contains (const std::vector<std::string>& v, const std::string& s)
{
    return std::find (v.begin(), v.end(), s) != v.end();
}

// Wraps one entry object in a JSON array.
static juce::String one (const std::string& entry) { return juce::String::fromUTF8 (("[" + entry + "]").c_str()); }

static std::string errorOf (const juce::String& json)
{
    try
    {
        Library::fromJson (json);
    }
    catch (const std::runtime_error& e)
    {
        return e.what();
    }
    return "<no exception>";
}

TEST_CASE ("Library: fixed genre and mood lists")
{
    REQUIRE (Library::genres() == std::vector<std::string> { "Pop", "Hip-Hop/Trap", "Lo-Fi", "R&B/Neo-Soul",
                                                             "House/EDM", "Cinematic", "Jazz", "Rock/Indie" });
    REQUIRE (Library::moods() == std::vector<std::string> { "Happy", "Melancholic", "Dark", "Dreamy", "Epic",
                                                            "Romantic", "Tense", "Chill" });
}

TEST_CASE ("Library: parses a valid entry")
{
    auto lib = Library::fromJson (one (R"({"id":"t-1","name":"Test","genre":"Lo-Fi","moods":["Chill"],
        "scale":"Harmonic Minor","chords":[{"r":"i","q":"m9"},{"r":"bVI","q":"maj7"},{"r":"vii°","q":"dim7"},
        {"r":"V","q":"7","bass":"vii"}],"bars":[2,1,0.5,0.5]})"));
    REQUIRE (lib.entries().size() == 1);
    const auto& e = lib.entries()[0];
    REQUIRE (e.id == "t-1");
    REQUIRE (e.name == "Test");
    REQUIRE (e.scale == Scale::HarmonicMinor);
    REQUIRE (e.moods == std::vector<std::string> { "Chill" });
    REQUIRE (e.chords.size() == 4);
    REQUIRE (e.chords[0] == RelChord { 0, Quality::Min9, -1, "i" });
    REQUIRE (e.chords[1] == RelChord { 8, Quality::Maj7, -1, "bVI" });
    REQUIRE (e.chords[2] == RelChord { 11, Quality::Dim7, -1, "vii°" });
    REQUIRE (e.chords[3] == RelChord { 7, Quality::Dom7, 11, "V" });
    REQUIRE (e.lengthsBars == std::vector<double> { 2.0, 1.0, 0.5, 0.5 });

    auto strip = toStrip (e);
    REQUIRE (strip.slots().size() == 4);
    REQUIRE (strip.slots()[0].chord == e.chords[0]);
    REQUIRE (strip.slots()[0].lengthBars == 2.0);
    REQUIRE (strip.slots()[3].lengthBars == 0.5);
    REQUIRE (strip.totalBeats() == 16.0);
}

TEST_CASE ("Library: bars default to one bar each")
{
    auto lib = Library::fromJson (one (R"({"id":"t-2","name":"Two","genre":"Pop","moods":["Happy"],
        "scale":"Major","chords":[{"r":"I","q":""},{"r":"V","q":""}]})"));
    REQUIRE (lib.entries()[0].lengthsBars == std::vector<double> { 1.0, 1.0 });
}

TEST_CASE ("Library: malformed entries throw naming the id")
{
    auto entry = [] (const char* id, const char* chords, const char* extra = "", const char* genre = "Pop",
                     const char* mood = "Happy", const char* scale = "Major")
    {
        return one (std::string (R"({"id":")") + id + R"(","name":"N)" + id + R"(","genre":")" + genre
                    + R"(","moods":[")" + mood + R"("],"scale":")" + scale + R"(","chords":)" + chords + extra
                    + "}");
    };
    const char* good = R"([{"r":"I","q":""},{"r":"IV","q":""}])";

    // Strict roman validation (romanToOffset alone would accept these).
    REQUIRE (errorOf (entry ("bad-ix", R"([{"r":"ix","q":"m"},{"r":"I","q":""}])")).find ("bad-ix") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-vzzz", R"([{"r":"Vzzz","q":""},{"r":"I","q":""}])")).find ("bad-vzzz") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-iiii", R"([{"r":"IIII","q":""},{"r":"I","q":""}])")).find ("bad-iiii") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-dbl", R"([{"r":"bbVI","q":""},{"r":"I","q":""}])")).find ("bad-dbl") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-mark", R"([{"r":"vii°°","q":"dim"},{"r":"I","q":""}])")).find ("bad-mark") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-empty", R"([{"r":"","q":""},{"r":"I","q":""}])")).find ("bad-empty") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-bass", R"([{"r":"I","q":"","bass":"V7"},{"r":"I","q":""}])")).find ("bad-bass") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-q", R"([{"r":"I","q":"mega"},{"r":"I","q":""}])")).find ("bad-q") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-genre", good, "", "Polka")).find ("bad-genre") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-mood", good, "", "Pop", "Grumpy")).find ("bad-mood") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-scale", good, "", "Pop", "Happy", "Locrian")).find ("bad-scale") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-bars", good, R"(,"bars":[1])")).find ("bad-bars") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-len", good, R"(,"bars":[1,3])")).find ("bad-len") != std::string::npos);
    REQUIRE (errorOf (entry ("bad-one", R"([{"r":"I","q":""}])")).find ("bad-one") != std::string::npos);

    // Accepted markers and accidentals.
    REQUIRE_NOTHROW (Library::fromJson (entry ("ok-1", R"([{"r":"#iv°","q":"dim7"},{"r":"bIII+","q":"aug"},{"r":"iiø","q":"m7b5"},{"r":"viio","q":"dim"}])")));

    // Not an array / duplicate ids.
    REQUIRE_THROWS_AS (Library::fromJson ("{\"id\":1}"), std::runtime_error);
    const auto dup = juce::String (R"([{"id":"d-1","name":"A","genre":"Pop","moods":["Happy"],"scale":"Major","chords":)") + good
                     + R"(},{"id":"d-1","name":"B","genre":"Pop","moods":["Happy"],"scale":"Major","chords":)" + good + "}]";
    REQUIRE (errorOf (dup).find ("d-1") != std::string::npos);
}

TEST_CASE ("Library: bundled library size, genres, moods, ids")
{
    const auto& lib = bundled();
    REQUIRE (lib.entries().size() >= 200);

    std::map<std::string, int> perGenre, perMood;
    std::set<std::string> ids, names;
    for (const auto& e : lib.entries())
    {
        REQUIRE (contains (Library::genres(), e.genre));
        REQUIRE_FALSE (e.moods.empty());
        for (const auto& m : e.moods)
        {
            REQUIRE (contains (Library::moods(), m));
            ++perMood[m];
        }
        ++perGenre[e.genre];
        REQUIRE (ids.insert (e.id).second);
        REQUIRE (names.insert (e.name).second);
        REQUIRE (e.chords.size() >= 2);
        REQUIRE (e.chords.size() <= 8);
        REQUIRE (e.lengthsBars.size() == e.chords.size());
    }
    for (const auto& g : Library::genres()) { INFO (g); REQUIRE (perGenre[g] >= 20); }
    for (const auto& m : Library::moods()) { INFO (m); REQUIRE (perMood[m] >= 15); }
}

TEST_CASE ("Library: every entry voices in all keys and complexities")
{
    for (const auto& e : bundled().entries())
    {
        INFO (e.id);
        auto strip = toStrip (e);
        REQUIRE (strip.slots().size() == e.chords.size());
        for (int tonic = 0; tonic < 12; ++tonic)
            for (int complexity = 0; complexity <= 3; ++complexity)
            {
                auto voiced = voiceStrip (strip, tonic, e.scale, complexity, VoicingParams {});
                REQUIRE (voiced.size() == e.chords.size());
                for (const auto& v : voiced)
                {
                    REQUIRE_FALSE (v.notes.empty());
                    for (int n : v.notes)
                    {
                        REQUIRE (n >= 0);
                        REQUIRE (n <= 127);
                    }
                }
            }
    }
}

TEST_CASE ("Library: filter by genre and mood")
{
    const auto& lib = bundled();
    auto hits = lib.filter ("Lo-Fi", "Melancholic", "");
    REQUIRE_FALSE (hits.empty());
    for (auto* e : hits)
    {
        REQUIRE (e->genre == "Lo-Fi");
        REQUIRE (contains (e->moods, "Melancholic"));
    }
    REQUIRE (lib.filter ("", "", "").size() == lib.entries().size());
    REQUIRE (lib.filter ("Jazz", "", "").size() >= 20);
}

TEST_CASE ("Library: search matches names and resolved chord names, case-insensitive")
{
    const auto& lib = bundled();
    size_t expected = 0;
    for (const auto& e : lib.entries())
        for (const auto& c : e.chords)
            if (chordName (resolve (c, 9)).find ("Am9") != std::string::npos) { ++expected; break; }
    REQUIRE (expected > 0);

    auto hits = lib.filter ("", "", "am9");
    REQUIRE (hits.size() == expected);
    for (auto* e : hits)
    {
        bool found = false;
        for (const auto& c : e->chords)
            found = found || chordName (resolve (c, 9)).find ("Am9") != std::string::npos;
        REQUIRE (found);
    }

    const auto& first = lib.entries().front();
    auto byName = lib.filter ("", "", juce::String (first.name).toLowerCase().toStdString());
    REQUIRE (std::find (byName.begin(), byName.end(), &first) != byName.end());

    // The key changes what a chord-name search finds: tonic C turns i:m9 into Cm9.
    auto inC = lib.filter ("", "", "cm9", 0);
    for (auto* e : inC)
    {
        bool found = juce::String (e->name).containsIgnoreCase ("cm9");
        for (const auto& c : e->chords)
            found = found || chordName (resolve (c, 0)).find ("Cm9") != std::string::npos;
        REQUIRE (found);
    }
}

TEST_CASE ("Library: loading an entry sets scale, keeps root, replaces strip")
{
    const auto& e = bundled().entries().front();
    ParamValues p;
    p.root = 4;
    p.scale = e.scale == Scale::Lydian ? Scale::Major : Scale::Lydian;
    Strip strip;
    for (int i = 0; i < 6; ++i) strip.append ({ RelChord { 0, Quality::Maj, -1, "I" }, 1.0, std::nullopt });

    applyLibraryEntry (e, p, strip);
    REQUIRE (p.scale == e.scale);
    REQUIRE (p.root == 4);
    REQUIRE (strip.toValueTree().isEquivalentTo (toStrip (e).toValueTree()));
}

TEST_CASE ("Library: search uses key-aware chord spelling")
{
    // In A, a raised seventh (G#) is found as "G#", never as "Ab".
    const auto& lib = bundled();
    size_t expected = 0;
    for (const auto& e : lib.entries())
        for (const auto& c : e.chords)
            if (chordNameInKey (resolve (c, 9), 9, e.scale).find ("G#") != std::string::npos) { ++expected; break; }
    REQUIRE (lib.filter ("", "", "g#", 9).size() == expected);
    for (auto* e : lib.filter ("", "", "ab", 9))   // only by name, or a genuinely flat chord (e.g. bVI in A major = F, not Ab)
    {
        bool found = juce::String (e->name).containsIgnoreCase ("ab");
        for (const auto& c : e->chords)
            found = found || chordNameInKey (resolve (c, 9), 9, e->scale).find ("Ab") != std::string::npos;
        REQUIRE (found);
    }
}
