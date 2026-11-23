#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Visual tokens for the whole plugin. Near-black matte surfaces, thin lines and exactly
// one saturated colour (accent), which only glows where something is active.
namespace Theme
{
    inline const juce::Colour bg       { 0xff0b0c0f };
    inline const juce::Colour surface  { 0xff14161b };
    inline const juce::Colour surface2 { 0xff1a1d23 };
    inline const juce::Colour line     { 0xff262a32 };
    inline const juce::Colour text     { 0xffe6e7ea };
    inline const juce::Colour dim      { 0xff8a8f98 };
    inline const juce::Colour accent   { 0xffe8b04b };

    inline constexpr float lineWidth   = 1.0f;   // hairlines (borders, separators)
    inline constexpr float trackWidth  = 2.0f;   // knob track / value arc / pointer
    inline constexpr float cornerRadius = 6.0f;

    // System sans, no bundled fonts. JUCE's generic default sans on macOS is Lucida Grande, and its
    // "system-ui" placeholder (SF) only resolves the Regular weight. Prefer "SF Pro Text" (real weights),
    // then "Helvetica Neue" (has a real Semibold), and only then the system-ui placeholder.
    inline const juce::String& uiFamily()
    {
        static const juce::String family = []
        {
            const auto installed = juce::Font::findAllTypefaceNames();
            for (auto* name : { "SF Pro Text", "Helvetica Neue" })
                if (installed.contains (name))
                    return juce::String (name);
            return juce::Font::getSystemUIFontName();
        }();
        return family;
    }

    inline juce::Font uiFont (float height, const juce::String& style, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (uiFamily(), style, height).withKerningFactor (tracking));
    }

    inline juce::Font title()  { return uiFont (15.0f, "Semibold", 0.30f); }   // wordmark, letter-spaced
    inline juce::Font label()  { return uiFont (10.5f, "Semibold", 0.14f); }   // small caps labels / buttons
    inline juce::Font value()  { return uiFont (13.0f, "Regular"); }           // values, menu items

    // Display spelling for chord names and roman numerals: "Bb" -> "B♭", "bVI" -> "♭VI", "#" -> "♯".
    inline juce::String pretty (const juce::String& s)
    {
        juce::String out;
        for (int i = 0; i < s.length(); ++i)
        {
            const auto c = s[i];
            const auto prev = i > 0 ? s[i - 1] : juce::juce_wchar (0);
            const auto next = i + 1 < s.length() ? s[i + 1] : juce::juce_wchar (0);
            const bool afterNote = prev >= 'A' && prev <= 'G';
            const bool beforeRoman = juce::String ("IViv").containsChar (next);
            if (c == 'b' && (afterNote || beforeRoman))
                out << juce::String::fromUTF8 ("\xe2\x99\xad");
            else if (c == '#')
                out << juce::String::fromUTF8 ("\xe2\x99\xaf");
            else
                out << juce::String::charToString (c);
        }
        return out;
    }

    // Matte panel: surface fill, hairline border and an optional small-caps title in the top-left.
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        r = r.reduced (0.5f);
        g.setColour (surface);
        g.fillRoundedRectangle (r, cornerRadius + 2.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, cornerRadius + 2.0f, lineWidth);
        if (title.isNotEmpty())
        {
            g.setColour (dim);
            g.setFont (label());
            g.drawText (title, r.reduced (16.0f, 0.0f).withHeight (36.0f), juce::Justification::centredLeft, false);
        }
    }
}
