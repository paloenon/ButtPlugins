#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Custom knob look: dark body, glowing value arc
class CrushLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrushLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
class ButtcrusherAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ButtcrusherAudioProcessorEditor (ButtcrusherAudioProcessor&);
    ~ButtcrusherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramID, const juce::String& name);

    ButtcrusherAudioProcessor& buttcrusherProcessor;
    CrushLookAndFeel lookAndFeel;

    Knob drive, bits, downsample, tone, mix, output;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButtcrusherAudioProcessorEditor)
};
