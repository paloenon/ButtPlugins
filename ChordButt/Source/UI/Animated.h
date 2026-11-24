#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/Anim.h"

// Implemented by components that run cb::Anim smoothers. The editor's animation driver walks
// every descendant component, calls tickAnimations once per frame and repaints the component
// when it returns true. Implementations must not forward ticks to their own children (the
// driver already visits them).
struct Animated
{
    virtual ~Animated() = default;
    virtual bool tickAnimations (float dtSeconds) = 0;   // true: visual state changed, repaint me
};

namespace AnimUtil
{
    // Ticks `anim`, mirrors its value into the component property `id` (read by the
    // LookAndFeel) and returns true when the drawn value changed. "Changed" is measured against
    // the value last written to the property, not the anim's previous value: callers may snap()
    // the anim before ticking (e.g. a knob following the mouse), which would otherwise look like
    // "no change" and leave the drawn value stale.
    inline bool tickProperty (juce::Component& c, cb::Anim& anim, const juce::Identifier& id, float dt)
    {
        const bool moving = anim.tick (dt);
        auto& props = c.getProperties();
        const bool stale = ! props.contains (id) || ! juce::exactlyEqual ((float) props[id], anim.value);
        if (stale)
            props.set (id, anim.value);
        return moving || stale;
    }

    inline float property (const juce::Component& c, const juce::Identifier& id, float fallback = 0.0f)
    {
        return juce::jlimit (0.0f, 1.0f, (float) c.getProperties().getWithDefault (id, fallback));
    }

    // Ticks a plain Anim; true when the drawn value changed.
    inline bool tick (cb::Anim& anim, float dt)
    {
        const float before = anim.value;
        return anim.tick (dt) || ! juce::exactlyEqual (anim.value, before);
    }
}

// Text that cross-fades when it changes: the old string fades out while the new one fades in
// (with a slight upward drift). The first set() shows the text without animating.
struct FadeText
{
    juce::String from, to;
    cb::Anim t;   // 0 = `from` fully shown, 1 = `to` fully shown

    const juce::String& current() const { return to; }

    void set (const juce::String& text, bool animate = true)
    {
        if (primed && text == to)
            return;

        if (! primed || ! animate)
        {
            from = to = text;
            t.snap (1.0f);
            primed = true;
            return;
        }

        if (t.value < 0.5f)   // interrupted early: keep the string that is still mostly visible
            to = from;
        from = to;
        to = text;
        t.snap (0.0f);
        t.setTarget (1.0f);
    }

    bool tick (float dt) { return AnimUtil::tick (t, dt); }

    // fn (text, alpha, yOffset) is called for each visible string. The alphas are squared so the
    // old text is mostly gone before the new one becomes legible (less double-exposure mid-fade).
    template <typename Fn>
    void draw (Fn&& fn, float drift = 3.0f) const
    {
        const float out = 1.0f - t.value;
        if (out > 0.001f && from.isNotEmpty())
            fn (from, out * out, -drift * t.value);
        if (t.value > 0.001f && to.isNotEmpty())
            fn (to, t.value * t.value, drift * out);
    }

private:
    bool primed = false;
};
