#include "PluginProcessor.h"
#include "PluginEditor.h"

ChordButtProcessor::ChordButtProcessor()
    : AudioProcessor (BusesProperties())
{
}

void ChordButtProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // Passthrough: MIDI is left untouched.
}

juce::AudioProcessorEditor* ChordButtProcessor::createEditor()
{
    return new ChordButtEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordButtProcessor();
}
