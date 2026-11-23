#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"

// 2 x 7 chord pads built from the processor's current palette. Press = audition through MIDI out;
// release = commit (append to the strip, or replace the selected slot) unless ⌥ is held.
class PadGrid : public juce::Component
{
public:
    static constexpr int rows = 2, columns = 7, gap = 8;

    class Pad : public juce::Component, public Animated
    {
    public:
        Pad (PadGrid& owner, int index);

        void setChord (const juce::String& name, const juce::String& roman, bool animate);
        void setHighlighted (bool shouldHighlight) { forcedHighlight = shouldHighlight; }   // snapshots

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool tickAnimations (float dt) override;

    private:
        PadGrid& owner;
        const int index;
        FadeText name, roman;
        cb::Anim press, hover;
        bool held = false, forcedHighlight = false;
    };

    explicit PadGrid (ChordButtProcessor&);

    // Re-reads the palette (key / scale / complexity); changed labels cross-fade.
    void refresh (bool animate = true);

    // Released without ⌥: the pad's chord as stored in the strip (PadChord::base, before complexity; the strip applies
    // the current complexity itself, so turning Complexity later still re-voices it).
    std::function<void (const cb::RelChord&)> onPadCommitted;

    Pad& getPad (int index) { return *pads[index]; }
    void resized() override;

private:
    void padPressed (int index);
    void padReleased (int index, bool auditionOnly);

    ChordButtProcessor& processor;
    juce::OwnedArray<Pad> pads;
    std::vector<cb::PadChord> palette;   // current complexity (PadChord::base is what a click stores)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadGrid)
};

// The four global voicing knobs under the pads. While a strip slot is selected, the Voicing knob
// edits that slot's inversion override instead of the parameter ("VOICING · CHORD n" + reset ×).
class VoicingPanel : public juce::Component
{
public:
    explicit VoicingPanel (ChordButtProcessor&);
    ~VoicingPanel() override;

    void setSelectedSlot (int index);   // -1 = none (knob edits the global parameter)
    void refresh();                     // strip or parameters changed elsewhere
    std::function<void()> onStripEdited;

    LabelledKnob& getVoicingKnob() { return voicing; }

    void paint (juce::Graphics&) override;
    void resized() override;

    // Small "×" that clears the selected slot's override; fades in only while an override exists.
    class ResetButton : public juce::Component, public Animated
    {
    public:
        std::function<void()> onClick;
        void setShown (bool shouldShow);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool tickAnimations (float dt) override;

    private:
        cb::Anim shown, hover;
        bool primed = false;
    };

private:
    void attachVoicing();
    void updateOverrideUi();
    std::optional<int> selectedOverride() const;

    ChordButtProcessor& processor;
    LabelledKnob voicing { "VOICING" }, spread { "SPREAD" }, octave { "OCTAVE" }, complexity { "COMPLEXITY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voicingAtt, spreadAtt, octaveAtt, complexityAtt;
    ResetButton reset;
    int selected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicingPanel)
};
