#pragma once
#include <array>
#include <atomic>
#include <bitset>
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
// Audio-thread API: process, processBypassed. isPlaying/playheadBeat: any thread.
class PreviewEngine
{
public:
    using Sequence = std::vector<NoteEvent>;

    static constexpr int fifoSize = 64;
    static constexpr int maxCommandNotes = 16;
    static constexpr int auditionVelocity = 100;

    PreviewEngine();

    void prepare (double sampleRate);

    // ---- message thread ----
    // Swaps at the next block; playhead kept (mod new length). Old sounding notes get note-offs.
    void setSequence (std::shared_ptr<const Sequence>, double lengthBeats, bool loop);
    void play();             // always from beat 0
    void stop();             // note-offs next block, playhead 0
    void auditionOn (const std::vector<int>& notes);   // velocity 100, max 16 notes
    void auditionOff();
    void allNotesOff();      // releases sequence + audition notes and stops playback

    bool isPlaying() const   { return playingFlag.load(); }
    double playheadBeat() const { return playheadFlag.load(); }

    // ---- audio thread ----
    void process (juce::MidiBuffer& midi, int numSamples, std::optional<double> hostBpm);
    void processBypassed (juce::MidiBuffer& midi);   // emits note-offs for everything sounding, then silent

private:
    struct TimedEvent { double beat; int note; int velocity; bool isOff; };

    // Built on the message thread; immutable once published.
    struct Compiled
    {
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

    void push (const Command&);
    void handleCommand (const Command&, juce::MidiBuffer&, bool bypassed);
    void releaseSequence (juce::MidiBuffer&, int sample);
    void releaseAudition (juce::MidiBuffer&, int sample);
    void releaseAll (juce::MidiBuffer&, int sample);
    void emitOn (juce::MidiBuffer&, int note, int velocity, int sample);
    void seek (const Compiled&, double samplesPerBeat);
    void render (const Compiled&, juce::MidiBuffer&, int numSamples, double samplesPerBeat);

    // shared
    std::shared_ptr<const Compiled> pending;            // atomic_store / atomic_load only
    std::atomic<uint64_t> generationCounter { 0 };
    std::atomic<bool> playingFlag { false };
    std::atomic<double> playheadFlag { 0.0 };
    juce::AbstractFifo fifo { fifoSize };
    std::array<Command, fifoSize> commands {};

    // message thread only: keeps the previously published sequence alive so the audio
    // thread is unlikely to drop the last reference (deallocate) on a swap.
    std::shared_ptr<const Compiled> retired;

    // audio thread only
    double sampleRate = 44100.0;
    uint64_t activeGeneration = 0;
    bool playing = false;
    double playhead = 0.0;
    size_t cursor = 0;
    bool needsSeek = true;
    std::bitset<128> seqSounding;
    std::bitset<128> auditionSounding;
};
} // namespace cb
