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
    auto compiled = std::make_shared<Compiled>();
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

    std::shared_ptr<const Compiled> published = std::move (compiled);

    // Retire the outgoing sequence, publish the new one, then destroy (here, on the message
    // thread) every retired sequence older than the generation the audio thread has loaded.
    // The audio thread only ever holds the generation it acknowledged or a newer one, and it
    // stores the acknowledgement after dropping its previous copy, so it never frees one.
    if (auto outgoing = std::atomic_load (&pending))
        retired.push_back (std::move (outgoing));
    std::atomic_store (&pending, published);

    const auto ack = acknowledgedGeneration.load();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [ack] (const std::shared_ptr<const Compiled>& c) { return c->generation < ack; }),
                   retired.end());
}

void PreviewEngine::play()
{
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

void PreviewEngine::seek (const Compiled& seq, double samplesPerBeat)
{
    cursor = 0;
    while (cursor < seq.events.size() && offsetFor (seq.events[cursor].beat, playhead, samplesPerBeat) < 0)
        ++cursor;
    needsSeek = false;
}

void PreviewEngine::render (const Compiled& seq, juce::MidiBuffer& midi, int numSamples, double samplesPerBeat)
{
    if (needsSeek)
        seek (seq, samplesPerBeat);

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

void PreviewEngine::process (juce::MidiBuffer& midi, int numSamples, std::optional<double> hostBpm)
{
    // 1. commands first, so setSequence(B) + play() is never started on the old sequence
    drainCommands (midi, false);

    // 2. current sequence; acknowledge its generation (our previous copy is already released)
    const auto seq = std::atomic_load (&pending);
    if (seq != nullptr)
    {
        acknowledgedGeneration.store (seq->generation);
        if (seq->generation != activeGeneration)
        {
            activeGeneration = seq->generation;
            releaseSource (seqSource, midi, 0);
            playhead = seq->lengthBeats > 0.0 ? std::fmod (playhead, seq->lengthBeats) : 0.0;
            needsSeek = true;
        }
    }

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
    releaseAll (midi, 0);
    needsSeek = true;

    playingFlag.store (playing);
    playheadFlag.store (playhead);
}
} // namespace cb
