#include "UI/LibraryDrawer.h"
#include "UI/Theme.h"
#include "Core/Patterns.h"
#include "Core/Strip.h"

using namespace juce;

namespace
{
    constexpr float slideDistance = 24.0f;
    constexpr int panelInset = 20, panelTopBottom = 14, panelPad = 16, leftWidth = 148, rowHeight = 52,
                  searchHeight = 32, titleHeight = 24, itemHeight = 18, searchWidth = 340;

    const String dash = String::fromUTF8 (" \xe2\x80\x93 ");    // " – "
    const String dot = String::fromUTF8 (" \xc2\xb7 ");         // " · "

    const char* const paramIds[] = { "root", "scale", "inversion", "spread", "octave", "complexity", "mode", "rhythm",
                                     "arp", "rate", "gate", "swing", "arpOctaves", "humanize" };

    float raw (ChordButtProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    cb::VoicingParams globalVoicing (ChordButtProcessor& p)
    {
        cb::VoicingParams v;
        v.inversion = jlimit (0, 3, roundToInt (raw (p, "inversion")));
        v.spread = raw (p, "spread");
        v.octave = jlimit (-2, 2, roundToInt (raw (p, "octave")));
        return v;
    }

    // Soft inner glow along a rounded rect (same treatment as the pressed pads).
    void innerGlow (Graphics& g, Rectangle<float> r, float radius, float amount)
    {
        for (int k = 1; k <= 6; ++k)
        {
            g.setColour (Theme::accent.withAlpha (0.07f * amount * (1.0f - (float) (k - 1) / 6.0f)));
            g.drawRoundedRectangle (r.reduced ((float) k), jmax (1.0f, radius - (float) k * 0.5f), 1.0f);
        }
    }

    // Image blurred once: downscale, gaussian kernel, drawn back up with smooth resampling.
    Image blurred (const Image& source)
    {
        if (! source.isValid())
            return {};
        const int w = jmax (1, source.getWidth() / 4), h = jmax (1, source.getHeight() / 4);
        auto small = source.convertedToFormat (Image::ARGB).rescaled (w, h, Graphics::highResamplingQuality);
        ImageConvolutionKernel kernel (9);
        kernel.createGaussianBlur (2.6f);
        for (int pass = 0; pass < 2; ++pass)
            kernel.applyToImage (small, small, small.getBounds());
        return small;
    }
}

// =============================================================================================
// FilterList

LibraryDrawer::FilterList::FilterList (const String& t, const std::vector<std::string>& names)
    : title (t)
{
    for (auto& n : names)
        items.add (String (n));
    sel.resize ((size_t) items.size());
    hover.resize ((size_t) items.size());
    setMouseCursor (MouseCursor::PointingHandCursor);
}

String LibraryDrawer::FilterList::getSelected() const
{
    return selected >= 0 ? items[selected] : String();
}

void LibraryDrawer::FilterList::setSelected (const String& name)
{
    selected = items.indexOf (name);
    repaint();
}

int LibraryDrawer::FilterList::preferredHeight() const
{
    return titleHeight + items.size() * itemHeight;
}

Rectangle<float> LibraryDrawer::FilterList::itemRect (int index) const
{
    return { 0.0f, (float) (titleHeight + index * itemHeight), (float) getWidth(), (float) itemHeight };
}

int LibraryDrawer::FilterList::itemAt (Point<int> p) const
{
    const int i = (p.y - titleHeight) / itemHeight;
    return p.y >= titleHeight && i >= 0 && i < items.size() ? i : -1;
}

void LibraryDrawer::FilterList::paint (Graphics& g)
{
    g.setFont (Theme::label());
    g.setColour (Theme::dim);
    g.drawText (title, Rectangle<float> (12.0f, 0.0f, (float) getWidth() - 12.0f, (float) titleHeight - 6.0f),
                Justification::centredLeft, false);

    g.setFont (Theme::uiFont (12.5f, "Regular"));
    for (int i = 0; i < items.size(); ++i)
    {
        const float s = sel[(size_t) i].value, h = hover[(size_t) i].value;
        auto r = itemRect (i);

        if (h > 0.001f)
        {
            g.setColour (Theme::surface2.withAlpha (h));
            g.fillRoundedRectangle (r.reduced (0.0f, 1.0f), 4.0f);
        }
        // Thin accent bar on the left; grows from its centre.
        if (s > 0.001f)
        {
            g.setColour (Theme::accent.withAlpha (s));
            g.fillRoundedRectangle (Rectangle<float> (2.0f, r.getHeight() * 0.62f * s).withCentre ({ 3.0f, r.getCentreY() }), 1.0f);
        }

        g.setColour (Theme::dim.interpolatedWith (Theme::text, h).interpolatedWith (Theme::accent, s));
        g.drawText (items[i], r.withTrimmedLeft (12.0f + 2.0f * s), Justification::centredLeft, true);
    }
}

void LibraryDrawer::FilterList::mouseMove (const MouseEvent& e) { hovered = itemAt (e.getPosition()); }
void LibraryDrawer::FilterList::mouseExit (const MouseEvent&) { hovered = -1; }

void LibraryDrawer::FilterList::mouseUp (const MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;
    const int i = itemAt (e.getPosition());
    if (i < 0)
        return;
    selected = (i == selected) ? -1 : i;   // click again to clear
    if (onChange)
        onChange();
}

bool LibraryDrawer::FilterList::tickAnimations (float dt)
{
    bool moving = false;
    for (int i = 0; i < items.size(); ++i)
    {
        auto& s = sel[(size_t) i];
        auto& h = hover[(size_t) i];
        s.setTarget (i == selected ? 1.0f : 0.0f);
        h.setTarget (i == hovered && isMouseOver() ? 1.0f : 0.0f);
        if (! primed)
            s.snap (s.target);
        moving = AnimUtil::tick (s, dt) || moving;
        moving = AnimUtil::tick (h, dt) || moving;
    }
    primed = true;
    return moving;
}

// =============================================================================================
// SearchBox

LibraryDrawer::SearchBox::SearchBox()
{
    setMultiLine (false);
    setFont (Theme::value());
    setJustification (Justification::centredLeft);
    setIndents (30, 0);
    setTextToShowWhenEmpty (String::fromUTF8 ("Search progressions\xe2\x80\xa6"), Theme::dim);
    for (auto id : { TextEditor::backgroundColourId, TextEditor::outlineColourId, TextEditor::focusedOutlineColourId,
                     TextEditor::shadowColourId })
        setColour (id, Colours::transparentBlack);
    setColour (TextEditor::textColourId, Theme::text);
    setColour (TextEditor::highlightColourId, Theme::accent.withAlpha (0.28f));
    setColour (TextEditor::highlightedTextColourId, Theme::text);
    setColour (CaretComponent::caretColourId, Theme::accent);
}

void LibraryDrawer::SearchBox::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::surface2.brighter (0.05f * hover.value));
    g.fillRoundedRectangle (r, Theme::cornerRadius);
    TextEditor::paint (g);
}

void LibraryDrawer::SearchBox::paintOverChildren (Graphics& g)
{
    TextEditor::paintOverChildren (g);

    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float f = focus.value;
    g.setColour (Theme::line.interpolatedWith (Theme::dim, 0.4f * hover.value).interpolatedWith (Theme::accent, 0.75f * f));
    g.drawRoundedRectangle (r, Theme::cornerRadius, Theme::lineWidth);

    // Magnifier.
    const Point<float> c { r.getX() + 15.0f, r.getCentreY() - 1.0f };
    g.setColour (Theme::dim.interpolatedWith (Theme::text, f));
    g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c), 1.3f);
    g.drawLine (c.x + 3.4f, c.y + 3.4f, c.x + 6.5f, c.y + 6.5f, 1.4f);
}

bool LibraryDrawer::SearchBox::tickAnimations (float dt)
{
    focus.setTarget (hasKeyboardFocus (true) ? 1.0f : 0.0f);
    hover.setTarget (isMouseOver (true) ? 1.0f : 0.0f);
    const bool a = AnimUtil::tick (focus, dt);
    const bool b = AnimUtil::tick (hover, dt);
    return a || b;
}

// =============================================================================================
// Row

LibraryDrawer::Row::Row (LibraryDrawer& o)
    : owner (o)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void LibraryDrawer::Row::setEntry (const cb::LibraryEntry* e, int gen)
{
    const bool newContent = gen != shownGeneration;
    if (e == entry && ! newContent)
    {
        refreshText (true);   // same entry: e.g. the key changed, names cross-fade
        return;
    }

    entry = e;
    shownGeneration = gen;
    refreshText (false);

    // Reused for another entry: state snaps (it is not a change of this entry). A new result set
    // fades the rows in instead of cutting.
    loadedAnim.snap (entry != nullptr && owner.getLoaded() == entry ? 1.0f : 0.0f);
    playing.snap (entry != nullptr && owner.isPreviewing (entry) ? 1.0f : 0.0f);
    if (newContent && owner.isVisible())
    {
        appear.snap (0.0f);
        setAlpha (0.0f);
    }
    else
    {
        appear.snap (1.0f);
        setAlpha (1.0f);
    }
    appear.setTarget (1.0f);
    repaint();
}

void LibraryDrawer::Row::refreshText (bool animate)
{
    chords.set (entry != nullptr ? owner.chordLine (*entry) : String(), animate);
    detail.set (entry != nullptr ? owner.detailLine (*entry) : String(), animate);
}

Rectangle<float> LibraryDrawer::Row::card() const
{
    return getLocalBounds().toFloat().reduced (0.5f, 3.5f).withTrimmedRight (6.0f);
}

Rectangle<float> LibraryDrawer::Row::playCircle() const
{
    auto c = card();
    return Rectangle<float> (26.0f, 26.0f).withCentre ({ c.getX() + 26.0f, c.getCentreY() });
}

bool LibraryDrawer::Row::inPlayZone (Point<float> p) const
{
    return p.x < playCircle().getRight() + 10.0f;
}

void LibraryDrawer::Row::paint (Graphics& g)
{
    if (entry == nullptr)
        return;

    const float h = hover.value, l = loadedAnim.value, pl = playing.value;
    auto r = card();
    const float radius = Theme::cornerRadius;

    g.setColour (Theme::surface2.brighter (0.06f * h));
    g.fillRoundedRectangle (r, radius);
    if (l > 0.001f)
    {
        g.setColour (Theme::accent.withAlpha (0.04f * l));
        g.fillRoundedRectangle (r, radius);
        innerGlow (g, r, radius, l);
    }
    g.setColour (Theme::line.interpolatedWith (Theme::dim, 0.3f * h).interpolatedWith (Theme::accent, l));
    g.drawRoundedRectangle (r, radius, Theme::lineWidth);

    // Preview progress: a hairline along the bottom edge.
    if (pl > 0.001f && progress > 0.0f)
    {
        auto line = r.reduced (radius, 0.0f).removeFromBottom (1.5f).translated (0.0f, -1.0f);
        g.setColour (Theme::accent.withAlpha (0.8f * pl));
        g.fillRect (line.withWidth (line.getWidth() * jlimit (0.0f, 1.0f, progress)));
    }

    // ▶ / ■
    const auto circle = playCircle();
    const float ph = playHover.value;
    Path ring;
    ring.addEllipse (circle.reduced (0.5f));
    ChordLookAndFeel::drawGlow (g, ring, Theme::accent, 0.7f * pl, 1.0f);
    g.setColour (Theme::surface.brighter (0.06f * ph));
    g.fillEllipse (circle);
    g.setColour (Theme::line.interpolatedWith (Theme::dim, 0.6f * ph).interpolatedWith (Theme::accent, pl));
    g.strokePath (ring, PathStrokeType (Theme::lineWidth));

    const auto c = circle.getCentre();
    const auto icon = Theme::dim.interpolatedWith (Theme::text, jmax (ph, h * 0.5f)).interpolatedWith (Theme::accent, pl);
    if (pl < 0.999f)
    {
        Path tri;
        tri.addTriangle (c.x - 3.0f, c.y - 5.0f, c.x - 3.0f, c.y + 5.0f, c.x + 5.5f, c.y);
        g.setColour (icon.withMultipliedAlpha (1.0f - pl));
        g.fillPath (tri);
    }
    if (pl > 0.001f)
    {
        g.setColour (icon.withMultipliedAlpha (pl));
        g.fillRoundedRectangle (Rectangle<float> (8.0f, 8.0f).withCentre (c), 1.2f);
    }

    // Genre tag (right) and the two text lines.
    auto text = r.withTrimmedLeft (circle.getRight() - r.getX() + 14.0f).withTrimmedRight (14.0f);
    auto tag = text.removeFromRight (104.0f);
    g.setFont (Theme::label());
    g.setColour (Theme::dim.withMultipliedAlpha (0.8f));
    g.drawText (String (entry->genre).toUpperCase(), tag, Justification::centredRight, true);
    text.removeFromRight (12.0f);

    auto top = text.withHeight (18.0f).withCentre ({ text.getCentreX(), text.getCentreY() - 8.0f });
    auto bottom = text.withHeight (15.0f).withCentre ({ text.getCentreX(), text.getCentreY() + 10.0f });

    g.setFont (Theme::uiFont (13.5f, "Semibold"));
    chords.draw ([&] (const String& s, float a, float dy)
    {
        g.setColour (Theme::text.interpolatedWith (Theme::accent, 0.85f * pl).withMultipliedAlpha (a));
        g.drawText (s, top.translated (0.0f, dy), Justification::centredLeft, true);
    });
    g.setFont (Theme::uiFont (11.5f, "Regular", 0.02f));
    detail.draw ([&] (const String& s, float a, float dy)
    {
        g.setColour (Theme::dim.withMultipliedAlpha (a));
        g.drawText (s, bottom.translated (0.0f, dy), Justification::centredLeft, true);
    });
}

void LibraryDrawer::Row::mouseMove (const MouseEvent& e) { overPlay = inPlayZone (e.position); }
void LibraryDrawer::Row::mouseExit (const MouseEvent&) { overPlay = false; }

void LibraryDrawer::Row::mouseUp (const MouseEvent& e)
{
    if (entry == nullptr || ! e.mouseWasClicked() || ! getLocalBounds().contains (e.getPosition()))
        return;
    if (inPlayZone (e.position))
        owner.togglePreview (*entry);
    else
        owner.load (*entry);
}

bool LibraryDrawer::Row::tickAnimations (float dt)
{
    const bool isPlaying = entry != nullptr && owner.isPreviewing (entry);
    hover.setTarget (isMouseOver() ? 1.0f : 0.0f);
    playHover.setTarget (isMouseOver() && overPlay ? 1.0f : 0.0f);
    loadedAnim.setTarget (entry != nullptr && owner.getLoaded() == entry ? 1.0f : 0.0f);
    playing.setTarget (isPlaying ? 1.0f : 0.0f);
    if (! primed)
    {
        loadedAnim.snap (loadedAnim.target);
        playing.snap (playing.target);
    }
    primed = true;

    bool moving = false;
    for (auto* a : { &hover, &playHover, &loadedAnim, &playing })
        moving = AnimUtil::tick (*a, dt) || moving;
    moving = chords.tick (dt) || moving;
    moving = detail.tick (dt) || moving;
    if (AnimUtil::tick (appear, dt) || ! exactlyEqual (getAlpha(), appear.value))
        setAlpha (appear.value);

    const float p = isPlaying ? owner.previewProgress() : progress;
    if (! exactlyEqual (p, progress))
    {
        progress = p;
        moving = true;
    }
    return moving;
}

// =============================================================================================
// LibraryDrawer

LibraryDrawer::LibraryDrawer (ChordButtProcessor& p)
    : processor (p),
      genreList ("GENRE", cb::Library::genres()),
      moodList ("MOOD", cb::Library::moods())
{
    setWantsKeyboardFocus (true);
    setVisible (false);
    setInterceptsMouseClicks (false, false);

    genreList.onChange = [this] { refilter (true); };
    moodList.onChange = [this] { refilter (true); };
    searchBox.onTextChange = [this] { refilter (true); };
    searchBox.onEscapeKey = [this] { if (onCloseRequested) onCloseRequested(); };
    searchBox.onReturnKey = [this] { grabKeyboardFocus(); };

    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setOutlineThickness (0);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    list.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, Theme::line.brighter (0.35f));
    list.getVerticalScrollBar().setColour (ScrollBar::trackColourId, Colours::transparentBlack);
    if (auto* vp = list.getViewport())
        vp->setScrollBarThickness (6);

    for (auto* c : std::initializer_list<Component*> { &genreList, &moodList, &searchBox, &list })
        panel.addAndMakeVisible (c);
    addAndMakeVisible (panel);

    count.set (countText(), false);
    refilter (true);
    applyOpenAmount();
    processor.stateBroadcaster.addChangeListener (this);
}

LibraryDrawer::~LibraryDrawer()
{
    processor.stateBroadcaster.removeChangeListener (this);
    stopPreview();   // never leave the engine on a library sequence
    list.setModel (nullptr);
}

// ---- open / close ----

void LibraryDrawer::setOpen (bool shouldOpen, const Image& backdropSnapshot)
{
    if (shouldOpen == open)
        return;
    open = shouldOpen;
    openAnim.setTarget (open ? 1.0f : 0.0f);

    if (open)
    {
        if (backdropSnapshot.isValid())
            backdrop = blurred (backdropSnapshot);
        setVisible (true);
        setInterceptsMouseClicks (true, true);
        if (isShowing())
            grabKeyboardFocus();   // Esc closes
    }
    else
    {
        stopPreview();
        setInterceptsMouseClicks (false, false);   // clicks reach the pads while it fades out
        if (hasKeyboardFocus (true))
            giveAwayKeyboardFocus();
    }
}

void LibraryDrawer::applyOpenAmount()
{
    const float a = openAnim.value;
    if (exactlyEqual (a, appliedOpen))
        return;
    appliedOpen = a;
    // The panel's opacity lags the slide (squared) while the backdrop leads (see paint), so the
    // pads are already blurred away before the panel's text becomes legible: less double exposure.
    panel.setAlpha (a * a);
    panel.setTransform (AffineTransform::translation (0.0f, slideDistance * (1.0f - a)));
    if (! open && a <= 0.0f)
    {
        setVisible (false);
        backdrop = {};
    }
}

bool LibraryDrawer::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::escapeKey && open)
    {
        if (onCloseRequested)
            onCloseRequested();
        return true;
    }
    return false;
}

// ---- filters ----

void LibraryDrawer::setGenre (const String& g) { genreList.setSelected (g); refilter (true); }
void LibraryDrawer::setMood (const String& m)  { moodList.setSelected (m); refilter (true); }
void LibraryDrawer::setSearchText (const String& s) { searchBox.setText (s, true); refilter (true); }

const cb::LibraryEntry* LibraryDrawer::getResult (int index) const
{
    return index >= 0 && index < (int) results.size() ? results[(size_t) index] : nullptr;
}

void LibraryDrawer::refilter (bool newContent)
{
    auto next = processor.library().filter (genreList.getSelected().toStdString(), moodList.getSelected().toStdString(),
                                            searchBox.getText().trim().toStdString(), processor.tonic());
    if (next != results)
    {
        // A new result set (filters, or a key change altering which chords a search finds): rows fade in.
        results = std::move (next);
        ++generation;
        if (newContent)
            list.scrollToEnsureRowIsOnscreen (0);
    }
    list.updateContent();
    count.set (countText());
    emptyHint.setTarget (results.empty() ? 1.0f : 0.0f);
    panel.repaint();
}

String LibraryDrawer::countText() const
{
    StringArray parts;
    for (auto& s : { genreList.getSelected(), moodList.getSelected() })
        if (s.isNotEmpty())
            parts.add (s);
    const auto search = searchBox.getText().trim();
    if (search.isNotEmpty())
        parts.add (String::fromUTF8 ("\xe2\x80\x9c") + search + String::fromUTF8 ("\xe2\x80\x9d"));
    if (parts.isEmpty())
        parts.add ("All progressions");
    return parts.joinIntoString (dot) + dot + String ((int) results.size());
}

Component* LibraryDrawer::refreshComponentForRow (int row, bool, Component* existing)
{
    auto* r = dynamic_cast<Row*> (existing);
    if (row < 0 || row >= (int) results.size())
    {
        delete existing;
        return nullptr;
    }
    if (r == nullptr)
    {
        delete existing;
        r = new Row (*this);
    }
    r->setEntry (results[(size_t) row], generation);
    return r;
}

String LibraryDrawer::chordLine (const cb::LibraryEntry& e) const
{
    const int tonic = processor.tonic();
    StringArray names;
    for (auto& c : e.chords)
        names.add (cb::chordNameInKey (cb::resolve (c, tonic), tonic, e.scale));
    return Theme::pretty (names.joinIntoString (dash));
}

String LibraryDrawer::detailLine (const cb::LibraryEntry& e) const
{
    StringArray romans;
    for (auto& c : e.chords)
        romans.add (c.roman);
    return String (e.name) + dot + Theme::pretty (romans.joinIntoString (String::fromUTF8 ("\xe2\x80\x93")));
}

// ---- preview / load ----

String LibraryDrawer::paramSignature() const
{
    String s;
    for (auto* id : paramIds)
        s << raw (processor, id) << ";";
    return s << (int64) processor.currentStyle().seed;
}

void LibraryDrawer::togglePreview (const cb::LibraryEntry& e)
{
    if (isPreviewing (&e))
        stopPreview();
    else
        startPreview (e);
}

void LibraryDrawer::startPreview (const cb::LibraryEntry& e)
{
    stopPreview();
    auto& engine = processor.preview();
    if (engine.isPlaying())
        engine.stop();   // the strip was playing

    const auto strip = cb::toStrip (e);
    const int complexity = jlimit (0, 3, roundToInt (raw (processor, "complexity")));
    const auto slots = cb::voiceStrip (strip, processor.tonic(), e.scale, complexity, globalVoicing (processor));
    auto events = std::make_shared<const std::vector<cb::NoteEvent>> (cb::generateEvents (slots, processor.currentStyle()));

    previewBeats = jmax (strip.totalBeats(), 1.0);
    engine.setSequence (events, previewBeats, false);
    engine.play();
    previewEntry = &e;
    previewAge = 0.0f;
    previewSignature = paramSignature();
}

bool LibraryDrawer::stopPreview()
{
    if (previewEntry == nullptr)
        return false;
    previewEntry = nullptr;
    processor.preview().stop();
    processor.stripChanged();   // the strip's own (looping) sequence again
    return true;
}

bool LibraryDrawer::isPreviewing (const cb::LibraryEntry* e) const
{
    return previewEntry != nullptr && (e == nullptr || e == previewEntry);
}

float LibraryDrawer::previewProgress() const
{
    if (previewEntry == nullptr || previewBeats <= 0.0)
        return 0.0f;
    return (float) jlimit (0.0, 1.0, processor.preview().playheadBeat() / previewBeats);
}

void LibraryDrawer::load (const cb::LibraryEntry& e)
{
    stopPreview();
    processor.loadLibraryEntry (e);
    loaded = &e;
}

void LibraryDrawer::changeListenerCallback (ChangeBroadcaster*)
{
    // A parameter change rebuilt the engine's sequence from the strip: the preview is over.
    if (previewEntry != nullptr && paramSignature() != previewSignature)
        stopPreview();

    // The loaded outline only stays while the strip still is that progression.
    if (loaded != nullptr && ! processor.strip().toValueTree().isEquivalentTo (cb::toStrip (*loaded).toValueTree()))
        loaded = nullptr;

    refilter (false);   // key changes rename chords (rows cross-fade) and can change search hits
}

// ---- animation / paint ----

bool LibraryDrawer::tickAnimations (float dt)
{
    const bool moving = AnimUtil::tick (openAnim, dt);
    applyOpenAmount();

    // One-shot preview finished (grace period: the audio thread may not have picked up play() yet).
    if (previewEntry != nullptr)
    {
        previewAge += dt;
        if (previewAge > 0.15f && ! processor.preview().isPlaying())
            stopPreview();
    }

    const bool c = count.tick (dt);
    const bool e = AnimUtil::tick (emptyHint, dt);
    if (c || e)
        panel.repaint();
    return moving;
}

void LibraryDrawer::paint (Graphics& g)
{
    const float a = openAnim.value;
    if (a <= 0.001f)
        return;
    const float lead = 1.0f - std::pow (1.0f - a, 4.0f);   // backdrop leads the panel

    // Blurred snapshot of what is behind, cross-faded in, then dimmed.
    if (backdrop.isValid())
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.setOpacity (lead);
        g.drawImage (backdrop, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
    }
    g.setColour (Theme::bg.withAlpha (0.55f * lead));
    g.fillAll();
}

void LibraryDrawer::paintPanel (Graphics& g)
{
    auto r = panel.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::surface.withAlpha (0.97f));
    g.fillRoundedRectangle (r, Theme::cornerRadius + 2.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (r, Theme::cornerRadius + 2.0f, Theme::lineWidth);

    // Column separator.
    const float sepX = (float) genreList.getRight() + (float) panelPad - 0.5f;
    g.fillRect (Rectangle<float> (sepX, (float) panelPad, 1.0f, r.getHeight() - 2.0f * panelPad));

    // Result count, right of the search box.
    auto countArea = Rectangle<float> ((float) searchBox.getRight() + 12.0f, (float) searchBox.getY(),
                                       (float) list.getRight() - (float) searchBox.getRight() - 18.0f,
                                       (float) searchBox.getHeight());
    g.setFont (Theme::uiFont (12.0f, "Regular", 0.02f));
    count.draw ([&] (const String& s, float alpha, float dy)
    {
        g.setColour (Theme::dim.withMultipliedAlpha (alpha));
        g.drawText (s, countArea.translated (0.0f, dy), Justification::centredRight, true);
    });

    // Empty result.
    if (emptyHint.value > 0.001f)
    {
        auto area = list.getBounds().toFloat().withHeight (90.0f).translated (0.0f, 6.0f * (1.0f - emptyHint.value));
        g.setFont (Theme::uiFont (13.0f, "Regular"));
        g.setColour (Theme::dim.withMultipliedAlpha (emptyHint.value));
        g.drawText ("No progressions match", area, Justification::centred, false);
    }
}

void LibraryDrawer::resized()
{
    panel.setBounds (getLocalBounds().reduced (panelInset, 0).withTrimmedTop (panelTopBottom).withTrimmedBottom (panelTopBottom));
    auto inner = panel.getLocalBounds().reduced (panelPad);

    auto left = inner.removeFromLeft (leftWidth);
    genreList.setBounds (left.removeFromTop (genreList.preferredHeight()));
    left.removeFromTop (10);
    moodList.setBounds (left.removeFromTop (moodList.preferredHeight()));

    inner.removeFromLeft (2 * panelPad + 1);
    auto top = inner.removeFromTop (searchHeight);
    searchBox.setBounds (top.removeFromLeft (searchWidth));
    inner.removeFromTop (10);
    list.setBounds (inner);
}
