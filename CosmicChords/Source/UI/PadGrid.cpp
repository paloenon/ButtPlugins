#include "UI/PadGrid.h"
#include "UI/ChordStrip.h"
#include "UI/Theme.h"
#include "Core/Voicing.h"

using namespace juce;

namespace
{
    cb::VoicingParams globalVoicing (AudioProcessorValueTreeState& apvts)
    {
        auto raw = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        cb::VoicingParams v;
        v.inversion = jlimit (0, 3, roundToInt (raw ("inversion")));
        v.spread = raw ("spread");
        v.octave = jlimit (-2, 2, roundToInt (raw ("octave")));
        return v;
    }

    constexpr int shapeKnobSize = 76;

    int globalInversion (AudioProcessorValueTreeState& apvts)
    {
        return globalVoicing (apvts).inversion;
    }
}

// =============================================================================================
PadGrid::Pad::Pad (PadGrid& o, int i)
    : owner (o), index (i)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void PadGrid::Pad::setChord (const String& newName, const String& newRoman, bool animate)
{
    name.set (newName, animate);
    roman.set (newRoman, animate);
    repaint();
}

void PadGrid::Pad::paint (Graphics& g)
{
    const float p = press.value, h = hover.value;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float radius = Theme::cornerRadius + 1.0f;

    // Matte body with a faint top-down sheen; brightens a touch on hover.
    const auto top = Theme::surface2.interpolatedWith (Theme::surface2.brighter (0.10f), 0.6f * h);
    g.setGradientFill (ColourGradient (top, r.getX(), r.getY(), Theme::surface, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    // Pressed: faint accent wash plus an inner glow that falls off towards the centre.
    if (p > 0.001f)
    {
        g.setColour (Theme::accent.withAlpha (0.05f * p));
        g.fillRoundedRectangle (r, radius);
        for (int k = 1; k <= 7; ++k)
        {
            g.setColour (Theme::accent.withAlpha (0.085f * p * (1.0f - (float) (k - 1) / 7.0f)));
            g.drawRoundedRectangle (r.reduced ((float) k), jmax (1.0f, radius - (float) k * 0.5f), 1.0f);
        }
    }

    g.setColour (Theme::line.interpolatedWith (Theme::dim, 0.35f * h).interpolatedWith (Theme::accent, p));
    g.drawRoundedRectangle (r, radius, Theme::lineWidth);

    // Chord name and function.
    auto nameRow = r.withHeight (22.0f).withCentre ({ r.getCentreX(), r.getCentreY() - 7.0f });
    auto romanRow = r.withHeight (14.0f).withCentre ({ r.getCentreX(), r.getCentreY() + 14.0f });

    g.setFont (Theme::uiFont (17.0f, "Semibold"));
    name.draw ([&] (const String& s, float a, float dy)
    {
        g.setColour (Theme::text.withMultipliedAlpha (a));
        g.drawFittedText (s, nameRow.translated (0.0f, dy).reduced (6.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.75f);
    });

    g.setFont (Theme::uiFont (11.5f, "Regular", 0.04f));
    roman.draw ([&] (const String& s, float a, float dy)
    {
        g.setColour (Theme::dim.interpolatedWith (Theme::accent, 0.7f * p).withMultipliedAlpha (a));
        g.drawText (s, romanRow.translated (0.0f, dy), Justification::centred, false);
    });
}

void PadGrid::Pad::mouseDown (const MouseEvent&)
{
    held = true;
    dragging = false;
    owner.padPressed (index);
}

void PadGrid::Pad::mouseDrag (const MouseEvent& e)
{
    if (! held || dragging || e.getDistanceFromDragStart() < 6)
        return;
    dragging = true;
    owner.startPadDrag (*this);
}

void PadGrid::Pad::mouseUp (const MouseEvent&)
{
    if (! held)
        return;
    held = false;
    dragging = false;
    owner.padReleased();   // a click only previews; adding happens by dropping on the strip
}

bool PadGrid::Pad::tickAnimations (float dt)
{
    press.setTarget (held || forcedHighlight ? 1.0f : 0.0f);
    hover.setTarget (isMouseOver() ? 1.0f : 0.0f);
    const bool p = AnimUtil::tick (press, dt);
    const bool h = AnimUtil::tick (hover, dt);
    const bool n = name.tick (dt);
    const bool r = roman.tick (dt);
    return p || h || n || r;
}

// =============================================================================================
PadGrid::PadGrid (CosmicChordsProcessor& p)
    : processor (p)
{
    for (int i = 0; i < rows * columns; ++i)
        addAndMakeVisible (pads.add (new Pad (*this, i)));
    refresh (false);
}

void PadGrid::refresh (bool animate)
{
    palette = processor.currentPalette();
    const int tonic = processor.tonic();

    for (int i = 0; i < pads.size(); ++i)
    {
        if (i >= (int) palette.size())
        {
            pads[i]->setChord ({}, {}, animate);
            continue;
        }
        const auto& rel = palette[(size_t) i].chord;
        pads[i]->setChord (Theme::pretty (cb::chordNameInKey (rel, tonic, processor.scale())), Theme::pretty (rel.roman), animate);
    }
}

void PadGrid::resized()
{
    auto r = getLocalBounds();
    const int padW = (r.getWidth() - gap * (columns - 1)) / columns;
    const int padH = (r.getHeight() - gap * (rows - 1)) / rows;
    // Distribute the rounding remainder into the outer margins so the grid stays centred.
    const int x0 = r.getX() + (r.getWidth() - (padW * columns + gap * (columns - 1))) / 2;

    for (int i = 0; i < pads.size(); ++i)
    {
        const int row = i / columns, col = i % columns;
        pads[i]->setBounds (x0 + col * (padW + gap), r.getY() + row * (padH + gap), padW, padH);
    }
}

void PadGrid::padPressed (int index)
{
    pressedChord.reset();
    if (index >= (int) palette.size())
        return;
    pressedChord = palette[(size_t) index].base;
    const auto chord = cb::resolve (palette[(size_t) index].chord, processor.tonic());
    processor.preview().auditionOn (cb::voiceChord (chord, globalVoicing (processor.apvts)));
}

void PadGrid::padReleased()
{
    processor.preview().auditionOff();
    pressedChord.reset();
}

void PadGrid::startPadDrag (Pad& pad)
{
    // The chord was heard on press; stop it so a long drag doesn't hold the notes.
    processor.preview().auditionOff();
    if (! pressedChord)
        return;

    auto* container = DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr)
        return;

    const auto rel = cb::applyComplexity (*pressedChord, processor.scale(),
                                          jlimit (0, 3, roundToInt (processor.apvts.getRawParameterValue ("complexity")->load())));
    const auto name = Theme::pretty (cb::chordNameInKey (rel, processor.tonic(), processor.scale()));

    // Drag image: the pad itself, slightly translucent.
    const float scale = 2.0f;
    auto img = pad.createComponentSnapshot (pad.getLocalBounds(), true, scale);
    img.multiplyAllAlphas (0.85f);
    container->startDragging (ChordStrip::padDragDescription (*pressedChord, name), &pad,
                              ScaledImage (img, scale), true);
}

// =============================================================================================
void VoicingPanel::ResetButton::setShown (bool shouldShow)
{
    shown.setTarget (shouldShow ? 1.0f : 0.0f);
    setInterceptsMouseClicks (shouldShow, false);
}

void VoicingPanel::ResetButton::paint (Graphics& g)
{
    const float a = shown.value;
    if (a <= 0.001f)
        return;

    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Theme::surface2.withAlpha (a));
    g.fillEllipse (r);
    g.setColour (Theme::line.interpolatedWith (Theme::accent, 0.6f * hover.value).withAlpha (a));
    g.drawEllipse (r.reduced (0.5f), Theme::lineWidth);

    const auto c = r.getCentre();
    const float d = r.getWidth() * 0.18f;
    Path cross;
    cross.startNewSubPath (c.x - d, c.y - d); cross.lineTo (c.x + d, c.y + d);
    cross.startNewSubPath (c.x + d, c.y - d); cross.lineTo (c.x - d, c.y + d);
    g.setColour (Theme::dim.interpolatedWith (Theme::text, hover.value).withAlpha (a));
    g.strokePath (cross, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
}

void VoicingPanel::ResetButton::mouseUp (const MouseEvent& e)
{
    if (shown.target > 0.5f && getLocalBounds().contains (e.getPosition()) && onClick)
        onClick();
}

bool VoicingPanel::ResetButton::tickAnimations (float dt)
{
    if (! primed)
        shown.snap (shown.target);
    primed = true;
    hover.setTarget (isMouseOver() ? 1.0f : 0.0f);
    const bool s = AnimUtil::tick (shown, dt);
    const bool h = AnimUtil::tick (hover, dt);
    return s || h;
}

// =============================================================================================
VoicingPanel::VoicingPanel (CosmicChordsProcessor& p)
    : processor (p)
{
    using Att = AudioProcessorValueTreeState::SliderAttachment;
    attachVoicing();
    spreadAtt = std::make_unique<Att> (processor.apvts, "spread", spread.getSlider());
    octaveAtt = std::make_unique<Att> (processor.apvts, "octave", octave.getSlider());
    complexityAtt = std::make_unique<Att> (processor.apvts, "complexity", complexity.getSlider());

    voicing.formatValue = [] (double v)
    {
        static const char* names[] { "Root", "1st inv", "2nd inv", "3rd inv" };
        return String (names[jlimit (0, 3, roundToInt (v))]);
    };
    spread.formatValue = [] (double v) { return String (cb::spreadLevelName (cb::spreadLevel ((float) v))); };
    octave.formatValue = [] (double v)
    {
        const int o = roundToInt (v);
        return o == 0 ? String ("0") : (o > 0 ? "+" : "") + String (o);
    };
    complexity.formatValue = [] (double v)
    {
        static const char* names[] { "Triads", "7ths", "9ths", "11ths+" };
        return String (names[jlimit (0, 3, roundToInt (v))]);
    };

    for (auto* k : { &voicing, &spread, &octave, &complexity })
        addAndMakeVisible (k);
    addAndMakeVisible (reset);   // added last: sits above the (wide) voicing knob

    reset.onClick = [this]
    {
        if (selected < 0)
            return;
        processor.strip().setInversionOverride (selected, std::nullopt);
        processor.stripChanged();
        refresh();
        if (onStripEdited)
            onStripEdited();
    };
    reset.setShown (false);
}

VoicingPanel::~VoicingPanel()
{
    voicing.getSlider().onValueChange = nullptr;
    voicingAtt.reset();
    spreadAtt.reset();
    octaveAtt.reset();
    complexityAtt.reset();
}

void VoicingPanel::attachVoicing()
{
    voicing.getSlider().onValueChange = nullptr;
    voicingAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "inversion", voicing.getSlider());
}

std::optional<int> VoicingPanel::selectedOverride() const
{
    const auto& slots = processor.strip().slots();
    if (selected < 0 || selected >= (int) slots.size())
        return std::nullopt;
    return slots[(size_t) selected].inversionOverride;
}

void VoicingPanel::setSelectedSlot (int index)
{
    const int count = (int) processor.strip().slots().size();
    if (index >= count)
        index = -1;
    if (index == selected)
    {
        updateOverrideUi();
        return;
    }

    selected = index;
    auto& knob = voicing.getSlider();
    if (selected < 0)
    {
        attachVoicing();   // the attachment re-syncs the knob to the parameter; the knob glides there
    }
    else
    {
        voicingAtt.reset();   // keep the range/interval the attachment configured (0..3, step 1)
        knob.onValueChange = [this]
        {
            if (selected < 0)
                return;
            processor.strip().setInversionOverride (selected, roundToInt (voicing.getSlider().getValue()));
            processor.stripChanged();
            updateOverrideUi();
            if (onStripEdited)
                onStripEdited();
        };
    }
    updateOverrideUi();
}

void VoicingPanel::refresh()
{
    if (selected >= (int) processor.strip().slots().size())
        setSelectedSlot (-1);
    updateOverrideUi();
}

void VoicingPanel::updateOverrideUi()
{
    if (selected < 0)
    {
        voicing.setLabel ("VOICING", false);
        reset.setShown (false);
        return;
    }

    const auto ov = selectedOverride();
    const int effective = ov.value_or (globalInversion (processor.apvts));
    if (roundToInt (voicing.getSlider().getValue()) != effective)
        voicing.getSlider().setValue (effective, dontSendNotification);

    voicing.setLabel (String::fromUTF8 ("VOICING \xc2\xb7 CHORD ") + String (selected + 1), true);
    reset.setShown (ov.has_value());
}

void VoicingPanel::paint (Graphics& g)
{
    Theme::drawPanel (g, getLocalBounds().toFloat(), "SHAPE");
}

void VoicingPanel::resized()
{
    // The four shape knobs are the primary controls: larger, directly under the title (aligned with
    // the style panel's second row), so the panel has no dead band between title and knobs.
    auto r = getLocalBounds().reduced (16, 0);
    const int pitch = r.getWidth() / 4;
    const int knobH = shapeKnobSize + LabelledKnob::textHeight;
    const int y = 42;

    LabelledKnob* knobs[] { &voicing, &spread, &octave, &complexity };
    for (int i = 0; i < 4; ++i)
    {
        const int cx = r.getX() + pitch * i + pitch / 2;
        // Voicing is wider so "VOICING · CHORD 16" fits; it ignores the mouse outside its knob.
        const int w = i == 0 ? 150 : pitch;
        knobs[i]->setKnobSize (shapeKnobSize);
        knobs[i]->setBounds (cx - w / 2, y, w, knobH);
    }

    const int vcx = r.getX() + pitch / 2;
    reset.setBounds (vcx + shapeKnobSize / 2 - 6, y - 2, 18, 18);
}
