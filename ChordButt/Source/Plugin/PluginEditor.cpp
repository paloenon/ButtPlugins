#include "PluginEditor.h"

namespace
{
    // Temporary local token; replaced by Source/UI/Theme.h in a later task.
    const juce::Colour themeBg { 0xff0b0c0f };
}

ChordButtEditor::ChordButtEditor (ChordButtProcessor& p)
    : AudioProcessorEditor (&p)
{
    setSize (900, 560);
}

void ChordButtEditor::paint (juce::Graphics& g)
{
    g.fillAll (themeBg);
}
