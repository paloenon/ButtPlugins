#include "UI/ChordLookAndFeel.h"
#include "UI/Theme.h"

using namespace juce;

ChordLookAndFeel::ChordLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, Theme::bg);
    setColour (ComboBox::textColourId, Theme::text);
    setColour (ComboBox::backgroundColourId, Theme::surface2);
    setColour (ComboBox::outlineColourId, Theme::line);
    setColour (ComboBox::arrowColourId, Theme::dim);
    setColour (Label::textColourId, Theme::text);
    setColour (TextButton::textColourOffId, Theme::text);
    setColour (TextButton::textColourOnId, Theme::bg);
    setColour (TextButton::buttonColourId, Theme::surface2);
    setColour (TextButton::buttonOnColourId, Theme::accent);
    setColour (PopupMenu::backgroundColourId, Theme::surface);
    setColour (PopupMenu::textColourId, Theme::text);
    setColour (PopupMenu::headerTextColourId, Theme::dim);
    setColour (PopupMenu::highlightedBackgroundColourId, Theme::line);
    setColour (PopupMenu::highlightedTextColourId, Theme::text);
    setColour (Slider::rotarySliderFillColourId, Theme::accent);
    setColour (Slider::rotarySliderOutlineColourId, Theme::line);
    setColour (Slider::thumbColourId, Theme::text);
}

void ChordLookAndFeel::drawGlow (Graphics& g, const Path& p, Colour c, float amount, float baseWidth)
{
    if (amount <= 0.001f)
        return;

    // Wide-and-faint to narrow-and-stronger; the layers add up to a soft falloff.
    const float widths[] = { 7.0f, 4.0f, 2.0f };
    const float alphas[] = { 0.035f, 0.07f, 0.13f };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (c.withAlpha (alphas[i] * amount));
        g.strokePath (p, PathStrokeType (baseWidth + 2.0f * widths[i], PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void ChordLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, Slider& slider)
{
    const float glow = AnimUtil::property (slider, LnfProps::glow);
    const float enabledAlpha = slider.isEnabled() ? 1.0f : 0.4f;

    auto area = Rectangle<int> (x, y, width, height).toFloat();
    const float size = jmin (area.getWidth(), area.getHeight());
    const float margin = 8.0f;                                   // room for the glow
    const float ringR = size * 0.5f - margin;
    const auto centre = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Track: thin full-range arc.
    Path track;
    track.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::line.withMultipliedAlpha (enabledAlpha));
    g.strokePath (track, PathStrokeType (Theme::trackWidth, PathStrokeType::curved, PathStrokeType::rounded));

    // Value arc in accent with a soft glow that fades in on hover / drag.
    if (pos > 0.0f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
        drawGlow (g, value, Theme::accent, glow * enabledAlpha, Theme::trackWidth);
        g.setColour (Theme::accent.withAlpha ((0.82f + 0.18f * glow) * enabledAlpha));
        g.strokePath (value, PathStrokeType (Theme::trackWidth, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Matte body.
    const float bodyR = ringR * 0.72f;
    auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setGradientFill (ColourGradient (Theme::surface2.brighter (0.06f), centre.x, body.getY(),
                                       Theme::surface2.darker (0.15f), centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Theme::line.withMultipliedAlpha (enabledAlpha));
    g.drawEllipse (body.reduced (0.5f), Theme::lineWidth);

    // 2px pointer in text colour.
    const auto dir = Point<float> (std::sin (angle), -std::cos (angle));
    const auto p0 = centre + dir * (bodyR * 0.38f);
    const auto p1 = centre + dir * (bodyR * 0.82f);
    Path pointer;
    pointer.startNewSubPath (p0);
    pointer.lineTo (p1);
    g.setColour (Theme::text.withAlpha (0.92f * enabledAlpha));
    g.strokePath (pointer, PathStrokeType (Theme::trackWidth, PathStrokeType::curved, PathStrokeType::rounded));
}

void ChordLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box)
{
    const float hover = jmax (AnimUtil::property (box, LnfProps::hover), box.isPopupActive() ? 1.0f : 0.0f);
    auto r = Rectangle<float> ((float) width, (float) height).reduced (0.5f);

    g.setColour (Theme::surface2);
    g.fillRoundedRectangle (r, Theme::cornerRadius);
    g.setColour (Theme::line.interpolatedWith (Theme::dim, 0.45f * hover));
    g.drawRoundedRectangle (r, Theme::cornerRadius, Theme::lineWidth);

    auto inner = r.reduced (12.0f, 0.0f);
    const auto prefix = box.getProperties()[LnfProps::prefix].toString();
    if (prefix.isNotEmpty())
    {
        g.setColour (Theme::dim);
        g.setFont (Theme::label());
        g.drawText (prefix, inner, Justification::centredLeft, false);
    }

    // Small chevron.
    const float cx = r.getRight() - 14.0f, cy = r.getCentreY();
    Path chevron;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (Theme::dim.interpolatedWith (Theme::text, hover));
    g.strokePath (chevron, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
}

void ChordLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    int left = 12;
    const auto prefix = box.getProperties()[LnfProps::prefix].toString();
    if (prefix.isNotEmpty())
        left += roundToInt (GlyphArrangement::getStringWidth (Theme::label(), prefix)) + 8;

    label.setBounds (left, 0, jmax (0, box.getWidth() - left - 24), box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setBorderSize ({});
    label.setJustificationType (Justification::centredLeft);
}

Font ChordLookAndFeel::getComboBoxFont (ComboBox&) { return Theme::value(); }

void ChordLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool, bool isButtonDown)
{
    const float hover = AnimUtil::property (b, LnfProps::hover);
    const float on = AnimUtil::property (b, LnfProps::on);
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);

    // Body: surface2, blending to accent as the toggle opens.
    auto fill = Theme::surface2.interpolatedWith (Theme::accent, on);
    if (isButtonDown)
        fill = fill.darker (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, Theme::cornerRadius);

    auto border = Theme::line.interpolatedWith (Theme::dim, 0.45f * hover).interpolatedWith (Theme::accent, on);
    g.setColour (border);
    g.drawRoundedRectangle (r, Theme::cornerRadius, Theme::lineWidth);
}

void ChordLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const float hover = AnimUtil::property (b, LnfProps::hover);
    const float on = AnimUtil::property (b, LnfProps::on);
    // Light text stays light until the accent fill is bright enough to carry dark text, so the
    // label never dissolves into a mid-tone fill during the transition (still continuous, no cut).
    const float x = jlimit (0.0f, 1.0f, (on - 0.3f) / 0.4f);
    const float toDark = x * x * (3.0f - 2.0f * x);
    auto colour = Theme::dim.interpolatedWith (Theme::text, jmax (hover, on)).interpolatedWith (Theme::bg, toDark);
    if (! b.isEnabled())
        colour = colour.withMultipliedAlpha (0.4f);

    g.setColour (colour);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

Font ChordLookAndFeel::getTextButtonFont (TextButton&, int) { return Theme::label(); }

void ChordLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    g.fillAll (Theme::surface);
    g.setColour (Theme::line);
    g.drawRect (0, 0, width, height, 1);
}

Font ChordLookAndFeel::getPopupMenuFont() { return Theme::value(); }

// ---------------------------------------------------------------------------------------------

GlowSlider::GlowSlider()
    : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox)
{
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    getProperties().set (LnfProps::glow, 0.0f);
}

bool GlowSlider::tickAnimations (float dt)
{
    glow.setTarget ((forcedHighlight || isMouseOverOrDragging (true)) && isEnabled() ? 1.0f : 0.0f);
    return AnimUtil::tickProperty (*this, glow, LnfProps::glow, dt);
}

AnimatedComboBox::AnimatedComboBox (const String& prefixLabel)
{
    getProperties().set (LnfProps::prefix, prefixLabel);
    getProperties().set (LnfProps::hover, 0.0f);
}

bool AnimatedComboBox::tickAnimations (float dt)
{
    hover.setTarget (isMouseOver (true) || isPopupActive() ? 1.0f : 0.0f);
    return AnimUtil::tickProperty (*this, hover, LnfProps::hover, dt);
}

AnimatedTextButton::AnimatedTextButton (const String& text)
    : TextButton (text)
{
    getProperties().set (LnfProps::hover, 0.0f);
    getProperties().set (LnfProps::on, 0.0f);
}

bool AnimatedTextButton::tickAnimations (float dt)
{
    hover.setTarget (forcedHighlight || isMouseOver (true) ? 1.0f : 0.0f);
    on.setTarget (getToggleState() ? 1.0f : 0.0f);
    const bool h = AnimUtil::tickProperty (*this, hover, LnfProps::hover, dt);
    const bool o = AnimUtil::tickProperty (*this, on, LnfProps::on, dt);
    return h || o;
}
