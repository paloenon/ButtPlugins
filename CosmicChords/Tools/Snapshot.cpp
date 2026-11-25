// CosmicChordsSnapshot <outDir> [scenario...]
// Renders the editor (or single widgets) offscreen to PNGs so visuals can be checked without
// screen-recording permissions. Animations are advanced deterministically via advanceAnimations().
#include <juce_audio_utils/juce_audio_utils.h>
#include "Plugin/PluginEditor.h"
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"
#include "UI/Theme.h"
#include "Core/Palette.h"

using namespace juce;

namespace
{
    constexpr float scale = 2.0f;
    constexpr float frame = 1.0f / 60.0f;

    bool writePng (const Image& img, const File& file)
    {
        file.deleteFile();
        FileOutputStream out (file);
        if (! out.openedOk() || ! PNGImageFormat().writeImageToStream (img, out))
        {
            std::cerr << "failed to write " << file.getFullPathName() << "\n";
            return false;
        }
        std::cout << "wrote " << file.getFullPathName() << "\n";
        return true;
    }

    bool snap (Component& c, const File& dir, const String& name)
    {
        return writePng (c.createComponentSnapshot (c.getLocalBounds(), true, scale), dir.getChildFile (name + ".png"));
    }

    // Advances `seconds` in 60 fps steps (last step shortened so the total is exact).
    template <typename Fn>
    void advance (float seconds, Fn&& step)
    {
        while (seconds > 1.0e-6f)
        {
            const float dt = jmin (frame, seconds);
            step (dt);
            seconds -= dt;
        }
    }

    // A single 80x80 knob on the editor background.
    struct KnobHost : public Component
    {
        KnobHost()
        {
            setLookAndFeel (&lnf);
            knob.setRange (0.0, 1.0);
            knob.setValue (0.6, dontSendNotification);
            addAndMakeVisible (knob);
            setSize (80, 80);
        }
        ~KnobHost() override { setLookAndFeel (nullptr); }
        void paint (Graphics& g) override { g.fillAll (Theme::bg); }
        void resized() override { knob.setBounds (getLocalBounds()); }

        ChordLookAndFeel lnf;
        GlowSlider knob;
    };

    // Host playhead whose transport can be started (Logic: Play / Record).
    struct FakePlayHead : public AudioPlayHead
    {
        bool playing = false;
        Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setBpm (120.0);
            return info;
        }
    };

    struct EditorFixture
    {
        EditorFixture()
            : editor (dynamic_cast<CosmicChordsEditor*> (processor.createEditor()))
        {
            jassert (editor != nullptr);
            editor->advanceAnimations (1.0f);   // settle initial state
        }
        ~EditorFixture() { editor.reset(); }

        // host: the host calls process() once per frame (Logic with the track selected); otherwise it
        // stays silent (Logic stopped, track not selected).
        void settle (float seconds = 1.0f, bool host = true)
        {
            advance (seconds, [&] (float dt)
            {
                if (host)
                    block();
                editor->advanceAnimations (dt);
            });
        }

        int block()   // one 512-sample host block; returns the number of note-ons it produced
        {
            AudioBuffer<float> audio (0, 512);
            MidiBuffer midi;
            processor.processBlock (audio, midi);
            int ons = 0;
            for (const auto m : midi)
                ons += m.getMessage().isNoteOn() ? 1 : 0;
            return ons;
        }

        void setParam (const String& id, float value)
        {
            auto* param = processor.apvts.getParameter (id);
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        }

        // Appends palette pads (PadChord::base, as a pad click stores it) with the given lengths.
        void addPads (std::initializer_list<int> pads, std::initializer_list<double> lengths = {})
        {
            const auto base = processor.currentPalette();
            auto len = lengths.begin();
            for (int pad : pads)
            {
                cb::StripSlot slot;
                slot.chord = base[(size_t) pad].base;
                if (len != lengths.end())
                    slot.lengthBars = *len++;
                processor.strip().append (slot);
            }
            processor.stripChanged();
            editor->refreshAll();
        }

        // Runs the preview engine (no host: 120 BPM) until the playhead reaches `beat`.
        void playTo (double beat)
        {
            processor.prepareToPlay (44100.0, 512);
            processor.preview().play();
            AudioBuffer<float> audio (0, 512);
            for (int i = 0; i < 100000 && ! (processor.preview().isPlaying() && processor.preview().playheadBeat() >= beat); ++i)
            {
                MidiBuffer midi;
                processor.processBlock (audio, midi);
            }
        }

        void printDrag (const String& description)
        {
            StringArray files;
            bool canMove = true;
            DragAndDropTarget::SourceDetails details (description, nullptr, {});
            const bool ok = editor->shouldDropFilesWhenDraggedExternally (details, files, canMove);
            std::cout << "drag " << description << ": " << (ok ? "yes" : "no");
            for (auto& f : files)
                std::cout << " -> " << f << " (" << File (f).getSize() << " bytes)";
            std::cout << (canMove ? " canMove" : "") << "\n";
        }

        CosmicChordsProcessor processor;
        std::unique_ptr<CosmicChordsEditor> editor;
    };

    bool runScenario (const String& name, const File& dir)
    {
        if (name == "default")
        {
            EditorFixture f;
            return snap (*f.editor, dir, "default");
        }
        if (name == "library-closed")
        {
            EditorFixture f;
            f.addPads ({ 0, 5, 2, 6 });
            f.settle();
            std::cout << "library-closed: drawer visible " << f.editor->getLibraryDrawer().isVisible() << "\n";
            return snap (*f.editor, dir, "library-closed");
        }
        if (name == "library-mid")
        {
            EditorFixture f;
            f.addPads ({ 0, 5, 2, 6 });
            f.settle();
            f.editor->setLibraryOpen (true);
            advance (0.06f, [&] (float dt) { f.editor->advanceAnimations (dt); });
            std::cout << "library-mid: open amount " << f.editor->getLibraryDrawer().getOpenAmount()
                      << ", header toggle " << f.editor->getHeader().isLibraryOpen() << "\n";
            const auto& backdrop = f.editor->getLibraryDrawer().getBackdrop();
            std::cout << "library-mid: backdrop " << backdrop.getWidth() << "x" << backdrop.getHeight() << "\n";
            bool ok = backdrop.isValid() && writePng (backdrop, dir.getChildFile ("library-backdrop-blurred.png"));
            return snap (*f.editor, dir, "library-mid") && ok;
        }
        if (name == "library-open-lofi-melancholic")
        {
            EditorFixture f;
            auto& drawer = f.editor->getLibraryDrawer();
            f.editor->setLibraryOpen (true);
            drawer.setGenre ("Lo-Fi");
            drawer.setMood ("Melancholic");
            f.settle();
            std::cout << "lofi+melancholic: " << drawer.getNumResults() << " results\n";
            bool ok = snap (*f.editor, dir, "library-open-lofi-melancholic");

            // Row click: loads into the strip, drawer stays open, the row gets the accent outline.
            drawer.load (*drawer.getResult (1));
            f.editor->refreshAll();
            f.editor->advanceAnimations (0.06f);
            ok = snap (*f.editor, dir, "library-load-60ms") && ok;
            f.settle();
            std::cout << "loaded \"" << drawer.getResult (1)->name << "\": strip " << f.processor.strip().slots().size()
                      << " slots, scale " << cb::scaleName (f.processor.scale()) << ", drawer open " << drawer.isOpen() << "\n";
            return snap (*f.editor, dir, "library-loaded") && ok;
        }
        if (name == "demo-json")
        {
            // The website's in-browser demo: voiced exactly like the plugin (complexity as written, Spread Open),
            // major-family keys in C, minor-family keys in A, top lines included.
            const char* ids[] { "cosmic-001", "cosmic-009", "cosmic-005", "lofi-001", "rnb-016", "jazz-006" };
            Array<var> out;
            for (auto* id : ids)
            {
                const cb::LibraryEntry* e = nullptr;
                for (auto& x : CosmicChordsProcessor::library().entries())
                    if (x.id == id) e = &x;
                if (e == nullptr) { std::cerr << "missing " << id << "\n"; return false; }

                const int tonic = cb::isMinorFamily (e->scale) ? 9 : 0;
                cb::VoicingParams vp;
                vp.spread = e->hasMelody() ? 0.25f : 0.5f;   // Bass under a top line, Open otherwise
                const auto strip = cb::toStrip (*e);
                const auto voiced = cb::voiceStrip (strip, tonic, e->scale, 0, vp);

                auto* obj = new DynamicObject();
                obj->setProperty ("id", String (e->id));
                obj->setProperty ("name", String::fromUTF8 (e->name.c_str()));
                obj->setProperty ("genre", String::fromUTF8 (e->genre.c_str()));
                obj->setProperty ("key", String (cb::noteNameInKey (tonic, tonic, e->scale)) + " " + String (cb::scaleName (e->scale)));
                Array<var> chords;
                for (size_t i = 0; i < voiced.size(); ++i)
                {
                    auto* c = new DynamicObject();
                    c->setProperty ("name", String::fromUTF8 (cb::chordNameInKey (e->chords[i], tonic, e->scale).c_str()));
                    c->setProperty ("roman", String (e->chords[i].roman));
                    c->setProperty ("beats", voiced[i].lengthBeats);
                    Array<var> notes;
                    for (int n : voiced[i].notes) notes.add (n);
                    c->setProperty ("notes", notes);
                    Array<var> melody;
                    cb::Strip single;
                    single.append (strip.slots()[i]);
                    for (auto& m : cb::melodyEvents (single, tonic, 0))
                        melody.add (Array<var> { m.note, m.startBeat, m.lengthBeats });
                    if (! melody.isEmpty())
                        c->setProperty ("melody", melody);
                    chords.add (var (c));
                }
                obj->setProperty ("chords", chords);
                out.add (var (obj));
            }
            auto file = dir.getChildFile ("demo-data.js");
            file.replaceWithText ("// Generated by CosmicChordsSnapshot demo-json from CosmicChords/Resources/library.json\n"
                                  "// (voiced like the plugin: Spread Bass under a top line, Open otherwise).\n"
                                  "window.COSMIC_DEMO = " + JSON::toString (var (out), true) + ";\n");
            std::cout << "wrote " << file.getFullPathName() << "\n";
            return true;
        }
        if (name == "cosmic-pack")
        {
            EditorFixture f;
            auto& drawer = f.editor->getLibraryDrawer();
            f.editor->setLibraryOpen (true);
            drawer.setGenre ("Cosmic Pack");
            f.settle();
            std::cout << "cosmic pack: " << drawer.getNumResults() << " results\n";
            bool ok = snap (*f.editor, dir, "cosmic-pack-open");

            drawer.load (*drawer.getResult (0));
            f.editor->setLibraryOpen (false);
            f.editor->refreshAll();
            f.playTo (6.0);
            f.settle();
            int melody = 0;
            for (auto& s : f.processor.strip().slots()) melody += (int) s.melody.size();
            std::cout << "loaded \"" << drawer.getResult (0)->name << "\": " << f.processor.strip().slots().size()
                      << " chords, " << melody << " top-line notes, " << f.processor.currentEvents().size() << " events\n";
            f.printDrag ("strip");
            ok = snap (*f.editor, dir, "cosmic-pack-loaded") && ok;

            f.processor.apvts.getParameter ("topline")->setValueNotifyingHost (0.0f);
            f.settle();
            std::cout << "top line off: " << f.processor.currentEvents().size() << " events\n";
            return snap (*f.editor, dir, "cosmic-pack-topline-off") && ok;
        }
        if (name == "library-preview")
        {
            EditorFixture f;
            f.addPads ({ 0, 5 });
            auto& drawer = f.editor->getLibraryDrawer();
            f.editor->setLibraryOpen (true);
            drawer.setGenre ("Lo-Fi");
            drawer.setMood ("Melancholic");
            f.settle();

            f.processor.prepareToPlay (44100.0, 512);
            const auto* entry = drawer.getResult (0);
            drawer.togglePreview (*entry);
            AudioBuffer<float> audio (0, 512);
            int noteOns = 0;
            auto block = [&]
            {
                MidiBuffer midi;
                f.processor.processBlock (audio, midi);
                for (const auto m : midi)
                    noteOns += m.getMessage().isNoteOn() ? 1 : 0;
            };
            for (int i = 0; i < 2000 && f.processor.preview().playheadBeat() < 5.0; ++i)
                block();
            f.settle (0.5f);
            std::cout << "library-preview: previewing " << drawer.isPreviewing (entry) << ", engine playing "
                      << f.processor.preview().isPlaying() << ", beat " << f.processor.preview().playheadBeat()
                      << ", note-ons " << noteOns << ", strip defers (no playhead) " << f.editor->getChordStrip().isExternalPreview() << "\n";
            bool ok = snap (*f.editor, dir, "library-preview");

            // Let the one-shot finish: the drawer restores the strip's sequence.
            for (int i = 0; i < 20000 && f.processor.preview().isPlaying(); ++i)
                block();
            f.settle (0.5f);
            std::cout << "library-preview end: previewing " << drawer.isPreviewing() << ", engine playing "
                      << f.processor.preview().isPlaying() << "\n";

            // Strip Play while a preview runs: restores first, then plays the strip.
            drawer.togglePreview (*entry);
            for (int i = 0; i < 20; ++i) block();
            f.editor->getChordStrip().getPlayButton().onClick();
            for (int i = 0; i < 20; ++i) block();
            f.settle (0.2f);
            std::cout << "strip play during preview: previewing " << drawer.isPreviewing() << ", engine playing "
                      << f.processor.preview().isPlaying() << "\n";
            f.processor.preview().stop();
            for (int i = 0; i < 5; ++i) block();

            // Regression (review R15): editing the strip while a preview plays ends the preview; the
            // engine must not keep looping the strip, and the row must not stay on ■.
            for (int variant = 0; variant < 3; ++variant)
            {
                drawer.togglePreview (*entry);
                for (int i = 0; i < 20; ++i) block();
                if (variant == 0)        // remove a slot (as ChordStrip::commit does)
                {
                    f.processor.strip().remove (0);
                    f.processor.stripChanged();
                }
                else if (variant == 1)   // remove the last slot too: strip becomes empty
                {
                    while (! f.processor.strip().slots().empty())
                        f.processor.strip().remove (0);
                    f.processor.stripChanged();
                }
                else                     // host state recall (rebuilds the preview directly)
                {
                    MemoryBlock state;
                    f.processor.getStateInformation (state);
                    f.processor.setStateInformation (state.getData(), (int) state.getSize());
                }
                f.editor->advanceAnimations (frame);
                for (int i = 0; i < 20; ++i) block();
                f.settle (0.3f);
                const bool pass = ! drawer.isPreviewing() && ! f.processor.preview().isPlaying()
                                  && ! f.editor->getChordStrip().isExternalPreview();
                std::cout << "preview interrupted (variant " << variant << "): " << (pass ? "PASS" : "FAIL")
                          << " (previewing " << drawer.isPreviewing() << ", engine playing "
                          << f.processor.preview().isPlaying() << ")\n";
                ok = pass && ok;
                if (variant == 1)
                    f.addPads ({ 0, 5 });
            }
            return ok;
        }
        if (name == "library-search-sharp")
        {
            EditorFixture f;   // A minor
            auto& drawer = f.editor->getLibraryDrawer();
            f.editor->setLibraryOpen (true);
            drawer.setSearchText ("a#");
            f.settle();
            std::cout << "search a#:";
            for (int i = 0; i < drawer.getNumResults(); ++i)
                std::cout << " " << drawer.getResult (i)->id;
            std::cout << "\n";
            f.setParam ("scale", (float) (int) cb::Scale::Dorian);
            std::cout << "dorian pads:";
            for (auto& pc : f.processor.currentPalette())
                std::cout << " " << cb::chordNameInKey (pc.chord, f.processor.tonic(), f.processor.scale()) << " (" << pc.chord.roman << ")";
            std::cout << "\n";
            return snap (*f.editor, dir, "library-search-sharp");
        }
        if (name == "library-empty")
        {
            EditorFixture f;
            auto& drawer = f.editor->getLibraryDrawer();
            f.editor->setLibraryOpen (true);
            drawer.setGenre ("Jazz");
            drawer.setSearchText ("zzzz");
            f.settle();
            std::cout << "library-empty: " << drawer.getNumResults() << " results\n";
            bool ok = snap (*f.editor, dir, "library-empty");

            // Esc closes and un-toggles LIBRARY.
            f.editor->keyPressed (KeyPress (KeyPress::escapeKey));
            f.settle();
            std::cout << "after Esc: drawer open " << drawer.isOpen() << ", visible " << drawer.isVisible()
                      << ", header toggle " << f.editor->getHeader().isLibraryOpen() << "\n";
            return snap (*f.editor, dir, "library-after-esc") && ok;
        }
        if (name == "hover-header")
        {
            EditorFixture f;
            f.editor->getHeader().getWordmark().setHighlighted (true);
            f.editor->getHeader().getLibraryButton().setHighlighted (true);
            advance (1.0f, [&] (float dt) { f.editor->advanceAnimations (dt); });
            return snap (*f.editor, dir, "hover-header");
        }
        if (name == "knob")
        {
            bool ok = true;
            for (float glow : { 0.0f, 0.5f, 1.0f })
            {
                KnobHost host;
                host.knob.getProperties().set (LnfProps::glow, glow);
                ok = snap (host, dir, "knob-glow-" + String (glow, 1)) && ok;
            }
            return ok;
        }
        if (name == "hover-knob" || name == "mid-anim")
        {
            KnobHost host;
            host.knob.tickAnimations (0.0f);
            host.knob.setHighlighted (true);
            const float seconds = name == "hover-knob" ? 1.0f : 0.06f;
            advance (seconds, [&] (float dt) { host.knob.tickAnimations (dt); });
            std::cout << name << ": glow = " << (float) host.knob.getProperties()[LnfProps::glow] << "\n";
            return snap (host, dir, name);
        }

        if (name == "pads")
        {
            EditorFixture f;   // default: A minor, complexity 1
            std::cout << "pads:";
            for (auto& pc : f.processor.currentPalette())
                std::cout << " " << cb::chordNameInKey (pc.chord, f.processor.tonic(), f.processor.scale()) << " (" << pc.chord.roman << ")";
            std::cout << "\n";
            bool ok = snap (*f.editor, dir, "pads");
            f.editor->getPadGrid().getPad (0).setHighlighted (true);
            f.editor->advanceAnimations (0.06f);
            ok = snap (*f.editor, dir, "pads-press-60ms") && ok;
            f.settle();
            ok = snap (*f.editor, dir, "pads-pressed") && ok;
            // Key change: labels cross-fade.
            f.editor->getPadGrid().getPad (0).setHighlighted (false);
            f.setParam ("root", 0.0f);
            f.setParam ("complexity", 2.0f);
            f.editor->refreshAll();
            f.editor->advanceAnimations (0.06f);
            ok = snap (*f.editor, dir, "pads-relabel-60ms") && ok;
            return ok;
        }
        if (name == "strip")
        {
            EditorFixture f;
            f.addPads ({ 0, 5, 2, 6 });   // i VI III VII
            f.editor->getChordStrip().setSelected (1);
            f.processor.strip().setInversionOverride (1, 2);
            f.processor.stripChanged();
            f.editor->refreshAll();
            f.playTo (8.0);               // mid-strip (4 x 1 bar = 16 beats)
            f.settle();
            f.printDrag ("strip");
            f.printDrag ("slot:1");
            bool ok = snap (*f.editor, dir, "strip");
            f.editor->getChordStrip().getDragButton().setHighlighted (true);   // hover: accent appears only now
            f.settle();
            return snap (*f.editor, dir, "strip-drag-hover") && ok;
        }
        if (name == "mode-chords" || name == "mode-rhythm" || name == "mode-arp")
        {
            EditorFixture f;
            f.addPads ({ 0, 3, 4, 0 }, { 1.0, 0.5, 0.5, 2.0 });
            f.settle();
            const int mode = name == "mode-chords" ? 0 : (name == "mode-rhythm" ? 1 : 2);
            f.setParam ("mode", (float) mode);
            f.editor->advanceAnimations (0.06f);
            bool ok = snap (*f.editor, dir, name + "-60ms");
            f.settle();
            return snap (*f.editor, dir, name) && ok;
        }
        if (name == "empty-strip")
        {
            EditorFixture f;
            f.printDrag ("strip");
            f.editor->getChordStrip().getDragButton().setHighlighted (true);
            f.settle();
            return snap (*f.editor, dir, "empty-strip");
        }
        if (name == "pad-drop" || name == "spread-open")
        {
            // A pad dragged over the strip between slot 1 and 2: the placeholder opens there, then the drop.
            EditorFixture f;
            f.addPads ({ 0, 5, 2, 6 });
            if (name == "spread-open")
                f.processor.apvts.getParameter ("spread")->setValueNotifyingHost (0.5f);
            f.settle();
            auto& strip = f.editor->getChordStrip();
            const auto palette = f.processor.currentPalette();
            const auto desc = ChordStrip::padDragDescription (palette[3].base, "Dm7");
            const auto between = strip.getSlotBounds (1).getRight() + 2.0f;
            DragAndDropTarget::SourceDetails details (desc, nullptr, Point<int> (roundToInt (between), strip.getHeight() / 2));
            strip.itemDragEnter (details);
            strip.itemDragMove (details);
            std::cout << "pad-drop: insert index " << strip.getInsertIndex() << " (want 2)\n";
            bool ok = strip.getInsertIndex() == 2;
            f.editor->advanceAnimations (0.06f);
            ok = snap (*f.editor, dir, name + "-opening") && ok;
            f.settle (0.6f);
            ok = snap (*f.editor, dir, name + "-placeholder") && ok;
            strip.itemDropped (details);
            const auto& slots = f.processor.strip().slots();
            const bool placed = slots.size() == 5 && slots[2].chord.rootOffset == palette[3].base.rootOffset;
            std::cout << "pad-drop: " << slots.size() << " slots, slot 2 is the dropped chord: " << (placed ? "PASS" : "FAIL") << "\n";
            ok = ok && placed;
            f.settle();
            return snap (*f.editor, dir, name + "-dropped") && ok;
        }
        if (name == "full-strip-flash")
        {
            EditorFixture f;
            f.addPads ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 4 },
                       { 0.5, 0.5, 1, 0.5, 0.5, 1, 2, 0.5, 0.5, 0.5, 1, 0.5, 0.5, 0.5, 1, 1 });
            f.settle();
            f.editor->getChordStrip().addChord (cb::buildPalette (f.processor.scale(), 0)[0].chord);   // 17th: refused
            std::cout << "full strip: " << f.processor.strip().slots().size() << " slots after 17th add\n";
            f.editor->advanceAnimations (0.1f);
            return snap (*f.editor, dir, "full-strip-flash");
        }

        if (name == "host-silent")
        {
            // Logic stopped and the track not selected: no process() calls at all.
            EditorFixture f;
            f.addPads ({ 0, 5, 2, 6 });
            f.settle (1.0f, false);
            auto& hint = f.editor->getHostHint();
            const bool quiet = ! hint.isShown();

            f.editor->getChordStrip().getPlayButton().onClick();
            f.settle (0.2f, false);
            const bool notYet = ! hint.isShown() && f.processor.preview().isPlaying();
            f.settle (0.15f, false);   // 0.35 s: hint fading in, still ■
            bool ok = snap (*f.editor, dir, "host-hint-fading-in");
            const bool shownWhilePlaying = hint.isShown() && f.processor.preview().isPlaying();
            f.settle (0.65f, false);   // 1.0 s: play state reset, hint still there
            ok = snap (*f.editor, dir, "host-hint") && ok;
            const bool reset = hint.isShown() && ! f.processor.preview().isPlaying();

            f.settle (0.5f, true);     // track selected: hint fades out at once
            const bool gone = ! hint.isShown() && hint.getAmount() < 0.02f;
            ok = snap (*f.editor, dir, "host-hint-gone") && ok;

            // Library ▶ with a silent host: the row must not stay on ■.
            auto& drawer = f.editor->getLibraryDrawer();
            f.editor->setLibraryOpen (true);
            f.settle (0.5f, false);
            drawer.togglePreview (*drawer.getResult (0));
            f.settle (1.0f, false);
            const bool libraryReset = ! drawer.isPreviewing() && ! f.processor.preview().isPlaying() && hint.isShown();
            ok = snap (*f.editor, dir, "host-hint-library") && ok;

            // Pad audition with a silent host shows the hint too.
            f.editor->setLibraryOpen (false);
            f.settle (4.0f, true);
            f.processor.preview().auditionOn ({ 57, 60, 64 });
            f.settle (0.4f, false);
            const bool audition = hint.isShown();
            f.processor.preview().auditionOff();

            const bool pass = quiet && notYet && shownWhilePlaying && reset && gone && libraryReset && audition;
            std::cout << "host-silent: " << (pass ? "PASS" : "FAIL") << " (quiet " << quiet << ", notYet " << notYet
                      << ", shownWhilePlaying " << shownWhilePlaying << ", reset " << reset << ", gone " << gone
                      << ", libraryReset " << libraryReset << ", audition " << audition << ")\n";
            return pass && ok;
        }
        if (name == "host-transport")
        {
            // Strip loop playing while Logic is stopped; Logic's transport starts -> preview stops.
            EditorFixture f;
            FakePlayHead head;
            f.processor.setPlayHead (&head);
            f.processor.prepareToPlay (44100.0, 512);
            f.addPads ({ 0, 5 });
            f.editor->getChordStrip().getPlayButton().onClick();
            f.settle (0.5f);
            const bool playing = f.processor.preview().isPlaying();
            head.playing = true;
            f.block();
            const bool stopped = ! f.processor.preview().isPlaying();
            f.settle (0.5f);
            const bool staysStopped = ! f.processor.preview().isPlaying();
            // Play pressed while the song plays: allowed (only the start of the transport stops it).
            f.editor->getChordStrip().getPlayButton().onClick();
            f.settle (0.3f);
            const bool canPlayDuringSong = f.processor.preview().isPlaying();
            f.processor.setPlayHead (nullptr);
            const bool pass = playing && stopped && staysStopped && canPlayDuringSong;
            std::cout << "host-transport: " << (pass ? "PASS" : "FAIL") << " (playing " << playing << ", stopped "
                      << stopped << ", staysStopped " << staysStopped << ", canPlayDuringSong " << canPlayDuringSong << ")\n";
            return pass;
        }
        if (name == "editor-close")
        {
            // Closing the plugin window stops the strip loop; all notes are released.
            CosmicChordsProcessor processor;
            processor.prepareToPlay (44100.0, 512);
            std::unique_ptr<CosmicChordsEditor> editor (dynamic_cast<CosmicChordsEditor*> (processor.createEditor()));
            const auto base = processor.currentPalette();
            for (int pad : { 0, 5 })
            {
                cb::StripSlot slot;
                slot.chord = base[(size_t) pad].base;
                processor.strip().append (slot);
            }
            processor.stripChanged();
            editor->refreshAll();
            editor->getChordStrip().getPlayButton().onClick();
            AudioBuffer<float> audio (0, 512);
            int sounding = 0;
            auto run = [&] (int blocks)
            {
                for (int i = 0; i < blocks; ++i)
                {
                    MidiBuffer midi;
                    processor.processBlock (audio, midi);
                    for (const auto m : midi)
                        sounding += m.getMessage().isNoteOn() ? 1 : (m.getMessage().isNoteOff() ? -1 : 0);
                }
            };
            run (20);
            const bool wasPlaying = processor.preview().isPlaying() && sounding > 0;
            editor.reset();
            run (2000);
            const bool pass = wasPlaying && ! processor.preview().isPlaying() && sounding == 0;
            std::cout << "editor-close: " << (pass ? "PASS" : "FAIL") << " (wasPlaying " << wasPlaying << ", playing after close "
                      << processor.preview().isPlaying() << ", notes still on " << sounding << ")\n";
            return pass;
        }
        if (name == "drag-length")
        {
            // Exported regions end at the strip / slot length (so they loop like the preview).
            EditorFixture f;
            f.addPads ({ 0, 5, 2 }, { 1.0, 0.5, 2.0 });
            f.setParam ("mode", 1.0f);   // rhythm: the last note ends before the slot does
            bool ok = true;
            const std::pair<const char*, double> cases[] { { "strip", 14.0 }, { "slot:1", 2.0 }, { "slot:2", 8.0 } };
            for (const auto& [desc, beats] : cases)
            {
                StringArray files;
                bool canMove = true;
                DragAndDropTarget::SourceDetails details (String (desc), nullptr, {});
                f.editor->shouldDropFilesWhenDraggedExternally (details, files, canMove);
                MidiFile mf;
                FileInputStream in { File (files[0]) };
                const bool read = in.openedOk() && mf.readFrom (in);
                const double last = read ? mf.getLastTimestamp() / 960.0 : -1.0;
                const bool pass = std::abs (last - beats) < 1.0e-9;
                std::cout << "drag-length " << desc << ": end " << last << " beats (want " << beats << ") " << (pass ? "PASS" : "FAIL") << "\n";
                ok = pass && ok;
            }
            return ok;
        }

        if (name == "reorder-shrink")
        {
            // Regression (review I1): a slot reorder is in progress when the strip shrinks from
            // outside (host state restore / library load). Variant 0 refreshes via the editor (as the
            // stateBroadcaster would), variant 1 leaves it to the strip's own per-frame resync.
            bool ok = true;
            for (int variant = 0; variant < 2; ++variant)
            {
                EditorFixture f;
                f.addPads ({ 0, 5, 2, 6, 4, 3 });
                f.settle();
                auto& strip = f.editor->getChordStrip();
                const auto last = strip.getSlotBounds (5).getCentre();
                const auto t0 = Time::getCurrentTime();
                auto event = [&] (Point<float> pos)
                {
                    return MouseEvent (Desktop::getInstance().getMainMouseSource(), pos, ModifierKeys (ModifierKeys::leftButtonModifier),
                                       1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &strip, &strip, t0, last, t0, 1, pos != last);
                };

                strip.mouseDown (event (last));
                strip.mouseDrag (event (last.translated (-120.0f, 0.0f)));
                f.editor->advanceAnimations (0.05f);
                const bool started = strip.isReordering();

                for (int i = 0; i < 4; ++i)                     // 6 -> 2 slots, dragged slot (5) is gone
                    f.processor.strip().remove ((int) f.processor.strip().slots().size() - 1);
                f.processor.stripChanged();
                if (variant == 0)
                    f.editor->refreshAll();
                f.settle (0.5f);                                // ticks + layout with the new size
                (void) f.editor->createComponentSnapshot (f.editor->getLocalBounds());   // paint path
                strip.mouseDrag (event (last.translated (-200.0f, 0.0f)));
                strip.mouseUp (event (last.translated (-200.0f, 0.0f)));
                f.settle();

                const bool pass = started && ! strip.isReordering() && f.processor.strip().slots().size() == 2;
                std::cout << "reorder-shrink variant " << variant << ": " << (pass ? "PASS" : "FAIL")
                          << " (started " << started << ", slots " << f.processor.strip().slots().size() << ")\n";
                ok = pass && ok;
                if (variant == 0)
                    ok = snap (*f.editor, dir, "reorder-shrink") && ok;
            }
            return ok;
        }

        std::cerr << "unknown scenario: " << name << "\n";
        return false;
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: CosmicChordsSnapshot <outDir> [default|knob|hover-knob|mid-anim|library-closed|library-mid|library-open-lofi-melancholic|library-preview|library-search-sharp|library-empty|hover-header|pads|strip|mode-chords|mode-rhythm|mode-arp|empty-strip|full-strip-flash|reorder-shrink|host-silent|host-transport|editor-close|drag-length|cosmic-pack|demo-json ...]\n";
        return 2;
    }

    ScopedJuceInitialiser_GUI gui;

    const File dir = File::getCurrentWorkingDirectory().getChildFile (String (argv[1]));
    dir.createDirectory();

    StringArray scenarios;
    for (int i = 2; i < argc; ++i)
        scenarios.add (argv[i]);
    if (scenarios.isEmpty())
        scenarios.addArray ({ "default", "knob", "hover-knob", "mid-anim", "library-closed", "library-mid",
                              "library-open-lofi-melancholic", "library-preview", "library-search-sharp", "library-empty", "hover-header", "pads", "strip",
                              "mode-chords", "mode-rhythm", "mode-arp", "empty-strip", "full-strip-flash", "reorder-shrink",
                              "host-silent", "host-transport", "editor-close", "drag-length", "pad-drop", "spread-open" });

    bool ok = true;
    for (auto& s : scenarios)
        ok = runScenario (s, dir) && ok;

    return ok ? 0 : 1;
}
