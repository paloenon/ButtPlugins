#pragma once
#include "PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"
#include "UI/HeaderBar.h"

class ChordButtEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int editorWidth = 900, editorHeight = 560;

    explicit ChordButtEditor (ChordButtProcessor&);
    ~ChordButtEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Ticks every descendant implementing Animated and repaints those that changed.
    // Driven by the VBlank callback; callable directly for deterministic snapshots.
    void advanceAnimations (float dtSeconds);

    HeaderBar& getHeader() { return header; }

private:
    ChordLookAndFeel lookAndFeel;
    HeaderBar header;
    juce::VBlankAttachment vblank;
    double lastFrameTime = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordButtEditor)
};
