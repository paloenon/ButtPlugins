#pragma once
#include "UI/Animated.h"

// Small, dim one-line hint that fades in when previews are requested but the host is not processing
// ("Select the track in Logic to hear previews"). Purely visual: never takes mouse clicks.
class HostHint : public juce::Component, public Animated
{
public:
    static const char* const text;

    HostHint();
    void setShown (bool shouldShow) { shown.setTarget (shouldShow ? 1.0f : 0.0f); }
    bool isShown() const { return shown.target > 0.5f; }
    float getAmount() const { return shown.value; }

    void paint (juce::Graphics&) override;
    bool tickAnimations (float dt) override;

private:
    cb::Anim shown;
};
