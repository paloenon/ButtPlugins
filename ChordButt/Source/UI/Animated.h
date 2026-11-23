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
    // LookAndFeel) and returns true when the drawn value changed.
    inline bool tickProperty (juce::Component& c, cb::Anim& anim, const juce::Identifier& id, float dt)
    {
        const float before = anim.value;
        const bool moving = anim.tick (dt);
        const bool changed = moving || ! juce::exactlyEqual (anim.value, before);
        if (changed || ! c.getProperties().contains (id))
            c.getProperties().set (id, anim.value);
        return changed;
    }

    inline float property (const juce::Component& c, const juce::Identifier& id)
    {
        return juce::jlimit (0.0f, 1.0f, (float) c.getProperties().getWithDefault (id, 0.0f));
    }
}
