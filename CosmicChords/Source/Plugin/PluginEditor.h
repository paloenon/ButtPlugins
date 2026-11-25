#pragma once
#include "PluginProcessor.h"
#include "Core/HostWatch.h"
#include "UI/HostHint.h"
#include "UI/ChordLookAndFeel.h"
#include "UI/ChordStrip.h"
#include "UI/HeaderBar.h"
#include "UI/LibraryDrawer.h"
#include "UI/PadGrid.h"
#include "UI/StylePanel.h"

class CosmicChordsEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::ChangeListener
{
public:
    static constexpr int editorWidth = 900, editorHeight = 560;

    explicit CosmicChordsEditor (CosmicChordsProcessor&);
    ~CosmicChordsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress&) override;   // Esc closes the library

    // Ticks every descendant implementing Animated and repaints those that changed.
    // Driven by the VBlank callback; callable directly for deterministic snapshots.
    void advanceAnimations (float dtSeconds);

    // Re-reads palette, strip and voicing state from the processor (also runs on stateBroadcaster).
    void refreshAll();

    // Pads + knobs + style panel: the area the library drawer covers (the strip stays visible).
    juce::Rectangle<int> drawerArea() const;

    // Opens/closes the library drawer and keeps the header's LIBRARY toggle in sync.
    void setLibraryOpen (bool open);

    // External MIDI-file drag of the whole strip or a single slot (see ChordStrip).
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

    HeaderBar& getHeader() { return header; }
    PadGrid& getPadGrid() { return padGrid; }
    VoicingPanel& getVoicingPanel() { return voicingPanel; }
    StylePanel& getStylePanel() { return stylePanel; }
    ChordStrip& getChordStrip() { return chordStrip; }
    LibraryDrawer& getLibraryDrawer() { return libraryDrawer; }
    HostHint& getHostHint() { return hostHint; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    CosmicChordsProcessor& chordProcessor;
    ChordLookAndFeel lookAndFeel;
    HeaderBar header;
    PadGrid padGrid;
    VoicingPanel voicingPanel;
    StylePanel stylePanel;
    ChordStrip chordStrip;
    LibraryDrawer libraryDrawer;      // above the pads and panels; never covers header or strip
    HostHint hostHint;                // over the header's free middle
    cb::HostWatch hostWatch;
    double lastFrameTime = -1.0;      // declared before vblank, whose callback uses it
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CosmicChordsEditor)
};
