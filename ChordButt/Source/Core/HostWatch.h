#pragma once
#include <algorithm>
#include <cstdint>

namespace cb
{
// Notices previews that cannot sound because the host does not call process(): Logic only runs a
// stopped instrument track's MIDI FX while the track is selected or record-enabled. Fed once per
// UI frame (message thread) with the processor's process() heartbeat, the preview engine's request
// counter (play / audition) and whether the engine reports playing.
class HostWatch
{
public:
    static constexpr float hintAfter = 0.25f;    // no process() for this long after a request: show the hint
    static constexpr float resetAfter = 0.5f;    // ... and for this long while "playing": stop (UI shows ▶ again)
    static constexpr float hintHold = 3.0f;      // a request keeps the hint relevant this long

    struct Output
    {
        bool showHint = false;
        bool stopPlayback = false;
    };

    Output update (float dtSeconds, uint32_t heartbeat, uint64_t requests, bool playing)
    {
        if (! primed)
        {
            primed = true;
            lastHeartbeat = heartbeat;
            lastRequests = requests;
        }
        else
        {
            if (heartbeat != lastHeartbeat) { lastHeartbeat = heartbeat; sinceHeartbeat = 0.0f; }
            else                            sinceHeartbeat = std::min (sinceHeartbeat + dtSeconds, 1.0e6f);
            if (requests != lastRequests)   { lastRequests = requests; sinceRequest = 0.0f; }
            else                            sinceRequest = std::min (sinceRequest + dtSeconds, 1.0e6f);
        }

        // How long the host has been silent since something was asked to sound.
        const float stall = std::min (sinceHeartbeat, sinceRequest);
        const bool wanted = playing || sinceRequest < hintHold;
        return { wanted && stall > hintAfter, playing && stall > resetAfter };
    }

private:
    bool primed = false;
    uint32_t lastHeartbeat = 0;
    uint64_t lastRequests = 0;
    float sinceHeartbeat = 0.0f, sinceRequest = 1.0e6f;
};
} // namespace cb
