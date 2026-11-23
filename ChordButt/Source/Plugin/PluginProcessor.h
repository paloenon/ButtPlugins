#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/Library.h"
#include "Core/Palette.h"
#include "Core/PluginState.h"
#include "Core/PreviewEngine.h"
#include "Core/Strip.h"

class ChordButtProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    ChordButtProcessor();
    ~ChordButtProcessor() override;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChordButt"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- chord generator API (message thread unless noted) ----
    juce::AudioProcessorValueTreeState apvts;

    cb::Strip& strip() { return stripData; }
    void stripChanged();    // rebuilds the preview sequence immediately and notifies listeners

    std::vector<cb::PadChord> currentPalette() const;
    std::vector<cb::VoicedSlot> voicedStrip() const;
    std::vector<cb::NoteEvent> currentEvents() const;   // generateEvents (voicedStrip(), currentStyle())
    cb::StyleParams currentStyle() const;
    int tonic() const;
    cb::Scale scale() const;
    double hostBpm() const;                              // effectiveBpm of the last seen host tempo (any thread)
    // Incremented by every processBlock / processBlockBypassed (any thread): lets the UI notice that
    // the host is not processing (Logic: track neither selected nor record-enabled while stopped).
    uint32_t processHeartbeat() const { return heartbeat.load (std::memory_order_relaxed); }
    cb::PreviewEngine& preview() { return previewEngine; }
    void newHumanizeSeed();

    static const cb::Library& library();                 // bundled progressions, parsed once
    void loadLibraryEntry (const cb::LibraryEntry&);     // scale := entry scale, root kept, strip replaced

    juce::ChangeBroadcaster stateBroadcaster;            // fires when strip/params change

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    cb::ParamValues paramValues() const;
    void rebuildPreview();
    void parameterChanged (const juce::String&, float) override;   // may run on any thread
    void timerCallback() override;
    std::optional<double> readHostPosition();            // audio thread: tempo + transport start

    cb::Strip stripData;
    uint32_t seed = 1;
    cb::PreviewEngine previewEngine;
    std::atomic<bool> dirty { false };
    std::atomic<double> lastHostBpm { 0.0 };   // <= 0: none seen yet
    std::atomic<uint32_t> heartbeat { 0 };
    bool hostWasPlaying = false;               // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordButtProcessor)
};
