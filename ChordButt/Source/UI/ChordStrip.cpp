#include "UI/ChordStrip.h"
#include "UI/Theme.h"
#include "Core/MidiExport.h"
#include "Core/Palette.h"

using namespace juce;

namespace
{
    constexpr float slotGap = 6.0f, maxBarWidth = 120.0f, wellPad = 6.0f;
    constexpr int sidePad = 20, playSize = 52, dragW = 124, dragH = 52, innerGap = 10;

    String lengthLabel (double bars, bool longForm)
    {
        const String n = bars < 0.75 ? String::fromUTF8 ("\xc2\xbd") : String (roundToInt (bars));
        if (! longForm)
            return n;
        return n + (bars > 1.5 ? " BARS" : " BAR");
    }
}

// =============================================================================================
void ChordStrip::PlayButton::paint (Graphics& g)
{
    const float en = 0.35f + 0.65f * enabled.value, p = playing.value;
    auto circle = getLocalBounds().toFloat().withSizeKeepingCentre (40.0f, 40.0f);

    Path ring;
    ring.addEllipse (circle.reduced (0.5f));
    ChordLookAndFeel::drawGlow (g, ring, Theme::accent, 0.8f * p * en, 1.0f);

    g.setColour (Theme::surface2.brighter (0.08f * hover.value).darker (0.15f * down.value).withMultipliedAlpha (en));
    g.fillEllipse (circle);
    g.setColour (Theme::line.interpolatedWith (Theme::dim, 0.4f * hover.value).interpolatedWith (Theme::accent, p).withMultipliedAlpha (en));
    g.strokePath (ring, PathStrokeType (Theme::lineWidth));

    const auto c = circle.getCentre();
    const auto iconColour = Theme::text.interpolatedWith (Theme::accent, p);

    if (p < 0.999f)   // play triangle (optically centred)
    {
        Path tri;
        tri.addTriangle (c.x - 4.0f, c.y - 6.5f, c.x - 4.0f, c.y + 6.5f, c.x + 7.0f, c.y);
        g.setColour (iconColour.withMultipliedAlpha ((1.0f - p) * en));
        g.fillPath (tri);
    }
    if (p > 0.001f)   // stop square
    {
        g.setColour (iconColour.withMultipliedAlpha (p * en));
        g.fillRoundedRectangle (Rectangle<float> (11.0f, 11.0f).withCentre (c), 1.5f);
    }
}

void ChordStrip::PlayButton::mouseUp (const MouseEvent& e)
{
    if (isEnabled() && getLocalBounds().contains (e.getPosition()) && onClick)
        onClick();
}

bool ChordStrip::PlayButton::tickAnimations (float dt)
{
    playing.setTarget (playingTarget ? 1.0f : 0.0f);
    hover.setTarget (isMouseOver() && isEnabled() ? 1.0f : 0.0f);
    down.setTarget (isMouseButtonDown() && isEnabled() ? 1.0f : 0.0f);
    enabled.setTarget (isEnabled() ? 1.0f : 0.0f);
    if (! primed)
        enabled.snap (enabled.target);
    primed = true;

    bool moving = false;
    for (auto* a : { &playing, &hover, &down, &enabled })
        moving = AnimUtil::tick (*a, dt) || moving;
    return moving;
}

// =============================================================================================
void ChordStrip::DragButton::paint (Graphics& g)
{
    const float en = enabled.value, h = jmax (hover.value, forcedHighlight ? 1.0f : 0.0f);
    const float alpha = 0.35f + 0.65f * en;
    auto r = getLocalBounds().toFloat().reduced (6.0f);

    Path outline;
    outline.addRoundedRectangle (r, Theme::cornerRadius);
    // Neutral at rest (like any button); accent border, wash and glow only on hover / while dragging.
    const float active = en * h;
    ChordLookAndFeel::drawGlow (g, outline, Theme::accent, active, 1.0f);

    g.setColour (Theme::surface2.interpolatedWith (Theme::accent, 0.08f * active).withMultipliedAlpha (alpha));
    g.fillPath (outline);
    g.setColour (Theme::line.interpolatedWith (Theme::accent, active).withMultipliedAlpha (alpha));
    g.strokePath (outline, PathStrokeType (Theme::lineWidth));

    // Grip: 2 x 3 dots, then the label.
    const auto textColour = Theme::dim.interpolatedWith (Theme::text, 0.8f * en).interpolatedWith (Theme::accent, active)
                                .withMultipliedAlpha (alpha);
    g.setColour (textColour);
    const float gx = r.getX() + 14.0f, gy = r.getCentreY();
    for (int col = 0; col < 2; ++col)
        for (int row = -1; row <= 1; ++row)
            g.fillEllipse (gx + (float) col * 4.0f - 1.1f, gy + (float) row * 4.0f - 1.1f, 2.2f, 2.2f);

    g.setFont (Theme::label());
    g.drawText ("DRAG MIDI", r.withTrimmedLeft (22.0f), Justification::centred, false);
}

void ChordStrip::DragButton::mouseDrag (const MouseEvent& e)
{
    if (! isEnabled() || dragStarted || e.getDistanceFromDragStart() < 4)
        return;
    dragStarted = true;
    if (onDragStart)
        onDragStart();
}

void ChordStrip::DragButton::enablementChanged()
{
    setMouseCursor (isEnabled() ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    repaint();
}

bool ChordStrip::DragButton::tickAnimations (float dt)
{
    hover.setTarget (isMouseOverOrDragging() && isEnabled() ? 1.0f : 0.0f);
    enabled.setTarget (isEnabled() ? 1.0f : 0.0f);
    if (! primed)
        enabled.snap (enabled.target);
    primed = true;
    const bool h = AnimUtil::tick (hover, dt);
    const bool e = AnimUtil::tick (enabled, dt);
    return h || e;
}

// =============================================================================================
ChordStrip::ChordStrip (ChordButtProcessor& p)
    : processor (p)
{
    playButton.onClick = [this]
    {
        if (processor.strip().slots().empty())
            return;
        if (onBeforePlay)
            onBeforePlay();
        if (processor.preview().isPlaying())
            processor.preview().stop();
        else
            processor.preview().play();
    };
    dragButton.onDragStart = [this]
    {
        String label;
        for (int i = 0; i < (int) vis.size(); ++i)
            label << (i > 0 ? String::fromUTF8 (" \xe2\x80\x93 ") : String()) << vis[(size_t) i].name.current();
        startExternalDrag ("strip", label);
    };
    dragButton.enablementChanged();

    addAndMakeVisible (playButton);
    addAndMakeVisible (dragButton);
    refresh();
}

// ---- geometry ----

Rectangle<float> ChordStrip::well() const
{
    auto r = getLocalBounds().reduced (sidePad, 0);
    r.removeFromLeft (playSize + innerGap);
    r.removeFromRight (dragW + innerGap);
    return r.toFloat().withSizeKeepingCentre ((float) r.getWidth(), 60.0f);
}

Rectangle<float> ChordStrip::slotArea() const { return well().reduced (wellPad); }

float ChordStrip::barWidth (double totalBars) const
{
    if (totalBars <= 0.0)
        return maxBarWidth;
    return jmin (maxBarWidth, (slotArea().getWidth() + slotGap) / (float) totalBars);
}

std::vector<ChordStrip::Span> ChordStrip::layout (const std::vector<int>& order) const
{
    const auto& slots = processor.strip().slots();
    auto lengthOf = [&] (int i) { return i == phantomSlot ? 1.0 : slots[(size_t) i].lengthBars; };
    double total = 0.0;
    for (int i : order)
        total += lengthOf (i);

    const float bw = barWidth (total);
    std::vector<Span> spans;
    float x = slotArea().getX();
    for (int i : order)
    {
        const float span = (float) lengthOf (i) * bw;
        spans.push_back ({ x, span - slotGap });
        x += span;
    }
    return spans;
}

std::vector<int> ChordStrip::displayOrder() const
{
    const bool dragging = reordering && validSlot (downSlot);
    std::vector<int> order;
    for (int i = 0; i < (int) vis.size(); ++i)
        if (! (dragging && i == downSlot))
            order.push_back (i);
    if (dragging)
        order.insert (order.begin() + jlimit (0, (int) order.size(), dropIndex), downSlot);
    else if (insertIndex >= 0)
        order.insert (order.begin() + jlimit (0, (int) order.size(), insertIndex), phantomSlot);
    return order;
}

int ChordStrip::insertIndexAt (float x) const
{
    // Decided on the committed layout (without the placeholder) so the index does not flip back and
    // forth while the other slots slide aside.
    std::vector<int> order;
    for (int i = 0; i < (int) vis.size(); ++i)
        order.push_back (i);
    const auto spans = layout (order);

    int best = 0;
    float bestDist = std::numeric_limits<float>::max();
    for (int k = 0; k <= (int) spans.size(); ++k)
    {
        const float edge = k < (int) spans.size() ? spans[(size_t) k].x
                                                  : (spans.empty() ? slotArea().getX() : spans.back().x + spans.back().w + slotGap);
        const float d = std::abs (edge - x);
        if (d < bestDist)
        {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

int ChordStrip::computeDropIndex() const
{
    std::vector<int> rest;
    for (int i = 0; i < (int) vis.size(); ++i)
        if (i != downSlot)
            rest.push_back (i);

    int best = downSlot;
    float bestDist = std::numeric_limits<float>::max();
    for (int k = 0; k <= (int) rest.size(); ++k)
    {
        auto order = rest;
        order.insert (order.begin() + k, downSlot);
        const auto spans = layout (order);
        const float d = std::abs (spans[(size_t) k].x - dragX);
        if (d < bestDist)
        {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

Rectangle<float> ChordStrip::slotRect (const SlotVis& v) const
{
    auto area = slotArea();
    return { v.x.value, area.getY(), jmax (0.0f, v.w.value), area.getHeight() };
}

Rectangle<float> ChordStrip::removeRect (Rectangle<float> slot) const
{
    return { slot.getRight() - 18.0f, slot.getY() + 2.0f, 16.0f, 16.0f };
}

Rectangle<float> ChordStrip::chipRect (Rectangle<float> slot, float longForm) const
{
    // Width grows with the animated long-form amount; in tiny slots the chip drifts to the centre.
    // Everything is a continuous function of animated values, so nothing snaps.
    const float w = jmin (20.0f + 24.0f * longForm, jmax (0.0f, slot.getWidth() - 4.0f));
    const float toLeft = jlimit (0.0f, 1.0f, (slot.getWidth() - 28.0f) / 12.0f);
    const float x = jmap (toLeft, slot.getCentreX() - w * 0.5f, slot.getX() + 5.0f);
    return { x, slot.getBottom() - 19.0f, w, 14.0f };
}

Rectangle<float> ChordStrip::getSlotBounds (int index) const
{
    return validSlot (index) ? slotRect (vis[(size_t) index]) : Rectangle<float>();
}

int ChordStrip::slotAt (Point<float> p) const
{
    if (reordering && validSlot (downSlot) && slotRect (vis[(size_t) downSlot]).contains (p))
        return downSlot;
    for (int i = 0; i < (int) vis.size(); ++i)
        if (slotRect (vis[(size_t) i]).contains (p))
            return i;
    return -1;
}

ChordStrip::Part ChordStrip::partAt (int slot, Point<float> p) const
{
    if (! validSlot (slot))
        return Part::none;
    const auto r = slotRect (vis[(size_t) slot]);
    if (r.getWidth() > 40.0f && removeRect (r).expanded (2.0f).contains (p))
        return Part::remove;
    if (chipRect (r, vis[(size_t) slot].chipLong.value).expanded (2.0f).contains (p))
        return Part::lengthChip;
    return Part::body;
}

float ChordStrip::beatToX (double beat) const
{
    const auto& slots = processor.strip().slots();
    double start = 0.0;
    for (size_t i = 0; i < slots.size() && i < vis.size(); ++i)
    {
        const double len = slots[i].lengthBars * 4.0;
        if (beat < start + len || i + 1 == slots.size())
        {
            const float frac = (float) jlimit (0.0, 1.0, (beat - start) / len);
            return vis[i].x.value + frac * (vis[i].w.value + slotGap);
        }
        start += len;
    }
    return slotArea().getX();
}

String ChordStrip::slotName (int index, bool forFileName) const
{
    const auto& slot = processor.strip().slots()[(size_t) index];
    const int complexity = jlimit (0, 3, roundToInt (processor.apvts.getRawParameterValue ("complexity")->load()));
    const auto rel = cb::applyComplexity (slot.chord, processor.scale(), complexity);
    auto name = Theme::pretty (cb::chordNameInKey (rel, processor.tonic(), processor.scale()));
    return forFileName ? name.replaceCharacter ('/', '_') : name;
}

ScaledImage ChordStrip::dragImage (const String& text) const
{
    const auto font = Theme::label();
    const int w = jmin (420, roundToInt (GlyphArrangement::getStringWidth (font, text)) + 36), h = 30;
    const float scale = 2.0f;
    Image img (Image::ARGB, roundToInt ((float) w * scale), roundToInt ((float) h * scale), true);
    {
        Graphics g (img);
        g.addTransform (AffineTransform::scale (scale));
        auto r = Rectangle<float> ((float) w, (float) h).reduced (1.0f);
        g.setColour (Theme::surface2.withAlpha (0.94f));
        g.fillRoundedRectangle (r, Theme::cornerRadius);
        g.setColour (Theme::accent);
        g.drawRoundedRectangle (r, Theme::cornerRadius, Theme::lineWidth);
        g.setFont (font);
        g.drawFittedText (text, r.reduced (12.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.8f);
    }
    return ScaledImage (img, scale);
}

// ---- model edits ----

void ChordStrip::commit()
{
    processor.stripChanged();
    refresh();
}

void ChordStrip::cancelGesture()
{
    if (reordering && validSlot (downSlot))
        vis[(size_t) downSlot].lift.setTarget (0.0f);
    reordering = false;
    downSlot = -1;
    dropIndex = -1;
    downPart = Part::none;
}

void ChordStrip::syncIfStale()
{
    if (vis.size() != processor.strip().slots().size())
        refresh();   // refresh() cancels the gesture on a size change
}

void ChordStrip::refresh()
{
    const auto& slots = processor.strip().slots();
    const size_t n = slots.size();

    // The strip was resized (or replaced) under a running gesture: its indices are meaningless now.
    if (vis.size() != n || downSlot >= (int) n)
        cancelGesture();

    while (vis.size() > n)
    {
        ghosts.push_back (vis.back());
        ghosts.back().appear.setTarget (0.0f);
        vis.pop_back();
    }
    while (vis.size() < n)
    {
        SlotVis v;
        v.appear.snap (primed ? 0.0f : 1.0f);
        v.appear.setTarget (1.0f);
        vis.push_back (v);
    }

    for (size_t i = 0; i < n; ++i)
    {
        auto& v = vis[i];
        v.name.set (slotName ((int) i, false));
        v.lengthBars = slots[i].lengthBars;
        v.overrideText.set (slots[i].inversionOverride ? "INV " + String (*slots[i].inversionOverride) : String());
    }

    if (selected >= (int) n)
        setSelected (-1);

    playButton.setEnabled (n > 0);
    dragButton.setEnabled (n > 0);
    empty.setTarget (n == 0 ? 1.0f : 0.0f);
    if (n == 0 && processor.preview().isPlaying() && ! externalPreview())
        processor.preview().stop();
    repaint();
}

void ChordStrip::setSelected (int index)
{
    if (index == selected)
        return;
    selected = index;
    repaint();
    if (onSelectionChanged)
        onSelectionChanged (selected);
}

bool ChordStrip::insertChord (int index, const cb::RelChord& chord)
{
    syncIfStale();
    cancelGesture();
    auto& strip = processor.strip();
    index = jlimit (0, (int) strip.slots().size(), index);

    cb::StripSlot slot;
    slot.chord = chord;
    if (! strip.append (slot))
    {
        flashBorder();
        return false;
    }
    const int last = (int) strip.slots().size() - 1;
    if (index != last)
        strip.move (last, index);

    // The new slot takes over the placeholder's place (if one was open) and fades in from there.
    SlotVis v;
    if (insertIndex >= 0 && phantom.appear.value > 0.001f)
    {
        v.x.snap (phantom.x.value);
        v.w.snap (phantom.w.value);
        v.placed = true;
        v.appear.snap (0.6f);
    }
    else
    {
        v.appear.snap (0.0f);
    }
    v.appear.setTarget (1.0f);
    vis.insert (vis.begin() + index, v);

    if (selected >= index)
        setSelected (selected + 1);
    commit();
    return true;
}

var ChordStrip::padDragDescription (const cb::RelChord& chord, const String& displayName)
{
    auto* o = new DynamicObject();
    o->setProperty ("padChord", true);
    o->setProperty ("root", chord.rootOffset);
    o->setProperty ("quality", (int) chord.quality);
    o->setProperty ("bass", chord.bassOffset);
    o->setProperty ("roman", String (chord.roman));
    o->setProperty ("name", displayName);
    return var (o);
}

bool ChordStrip::decodePadDrag (const var& d, cb::RelChord& chord, String& displayName)
{
    if (! d.isObject() || ! (bool) d.getProperty ("padChord", false))
        return false;
    chord.rootOffset = jlimit (0, 11, (int) d.getProperty ("root", 0));
    chord.quality = (cb::Quality) jlimit ((int) cb::Quality::Maj, (int) cb::Quality::Min13, (int) d.getProperty ("quality", 0));
    chord.bassOffset = jlimit (-1, 11, (int) d.getProperty ("bass", -1));
    chord.roman = d.getProperty ("roman", "").toString().toStdString();
    displayName = d.getProperty ("name", "").toString();
    return true;
}

bool ChordStrip::isInterestedInDragSource (const SourceDetails& details)
{
    cb::RelChord c;
    String n;
    return decodePadDrag (details.description, c, n);
}

void ChordStrip::itemDragEnter (const SourceDetails& details)
{
    syncIfStale();
    cancelGesture();
    if ((int) processor.strip().slots().size() >= cb::Strip::maxSlots)
    {
        flashBorder();   // full: no placeholder, the drop is refused
        return;
    }

    cb::RelChord c;
    String name;
    decodePadDrag (details.description, c, name);
    phantom.name.set (name, false);
    phantom.lengthBars = 1.0;
    insertIndex = insertIndexAt ((float) details.localPosition.x);

    // Open the placeholder in place (narrow, at its gap) and let it grow, rather than flying in.
    const auto order = displayOrder();
    const auto spans = layout (order);
    for (size_t k = 0; k < order.size(); ++k)
        if (order[k] == phantomSlot)
        {
            phantom.x.snap (spans[k].x);
            phantom.w.snap (0.0f);
        }
    phantom.appear.setTarget (1.0f);
}

void ChordStrip::itemDragMove (const SourceDetails& details)
{
    if (insertIndex >= 0)
        insertIndex = insertIndexAt ((float) details.localPosition.x);
}

void ChordStrip::itemDragExit (const SourceDetails&)
{
    insertIndex = -1;
    phantom.appear.setTarget (0.0f);
}

void ChordStrip::itemDropped (const SourceDetails& details)
{
    cb::RelChord chord;
    String name;
    if (insertIndex >= 0 && decodePadDrag (details.description, chord, name))
        insertChord (insertIndex, chord);
    insertIndex = -1;
    phantom.appear.snap (0.0f);
    phantom.appear.setTarget (0.0f);
}

void ChordStrip::removeSlot (int index)
{
    ghosts.push_back (vis[(size_t) index]);
    ghosts.back().appear.setTarget (0.0f);
    vis.erase (vis.begin() + index);
    processor.strip().remove (index);

    if (selected == index)
        setSelected (-1);
    else if (selected > index)
        setSelected (selected - 1);
    commit();
}

void ChordStrip::cycleLength (int index)
{
    const double len = processor.strip().slots()[(size_t) index].lengthBars;
    processor.strip().setLength (index, len < 0.75 ? 1.0 : (len < 1.5 ? 2.0 : 0.5));
    commit();
}

void ChordStrip::flashBorder()
{
    flashHold = 0.14f;
    flash.setTarget (1.0f);
}

void ChordStrip::startExternalDrag (const var& description, const String& label)
{
    if (processor.strip().slots().empty())
        return;
    if (auto* container = DragAndDropContainer::findParentDragContainerFor (this))
    {
        Component* source = description.toString() == "strip" ? static_cast<Component*> (&dragButton) : this;
        container->startDragging (description, source, dragImage (label));
    }
}

bool ChordStrip::prepareExternalDrag (const var& description, StringArray& files)
{
    const auto& slots = processor.strip().slots();
    if (slots.empty())
        return false;

    const auto desc = description.toString();
    std::vector<cb::NoteEvent> events;
    String name;
    double lengthBeats = 0.0;   // the region ends here, so it loops like the preview

    if (desc == "strip")
    {
        events = processor.currentEvents();
        lengthBeats = processor.strip().totalBeats();
        for (int i = 0; i < (int) slots.size(); ++i)
            name << (i > 0 ? "-" : "") << slotName (i, true);
    }
    else if (desc.startsWith ("slot:"))
    {
        const int index = desc.fromFirstOccurrenceOf (":", false, false).getIntValue();
        if (index < 0 || index >= (int) slots.size())
            return false;
        // Only this chord, generated on its own so it starts at beat 0 with the same style.
        const auto voiced = processor.voicedStrip();
        events = cb::generateEvents ({ voiced[(size_t) index] }, processor.currentStyle());
        name = slotName (index, true);
        lengthBeats = voiced[(size_t) index].lengthBeats;
    }
    else
    {
        return false;
    }

    if (events.empty())
        return false;

    const auto file = cb::writeTempMidi (events, processor.hostBpm(), name, lengthBeats);
    if (! file.existsAsFile())   // writeTempMidi returns a non-existent file when writing failed
        return false;

    files.add (file.getFullPathName());
    return true;
}

// ---- component ----

void ChordStrip::resized()
{
    auto r = getLocalBounds().reduced (sidePad, 0);
    playButton.setBounds (r.removeFromLeft (playSize).withSizeKeepingCentre (playSize, playSize));
    dragButton.setBounds (r.removeFromRight (dragW).withSizeKeepingCentre (dragW, dragH));
    for (auto& v : vis)
        v.placed = false;   // re-place without animating after a size change
}

void ChordStrip::paintSlot (Graphics& g, const SlotVis& v, float alpha) const
{
    alpha *= v.appear.value;
    if (alpha <= 0.001f)
        return;

    auto r = slotRect (v);
    if (r.getWidth() < 2.0f)
        return;
    r = r.reduced (0.0f, 5.0f * (1.0f - v.appear.value)).translated (0.0f, -2.0f * v.lift.value);
    const float radius = Theme::cornerRadius;
    const float sel = v.selected.value, hov = v.hover.value, play = v.playing.value;

    if (v.lift.value > 0.001f)   // soft drop shadow while being dragged
    {
        g.setColour (Colours::black.withAlpha (0.35f * v.lift.value * alpha));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.0f), radius);
    }

    g.setGradientFill (ColourGradient (Theme::surface2.brighter (0.06f + 0.08f * hov).withMultipliedAlpha (alpha), r.getX(), r.getY(),
                                       Theme::surface2.withMultipliedAlpha (alpha), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    if (play > 0.001f)   // the sounding chord glows from the inside
    {
        g.setColour (Theme::accent.withAlpha (0.07f * play * alpha));
        g.fillRoundedRectangle (r, radius);
        for (int k = 1; k <= 5; ++k)
        {
            g.setColour (Theme::accent.withAlpha (0.08f * play * alpha * (1.0f - (float) (k - 1) / 5.0f)));
            g.drawRoundedRectangle (r.reduced ((float) k), jmax (1.0f, radius - (float) k * 0.5f), 1.0f);
        }
    }

    const auto border = Theme::line.interpolatedWith (Theme::dim, 0.4f * hov)
                            .interpolatedWith (Theme::accent, jmax (sel, 0.55f * play, 0.5f * v.lift.value));
    g.setColour (border.withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, Theme::lineWidth);

    if (sel > 0.001f)   // selection tab on the top edge
    {
        g.setColour (Theme::accent.withAlpha (sel * alpha));
        g.fillRoundedRectangle (r.getCentreX() - 9.0f, r.getY() + 3.0f, 18.0f, 2.0f, 1.0f);
    }

    // Name: shrinks (down to 9 px) rather than squashing in narrow slots, then truncates.
    const float inset = jmap (jlimit (0.0f, 1.0f, (r.getWidth() - 40.0f) / 20.0f), 2.0f, 6.0f);   // continuous in width
    auto nameRow = Rectangle<float> (r.getX() + inset, r.getY() + 5.0f, r.getWidth() - 2.0f * inset, 20.0f);
    const auto nameColour = Theme::text.interpolatedWith (Theme::accent, 0.8f * play);
    v.name.draw ([&] (const String& s, float a, float dy)
    {
        auto font = Theme::uiFont (13.5f, "Semibold");
        const float textW = GlyphArrangement::getStringWidth (font, s);
        if (textW > nameRow.getWidth())
            font = Theme::uiFont (jmax (9.0f, 13.5f * nameRow.getWidth() / textW), "Semibold");
        g.setFont (font);
        g.setColour (nameColour.withMultipliedAlpha (a * alpha));
        g.drawText (s, nameRow.translated (0.0f, dy), Justification::centred, true);
    }, 2.0f);

    // Length chip (text cross-fades, incl. the short/long form switch; width follows chipLong).
    const auto chip = chipRect (r, v.chipLong.value);
    g.setColour (Theme::line.withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (chip.reduced (0.5f), 3.0f, Theme::lineWidth);
    g.setFont (Theme::uiFont (9.5f, "Semibold", 0.08f));
    v.chipText.draw ([&] (const String& s, float a, float)
    {
        g.setColour (Theme::dim.withMultipliedAlpha (a * alpha));
        g.drawText (s, chip, Justification::centred, false);
    }, 0.0f);

    // Per-chord voicing marker: fades with its text and with the room the slot has for it.
    const float room = jlimit (0.0f, 1.0f, (r.getWidth() - chip.getWidth() - 44.0f) / 10.0f);
    if (room > 0.001f)
        v.overrideText.draw ([&] (const String& s, float a, float dy)
        {
            g.setColour (Theme::accent.withMultipliedAlpha (0.85f * alpha * a * room));
            g.drawText (s, Rectangle<float> (r.getRight() - 46.0f, chip.getY() + dy, 40.0f, chip.getHeight()),
                        Justification::centredRight, false);
        }, 2.0f);

    // Remove "×" (on hover / selection).
    const float xa = jmax (hov, sel) * alpha * jlimit (0.0f, 1.0f, (r.getWidth() - 36.0f) / 10.0f);   // fades out in narrow slots
    if (xa > 0.001f)
    {
        const auto c = removeRect (r).getCentre();
        const float d = 3.2f;
        Path cross;
        cross.startNewSubPath (c.x - d, c.y - d); cross.lineTo (c.x + d, c.y + d);
        cross.startNewSubPath (c.x + d, c.y - d); cross.lineTo (c.x - d, c.y + d);
        g.setColour (Theme::dim.withMultipliedAlpha (xa));
        g.strokePath (cross, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void ChordStrip::paint (Graphics& g)
{
    g.fillAll (Theme::surface);
    g.setColour (Theme::line);
    g.fillRect (0, 0, getWidth(), 1);

    // Recessed well; flashes accent once when a chord could not be added (strip full).
    const auto w = well();
    g.setColour (Theme::bg);
    g.fillRoundedRectangle (w, Theme::cornerRadius + 2.0f);
    Path wellPath;
    wellPath.addRoundedRectangle (w.reduced (0.5f), Theme::cornerRadius + 2.0f);
    ChordLookAndFeel::drawGlow (g, wellPath, Theme::accent, flash.value, 1.0f);
    g.setColour (Theme::line.interpolatedWith (Theme::accent, flash.value));
    g.strokePath (wellPath, PathStrokeType (Theme::lineWidth));

    if (empty.value > 0.001f)
    {
        g.setColour (Theme::dim.withMultipliedAlpha (empty.value));
        g.setFont (Theme::uiFont (12.5f, "Regular"));
        g.drawText ("Drag pads here or open the library to add chords", w.translated (0.0f, 3.0f * (1.0f - empty.value)),
                    Justification::centred, false);
    }

    for (const auto& ghost : ghosts)
        paintSlot (g, ghost, 1.0f);

    if (phantom.appear.value > 0.001f)   // drop placeholder: dashed accent outline with the chord's name
    {
        const float a = phantom.appear.value;
        auto r = slotRect (phantom).reduced (0.0f, 4.0f * (1.0f - a));
        if (r.getWidth() > 2.0f)
        {
            g.setColour (Theme::accent.withAlpha (0.06f * a));
            g.fillRoundedRectangle (r, Theme::cornerRadius);
            Path outline, dashed;
            outline.addRoundedRectangle (r.reduced (0.5f), Theme::cornerRadius);
            const float dashes[] { 4.0f, 3.0f };
            PathStrokeType (Theme::lineWidth).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (Theme::accent.withAlpha (0.85f * a));
            g.fillPath (dashed);
            g.setFont (Theme::uiFont (13.5f, "Semibold"));
            phantom.name.draw ([&] (const String& s, float ta, float)
            {
                g.setColour (Theme::accent.withAlpha (0.75f * a * ta));
                g.drawFittedText (s, r.reduced (4.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
            });
        }
    }
    for (int i = 0; i < (int) vis.size(); ++i)
        if (! (reordering && validSlot (downSlot) && i == downSlot))
            paintSlot (g, vis[(size_t) i], 1.0f);

    if (playheadAlpha.value > 0.001f && ! vis.empty())
    {
        const float x = beatToX (playheadBeat);
        Path line;
        line.startNewSubPath (x, w.getY() + 3.0f);
        line.lineTo (x, w.getBottom() - 3.0f);
        ChordLookAndFeel::drawGlow (g, line, Theme::accent, 0.8f * playheadAlpha.value, 1.0f);
        g.setColour (Theme::accent.withAlpha (playheadAlpha.value));
        g.strokePath (line, PathStrokeType (1.5f));
    }

    if (reordering && validSlot (downSlot))
        paintSlot (g, vis[(size_t) downSlot], 1.0f);
}

// ---- mouse ----

void ChordStrip::mouseDown (const MouseEvent& e)
{
    syncIfStale();
    downSlot = slotAt (e.position);
    downPart = partAt (downSlot, e.position);
    reordering = false;
    externalDrag = false;
}

void ChordStrip::mouseDrag (const MouseEvent& e)
{
    syncIfStale();
    if (! validSlot (downSlot) || downPart != Part::body || externalDrag)
        return;

    if (! reordering)
    {
        if (e.getDistanceFromDragStart() < 5)
            return;
        reordering = true;
        grabOffset = e.mouseDownPosition.x - vis[(size_t) downSlot].x.value;
        vis[(size_t) downSlot].lift.setTarget (1.0f);
    }

    const auto area = slotArea();
    dragX = jlimit (area.getX() - 20.0f, area.getRight() - vis[(size_t) downSlot].w.value + 20.0f, e.position.x - grabOffset);
    dropIndex = computeDropIndex();

    // Leaving the plugin window turns the reorder into an external file drag of this chord.
    if (auto* container = DragAndDropContainer::findParentDragContainerFor (this))
        if (auto* top = dynamic_cast<Component*> (container))
            if (! top->getLocalBounds().contains (top->getLocalPoint (this, e.position).roundToInt()))
            {
                reordering = false;
                vis[(size_t) downSlot].lift.setTarget (0.0f);
                externalDrag = true;
                startExternalDrag ("slot:" + String (downSlot), vis[(size_t) downSlot].name.current());
            }
}

void ChordStrip::mouseUp (const MouseEvent& e)
{
    syncIfStale();
    const int from = downSlot;
    downSlot = -1;

    if (externalDrag)
    {
        externalDrag = false;
        return;
    }

    if (reordering)
    {
        reordering = false;
        if (! validSlot (from))
            return;
        vis[(size_t) from].lift.setTarget (0.0f);
        const int to = dropIndex;
        if (to != from && to >= 0 && to < (int) vis.size())
        {
            auto moved = vis[(size_t) from];
            vis.erase (vis.begin() + from);
            vis.insert (vis.begin() + to, moved);
            processor.strip().move (from, to);

            int sel = selected;
            if (sel == from)                     sel = to;
            else if (from < sel && sel <= to)    --sel;
            else if (to <= sel && sel < from)    ++sel;
            if (sel != selected)
                setSelected (sel);
            commit();
        }
        return;
    }

    const int upSlot = slotAt (e.position);
    if (validSlot (from) && upSlot == from && partAt (upSlot, e.position) == downPart)
    {
        switch (downPart)
        {
            case Part::remove:     removeSlot (from); break;
            case Part::lengthChip: cycleLength (from); break;
            case Part::body:       setSelected (selected == from ? -1 : from); break;
            case Part::none:       break;
        }
    }
    else if (from < 0 && upSlot < 0 && well().contains (e.position))
    {
        setSelected (-1);
    }
}

void ChordStrip::mouseMove (const MouseEvent& e)
{
    hovered = slotAt (e.position);
}

void ChordStrip::mouseExit (const MouseEvent&)
{
    hovered = -1;
}

// ---- animation ----

bool ChordStrip::tickAnimations (float dt)
{
    syncIfStale();
    bool moving = false;

    const bool playingNow = processor.preview().isPlaying() && ! externalPreview();
    const double beat = processor.preview().playheadBeat();
    if (playingNow != isPlaying || (playingNow && ! exactlyEqual (beat, playheadBeat)))
        moving = true;
    isPlaying = playingNow;
    playheadBeat = beat;
    playButton.setPlaying (isPlaying);
    playheadAlpha.setTarget (isPlaying ? 1.0f : 0.0f);

    if (flashHold > 0.0f)
    {
        flashHold -= dt;
        if (flashHold <= 0.0f)
            flash.setTarget (0.0f);
    }
    if (! primed)
        empty.snap (empty.target);
    primed = true;
    for (auto* a : { &playheadAlpha, &flash, &empty })
        moving = AnimUtil::tick (*a, dt) || moving;

    // Slot positions: committed layout, or the prospective order while reordering.
    const auto order = displayOrder();
    const auto spans = layout (order);
    for (size_t k = 0; k < order.size(); ++k)
    {
        if (order[k] == phantomSlot)
        {
            phantom.x.setTarget (spans[k].x);
            phantom.w.setTarget (spans[k].w);
            continue;
        }
        auto& v = vis[(size_t) order[k]];
        v.w.setTarget (spans[k].w);
        if (reordering && order[k] == downSlot)
            v.x.snap (dragX);
        else
            v.x.setTarget (spans[k].x);
        if (! v.placed)
        {
            v.x.snap (v.x.target);
            v.w.snap (v.w.target);
            v.placed = true;
        }
    }

    int playingSlot = -1;
    if (isPlaying)
    {
        double start = 0.0;
        const auto& slots = processor.strip().slots();
        for (size_t i = 0; i < slots.size(); ++i)
        {
            const double len = slots[i].lengthBars * 4.0;
            if (playheadBeat >= start && playheadBeat < start + len)
            {
                playingSlot = (int) i;
                break;
            }
            start += len;
        }
    }

    for (int i = 0; i < (int) vis.size(); ++i)
    {
        auto& v = vis[(size_t) i];
        v.selected.setTarget (i == selected ? 1.0f : 0.0f);
        v.hover.setTarget (i == hovered && ! reordering && isMouseOver() ? 1.0f : 0.0f);
        v.playing.setTarget (i == playingSlot ? 1.0f : 0.0f);
        const bool longForm = v.w.target > 96.0f;   // decided on the target width, so it switches once
        v.chipLong.setTarget (longForm ? 1.0f : 0.0f);
        const bool firstFrame = v.chipText.current().isEmpty();
        v.chipText.set (lengthLabel (v.lengthBars, longForm));   // first set() shows without fading
        if (firstFrame)
            v.chipLong.snap (v.chipLong.target);
        for (auto* a : { &v.x, &v.w, &v.selected, &v.hover, &v.playing, &v.appear, &v.lift, &v.chipLong })
            moving = AnimUtil::tick (*a, dt) || moving;
        moving = v.name.tick (dt) || moving;
        moving = v.chipText.tick (dt) || moving;
        moving = v.overrideText.tick (dt) || moving;
    }

    for (auto* a : { &phantom.x, &phantom.w, &phantom.appear })
        moving = AnimUtil::tick (*a, dt) || moving;
    moving = phantom.name.tick (dt) || moving;

    for (auto& ghost : ghosts)
        moving = AnimUtil::tick (ghost.appear, dt) || moving;
    ghosts.erase (std::remove_if (ghosts.begin(), ghosts.end(),
                                  [] (const SlotVis& gv) { return gv.appear.value <= 0.001f; }),
                  ghosts.end());
    return moving;
}
