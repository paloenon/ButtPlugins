#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs
{
    const juce::String drive      { "drive" };
    const juce::String bits       { "bits" };
    const juce::String downsample { "downsample" };
    const juce::String tone       { "tone" };
    const juce::String mix        { "mix" };
    const juce::String output     { "output" };
}

//==============================================================================
ButtcrusherAudioProcessor::ButtcrusherAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    driveParam      = apvts.getRawParameterValue (ParamIDs::drive);
    bitsParam       = apvts.getRawParameterValue (ParamIDs::bits);
    downsampleParam = apvts.getRawParameterValue (ParamIDs::downsample);
    toneParam       = apvts.getRawParameterValue (ParamIDs::tone);
    mixParam        = apvts.getRawParameterValue (ParamIDs::mix);
    outputParam     = apvts.getRawParameterValue (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout ButtcrusherAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::bits, 1 }, "Bits",
        NormalisableRange<float> (1.0f, 16.0f, 0.01f, 0.6f), 8.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::downsample, 1 }, "Downsample",
        NormalisableRange<float> (1.0f, 50.0f, 0.01f, 0.4f), 1.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return "1/" + String (v, 1); })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::tone, 1 }, "Tone",
        NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.3f), 20000.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
            {
                return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String ((int) v) + " Hz";
            })));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
void ButtcrusherAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto numChannels = (size_t) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());

    holdValue.assign (numChannels, 0.0f);
    holdPhase.assign (numChannels, 1.0f);   // forces a fresh sample on the first frame

    driveGain   .reset (sampleRate, 0.02);
    bitsSmoothed.reset (sampleRate, 0.02);
    mixSmoothed .reset (sampleRate, 0.02);
    outputGain  .reset (sampleRate, 0.02);

    driveGain   .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (driveParam->load()));
    bitsSmoothed.setCurrentAndTargetValue (bitsParam->load());
    mixSmoothed .setCurrentAndTargetValue (mixParam->load() / 100.0f);
    outputGain  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };
    toneFilter.prepare (spec);
    lastToneHz = -1.0f;
    updateToneFilter (toneParam->load());
    toneFilter.reset();

    dryBuffer.setSize ((int) numChannels, samplesPerBlock);
}

bool ButtcrusherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void ButtcrusherAudioProcessor::updateToneFilter (float cutoffHz)
{
    // Keep the cutoff safely below Nyquist
    cutoffHz = juce::jmin (cutoffHz, (float) (currentSampleRate * 0.45));
    if (std::abs (cutoffHz - lastToneHz) < 0.5f)
        return;

    lastToneHz = cutoffHz;
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, cutoffHz);
}

void ButtcrusherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numInputs  = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numInputs; ch < numOutputs; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = juce::jmin (numOutputs, (int) holdValue.size());

    // Keep a copy of the dry signal for the Mix knob
    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (dryBuffer.getNumChannels(), numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    driveGain   .setTargetValue (juce::Decibels::decibelsToGain (driveParam->load()));
    bitsSmoothed.setTargetValue (bitsParam->load());

    // Downsample step: how far the hold "phase" advances per sample.
    // 1.0 = take every sample, 0.1 = take every 10th sample, etc.
    const float step = 1.0f / juce::jmax (1.0f, downsampleParam->load());

    // --- Crush: drive -> sample-rate reduction -> bit-depth reduction ---
    for (int i = 0; i < numSamples; ++i)
    {
        const float drive = driveGain.getNextValue();
        const float bits  = bitsSmoothed.getNextValue();

        // Number of quantization levels across the -1..+1 range (2^bits).
        // Using pow() with a float bit depth lets the knob sweep smoothly.
        const float levels = std::pow (2.0f, bits);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            holdPhase[(size_t) ch] += step;
            if (holdPhase[(size_t) ch] >= 1.0f)
            {
                holdPhase[(size_t) ch] -= 1.0f;

                // Drive, then hard-clip at full scale (transparent below 0 dBFS,
                // gritty digital clipping when you push the Drive knob)
                float x = juce::jlimit (-1.0f, 1.0f, data[i] * drive);

                // Quantize to the reduced bit depth (mid-rise quantizer:
                // map -1..1 to 0..levels, pick a step, map back to its centre)
                float idx = std::floor ((x * 0.5f + 0.5f) * levels);
                idx = juce::jlimit (0.0f, levels - 1.0f, idx);
                x = ((idx + 0.5f) / levels) * 2.0f - 1.0f;

                holdValue[(size_t) ch] = x;
            }

            data[i] = holdValue[(size_t) ch];
        }
    }

    // --- Tone: low-pass the crushed signal to tame the harsh aliasing ---
    updateToneFilter (toneParam->load());
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        toneFilter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // --- Mix + output gain ---
    mixSmoothed.setTargetValue (mixParam->load() / 100.0f);
    outputGain .setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float out = outputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            wet[i] = (dry[i] * (1.0f - mix) + wet[i] * mix) * out;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ButtcrusherAudioProcessor::createEditor()
{
    return new ButtcrusherAudioProcessorEditor (*this);
}

void ButtcrusherAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ButtcrusherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ButtcrusherAudioProcessor();
}
