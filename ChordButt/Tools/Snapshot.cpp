// ChordButtSnapshot <outDir> [scenario...]
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

    struct EditorFixture
    {
        EditorFixture()
            : editor (dynamic_cast<ChordButtEditor*> (processor.createEditor()))
        {
            jassert (editor != nullptr);
            editor->advanceAnimations (1.0f);   // settle initial state
        }
        ~EditorFixture() { editor.reset(); }

        void settle (float seconds = 1.0f) { advance (seconds, [&] (float dt) { editor->advanceAnimations (dt); }); }

        void setParam (const String& id, float value)
        {
            auto* param = processor.apvts.getParameter (id);
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        }

        // Appends palette pads (complexity-0 chords, as a pad click stores them) with the given lengths.
        void addPads (std::initializer_list<int> pads, std::initializer_list<double> lengths = {})
        {
            const auto base = cb::buildPalette (processor.scale(), 0);
            auto len = lengths.begin();
            for (int pad : pads)
            {
                cb::StripSlot slot;
                slot.chord = base[(size_t) pad].chord;
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

        ChordButtProcessor processor;
        std::unique_ptr<ChordButtEditor> editor;
    };

    bool runScenario (const String& name, const File& dir)
    {
        if (name == "default")
        {
            EditorFixture f;
            return snap (*f.editor, dir, "default");
        }
        if (name == "library-open")
        {
            EditorFixture f;
            f.editor->getHeader().setLibraryOpen (true);
            f.editor->advanceAnimations (0.06f);
            bool ok = snap (*f.editor, dir, "library-open-60ms");
            advance (1.0f, [&] (float dt) { f.editor->advanceAnimations (dt); });
            return snap (*f.editor, dir, "library-open") && ok;
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
                std::cout << " " << cb::chordName (cb::resolve (pc.chord, f.processor.tonic())) << " (" << pc.chord.roman << ")";
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
        std::cerr << "usage: ChordButtSnapshot <outDir> [default|knob|hover-knob|mid-anim|library-open|hover-header|pads|strip|mode-chords|mode-rhythm|mode-arp|empty-strip|full-strip-flash|reorder-shrink ...]\n";
        return 2;
    }

    ScopedJuceInitialiser_GUI gui;

    const File dir = File::getCurrentWorkingDirectory().getChildFile (String (argv[1]));
    dir.createDirectory();

    StringArray scenarios;
    for (int i = 2; i < argc; ++i)
        scenarios.add (argv[i]);
    if (scenarios.isEmpty())
        scenarios.addArray ({ "default", "knob", "hover-knob", "mid-anim", "library-open", "hover-header", "pads", "strip",
                              "mode-chords", "mode-rhythm", "mode-arp", "empty-strip", "full-strip-flash", "reorder-shrink" });

    bool ok = true;
    for (auto& s : scenarios)
        ok = runScenario (s, dir) && ok;

    return ok ? 0 : 1;
}
