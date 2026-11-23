#pragma once
#include "PluginProcessor.h"

class ChordButtEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChordButtEditor (ChordButtProcessor&);
    ~ChordButtEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordButtEditor)
};
