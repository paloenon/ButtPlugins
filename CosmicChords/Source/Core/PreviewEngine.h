#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Core/Patterns.h"

namespace cb
{
// Real-time preview of note sequences / auditioned chords as MIDI (channel 1).
// Message-thread API: setSequence, play, stop, auditionOn/Off, allNotesOff.
// Audio-thread API: process, processBypassed, requestStop. isPlaying/playheadBeat/requestCount: any thread.
class PreviewEngine
{
public:
    using Sequence = std::vector<NoteEvent>;

    static constexpr int fifoSize = 64;
    static constexpr int maxCommandNotes = 16;
    static constexpr int auditionVelocity = 100;

    PreviewEngine();

    // Test hook: number of compiled sequences destroyed so far (process-wide).
    static int compiledDestroyedCount();

    void prepare (double sampleRate);

    // ---- message thread ----
    // Swaps at the next block; playhead kept (mod new length). While playing, the swap is seamless
    // (R16): pitches sounding and active in the new sequence at the playhead keep sounding, pitches
    // only the old one had get note-offs, pitches only the new one has start at the block start.
    void setSequence (std::shared_ptr<const Sequence>, double lengthBeats, bool loop);
    void play();             // always from beat 0
    void stop();             // note-offs next block, playhead 0
    void auditionOn (const std::vector<int>& notes);   // velocity 100, max 16 notes
    void auditionOff();
    void allNotesOff();      // releases sequence + audition notes and stops playback

    bool isPlaying() const   { return playingFlag.load(); }
    // Generation of the most recently published sequence (each setSequence increments it). Lets an
    // owner of a temporary sequence notice that someone else replaced it. Message thread.
    uint64_t sequenceGeneration() const { return generationCounter.load(); }
    double playheadBeat() const { return playheadFlag.load(); }
    // Incremented by every play() / auditionOn(): lets the UI notice that something should sound
    // (and warn when the host is not calling process). Any thread.
    uint64_t requestCount() const { return requestCounter.load(); }

    // ---- audio thread ----
    void process (juce::MidiBuffer& midi, int numSamples, std::optional<double> hostBpm);
    void processBypassed (juce::MidiBuffer& midi);   // emits note-offs for everything sounding, then silent
    // Like stop(), but callable from the audio thread (no FIFO, lock-free): takes effect at the start
    // of the next process()/processBypassed(), which may be the one in the same block.
    void requestStop() { pendingStop.store (true); }

private:
    struct TimedEvent { double beat; int note; int velocity; bool isOff; };

    // Built on the message thread; immutable once published.
    struct Compiled
    {
        ~Compiled();
        std::vector<TimedEvent> events;   // sorted by beat, offs before ons at equal beats
        double lengthBeats = 0.0;
        bool loop = false;
        uint64_t generation = 0;
    };

    enum class CommandType : uint8_t { Play, Stop, AuditionOn, AuditionOff, AllNotesOff };
    struct Command
    {
        CommandType type = CommandType::Stop;
        int numNotes = 0;
        std::array<uint8_t, maxCommandNotes> notes {};
    };

    enum Source { seqSource = 0, auditionSource = 1 };

    void push (const Command&);
    void drainCommands (juce::MidiBuffer&, bool bypassed);
    void handleCommand (const Command&, juce::MidiBuffer&, bool bypassed);
    void applyPendingReleases (juce::MidiBuffer&);
    void noteOn (Source, int note, int velocity, juce::MidiBuffer&, int sample);
    void noteOff (Source, int note, juce::MidiBuffer&, int sample);   // releases one hold
    void releaseSource (Source, juce::MidiBuffer&, int sample);       // releases all holds of a source
    void releaseAll (juce::MidiBuffer&, int sample);
    void stopTransport (juce::MidiBuffer&);
    const Compiled* loadSequence();   // audio thread: loads + acknowledges the newest sequence
    void seek (const Compiled&, juce::MidiBuffer&, double samplesPerBeat);
    void render (const Compiled&, juce::MidiBuffer&, int numSamples, double samplesPerBeat);

    // shared. The audio thread only reads through this pointer; it never owns or frees a sequence.
    std::atomic<const Compiled*> pending { nullptr };
    std::atomic<uint64_t> generationCounter { 0 }, requestCounter { 0 };
    std::atomic<uint64_t> acknowledgedGeneration { 0 }; // generation the audio thread last loaded
    std::atomic<bool> playingFlag { false };
    std::atomic<double> playheadFlag { 0.0 };
    // AbstractFifo of capacity N holds N-1 items, so +1 gives 64 usable commands.
    juce::AbstractFifo fifo { fifoSize + 1 };
    std::array<Command, fifoSize + 1> commands {};
    // Fallbacks when the FIFO is full: releases must never be lost.
    std::atomic<bool> pendingStop { false }, pendingAuditionOff { false }, pendingAllOff { false };

    // message thread only: owns every published sequence the audio thread might still read (the
    // current one included). Pruned (destroyed here, on the message thread) once older than the
    // acknowledged generation.
    std::vector<std::unique_ptr<const Compiled>> published;

    // audio thread only
    double sampleRate = 44100.0;
    uint64_t activeGeneration = 0;
    bool playing = false;
    double playhead = 0.0;
    size_t cursor = 0;
    bool needsSeek = true;
    // Per-pitch hold counts per source; output note-off only when the total reaches 0.
    std::array<std::array<uint8_t, 128>, 2> holds {};
    // seek scratch: per-pitch count / velocity of the sequence's notes active at the playhead
    std::array<uint8_t, 128> activeCount {}, activeVelocity {};
};
} // namespace cb
