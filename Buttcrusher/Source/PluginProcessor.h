#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class ButtcrusherAudioProcessor : public juce::AudioProcessor
{
public:
    ButtcrusherAudioProcessor();
    ~ButtcrusherAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    // Raw parameter pointers (read on the audio thread)
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* bitsParam       = nullptr;
    std::atomic<float>* downsampleParam = nullptr;
    std::atomic<float>* toneParam       = nullptr;
    std::atomic<float>* mixParam        = nullptr;
    std::atomic<float>* outputParam     = nullptr;

    // Smoothed values to avoid zipper noise when knobs move
    juce::SmoothedValue<float> driveGain, bitsSmoothed, mixSmoothed, outputGain;

    // Sample-and-hold state, per channel
    std::vector<float> holdValue;
    std::vector<float> holdPhase;

    // Low-pass filter on the crushed signal ("Tone")
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> toneFilter;
    float lastToneHz = -1.0f;
    double currentSampleRate = 44100.0;

    juce::AudioBuffer<float> dryBuffer;

    void updateToneFilter (float cutoffHz);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButtcrusherAudioProcessor)
};
