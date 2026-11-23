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
}

ChordButtEditor::ChordButtEditor (ChordButtProcessor& p)
    : AudioProcessorEditor (&p),
      header (p),
      vblank (this, [this] (double timestampSec)
              {
                  const double dt = lastFrameTime < 0.0 ? 1.0 / 60.0 : timestampSec - lastFrameTime;
                  lastFrameTime = timestampSec;
                  advanceAnimations ((float) juce::jlimit (0.0, 0.1, dt));   // clamp stalls so nothing jumps
              })
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (header);
    setSize (editorWidth, editorHeight);
    advanceAnimations (0.0f);   // publish initial animation values to the LookAndFeel properties
}

ChordButtEditor::~ChordButtEditor()
{
    setLookAndFeel (nullptr);
}

void ChordButtEditor::advanceAnimations (float dtSeconds)
{
    tickTree (*this, dtSeconds);
}

void ChordButtEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
}

void ChordButtEditor::resized()
{
    header.setBounds (getLocalBounds().removeFromTop (HeaderBar::height));
}
