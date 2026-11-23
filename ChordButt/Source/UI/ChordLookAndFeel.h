#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/Animated.h"

// Component properties read by ChordLookAndFeel (all 0..1, written by the Anim-driven widgets below).
namespace LnfProps
{
    inline const juce::Identifier glow   { "glow" };     // knob value-ring glow (hover / drag)
    inline const juce::Identifier hover  { "hover" };    // hover highlight of combos / buttons
    inline const juce::Identifier on     { "on" };       // toggle-button fill
    inline const juce::Identifier prefix { "prefix" };   // small dim label drawn inside a ComboBox
    inline const juce::Identifier down   { "down" };     // button pressed (fill darkens)
    inline const juce::Identifier enabled { "enabled" }; // 1 = enabled, 0 = disabled (dimmed); default 1
    inline const juce::Identifier pos    { "pos" };      // animated knob position 0..1 (absent: slider's own)
}

class ChordLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChordLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;

    // Soft glow around a stroked path: a few wide, faint strokes underneath (alpha scales with `amount`).
    static void drawGlow (juce::Graphics&, const juce::Path&, juce::Colour, float amount, float baseWidth);
};

// Rotary knob whose value-ring glow fades in on hover/drag (Anim, tau 0.12 s).
class GlowSlider : public juce::Slider, public Animated
{
public:
    GlowSlider();
    void setHighlighted (bool shouldHighlight) { forcedHighlight = shouldHighlight; }   // e.g. snapshots, MIDI learn
    bool tickAnimations (float dt) override;

private:
    cb::Anim glow, pos, enabled;
    bool forcedHighlight = false, primed = false;
};

// ComboBox with an animated hover border and an optional dim prefix label ("KEY", "SCALE").
class AnimatedComboBox : public juce::ComboBox, public Animated
{
public:
    explicit AnimatedComboBox (const juce::String& prefixLabel = {});
    bool tickAnimations (float dt) override;

private:
    cb::Anim hover;
};

// Text button whose hover state and toggle fill (accent) blend smoothly.
class AnimatedTextButton : public juce::TextButton, public Animated
{
public:
    explicit AnimatedTextButton (const juce::String& text);
    void setHighlighted (bool shouldHighlight) { forcedHighlight = shouldHighlight; }
    bool tickAnimations (float dt) override;

private:
    cb::Anim hover, on, down, enabled;
    bool forcedHighlight = false, primed = false;
};

// A GlowSlider with a small-caps label and a value line underneath. The label cross-fades when it
// changes and can switch to accent (e.g. "VOICING · CHORD 3"). The component itself ignores the
// mouse, so a wide label may overlap its neighbours without stealing their clicks.
class LabelledKnob : public juce::Component, public Animated, private juce::Slider::Listener
{
public:
    static constexpr int defaultKnobSize = 56, textHeight = 32;

    explicit LabelledKnob (const juce::String& label);
    void setKnobSize (int size) { knobSize = size; resized(); }
    int getKnobSize() const { return knobSize; }
    ~LabelledKnob() override;

    GlowSlider& getSlider() { return knob; }
    void setLabel (const juce::String& text, bool accent);
    std::function<juce::String (double)> formatValue;   // value line; empty function = no value line

    void paint (juce::Graphics&) override;
    void resized() override;
    bool tickAnimations (float dt) override;

private:
    void sliderValueChanged (juce::Slider*) override { updateValueText(); }

    void updateValueText();

    GlowSlider knob;
    FadeText label, valueText;
    cb::Anim accent;
    int knobSize = defaultKnobSize;
};
