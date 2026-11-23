#pragma once
#include <functional>
#include <vector>
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"

// The progression strip at the bottom: Play/Stop, slots sized by length (select, length chip,
// remove, drag to reorder, drag out of the window = MIDI file) and the DRAG MIDI handle.
class ChordStrip : public juce::Component, public Animated
{
public:
    static constexpr int height = 88;

    // Round Play/Stop button; the icon morphs between triangle and square.
    class PlayButton : public juce::Component, public Animated
    {
    public:
        std::function<void()> onClick;
        void setPlaying (bool shouldShowPlaying) { playingTarget = shouldShowPlaying; }

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool tickAnimations (float dt) override;

    private:
        cb::Anim playing, hover, down, enabled;
        bool playingTarget = false, primed = false;
    };

    // Drag handle that starts an external MIDI-file drag of the whole strip.
    class DragButton : public juce::Component, public Animated
    {
    public:
        std::function<void()> onDragStart;
        void setHighlighted (bool shouldHighlight) { forcedHighlight = shouldHighlight; }

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }
        void mouseDrag (const juce::MouseEvent&) override;
        void enablementChanged() override;
        bool tickAnimations (float dt) override;

    private:
        cb::Anim hover, enabled;
        bool dragStarted = false, forcedHighlight = false, primed = false;
    };

    explicit ChordStrip (ChordButtProcessor&);

    void refresh();                        // strip or parameters changed (names, lengths, count)
    void addChord (const cb::RelChord&);   // replace the selected slot, else append (border flash when full)

    int getSelected() const { return selected; }
    void setSelected (int index);          // -1 = none
    std::function<void (int)> onSelectionChanged;

    // For the editor's DragAndDropContainer: writes the MIDI file for a drag started here
    // ("strip" = whole progression, "slot:<n>" = that chord only). False = no drag.
    bool prepareExternalDrag (const juce::var& description, juce::StringArray& files);

    void flashBorder();
    PlayButton& getPlayButton() { return playButton; }
    juce::Rectangle<float> getSlotBounds (int index) const;   // current (animated) slot rect; snapshots/tests
    bool isReordering() const { return reordering; }
    DragButton& getDragButton() { return dragButton; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool tickAnimations (float dt) override;

private:
    enum class Part { none, body, lengthChip, remove };

    struct SlotVis
    {
        FadeText name;
        double lengthBars = 1.0;
        FadeText overrideText;       // "INV 2" when the slot has its own voicing
        FadeText chipText;           // "½" / "1 BAR" …
        bool placed = false;
        cb::Anim x, w, selected, hover, playing, appear, lift, chipLong;
    };

    struct Span { float x, w; };

    juce::Rectangle<float> well() const;                // the recessed slot area
    juce::Rectangle<float> slotArea() const;            // well minus its padding
    float barWidth (double totalBars) const;
    std::vector<Span> layout (const std::vector<int>& order) const;
    std::vector<int> displayOrder() const;              // strip order, or the prospective reorder
    juce::Rectangle<float> slotRect (const SlotVis&) const;
    juce::Rectangle<float> removeRect (juce::Rectangle<float> slot) const;
    juce::Rectangle<float> chipRect (juce::Rectangle<float> slot, float longForm) const;
    int slotAt (juce::Point<float>) const;
    Part partAt (int slot, juce::Point<float>) const;
    float beatToX (double beat) const;
    juce::String slotName (int index, bool forFileName) const;
    juce::ScaledImage dragImage (const juce::String& text) const;

    void paintSlot (juce::Graphics&, const SlotVis&, float alpha) const;
    void removeSlot (int index);
    void cycleLength (int index);
    int computeDropIndex() const;
    void commit();                                       // stripChanged + local refresh
    void syncIfStale();                                  // strip changed elsewhere and refresh() has not run yet
    void cancelGesture();                                // drop any reorder in progress (indices may be stale)
    bool validSlot (int index) const { return index >= 0 && index < (int) vis.size(); }
    void startExternalDrag (const juce::var& description, const juce::String& label);

    ChordButtProcessor& processor;
    PlayButton playButton;
    DragButton dragButton;

    std::vector<SlotVis> vis;        // parallel to processor.strip().slots()
    std::vector<SlotVis> ghosts;     // removed slots fading out
    int selected = -1, hovered = -1;
    cb::Anim flash, empty, playheadAlpha;
    float flashHold = 0.0f;
    bool primed = false;
    double playheadBeat = 0.0;
    bool isPlaying = false;

    // Mouse gesture on a slot.
    int downSlot = -1;
    Part downPart = Part::none;
    bool reordering = false, externalDrag = false;
    float grabOffset = 0.0f, dragX = 0.0f;
    int dropIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordStrip)
};
