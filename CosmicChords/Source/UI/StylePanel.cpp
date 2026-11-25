#include "UI/StylePanel.h"
#include "UI/Theme.h"

using namespace juce;

namespace
{
    const char* const modeNames[] { "CHORDS", "RHYTHM", "ARP" };

    void fillChoices (ComboBox& box, AudioProcessorValueTreeState& apvts, const String& paramId)
    {
        if (auto* choice = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (paramId)))
            box.addItemList (choice->choices, 1);   // item id = index + 1, as ComboBoxAttachment expects
        else
            jassertfalse;
    }

    String percent (double v) { return String (roundToInt (v * 100.0)) + "%"; }

    constexpr int rowH = 28, knobPitch = 80, comboY = 44, knobY = StylePanel::knobRowY;
}

// =============================================================================================
StylePanel::ModeSwitch::ModeSwitch (RangedAudioParameter& param, std::function<void (int)> changed)
    : attachment (param, [this] (float v)
                  {
                      index = jlimit (0, numSegments - 1, roundToInt (v));
                      if (onModeChanged)
                          onModeChanged (index);
                      repaint();
                  }),
      onModeChanged (std::move (changed))
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

Rectangle<float> StylePanel::ModeSwitch::segment (int i) const
{
    const float w = (float) getWidth() / (float) numSegments;
    return { w * (float) i, 0.0f, w, (float) getHeight() };
}

void StylePanel::ModeSwitch::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Hairline baseline across the whole control; the accent underline sits on top of it.
    g.setColour (Theme::line);
    g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);

    g.setFont (Theme::label());
    for (int i = 0; i < numSegments; ++i)
    {
        const auto col = Theme::dim.interpolatedWith (Theme::text, jmax (sel[(size_t) i].value, 0.6f * hover[(size_t) i].value));
        g.setColour (col);
        g.drawText (modeNames[i], segment (i).withTrimmedBottom (3.0f), Justification::centred, false);
    }

    const float segW = r.getWidth() / (float) numSegments;
    const float lineW = segW * 0.56f;
    const float x = segW * underline.value + (segW - lineW) * 0.5f;
    const Rectangle<float> bar (x, r.getBottom() - 2.0f, lineW, 2.0f);
    Path p;
    p.addRoundedRectangle (bar, 1.0f);
    ChordLookAndFeel::drawGlow (g, p, Theme::accent, 0.7f, 1.0f);
    g.setColour (Theme::accent);
    g.fillPath (p);
}

void StylePanel::ModeSwitch::mouseDown (const MouseEvent& e)
{
    const int i = jlimit (0, numSegments - 1, (int) ((float) e.x / ((float) getWidth() / (float) numSegments)));
    if (i != index)
        attachment.setValueAsCompleteGesture ((float) i);
}

bool StylePanel::ModeSwitch::tickAnimations (float dt)
{
    underline.setTarget ((float) index);
    if (! primed)
        underline.snap (underline.target);

    bool moving = AnimUtil::tick (underline, dt);
    const auto mouse = getMouseXYRelative();
    for (int i = 0; i < numSegments; ++i)
    {
        sel[(size_t) i].setTarget (i == index ? 1.0f : 0.0f);
        hover[(size_t) i].setTarget (isMouseOver() && segment (i).contains (mouse.toFloat()) ? 1.0f : 0.0f);
        if (! primed)
            sel[(size_t) i].snap (sel[(size_t) i].target);
        moving = AnimUtil::tick (sel[(size_t) i], dt) | moving;
        moving = AnimUtil::tick (hover[(size_t) i], dt) | moving;
    }
    primed = true;
    return moving;
}

// =============================================================================================
StylePanel::StylePanel (CosmicChordsProcessor& p)
    : processor (p),
      modeSwitch (*p.apvts.getParameter ("mode"), [this] (int m) { setMode (m); })
{
    using ComboAtt = AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = AudioProcessorValueTreeState::SliderAttachment;

    fillChoices (rhythmBox, processor.apvts, "rhythm");
    fillChoices (arpBox, processor.apvts, "arp");
    fillChoices (rateBox, processor.apvts, "rate");
    rhythmAtt = std::make_unique<ComboAtt> (processor.apvts, "rhythm", rhythmBox);
    arpAtt = std::make_unique<ComboAtt> (processor.apvts, "arp", arpBox);
    rateAtt = std::make_unique<ComboAtt> (processor.apvts, "rate", rateBox);

    gateAtt = std::make_unique<SliderAtt> (processor.apvts, "gate", gate.getSlider());
    swingAtt = std::make_unique<SliderAtt> (processor.apvts, "swing", swing.getSlider());
    octavesAtt = std::make_unique<SliderAtt> (processor.apvts, "arpOctaves", octaves.getSlider());
    humanizeAtt = std::make_unique<SliderAtt> (processor.apvts, "humanize", humanize.getSlider());

    gate.formatValue = percent;
    swing.formatValue = percent;
    humanize.formatValue = [] (double v) { return v < 0.005 ? String ("Off") : percent (v); };
    octaves.formatValue = [] (double v) { return String (roundToInt (v)) + (roundToInt (v) == 1 ? " octave" : " octaves"); };

    reroll.onClick = [this] { processor.newHumanizeSeed(); };

    topLine.setClickingTogglesState (true);
    topLine.setTooltip ("Play and export the melody that comes with Cosmic Pack progressions");
    topLineAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "topline", topLine);
    addAndMakeVisible (topLine);

    addAndMakeVisible (modeSwitch);
    for (Component* c : std::initializer_list<Component*> { &rhythmBox, &arpBox, &rateBox, &gate, &swing, &octaves, &humanize, &reroll })
    {
        addChildComponent (c);
        Managed m;
        m.component = c;
        controls.push_back (m);
    }

    setMode (modeSwitch.getIndex());
}

StylePanel::~StylePanel()
{
    topLineAtt.reset();
    rhythmAtt.reset(); arpAtt.reset(); rateAtt.reset();
    gateAtt.reset(); swingAtt.reset(); octavesAtt.reset(); humanizeAtt.reset();
}

StylePanel::Managed& StylePanel::managed (Component& c)
{
    for (auto& m : controls)
        if (m.component == &c)
            return m;
    jassertfalse;
    return controls.front();
}

void StylePanel::setMode (int newMode)
{
    mode = newMode;
    if (! controls.empty())
        layoutForMode();
}

void StylePanel::layoutForMode()
{
    for (auto& m : controls)
        m.shown = false;

    auto inner = getLocalBounds().reduced (16, 0);
    const int x0 = inner.getX();
    auto place = [&] (Component& c, Rectangle<int> r)
    {
        auto& m = managed (c);
        m.shown = true;
        m.target = r;
    };

    const int knobW = knobPitch, knobH = LabelledKnob::defaultKnobSize + LabelledKnob::textHeight;
    auto knobAt = [&] (Component& c, int slot) { place (c, { x0 + slot * knobPitch, knobY, knobW, knobH }); };

    int humanizeSlot = 0;
    if (mode == 1)   // Rhythm
    {
        place (rhythmBox, { x0, comboY, 196, rowH });
        knobAt (gate, 0);
        knobAt (swing, 1);
        humanizeSlot = 2;
    }
    else if (mode == 2)   // Arp
    {
        place (arpBox, { x0, comboY, 196, rowH });
        place (rateBox, { x0 + 204, comboY, 120, rowH });
        knobAt (gate, 0);
        knobAt (swing, 1);
        knobAt (octaves, 2);
        humanizeSlot = 3;
    }
    knobAt (humanize, humanizeSlot);
    place (reroll, { x0 + (humanizeSlot + 1) * knobPitch + 6, knobY + LabelledKnob::defaultKnobSize / 2 - 13, 78, 26 });

    // Hidden controls keep their last position (they fade out in place); shown ones without a
    // position yet start where they belong.
    for (auto& m : controls)
    {
        if (m.shown && m.component->getBounds().isEmpty())
            m.x.snap ((float) m.target.getX());
        m.x.setTarget ((float) (m.shown ? m.target.getX() : m.component->getX()));
        m.alpha.setTarget (m.shown ? 1.0f : 0.0f);
    }
    heldHint.setTarget (mode == 0 ? 1.0f : 0.0f);
}

void StylePanel::paint (Graphics& g)
{
    Theme::drawPanel (g, getLocalBounds().toFloat());

    if (heldHint.value > 0.001f)
    {
        const float a = heldHint.value * heldHint.value;
        g.setFont (Theme::uiFont (12.0f, "Regular"));
        g.setColour (Theme::dim.withMultipliedAlpha (a));
        g.drawText (String::fromUTF8 ("Held chords \xc2\xb7 each chord sustains for its full length"),
                    Rectangle<int> (16, comboY, getWidth() - 32, rowH).toFloat().translated (0.0f, 4.0f * (1.0f - a)),
                    Justification::centredLeft, true);
    }
}

void StylePanel::resized()
{
    modeSwitch.setBounds (16, 8, 228, 30);
    topLine.setBounds (getWidth() - 16 - 92, 10, 92, 26);
    layoutForMode();
    for (auto& m : controls)
    {
        if (! primed)
        {
            m.x.snap (m.x.target);
            m.alpha.snap (m.alpha.target);
        }
        m.component->setBounds (m.target.withX (roundToInt (m.x.value)));
    }
    if (! primed)
        heldHint.snap (heldHint.target);
    primed = true;
}

bool StylePanel::tickAnimations (float dt)
{
    bool moving = AnimUtil::tick (heldHint, dt);
    for (auto& m : controls)
    {
        const bool a = AnimUtil::tick (m.alpha, dt);
        const bool x = AnimUtil::tick (m.x, dt);
        if (! (a || x || m.component->isVisible() != (m.alpha.value > 0.001f)))
            continue;

        // Fade + a short upward slide on entry; hidden once fully faded so they take no clicks.
        const float dy = 6.0f * (1.0f - m.alpha.value);
        m.component->setBounds (m.target.withX (roundToInt (m.x.value)).translated (0, roundToInt (dy)));
        m.component->setAlpha (m.alpha.value * m.alpha.value);   // squared: outgoing clears before incoming reads
        m.component->setVisible (m.alpha.value > 0.001f);
        moving = true;
    }
    return moving;
}
