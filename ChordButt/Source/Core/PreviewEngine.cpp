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

// Sample offset (relative to the sample where the playhead is at beat `fromBeat`) at which an
// event at `beat` fires. Using ceil keeps every event in exactly one block, even across block
// boundaries, because the offset shifts by exactly numSamples from one block to the next.
int offsetFor (double beat, double fromBeat, double samplesPerBeat)
{
    return (int) std::ceil ((beat - fromBeat) * samplesPerBeat - sampleEpsilon);
}
} // namespace

PreviewEngine::PreviewEngine() = default;

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
                              if (a.beat != b.beat)
                                  return a.beat < b.beat;
                              return a.isOff && ! b.isOff;   // offs before ons at equal beats
                          });
    }

    std::shared_ptr<const Compiled> published = std::move (compiled);
    // Keep the outgoing sequence alive here so the audio thread is unlikely to drop its last reference.
    retired = std::atomic_load (&pending);
    std::atomic_store (&pending, published);
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
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        commands[(size_t) scope.startIndex1] = c;
    else if (scope.blockSize2 > 0)
        commands[(size_t) scope.startIndex2] = c;
    // FIFO full: command dropped (64 pending commands means the audio thread is not running).
}

//==============================================================================
// audio thread

void PreviewEngine::emitOn (juce::MidiBuffer& midi, int note, int velocity, int sample)
{
    midi.addEvent (juce::MidiMessage::noteOn (midiChannel, note, (juce::uint8) velocity), sample);
}

void PreviewEngine::releaseSequence (juce::MidiBuffer& midi, int sample)
{
    for (int n = 0; n < 128; ++n)
    {
        if (! seqSounding[(size_t) n])
            continue;
        seqSounding[(size_t) n] = false;
        if (! auditionSounding[(size_t) n])
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, n), sample);
    }
}

void PreviewEngine::releaseAudition (juce::MidiBuffer& midi, int sample)
{
    for (int n = 0; n < 128; ++n)
    {
        if (! auditionSounding[(size_t) n])
            continue;
        auditionSounding[(size_t) n] = false;
        if (! seqSounding[(size_t) n])
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, n), sample);
    }
}

void PreviewEngine::releaseAll (juce::MidiBuffer& midi, int sample)
{
    const auto all = seqSounding | auditionSounding;
    seqSounding.reset();
    auditionSounding.reset();
    for (int n = 0; n < 128; ++n)
        if (all[(size_t) n])
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, n), sample);
}

void PreviewEngine::handleCommand (const Command& c, juce::MidiBuffer& midi, bool bypassed)
{
    switch (c.type)
    {
        case CommandType::Play:
            releaseSequence (midi, 0);
            playing = true;
            playhead = 0.0;
            needsSeek = true;
            break;

        case CommandType::Stop:
            releaseSequence (midi, 0);
            playing = false;
            playhead = 0.0;
            needsSeek = true;
            break;

        case CommandType::AuditionOn:
            releaseAudition (midi, 0);
            if (bypassed)
                break;
            for (int i = 0; i < c.numNotes; ++i)
            {
                const int n = c.notes[(size_t) i];
                emitOn (midi, n, auditionVelocity, 0);
                auditionSounding[(size_t) n] = true;
            }
            break;

        case CommandType::AuditionOff:
            releaseAudition (midi, 0);
            break;

        case CommandType::AllNotesOff:
            releaseAll (midi, 0);
            playing = false;
            playhead = 0.0;
            needsSeek = true;
            break;
    }
}

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

            const auto idx = (size_t) e.note;
            if (e.isOff)
            {
                if (seqSounding[idx])
                {
                    seqSounding[idx] = false;
                    if (! auditionSounding[idx])
                        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, e.note), off);
                }
            }
            else
            {
                emitOn (midi, e.note, e.velocity, off);
                seqSounding[idx] = true;
            }
            ++cursor;
        }

        const int endOff = segStart + std::max (0, offsetFor (seq.lengthBeats, playhead, samplesPerBeat));
        if (endOff >= numSamples)
        {
            playhead += (double) (numSamples - segStart) / samplesPerBeat;
            return;
        }

        // end of the pass inside this block (all events up to lengthBeats already emitted)
        releaseSequence (midi, endOff);   // anything still held (normally nothing)
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
    // 1. sequence swap
    const auto seq = std::atomic_load (&pending);
    if (seq != nullptr && seq->generation != activeGeneration)
    {
        activeGeneration = seq->generation;
        releaseSequence (midi, 0);
        playhead = seq->lengthBeats > 0.0 ? std::fmod (playhead, seq->lengthBeats) : 0.0;
        needsSeek = true;
    }

    // 2. commands
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            handleCommand (commands[(size_t) (scope.startIndex1 + i)], midi, false);
        for (int i = 0; i < scope.blockSize2; ++i)
            handleCommand (commands[(size_t) (scope.startIndex2 + i)], midi, false);
    }

    // 3. sequence playback
    if (playing)
    {
        if (seq == nullptr || seq->lengthBeats <= 0.0)
        {
            releaseSequence (midi, 0);
            playing = false;
            playhead = 0.0;
            needsSeek = true;
        }
        else if (numSamples > 0)
        {
            const double samplesPerBeat = sampleRate * 60.0 / effectiveBpm (hostBpm);
            render (*seq, midi, numSamples, samplesPerBeat);
        }
    }

    playingFlag.store (playing);
    playheadFlag.store (playhead);
}

void PreviewEngine::processBypassed (juce::MidiBuffer& midi)
{
    // Apply transport commands (audition note-ons are dropped), then release everything sounding.
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            handleCommand (commands[(size_t) (scope.startIndex1 + i)], midi, true);
        for (int i = 0; i < scope.blockSize2; ++i)
            handleCommand (commands[(size_t) (scope.startIndex2 + i)], midi, true);
    }

    releaseAll (midi, 0);
    needsSeek = true;

    playingFlag.store (playing);
    playheadFlag.store (playhead);
}
} // namespace cb
