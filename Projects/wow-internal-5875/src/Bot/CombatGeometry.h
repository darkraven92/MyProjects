#pragma once
#include <cmath>
#include <limits>

namespace Bot
{
    // Geometry only: no range, attack, facing tolerance or recovery policy.
    struct CombatGeometry
    {
        static constexpr float Pi = 3.14159265358979323846f;
        static float Normalize(float angle)
        {
            if (!std::isfinite(angle)) return std::numeric_limits<float>::quiet_NaN();
            float result = std::fmod(angle, 2.0f * Pi);
            return result < 0.0f ? result + 2.0f * Pi : result;
        }
        static float SignedError(float yaw, float bearing)
        {
            if (!std::isfinite(yaw) || !std::isfinite(bearing))
                return std::numeric_limits<float>::quiet_NaN();
            return std::remainder(Normalize(bearing) - Normalize(yaw), 2.0f * Pi);
        }
    };
}
