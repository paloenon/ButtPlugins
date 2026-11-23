#pragma once
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"

// Playing style: Chords / Rhythm / Arp segmented control plus the controls of the active mode.
// Controls fade and slide into place when the mode changes.
class StylePanel : public juce::Component, public Animated
{
public:
    // Segmented control bound to the "mode" parameter; the accent underline slides between segments.
    class ModeSwitch : public juce::Component, public Animated
    {
    public:
        ModeSwitch (juce::RangedAudioParameter&, std::function<void (int)> onModeChanged);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        bool tickAnimations (float dt) override;

        int getIndex() const { return index; }

    private:
        juce::Rectangle<float> segment (int i) const;

        static constexpr int numSegments = 3;
        juce::ParameterAttachment attachment;
        std::function<void (int)> onModeChanged;
        int index = 0;
        cb::Anim underline;                       // segment position (0..2) of the underline
        std::array<cb::Anim, numSegments> sel, hover;
        bool primed = false;
    };

    static constexpr int knobRowY = 86;   // shared with VoicingPanel so both knob rows line up

    explicit StylePanel (ChordButtProcessor&);
    ~StylePanel() override;

    ModeSwitch& getModeSwitch() { return modeSwitch; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool tickAnimations (float dt) override;

private:
    struct Managed
    {
        juce::Component* component = nullptr;
        juce::Rectangle<int> target;   // laid out for the current mode
        bool shown = false;
        cb::Anim alpha, x;
    };

    void setMode (int newMode);
    void layoutForMode();
    Managed& managed (juce::Component&);

    ChordButtProcessor& processor;
    int mode = 0;

    AnimatedComboBox rhythmBox { "PATTERN" }, arpBox { "PATTERN" }, rateBox { "RATE" };
    LabelledKnob gate { "GATE" }, swing { "SWING" }, octaves { "OCTAVES" }, humanize { "HUMANIZE" };
    AnimatedTextButton reroll { "RE-ROLL" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rhythmAtt, arpAtt, rateAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAtt, swingAtt, octavesAtt, humanizeAtt;

    std::vector<Managed> controls;
    cb::Anim heldHint;      // "Held chords" note shown in Chords mode
    bool primed = false;

    ModeSwitch modeSwitch;   // last: its attachment calls setMode() while being constructed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StylePanel)
};
