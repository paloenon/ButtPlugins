#include <catch2/catch_test_macros.hpp>
#include "Core/HostWatch.h"

using namespace cb;

namespace
{
constexpr float frame = 1.0f / 60.0f;

// Runs `seconds` of UI frames; the host calls process() once per frame when `hostRuns`.
HostWatch::Output runFrames (HostWatch& w, float seconds, uint32_t& heartbeat, uint64_t requests, bool playing, bool hostRuns)
{
    HostWatch::Output out {};
    for (float t = 0.0f; t < seconds - 1.0e-4f; t += frame)
    {
        if (hostRuns)
            ++heartbeat;
        out = w.update (frame, heartbeat, requests, playing);
    }
    return out;
}
} // namespace

TEST_CASE ("HostWatch: no hint without a request, even when the host is silent", "[HostWatch]")
{
    HostWatch w;
    uint32_t hb = 0;
    const auto out = runFrames (w, 5.0f, hb, 0, false, false);
    REQUIRE_FALSE (out.showHint);
    REQUIRE_FALSE (out.stopPlayback);
}

TEST_CASE ("HostWatch: no hint while the host processes", "[HostWatch]")
{
    HostWatch w;
    uint32_t hb = 0;
    runFrames (w, 1.0f, hb, 0, false, true);
    const auto out = runFrames (w, 5.0f, hb, 1, true, true);   // Play pressed, loop running
    REQUIRE_FALSE (out.showHint);
    REQUIRE_FALSE (out.stopPlayback);
}

TEST_CASE ("HostWatch: request without process shows the hint after 250 ms and resets play after 500 ms", "[HostWatch]")
{
    HostWatch w;
    uint32_t hb = 0;
    runFrames (w, 2.0f, hb, 0, false, false);   // editor open for a while, host silent

    auto out = runFrames (w, 0.2f, hb, 1, true, false);   // Play pressed
    REQUIRE_FALSE (out.showHint);
    REQUIRE_FALSE (out.stopPlayback);
    out = runFrames (w, 0.1f, hb, 1, true, false);   // 0.3 s
    REQUIRE (out.showHint);
    REQUIRE_FALSE (out.stopPlayback);
    out = runFrames (w, 0.25f, hb, 1, true, false);   // 0.55 s
    REQUIRE (out.stopPlayback);

    // UI stopped playback: the hint stays a while, then fades away.
    out = runFrames (w, 1.0f, hb, 1, false, false);
    REQUIRE (out.showHint);
    REQUIRE_FALSE (out.stopPlayback);
    out = runFrames (w, 3.0f, hb, 1, false, false);
    REQUIRE_FALSE (out.showHint);
}

TEST_CASE ("HostWatch: hint disappears as soon as the host processes again", "[HostWatch]")
{
    HostWatch w;
    uint32_t hb = 0;
    runFrames (w, 0.5f, hb, 0, false, false);
    auto out = runFrames (w, 0.4f, hb, 1, false, false);   // pad audition
    REQUIRE (out.showHint);
    out = runFrames (w, frame, hb, 1, false, true);   // track selected in Logic
    REQUIRE_FALSE (out.showHint);
}

TEST_CASE ("HostWatch: host stops processing while the loop plays", "[HostWatch]")
{
    HostWatch w;
    uint32_t hb = 0;
    runFrames (w, 1.0f, hb, 1, true, true);
    auto out = runFrames (w, 0.3f, hb, 1, true, false);   // track deselected
    REQUIRE (out.showHint);
    REQUIRE_FALSE (out.stopPlayback);
    out = runFrames (w, 0.3f, hb, 1, true, false);
    REQUIRE (out.stopPlayback);
}
