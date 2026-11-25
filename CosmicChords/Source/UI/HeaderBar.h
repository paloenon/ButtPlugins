#pragma once
#include <functional>
#include <memory>
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"

// Top bar: "COSMIC CHORDS" wordmark, root & scale selectors, LIBRARY toggle.
class HeaderBar : public juce::Component
{
public:
    static constexpr int height = 56;

    explicit HeaderBar (CosmicChordsProcessor&);
    ~HeaderBar() override;

    std::function<void (bool)> onLibraryToggled;   // true = library drawer should open
    void setLibraryOpen (bool open);               // updates the toggle without firing onLibraryToggled
    bool isLibraryOpen() const { return libraryButton.getToggleState(); }

    // Exposed for snapshots / tests.
    AnimatedTextButton& getLibraryButton() { return libraryButton; }
    AnimatedComboBox& getRootBox() { return rootBox; }
    AnimatedComboBox& getScaleBox() { return scaleBox; }

    void paint (juce::Graphics&) override;
    void resized() override;

    // Letter-spaced wordmark; an accent underline fades/grows in on hover.
    class Wordmark : public juce::Component, public Animated
    {
    public:
        void paint (juce::Graphics&) override;
        bool tickAnimations (float dt) override;
        void setHighlighted (bool shouldHighlight) { forcedHighlight = shouldHighlight; }
        int preferredWidth() const;

    private:
        cb::Anim hover;
        bool forcedHighlight = false;
    };

    Wordmark& getWordmark() { return wordmark; }

private:
    CosmicChordsProcessor& processor;
    Wordmark wordmark;
    AnimatedComboBox rootBox { "KEY" }, scaleBox { "SCALE" };
    AnimatedTextButton libraryButton { "LIBRARY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, scaleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
