#include "Core/PreviewEngine.h"
#include "Core/MidiExport.h"
#include <algorithm>
#include <cmath>

namespace cb
{
namespace
{
constexpr int midiChannel = 1;
constexpr double sampleEpsilon = 1.0e-6;   // tolerance (in samples) for beat->sample rounding

std::atomic<int> compiledDestroyed { 0 };

// Sample offset (relative to the sample where the playhead is at beat `fromBeat`) at which an
// event at `beat` fires. Using ceil keeps every event in exactly one block, even across block
// boundaries, because the offset shifts by exactly numSamples from one block to the next.
int offsetFor (double beat, double fromBeat, double samplesPerBeat)
{
    return (int) std::ceil ((beat - fromBeat) * samplesPerBeat - sampleEpsilon);
}
} // namespace

PreviewEngine::Compiled::~Compiled() { ++compiledDestroyed; }

PreviewEngine::PreviewEngine() = default;

int PreviewEngine::compiledDestroyedCount() { return compiledDestroyed.load(); }

void PreviewEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
}

//==============================================================================
// message thread

void PreviewEngine::setSequence (std::shared_ptr<const Sequence> sequence, double lengthBeats, bool loop)
{
    auto compiled = std::make_unique<Compiled>();
    compiled->lengthBeats = lengthBeats > 0.0 ? lengthBeats : 0.0;
    compiled->loop = loop;
    compiled->generation = ++generationCounter;

    if (sequence != nullptr && compiled->lengthBeats > 0.0)
    {
        compiled->events.reserve (sequence->size() * 2);
        for (const auto& e : *sequence)
        {
            if (e.note < 0 || e.note > 127)
                continue;
            const double start = std::max (0.0, e.startBeat);
            const double end = std::min (e.startBeat + e.lengthBeats, compiled->lengthBeats);
            if (start >= compiled->lengthBeats || end <= start)
                continue;
            const int vel = juce::jlimit (1, 127, e.velocity);
            compiled->events.push_back ({ start, e.note, vel, false });
            compiled->events.push_back ({ end, e.note, 0, true });
        }
        std::stable_sort (compiled->events.begin(), compiled->events.end(),
                          [] (const TimedEvent& a, const TimedEvent& b)
                          {
                              if (a.beat < b.beat) return true;
                              if (b.beat < a.beat) return false;
                              return a.isOff && ! b.isOff;   // offs before ons at equal beats
                          });
    }

    // Publish the new sequence (kept alive in `published`), then destroy (here, on the message
    // thread) every sequence older than the generation the audio thread has acknowledged. The
    // audio thread only reads the sequence it loaded during one process call and acknowledges its
    // generation right after loading it, so it never reads an older one again; it frees nothing.
    pending.store (compiled.get());
    published.push_back (std::move (compiled));

    const auto ack = acknowledgedGeneration.load();
    published.erase (std::remove_if (published.begin(), published.end(),
                                     [ack] (const std::unique_ptr<const Compiled>& c) { return c->generation < ack; }),
                     published.end());
}

void PreviewEngine::play()
{
    ++requestCounter;
    playingFlag.store (true);
    playheadFlag.store (0.0);
    push ({ CommandType::Play, 0, {} });
}

void PreviewEngine::stop()
{
    playingFlag.store (false);
    playheadFlag.store (0.0);
    push ({ CommandType::Stop, 0, {} });
}

void PreviewEngine::auditionOn (const std::vector<int>& notes)
{
    ++requestCounter;
    Command c { CommandType::AuditionOn, 0, {} };
    for (int n : notes)
    {
        if (c.numNotes >= maxCommandNotes)
            break;
        if (n >= 0 && n <= 127)
            c.notes[(size_t) c.numNotes++] = (uint8_t) n;
    }
    push (c);
}

void PreviewEngine::auditionOff()  { push ({ CommandType::AuditionOff, 0, {} }); }

void PreviewEngine::allNotesOff()
{
    playingFlag.store (false);
    playheadFlag.store (0.0);
    push ({ CommandType::AllNotesOff, 0, {} });
}

void PreviewEngine::push (const Command& c)
{
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
        {
            commands[(size_t) scope.startIndex1] = c;
            return;
        }
        if (scope.blockSize2 > 0)
        {
            commands[(size_t) scope.startIndex2] = c;
            return;
        }
    }

    // FIFO full: releases must still happen (play / audition-on are dropped).
    switch (c.type)
    {
        case CommandType::Stop:        pendingStop.store (true); break;
        case CommandType::AuditionOff: pendingAuditionOff.store (true); break;
        case CommandType::AllNotesOff: pendingAllOff.store (true); break;
        case CommandType::Play:
        case CommandType::AuditionOn:  break;
    }
}

//==============================================================================
// audio thread: note bookkeeping (per-pitch hold counts per source)

void PreviewEngine::noteOn (Source src, int note, int velocity, juce::MidiBuffer& midi, int sample)
{
    const auto n = (size_t) note;
    if (holds[0][n] + holds[1][n] > 0)   // already sounding: retrigger (off, then on)
        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, note), sample);
    midi.addEvent (juce::MidiMessage::noteOn (midiChannel, note, (juce::uint8) velocity), sample);
    if (holds[(size_t) src][n] < 255)
        ++holds[(size_t) src][n];
}

void PreviewEngine::noteOff (Source src, int note, juce::MidiBuffer& midi, int sample)
{
    const auto n = (size_t) note;
    if (holds[(size_t) src][n] == 0)
        return;   // not held by this source (e.g. a note of a swapped-in sequence that never started)
    --holds[(size_t) src][n];
    if (holds[0][n] + holds[1][n] == 0)
        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, note), sample);
}

void PreviewEngine::releaseSource (Source src, juce::MidiBuffer& midi, int sample)
{
    const auto other = (size_t) (src == seqSource ? auditionSource : seqSource);
    for (size_t n = 0; n < 128; ++n)
    {
        if (holds[(size_t) src][n] == 0)
            continue;
        holds[(size_t) src][n] = 0;
        if (holds[other][n] == 0)
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, (int) n), sample);
    }
}

void PreviewEngine::releaseAll (juce::MidiBuffer& midi, int sample)
{
    for (size_t n = 0; n < 128; ++n)
    {
        if (holds[0][n] + holds[1][n] == 0)
            continue;
        holds[0][n] = holds[1][n] = 0;
        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, (int) n), sample);
    }
}

void PreviewEngine::stopTransport (juce::MidiBuffer& midi)
{
    releaseSource (seqSource, midi, 0);
    playing = false;
    playhead = 0.0;
    needsSeek = true;
}

//==============================================================================
// audio thread: commands

void PreviewEngine::handleCommand (const Command& c, juce::MidiBuffer& midi, bool bypassed)
{
    switch (c.type)
    {
        case CommandType::Play:
            stopTransport (midi);
            playing = true;
            break;

        case CommandType::Stop:
            stopTransport (midi);
            break;

        case CommandType::AuditionOn:
            releaseSource (auditionSource, midi, 0);
            if (bypassed)
                break;
            for (int i = 0; i < c.numNotes; ++i)
                noteOn (auditionSource, c.notes[(size_t) i], auditionVelocity, midi, 0);
            break;

        case CommandType::AuditionOff:
            releaseSource (auditionSource, midi, 0);
            break;

        case CommandType::AllNotesOff:
            releaseAll (midi, 0);
            stopTransport (midi);
            break;
    }
}

void PreviewEngine::drainCommands (juce::MidiBuffer& midi, bool bypassed)
{
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            handleCommand (commands[(size_t) (scope.startIndex1 + i)], midi, bypassed);
        for (int i = 0; i < scope.blockSize2; ++i)
            handleCommand (commands[(size_t) (scope.startIndex2 + i)], midi, bypassed);
    }
    applyPendingReleases (midi);
}

void PreviewEngine::applyPendingReleases (juce::MidiBuffer& midi)
{
    if (pendingAllOff.exchange (false))
    {
        releaseAll (midi, 0);
        stopTransport (midi);
    }
    if (pendingStop.exchange (false))
        stopTransport (midi);
    if (pendingAuditionOff.exchange (false))
        releaseSource (auditionSource, midi, 0);
}

//==============================================================================
// audio thread: rendering

void PreviewEngine::seek (const Compiled& seq, juce::MidiBuffer& midi, double samplesPerBeat)
{
    // Events before the playhead are skipped. Replaying them into per-pitch counters gives exactly
    // the holds the sequence would have if it had played from beat 0 (offs sort before ons).
    activeCount.fill (0);
    cursor = 0;
    while (cursor < seq.events.size() && offsetFor (seq.events[cursor].beat, playhead, samplesPerBeat) < 0)
    {
        const auto& e = seq.events[cursor++];
        auto& count = activeCount[(size_t) e.note];
        if (e.isOff)
        {
            if (count > 0)
                --count;
        }
        else if (count < 255)
        {
            ++count;
            activeVelocity[(size_t) e.note] = (uint8_t) e.velocity;
        }
    }
    needsSeek = false;

    // Reconcile what is sounding (from the previous sequence, or nothing) with what is active at the
    // playhead: keep common pitches without retriggering (their holds now belong to the new notes,
    // whose offs end them), end pitches that are no longer active, start the newly active ones.
    for (size_t n = 0; n < 128; ++n)
    {
        const auto held = holds[seqSource][n];
        const auto active = activeCount[n];
        if (held == active)
            continue;
        if (active == 0)
        {
            holds[seqSource][n] = 0;
            if (holds[auditionSource][n] == 0)
                midi.addEvent (juce::MidiMessage::noteOff (midiChannel, (int) n), 0);
        }
        else if (held == 0)
        {
            noteOn (seqSource, (int) n, activeVelocity[n], midi, 0);
            holds[seqSource][n] = active;
        }
        else
        {
            holds[seqSource][n] = active;
        }
    }
}

void PreviewEngine::render (const Compiled& seq, juce::MidiBuffer& midi, int numSamples, double samplesPerBeat)
{
    if (needsSeek)
        seek (seq, midi, samplesPerBeat);

    int segStart = 0;   // sample in this block where the playhead is at `playhead`
    int lastWrap = -1;
    for (;;)
    {
        // events of the current pass that fall inside this block
        while (cursor < seq.events.size())
        {
            const auto& e = seq.events[cursor];
            const int off = segStart + std::max (0, offsetFor (e.beat, playhead, samplesPerBeat));
            if (off >= numSamples)
                break;

            if (e.isOff)
                noteOff (seqSource, e.note, midi, off);
            else
                noteOn (seqSource, e.note, e.velocity, midi, off);
            ++cursor;
        }

        const int endOff = segStart + std::max (0, offsetFor (seq.lengthBeats, playhead, samplesPerBeat));
        if (endOff >= numSamples)
        {
            playhead += (double) (numSamples - segStart) / samplesPerBeat;
            return;
        }

        // end of the pass inside this block (all events up to lengthBeats already emitted)
        releaseSource (seqSource, midi, endOff);   // anything still held (normally nothing)
        playhead = 0.0;
        cursor = 0;

        if (! seq.loop)
        {
            playing = false;
            needsSeek = true;
            return;
        }

        if (endOff == lastWrap)
            return;   // degenerate pass shorter than one sample: resume next block
        lastWrap = endOff;
        segStart = endOff;
    }
}

const PreviewEngine::Compiled* PreviewEngine::loadSequence()
{
    // Acknowledging right after the load: the message thread may then destroy anything older,
    // which this thread will never read again.
    const auto* seq = pending.load();
    if (seq == nullptr)
        return nullptr;
    acknowledgedGeneration.store (seq->generation);
    if (seq->generation != activeGeneration)
    {
        activeGeneration = seq->generation;
        playhead = seq->lengthBeats > 0.0 ? std::fmod (playhead, seq->lengthBeats) : 0.0;
        needsSeek = true;   // seek() reconciles the sounding notes with the new sequence (R16)
    }
    return seq;
}

void PreviewEngine::process (juce::MidiBuffer& midi, int numSamples, std::optional<double> hostBpm)
{
    // 1. commands first, so setSequence(B) + play() is never started on the old sequence
    drainCommands (midi, false);

    // 2. current sequence
    const auto* seq = loadSequence();
    if (! playing)
        releaseSource (seqSource, midi, 0);   // nothing of a sequence sounds while stopped

    // 3. sequence playback
    if (playing)
    {
        if (seq == nullptr || seq->lengthBeats <= 0.0)
            stopTransport (midi);
        else if (numSamples > 0)
            render (*seq, midi, numSamples, sampleRate * 60.0 / effectiveBpm (hostBpm));
    }

    playingFlag.store (playing);
    playheadFlag.store (playhead);
}

void PreviewEngine::processBypassed (juce::MidiBuffer& midi)
{
    // Apply transport commands (audition note-ons are dropped), then release everything sounding.
    drainCommands (midi, true);
    loadSequence();   // acknowledge, so retired sequences do not pile up while bypassed
    releaseAll (midi, 0);
    needsSeek = true;

    playingFlag.store (playing);
    playheadFlag.store (playhead);
}
} // namespace cb
