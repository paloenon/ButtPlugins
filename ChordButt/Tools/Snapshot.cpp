// ChordButtSnapshot <outDir> [scenario...]
// Renders the editor (or single widgets) offscreen to PNGs so visuals can be checked without
// screen-recording permissions. Animations are advanced deterministically via advanceAnimations().
#include <juce_audio_utils/juce_audio_utils.h>
#include "Plugin/PluginEditor.h"
#include "Plugin/PluginProcessor.h"
#include "UI/ChordLookAndFeel.h"
#include "UI/Theme.h"

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

        std::cerr << "unknown scenario: " << name << "\n";
        return false;
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: ChordButtSnapshot <outDir> [default|knob|hover-knob|mid-anim|library-open|hover-header ...]\n";
        return 2;
    }

    ScopedJuceInitialiser_GUI gui;

    const File dir = File::getCurrentWorkingDirectory().getChildFile (String (argv[1]));
    dir.createDirectory();

    StringArray scenarios;
    for (int i = 2; i < argc; ++i)
        scenarios.add (argv[i]);
    if (scenarios.isEmpty())
        scenarios.addArray ({ "default", "knob", "hover-knob", "mid-anim", "library-open", "hover-header" });

    bool ok = true;
    for (auto& s : scenarios)
        ok = runScenario (s, dir) && ok;

    return ok ? 0 : 1;
}
