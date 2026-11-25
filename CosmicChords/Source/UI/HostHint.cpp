#include "UI/HostHint.h"
#include "UI/Theme.h"

using namespace juce;

const char* const HostHint::text = "Select the track in Logic to hear previews";

HostHint::HostHint()
{
    setInterceptsMouseClicks (false, false);
}

void HostHint::paint (Graphics& g)
{
    const float a = shown.value;
    if (a <= 0.001f)
        return;
    // Fades in with a slight upward drift, like the other text transitions.
    auto r = getLocalBounds().toFloat().translated (0.0f, 3.0f * (1.0f - a));
    g.setFont (Theme::uiFont (12.0f, "Regular", 0.02f));
    g.setColour (Theme::dim.withMultipliedAlpha (a));
    g.drawText (text, r, Justification::centred, true);
}

bool HostHint::tickAnimations (float dt)
{
    return AnimUtil::tick (shown, dt);
}
