#include "PluginEditor.h"
#include "UI/Theme.h"

namespace
{
    void tickTree (juce::Component& c, float dt)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* animated = dynamic_cast<Animated*> (child))
                if (animated->tickAnimations (dt))
                    child->repaint();

            tickTree (*child, dt);
        }
    }

    constexpr int margin = 20, padsTop = 18, rowGap = 16, panelHeight = 196, shapeWidth = 400;
}

ChordButtEditor::ChordButtEditor (ChordButtProcessor& p)
    : AudioProcessorEditor (&p),
      chordProcessor (p),
      header (p),
      padGrid (p),
      voicingPanel (p),
      stylePanel (p),
      chordStrip (p),
      vblank (this, [this] (double timestampSec)
              {
                  const double dt = lastFrameTime < 0.0 ? 1.0 / 60.0 : timestampSec - lastFrameTime;
                  lastFrameTime = timestampSec;
                  advanceAnimations ((float) juce::jlimit (0.0, 0.1, dt));   // clamp stalls so nothing jumps
              })
{
    setLookAndFeel (&lookAndFeel);

    padGrid.onPadCommitted = [this] (const cb::RelChord& chord) { chordStrip.addChord (chord); };
    chordStrip.onSelectionChanged = [this] (int index) { voicingPanel.setSelectedSlot (index); };
    voicingPanel.onStripEdited = [this] { chordStrip.refresh(); };

    for (auto* c : std::initializer_list<juce::Component*> { &header, &padGrid, &voicingPanel, &stylePanel, &chordStrip })
        addAndMakeVisible (c);

    chordProcessor.stateBroadcaster.addChangeListener (this);
    setSize (editorWidth, editorHeight);
    advanceAnimations (0.0f);   // publish initial animation values to the LookAndFeel properties
}

ChordButtEditor::~ChordButtEditor()
{
    chordProcessor.stateBroadcaster.removeChangeListener (this);
    chordProcessor.preview().auditionOff();
    setLookAndFeel (nullptr);
}

void ChordButtEditor::advanceAnimations (float dtSeconds)
{
    tickTree (*this, dtSeconds);
}

void ChordButtEditor::visibilityChanged()
{
    lastFrameTime = -1.0;   // a re-shown editor starts with a normal frame step, not the hidden time
}

void ChordButtEditor::refreshAll()
{
    padGrid.refresh();
    chordStrip.refresh();
    voicingPanel.refresh();
}

void ChordButtEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

bool ChordButtEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details,
                                                            juce::StringArray& files, bool& canMoveFiles)
{
    canMoveFiles = false;
    return chordStrip.prepareExternalDrag (details.description, files);
}

juce::Rectangle<int> ChordButtEditor::drawerArea() const
{
    return getLocalBounds().withTrimmedTop (HeaderBar::height).withTrimmedBottom (ChordStrip::height);
}

void ChordButtEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
}

void ChordButtEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (HeaderBar::height));
    chordStrip.setBounds (r.removeFromBottom (ChordStrip::height));

    auto content = r.reduced (margin, 0).withTrimmedTop (padsTop).withTrimmedBottom (rowGap);
    auto panels = content.removeFromBottom (panelHeight);
    content.removeFromBottom (rowGap);
    padGrid.setBounds (content);

    voicingPanel.setBounds (panels.removeFromLeft (shapeWidth));
    panels.removeFromLeft (12);
    stylePanel.setBounds (panels);
}
