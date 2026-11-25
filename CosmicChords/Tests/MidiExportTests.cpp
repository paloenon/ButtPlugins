#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Core/MidiExport.h"

using namespace cb;

TEST_CASE ("MidiExport: single note at 90 BPM", "[MidiExport]")
{
    auto mf = buildMidiFile ({ { 0.0, 4.0, 60, 100 } }, 90.0);
    REQUIRE (mf.getNumTracks() == 1);
    REQUIRE (mf.getTimeFormat() == 960);
    auto* t = mf.getTrack (0);

    bool sawTempo = false, sawTimeSig = false;
    int ons = 0, offs = 0;
    for (auto* e : *t)
    {
        auto& m = e->message;
        if (m.isTempoMetaEvent())
        {
            sawTempo = true;
            REQUIRE (m.getTimeStamp() == 0.0);
            REQUIRE (60.0 / m.getTempoSecondsPerQuarterNote() == Catch::Approx (90.0).margin (0.001));
        }
        if (m.isTimeSignatureMetaEvent())
        {
            int n = 0, d = 0;
            m.getTimeSignatureInfo (n, d);
            sawTimeSig = (n == 4 && d == 4) && m.getTimeStamp() == 0.0;
        }
        if (m.isNoteOn())  { ++ons;  REQUIRE (m.getTimeStamp() == 0.0);    REQUIRE (m.getNoteNumber() == 60); REQUIRE (m.getVelocity() == 100); REQUIRE (m.getChannel() == 1); }
        if (m.isNoteOff()) { ++offs; REQUIRE (m.getTimeStamp() == 3840.0); }
    }
    REQUIRE (sawTempo);
    REQUIRE (sawTimeSig);
    REQUIRE (ons == 1);
    REQUIRE (offs == 1);
}

TEST_CASE ("MidiExport: note-off precedes note-on at equal ticks", "[MidiExport]")
{
    auto mf = buildMidiFile ({ { 0.0, 1.0, 60, 100 }, { 1.0, 1.0, 60, 100 } }, 120.0);
    auto* t = mf.getTrack (0);
    int idxOff = -1, idxOn = -1;
    for (int i = 0; i < t->getNumEvents(); ++i)
    {
        auto& m = t->getEventPointer (i)->message;
        if (m.getTimeStamp() == 960.0 && m.isNoteOff()) idxOff = i;
        if (m.getTimeStamp() == 960.0 && m.isNoteOn())  idxOn = i;
    }
    REQUIRE (idxOff >= 0);
    REQUIRE (idxOn >= 0);
    REQUIRE (idxOff < idxOn);
}

TEST_CASE ("MidiExport: written file reads back", "[MidiExport]")
{
    std::vector<NoteEvent> ev { { 0.0, 1.5, 60, 90 }, { 2.0, 2.0, 64, 110 } };
    auto f = writeTempMidi (ev, 100.0, "CbTest/Am9:Fmaj9-unique7");
    REQUIRE (f.existsAsFile());
    REQUIRE (f.getFileExtension() == ".mid");
    REQUIRE (f.getParentDirectory().getFileName() == "CosmicChords");

    juce::FileInputStream in (f);
    REQUIRE (in.openedOk());
    juce::MidiFile rf;
    REQUIRE (rf.readFrom (in));
    REQUIRE (rf.getTimeFormat() == 960);
    auto* t = rf.getTrack (0);
    int ons = 0;
    for (auto* e : *t)
        if (e->message.isNoteOn())
        {
            ++ons;
            if (ons == 1) { REQUIRE (e->message.getTimeStamp() == 0.0);    REQUIRE (e->message.getNoteNumber() == 60); }
            if (ons == 2) { REQUIRE (e->message.getTimeStamp() == 1920.0); REQUIRE (e->message.getNoteNumber() == 64); }
        }
    REQUIRE (ons == 2);
    f.deleteFile();
}

TEST_CASE ("MidiExport: effectiveBpm", "[MidiExport]")
{
    REQUIRE (effectiveBpm (std::nullopt) == 120.0);
    REQUIRE (effectiveBpm (0.0) == 120.0);
    REQUIRE (effectiveBpm (-5.0) == 120.0);
    REQUIRE (effectiveBpm (140.0) == 140.0);
}

TEST_CASE ("MidiExport: empty events give meta-only file", "[MidiExport]")
{
    auto mf = buildMidiFile ({}, 120.0);
    REQUIRE (mf.getNumTracks() == 1);
    for (auto* e : *mf.getTrack (0))
        REQUIRE (! e->message.isNoteOnOrOff());

    auto f = writeTempMidi ({}, 120.0, "");
    REQUIRE (f.getFileName() == "Cosmic Chords.mid");
    REQUIRE (f.existsAsFile());
    f.deleteFile();
}

namespace
{
double endOfTrackTick (const juce::MidiFile& mf)
{
    double t = -1.0;
    for (auto* e : *mf.getTrack (0))
        if (e->message.isEndOfTrackMetaEvent())
            t = e->message.getTimeStamp();
    return t;
}
} // namespace

TEST_CASE ("MidiExport: end of track at the given length so the region loops", "[MidiExport]")
{
    // A 4-bar strip whose last note ends early (staccato rhythm) must still give a 4-bar region.
    std::vector<NoteEvent> ev { { 0.0, 1.0, 60, 100 }, { 12.0, 0.5, 64, 100 } };
    REQUIRE (endOfTrackTick (buildMidiFile (ev, 120.0)) == Catch::Approx (12.5 * 960.0));   // no length: last note-off
    REQUIRE (endOfTrackTick (buildMidiFile (ev, 120.0, 16.0)) == Catch::Approx (16.0 * 960.0));

    // Single slot of half a bar
    REQUIRE (endOfTrackTick (buildMidiFile ({ { 0.0, 0.25, 60, 100 } }, 120.0, 2.0)) == Catch::Approx (1920.0));

    // As in the preview, notes are clipped to the length (never past the end of the region).
    auto clipped = buildMidiFile ({ { 0.0, 8.0, 60, 100 }, { 4.0, 1.0, 62, 100 } }, 120.0, 4.0);
    int ons = 0;
    for (auto* e : *clipped.getTrack (0))
    {
        if (e->message.isNoteOn()) ++ons;
        if (e->message.isNoteOff()) REQUIRE (e->message.getTimeStamp() <= 3840.0);
    }
    REQUIRE (ons == 1);
    REQUIRE (endOfTrackTick (clipped) == Catch::Approx (3840.0));
}

TEST_CASE ("MidiExport: written file keeps the length", "[MidiExport]")
{
    auto f = writeTempMidi ({ { 0.0, 1.0, 60, 100 } }, 100.0, "CbLengthTest", 8.0);
    juce::FileInputStream in (f);
    REQUIRE (in.openedOk());
    juce::MidiFile rf;
    REQUIRE (rf.readFrom (in));
    REQUIRE (rf.getLastTimestamp() == Catch::Approx (8.0 * 960.0));
    f.deleteFile();
}
