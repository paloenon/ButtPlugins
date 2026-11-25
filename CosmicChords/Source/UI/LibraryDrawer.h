#pragma once
#include <functional>
#include <vector>
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"

// Progression library: a panel that slides over the pads + style area (the strip stays usable).
// Left: GENRE and MOOD filters (single-select, click again to clear). Right: search, result count and
// the list. ▶ previews an entry once at the current key/voicing/style; clicking a row loads it into
// the strip. The area behind the panel dims and blurs (snapshot blurred once per open, cross-faded).
class LibraryDrawer : public juce::Component,
                      public Animated,
                      private juce::ListBoxModel,
                      private juce::ChangeListener
{
public:
    explicit LibraryDrawer (CosmicChordsProcessor&);
    ~LibraryDrawer() override;

    // Opening takes a snapshot of what is behind the drawer (drawer hidden) and blurs it once.
    void setOpen (bool shouldOpen, const juce::Image& backdropSnapshot = {});
    bool isOpen() const { return open; }
    float getOpenAmount() const { return openAnim.value; }
    const juce::Image& getBackdrop() const { return backdrop; }   // blurred, quarter size (snapshots)
    std::function<void()> onCloseRequested;   // Esc

    // Filters ("" = any). Also used by the snapshot tool.
    void setGenre (const juce::String&);
    void setMood (const juce::String&);
    void setSearchText (const juce::String&);
    int getNumResults() const { return (int) results.size(); }
    const cb::LibraryEntry* getResult (int index) const;

    // One-shot preview of an entry (▶ / ■). Stopping restores the strip's own sequence.
    void togglePreview (const cb::LibraryEntry&);
    void startPreview (const cb::LibraryEntry&);
    bool stopPreview();                                   // true when a preview was running
    bool isPreviewing (const cb::LibraryEntry* entry = nullptr) const;
    float previewProgress() const;                        // 0..1 of the running preview

    void load (const cb::LibraryEntry&);                  // into the strip; the drawer stays open
    const cb::LibraryEntry* getLoaded() const { return loaded; }

    // Row text at the current key (display spelling).
    juce::String chordLine (const cb::LibraryEntry&) const;
    juce::String detailLine (const cb::LibraryEntry&) const;
    int contentGeneration() const { return generation; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool tickAnimations (float dt) override;

    // ---- parts ----
    class FilterList : public juce::Component, public Animated
    {
    public:
        FilterList (const juce::String& title, const std::vector<std::string>& items);
        std::function<void()> onChange;
        juce::String getSelected() const;          // "" = none
        void setSelected (const juce::String&);    // no onChange
        int preferredHeight() const;

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool tickAnimations (float dt) override;

    private:
        int itemAt (juce::Point<int>) const;
        juce::Rectangle<float> itemRect (int index) const;

        juce::String title;
        juce::StringArray items;
        std::vector<cb::Anim> sel, hover;
        int selected = -1, hovered = -1;
        bool primed = false;
    };

    class SearchBox : public juce::TextEditor, public Animated
    {
    public:
        SearchBox();
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        bool tickAnimations (float dt) override;

    private:
        cb::Anim focus, hover;
    };

    class Row : public juce::Component, public Animated
    {
    public:
        explicit Row (LibraryDrawer&);
        void setEntry (const cb::LibraryEntry*, int generation);
        void refreshText (bool animate);

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool tickAnimations (float dt) override;

    private:
        juce::Rectangle<float> card() const;
        juce::Rectangle<float> playCircle() const;
        bool inPlayZone (juce::Point<float>) const;

        LibraryDrawer& owner;
        const cb::LibraryEntry* entry = nullptr;
        int shownGeneration = -1;
        FadeText chords, detail;
        cb::Anim hover, playHover, loadedAnim, playing, appear;
        float progress = 0.0f;
        bool overPlay = false, primed = false;
    };

    FilterList& getGenreList() { return genreList; }
    FilterList& getMoodList() { return moodList; }
    SearchBox& getSearchBox() { return searchBox; }

private:
    class Panel : public juce::Component
    {
    public:
        explicit Panel (LibraryDrawer& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.paintPanel (g); }

    private:
        LibraryDrawer& owner;
    };

    // ListBoxModel
    int getNumRows() override { return (int) results.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void paintPanel (juce::Graphics&);
    void refilter (bool newContent);   // newContent: filters changed by the user (scrolls to the top)
    juce::String countText() const;
    void checkPreviewStillOurs();
    void applyOpenAmount();

    CosmicChordsProcessor& processor;
    Panel panel { *this };
    FilterList genreList, moodList;
    SearchBox searchBox;
    juce::ListBox list;

    std::vector<const cb::LibraryEntry*> results;
    const cb::LibraryEntry* loaded = nullptr;
    const cb::LibraryEntry* previewEntry = nullptr;
    double previewBeats = 0.0;
    float previewAge = 0.0f;
    uint64_t previewGeneration = 0;   // engine sequence generation our one-shot was published as
    int generation = 0;

    bool open = false;
    cb::Anim openAnim, emptyHint;
    FadeText count;
    juce::Image backdrop;          // blurred snapshot of the area behind the drawer
    float appliedOpen = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryDrawer)
};
