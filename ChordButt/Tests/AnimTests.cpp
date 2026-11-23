#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Core/Anim.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("Anim: reaches one time constant (~63.2%) after 0.12 s")
{
    cb::Anim a;
    a.setTarget (1.0f);
    for (int i = 0; i < 12; ++i)
        a.tick (0.01f);
    REQUIRE_THAT (a.value, WithinAbs (0.632, 0.01));
}

TEST_CASE ("Anim: settles exactly on target after 1 s and stops ticking")
{
    cb::Anim a;
    a.setTarget (1.0f);
    bool stillMoving = true;
    for (int i = 0; i < 60; ++i)
        stillMoving = a.tick (1.0f / 60.0f);
    REQUIRE (a.value == 1.0f);
    REQUIRE_FALSE (stillMoving);
    REQUIRE_FALSE (a.tick (1.0f / 60.0f));
}

TEST_CASE ("Anim: tick reports motion while far from target")
{
    cb::Anim a;
    a.setTarget (1.0f);
    REQUIRE (a.tick (0.016f));
    REQUIRE (a.value > 0.0f);
    REQUIRE (a.value < 1.0f);
}

TEST_CASE ("Anim: snap sets value and target")
{
    cb::Anim a;
    a.setTarget (0.3f);
    a.snap (0.75f);
    REQUIRE (a.value == 0.75f);
    REQUIRE (a.target == 0.75f);
    REQUIRE_FALSE (a.tick (0.016f));
}
