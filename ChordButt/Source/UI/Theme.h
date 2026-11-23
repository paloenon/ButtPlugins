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
    // "system-ui" placeholder (SF) only resolves the Regular weight, so use the "SF Pro Text" family when
    // it is installed (gives real weights) and fall back to system-ui (SF, regular weight) otherwise.
    inline const juce::String& uiFamily()
    {
        static const juce::String family = juce::Font::findAllTypefaceNames().contains ("SF Pro Text")
                                               ? juce::String ("SF Pro Text")
                                               : juce::Font::getSystemUIFontName();
        return family;
    }

    inline juce::Font uiFont (float height, const juce::String& style, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (uiFamily(), style, height).withKerningFactor (tracking));
    }

    inline juce::Font title()  { return uiFont (15.0f, "Semibold", 0.30f); }   // wordmark, letter-spaced
    inline juce::Font label()  { return uiFont (10.5f, "Semibold", 0.14f); }   // small caps labels / buttons
    inline juce::Font value()  { return uiFont (13.0f, "Regular"); }           // values, menu items
}
