#include "UI/HeaderBar.h"
#include "UI/Theme.h"

using namespace juce;

namespace
{
    const String wordmarkText { "COSMIC CHORDS" };

    void fillChoices (ComboBox& box, AudioProcessorValueTreeState& apvts, const String& paramId)
    {
        auto* choice = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (paramId));
        jassert (choice != nullptr);
        if (choice != nullptr)
            box.addItemList (choice->choices, 1);   // item id = index + 1, as ComboBoxAttachment expects
    }
}

HeaderBar::HeaderBar (CosmicChordsProcessor& p)
    : processor (p)
{
    addAndMakeVisible (wordmark);

    fillChoices (rootBox, processor.apvts, "root");
    fillChoices (scaleBox, processor.apvts, "scale");
    rootAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "root", rootBox);
    scaleAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "scale", scaleBox);
    addAndMakeVisible (rootBox);
    addAndMakeVisible (scaleBox);

    libraryButton.setClickingTogglesState (true);
    libraryButton.onClick = [this]
    {
        if (onLibraryToggled)
            onLibraryToggled (libraryButton.getToggleState());
    };
    addAndMakeVisible (libraryButton);
}

HeaderBar::~HeaderBar()
{
    // Attachments must go before the combo boxes they observe.
    rootAttachment.reset();
    scaleAttachment.reset();
}

void HeaderBar::setLibraryOpen (bool open)
{
    libraryButton.setToggleState (open, dontSendNotification);
}

void HeaderBar::paint (Graphics& g)
{
    g.fillAll (Theme::surface);
    g.setColour (Theme::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().withTrimmedBottom (1).reduced (20, 0);
    const int controlH = 30;
    auto centreY = [&] (Rectangle<int> b) { return b.withSizeKeepingCentre (b.getWidth(), controlH); };

    wordmark.setBounds (r.removeFromLeft (wordmark.preferredWidth()));

    libraryButton.setBounds (centreY (r.removeFromRight (96)));
    r.removeFromRight (10);
    scaleBox.setBounds (centreY (r.removeFromRight (176)));
    r.removeFromRight (8);
    rootBox.setBounds (centreY (r.removeFromRight (92)));
}

// ---------------------------------------------------------------------------------------------

int HeaderBar::Wordmark::preferredWidth() const
{
    return roundToInt (GlyphArrangement::getStringWidth (Theme::title(), wordmarkText)) + 4;
}

void HeaderBar::Wordmark::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setFont (Theme::title());
    g.setColour (Theme::text);
    g.drawText (wordmarkText, r, Justification::centredLeft, false);

    // Hover underline: grows from the left and fades in, in accent.
    const float h = hover.value;
    if (h > 0.001f)
    {
        const float textW = GlyphArrangement::getStringWidth (Theme::title(), wordmarkText);
        const float y = r.getCentreY() + Theme::title().getHeight() * 0.5f + 4.0f;
        g.setColour (Theme::accent.withAlpha (h));
        g.fillRect (Rectangle<float> (r.getX(), y, textW * h, 1.5f));
    }
}

bool HeaderBar::Wordmark::tickAnimations (float dt)
{
    hover.setTarget (forcedHighlight || isMouseOver() ? 1.0f : 0.0f);
    const float before = hover.value;
    return hover.tick (dt) || ! exactlyEqual (hover.value, before);
}
