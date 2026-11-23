#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Core/PreviewEngine.h"

using namespace cb;

namespace
{
constexpr int blockSize = 512;
constexpr double sr = 48000.0;   // BPM 120 -> 24000 samples per beat

struct Ev { long pos; bool on; int note; int vel; int ch; };

struct Rig
{
    PreviewEngine eng;
    long block = 0;
    std::vector<Ev> events;
    std::optional<double> bpm = 120.0;

    Rig() { eng.prepare (sr); }

    void run (int numBlocks)
    {
        for (int i = 0; i < numBlocks; ++i)
        {
            juce::MidiBuffer buf;
            eng.process (buf, blockSize, bpm);
            collect (buf);
            ++block;
        }
    }

    void collect (const juce::MidiBuffer& buf)
    {
        for (const auto meta : buf)
        {
            auto m = meta.getMessage();
            if (m.isNoteOn())
                events.push_back ({ block * blockSize + meta.samplePosition, true, m.getNoteNumber(), m.getVelocity(), m.getChannel() });
            else if (m.isNoteOff())
                events.push_back ({ block * blockSize + meta.samplePosition, false, m.getNoteNumber(), 0, m.getChannel() });
        }
    }

    void runUntil (long samplePos) { while (block * blockSize < samplePos) run (1); }

    static std::shared_ptr<const PreviewEngine::Sequence> seq (PreviewEngine::Sequence s)
    {
        return std::make_shared<const PreviewEngine::Sequence> (std::move (s));
    }
};
} // namespace

TEST_CASE ("PreviewEngine: single note on at 0, off at 24000 across blocks", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 4.0, false);
    r.eng.play();
    REQUIRE (r.eng.isPlaying());
    r.runUntil (30000);

    REQUIRE (r.events.size() == 2);
    REQUIRE (r.events[0].on);
    REQUIRE (r.events[0].pos == 0);
    REQUIRE (r.events[0].note == 60);
    REQUIRE (r.events[0].vel == 100);
    REQUIRE (r.events[0].ch == 1);
    REQUIRE_FALSE (r.events[1].on);
    REQUIRE (r.events[1].pos == 24000);
    REQUIRE (r.events[1].note == 60);
    REQUIRE (r.events[1].ch == 1);
    REQUIRE (r.eng.playheadBeat() == Catch::Approx (r.block * blockSize / 24000.0));
}

TEST_CASE ("PreviewEngine: loop of 2 beats retriggers at 48000 and 96000", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 2.0, true);
    r.eng.play();
    r.runUntil (100000);

    std::vector<long> ons, offs;
    for (auto& e : r.events)
        (e.on ? ons : offs).push_back (e.pos);
    REQUIRE (ons == std::vector<long> { 0, 48000, 96000 });
    REQUIRE (offs == std::vector<long> { 24000, 72000 });
    REQUIRE (r.eng.isPlaying());
}

TEST_CASE ("PreviewEngine: loop wrap emits note-off before note-on at the same sample", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 2.0, 60, 90 }, { 0.0, 2.0, 64, 90 } }), 2.0, true);
    r.eng.play();
    r.runUntil (50000);

    REQUIRE (r.events.size() == 6);
    REQUIRE ((r.events[0].on && r.events[1].on && r.events[0].pos == 0 && r.events[1].pos == 0));
    REQUIRE ((! r.events[2].on && ! r.events[3].on));
    REQUIRE ((r.events[2].pos == 48000 && r.events[3].pos == 48000));
    REQUIRE ((r.events[4].on && r.events[5].on));
    REQUIRE ((r.events[4].pos == 48000 && r.events[5].pos == 48000));
}

TEST_CASE ("PreviewEngine: loop wrap inside one block is sample accurate", "[PreviewEngine]")
{
    // length 0.5 beat = 12000 samples; 12000 is not a multiple of 512, so the wrap falls mid-block
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 0.25, 60, 100 }, { 0.25, 0.25, 62, 100 } }), 0.5, true);
    r.eng.play();
    r.runUntil (24100);

    std::vector<std::pair<long, int>> ons;
    for (auto& e : r.events)
        if (e.on)
            ons.push_back ({ e.pos, e.note });
    REQUIRE (ons == std::vector<std::pair<long, int>> { { 0, 60 }, { 6000, 62 }, { 12000, 60 }, { 18000, 62 }, { 24000, 60 } });
}

TEST_CASE ("PreviewEngine: non-looping sequence stops at its end", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 }, { 1.0, 1.0, 62, 100 } }), 2.0, false);
    r.eng.play();
    r.runUntil (200000);

    REQUIRE (r.events.size() == 4);
    REQUIRE (r.events[3].pos == 48000);
    REQUIRE_FALSE (r.events[3].on);
    REQUIRE_FALSE (r.eng.isPlaying());
    REQUIRE (r.eng.playheadBeat() == 0.0);
}

TEST_CASE ("PreviewEngine: non-looping end releases notes still sounding", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 8.0, 60, 100 } }), 2.0, false);   // note longer than sequence
    r.eng.play();
    r.runUntil (100000);

    REQUIRE (r.events.size() == 2);
    REQUIRE_FALSE (r.events[1].on);
    REQUIRE (r.events[1].pos == 48000);
    REQUIRE_FALSE (r.eng.isPlaying());
}

TEST_CASE ("PreviewEngine: stop releases every sounding note in the next block", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 4.0, 60, 100 }, { 0.0, 4.0, 64, 100 }, { 0.0, 4.0, 67, 100 } }), 4.0, true);
    r.eng.play();
    r.run (10);
    REQUIRE (r.events.size() == 3);

    r.eng.stop();
    REQUIRE_FALSE (r.eng.isPlaying());
    const long stopBlockStart = r.block * blockSize;
    r.run (200);   // well past the sequence's own note-offs

    REQUIRE (r.events.size() == 6);
    for (size_t i = 3; i < 6; ++i)
    {
        REQUIRE_FALSE (r.events[i].on);
        REQUIRE (r.events[i].pos == stopBlockStart);
    }
    REQUIRE (r.eng.playheadBeat() == 0.0);
    REQUIRE_FALSE (r.eng.isPlaying());
}

TEST_CASE ("PreviewEngine: play restarts from beat 0", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 4.0, 60, 100 } }), 4.0, true);
    r.eng.play();
    r.run (10);
    r.eng.play();
    const long restart = r.block * blockSize;
    r.run (1);

    REQUIRE (r.events.size() == 3);
    REQUIRE ((! r.events[1].on && r.events[1].pos == restart));
    REQUIRE ((r.events[2].on && r.events[2].pos == restart));
    REQUIRE (r.eng.playheadBeat() == Catch::Approx (512.0 / 24000.0));
}

TEST_CASE ("PreviewEngine: setSequence while playing releases old notes without retriggering new ones", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 4.0, 60, 100 } }), 4.0, true);
    r.eng.play();
    r.run (10);
    REQUIRE (r.events.size() == 1);

    r.eng.setSequence (Rig::seq ({ { 0.0, 4.0, 64, 100 } }), 4.0, true);
    const long swapAt = r.block * blockSize;
    r.runUntil (100000);

    REQUIRE (r.events.size() == 3);
    REQUIRE ((! r.events[1].on && r.events[1].note == 60 && r.events[1].pos == swapAt));
    // 64 is not started mid-note; it starts at its next note-on (loop to beat 0 = 96000)
    REQUIRE ((r.events[2].on && r.events[2].note == 64 && r.events[2].pos == 96000));
}

TEST_CASE ("PreviewEngine: setSequence keeps playhead modulo the new length", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 4.0, true);
    r.eng.play();
    r.run (150);                                     // 76800 samples = 3.2 beats
    r.eng.setSequence (Rig::seq ({ { 1.0, 0.5, 62, 100 } }), 3.0, true);
    r.run (1);
    REQUIRE (r.eng.playheadBeat() == Catch::Approx (0.2 + 512.0 / 24000.0));   // 3.2 mod 3

    // next note-on of 62 at beat 1 of the new sequence: 76800 + 0.8 beats = 96000
    r.runUntil (100000);
    long on62 = -1;
    for (auto& e : r.events)
        if (e.on && e.note == 62) { on62 = e.pos; break; }
    REQUIRE (on62 == 96000);
}

TEST_CASE ("PreviewEngine: audition on then off", "[PreviewEngine]")
{
    Rig r;
    r.eng.auditionOn ({ 60, 64 });
    r.run (3);
    r.eng.auditionOff();
    const long offAt = r.block * blockSize;
    r.run (3);

    REQUIRE (r.events.size() == 4);
    REQUIRE ((r.events[0].on && r.events[1].on));
    REQUIRE ((r.events[0].pos == 0 && r.events[1].pos == 0));
    REQUIRE ((r.events[0].vel == 100 && r.events[1].vel == 100));
    REQUIRE (r.events[0].note + r.events[1].note == 124);
    REQUIRE ((! r.events[2].on && ! r.events[3].on));
    REQUIRE ((r.events[2].pos == offAt && r.events[3].pos == offAt));
    REQUIRE (r.events[2].note + r.events[3].note == 124);
    REQUIRE (r.events[0].ch == 1);
}

TEST_CASE ("PreviewEngine: second auditionOn releases previous audition notes first", "[PreviewEngine]")
{
    Rig r;
    r.eng.auditionOn ({ 60, 64 });
    r.run (1);
    r.eng.auditionOn ({ 62 });
    r.run (1);

    REQUIRE (r.events.size() == 5);
    REQUIRE ((! r.events[2].on && ! r.events[3].on));
    REQUIRE ((r.events[4].on && r.events[4].note == 62 && r.events[4].pos == 512));
}

TEST_CASE ("PreviewEngine: auditionOn ignores notes beyond 16", "[PreviewEngine]")
{
    Rig r;
    std::vector<int> many;
    for (int n = 40; n < 60; ++n)
        many.push_back (n);
    r.eng.auditionOn (many);
    r.run (1);
    REQUIRE (r.events.size() == 16);
}

TEST_CASE ("PreviewEngine: audition release does not cut a sounding sequence note and vice versa", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 4.0, false);
    r.eng.play();
    r.run (2);
    r.eng.auditionOn ({ 60 });
    r.run (2);
    r.eng.auditionOff();
    r.run (2);

    for (auto& e : r.events)
        REQUIRE (e.on);   // no note-off yet: sequence still holds 60

    r.runUntil (30000);
    REQUIRE ((! r.events.back().on && r.events.back().pos == 24000));

    // vice versa: audition holds 62 while the sequence's 62 ends
    Rig r2;
    r2.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 62, 100 } }), 4.0, false);
    r2.eng.play();
    r2.run (2);
    r2.eng.auditionOn ({ 62 });
    r2.runUntil (30000);
    for (auto& e : r2.events)
        REQUIRE (e.on);
    r2.eng.auditionOff();
    const long offAt = r2.block * blockSize;
    r2.run (1);
    REQUIRE ((! r2.events.back().on && r2.events.back().pos == offAt));
}

TEST_CASE ("PreviewEngine: incoming MIDI is passed through untouched", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 4.0, false);
    r.eng.play();

    juce::MidiBuffer buf;
    buf.addEvent (juce::MidiMessage::noteOn (3, 72, (juce::uint8) 50), 10);
    buf.addEvent (juce::MidiMessage::controllerEvent (3, 1, 64), 100);
    r.eng.process (buf, blockSize, 120.0);

    int count = 0;
    bool sawIn = false, sawCc = false, sawGen = false;
    for (const auto meta : buf)
    {
        auto m = meta.getMessage();
        ++count;
        if (m.isNoteOn() && m.getNoteNumber() == 72 && m.getChannel() == 3 && meta.samplePosition == 10 && m.getVelocity() == 50) sawIn = true;
        if (m.isController() && m.getChannel() == 3 && meta.samplePosition == 100) sawCc = true;
        if (m.isNoteOn() && m.getNoteNumber() == 60 && m.getChannel() == 1 && meta.samplePosition == 0) sawGen = true;
    }
    REQUIRE (count == 3);
    REQUIRE (sawIn);
    REQUIRE (sawCc);
    REQUIRE (sawGen);
}

TEST_CASE ("PreviewEngine: host tempo and 120 BPM fallback", "[PreviewEngine]")
{
    Rig r;
    r.bpm = 60.0;   // 48000 samples per beat
    r.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 4.0, false);
    r.eng.play();
    r.runUntil (50000);
    REQUIRE (r.events.size() == 2);
    REQUIRE (r.events[1].pos == 48000);

    Rig r2;
    r2.bpm = std::nullopt;   // fallback 120
    r2.eng.setSequence (Rig::seq ({ { 0.0, 1.0, 60, 100 } }), 4.0, false);
    r2.eng.play();
    r2.runUntil (30000);
    REQUIRE (r2.events.size() == 2);
    REQUIRE (r2.events[1].pos == 24000);
}

TEST_CASE ("PreviewEngine: allNotesOff releases both sources and stops", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 4.0, 60, 100 } }), 4.0, true);
    r.eng.play();
    r.eng.auditionOn ({ 64 });
    r.run (2);
    REQUIRE (r.events.size() == 2);

    r.eng.allNotesOff();
    const long at = r.block * blockSize;
    r.run (300);
    REQUIRE (r.events.size() == 4);
    REQUIRE ((! r.events[2].on && ! r.events[3].on));
    REQUIRE ((r.events[2].pos == at && r.events[3].pos == at));
    REQUIRE_FALSE (r.eng.isPlaying());
}

TEST_CASE ("PreviewEngine: processBypassed releases everything once, then stays silent", "[PreviewEngine]")
{
    Rig r;
    r.eng.setSequence (Rig::seq ({ { 0.0, 4.0, 60, 100 } }), 4.0, true);
    r.eng.play();
    r.eng.auditionOn ({ 64 });
    r.run (2);

    juce::MidiBuffer b1;
    b1.addEvent (juce::MidiMessage::noteOn (2, 50, (juce::uint8) 80), 5);
    r.eng.processBypassed (b1);
    int offs = 0, passthrough = 0;
    for (const auto meta : b1)
    {
        auto m = meta.getMessage();
        if (m.isNoteOff() && m.getChannel() == 1 && meta.samplePosition == 0) ++offs;
        if (m.isNoteOn() && m.getChannel() == 2) ++passthrough;
    }
    REQUIRE (offs == 2);
    REQUIRE (passthrough == 1);

    for (int i = 0; i < 300; ++i)
    {
        juce::MidiBuffer b;
        r.eng.processBypassed (b);
        REQUIRE (b.isEmpty());
    }
}
