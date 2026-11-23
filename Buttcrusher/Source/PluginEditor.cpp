#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff15161a };
    const juce::Colour panel      { 0xff1f2127 };
    const juce::Colour knobBody   { 0xff2a2d35 };
    const juce::Colour track      { 0xff3a3e48 };
    const juce::Colour accent     { 0xff38e8a0 };   // phosphor green
    const juce::Colour text       { 0xffd8dbe2 };
    const juce::Colour dimText    { 0xff8a8f9c };
}

//==============================================================================
CrushLookAndFeel::CrushLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               Colours::dimText);
}

void CrushLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centre  = bounds.getCentre();
    auto angle   = startAngle + pos * (endAngle - startAngle);
    auto arcR    = radius - 3.0f;
    const float lineW = 4.0f;

    // Background track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::track);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with a soft glow
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (Colours::accent.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (lineW + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colours::accent);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob body
    auto bodyR = arcR - 9.0f;
    g.setColour (Colours::knobBody);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyR + 4.0f, 3.0f, bodyR * 0.45f, 1.5f);
    g.setColour (Colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

juce::Label* CrushLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (13.0f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

//==============================================================================
ButtcrusherAudioProcessorEditor::ButtcrusherAudioProcessorEditor (ButtcrusherAudioProcessor& p)
    : AudioProcessorEditor (&p), buttcrusherProcessor (p)
{
    setupKnob (drive,      "drive",      "DRIVE");
    setupKnob (bits,       "bits",       "BITS");
    setupKnob (downsample, "downsample", "RATE");
    setupKnob (tone,       "tone",       "TONE");
    setupKnob (mix,        "mix",        "MIX");
    setupKnob (output,     "output",     "OUTPUT");

    setSize (620, 300);
}

ButtcrusherAudioProcessorEditor::~ButtcrusherAudioProcessorEditor()
{
    for (auto* k : { &drive, &bits, &downsample, &tone, &mix, &output })
        k->slider.setLookAndFeel (nullptr);
}

void ButtcrusherAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramID, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, Colours::dimText);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        buttcrusherProcessor.apvts, paramID, k.slider);
}

void ButtcrusherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Header
    auto header = getLocalBounds().removeFromTop (56).toFloat();
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("BUTTCRUSHER", header.reduced (24.0f, 0.0f), juce::Justification::centredLeft);

    // Little stepped "crushed wave" logo on the right
    {
        auto area = header.removeFromRight (140.0f).reduced (20.0f, 16.0f);
        juce::Path wave;
        const int steps = 12;
        for (int i = 0; i <= steps; ++i)
        {
            float t  = (float) i / steps;
            float v  = std::round (std::sin (t * juce::MathConstants<float>::twoPi) * 3.0f) / 3.0f;
            float px = area.getX() + t * area.getWidth();
            float py = area.getCentreY() - v * area.getHeight() * 0.5f;
            if (i == 0) wave.startNewSubPath (px, py);
            else
            {
                wave.lineTo (px, wave.getCurrentPosition().y);
                wave.lineTo (px, py);
            }
        }
        g.setColour (Colours::accent);
        g.strokePath (wave, juce::PathStrokeType (2.0f));
    }

    // Panels behind the two knob groups
    auto body = getLocalBounds().reduced (16).withTrimmedTop (44).toFloat();
    auto crushPanel = body.removeFromLeft (body.getWidth() * 4.0f / 6.0f).reduced (4.0f, 0.0f);
    auto outPanel   = body.reduced (4.0f, 0.0f);

    g.setColour (Colours::panel);
    g.fillRoundedRectangle (crushPanel, 10.0f);
    g.fillRoundedRectangle (outPanel, 10.0f);
}

void ButtcrusherAudioProcessorEditor::resized()
{
    auto body = getLocalBounds().reduced (16).withTrimmedTop (44).reduced (4, 14);
    const int knobW = body.getWidth() / 6;

    for (auto* k : { &drive, &bits, &downsample, &tone, &mix, &output })
    {
        auto col = body.removeFromLeft (knobW).reduced (4, 0);
        k->label.setBounds (col.removeFromTop (20));
        k->slider.setBounds (col);
    }
}
