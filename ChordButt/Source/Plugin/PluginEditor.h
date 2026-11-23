#pragma once
#include "PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"
#include "UI/ChordStrip.h"
#include "UI/HeaderBar.h"
#include "UI/PadGrid.h"
#include "UI/StylePanel.h"

class ChordButtEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::ChangeListener
{
public:
    static constexpr int editorWidth = 900, editorHeight = 560;

    explicit ChordButtEditor (ChordButtProcessor&);
    ~ChordButtEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // Ticks every descendant implementing Animated and repaints those that changed.
    // Driven by the VBlank callback; callable directly for deterministic snapshots.
    void advanceAnimations (float dtSeconds);

    // Re-reads palette, strip and voicing state from the processor (also runs on stateBroadcaster).
    void refreshAll();

    // Pads + knobs + style panel: the area the library drawer covers (the strip stays visible).
    juce::Rectangle<int> drawerArea() const;

    // External MIDI-file drag of the whole strip or a single slot (see ChordStrip).
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

    HeaderBar& getHeader() { return header; }
    PadGrid& getPadGrid() { return padGrid; }
    VoicingPanel& getVoicingPanel() { return voicingPanel; }
    StylePanel& getStylePanel() { return stylePanel; }
    ChordStrip& getChordStrip() { return chordStrip; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    ChordButtProcessor& chordProcessor;
    ChordLookAndFeel lookAndFeel;
    HeaderBar header;
    PadGrid padGrid;
    VoicingPanel voicingPanel;
    StylePanel stylePanel;
    ChordStrip chordStrip;
    double lastFrameTime = -1.0;      // declared before vblank, whose callback uses it
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordButtEditor)
};
