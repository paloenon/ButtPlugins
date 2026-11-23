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
    cb::Anim glow;
    bool forcedHighlight = false;
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
    cb::Anim hover, on;
    bool forcedHighlight = false;
};
