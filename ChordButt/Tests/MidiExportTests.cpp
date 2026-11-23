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
    REQUIRE (f.getParentDirectory().getFileName() == "ChordButt");

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
    REQUIRE (f.getFileName() == "ChordButt.mid");
    REQUIRE (f.existsAsFile());
    f.deleteFile();
}
