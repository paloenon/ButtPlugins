#pragma once
#include <cmath>

namespace cb
{
    // Exponential smoother used for every visual state change (time constant 0.12 s).
    // Header-only, no GUI dependency, so it is unit-testable in ChordCoreTests.
    struct Anim
    {
        static constexpr float timeConstant = 0.12f;
        static constexpr float epsilon = 0.001f;

        float value = 0, target = 0;

        void snap (float v) { value = target = v; }
        void setTarget (float t) { target = t; }

        // Advances the value towards the target; returns true while still moving.
        bool tick (float dtSeconds)
        {
            if (dtSeconds > 0.0f)
                value += (target - value) * (1.0f - std::exp (-dtSeconds / timeConstant));

            if (std::abs (target - value) > epsilon)
                return true;

            value = target;
            return false;
        }
    };
}
