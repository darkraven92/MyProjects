#pragma once

#include <cmath>
#include <cstdint>

namespace Navigation
{
    // A generated XY sector is a search request, never a movement target.
    // These tight bounds apply only to optional generated destinations.
    struct GeneratedRoamTargetPolicy
    {
        static constexpr float MaximumHorizontalProjection = 6.0f;
        static constexpr float MaximumVerticalProjection = 24.0f;
        static constexpr float ProbeHorizontalExtent = 3.0f;
        static constexpr float ProbeVerticalExtent = 2.0f;
        static constexpr int ProbeStep = 4;

        static bool BoundedProjection(float rawX, float rawY, float rawZ,
                                      float projectedX, float projectedY,
                                      float projectedZ)
        {
            return std::isfinite(rawX) && std::isfinite(rawY) &&
                std::isfinite(rawZ) && std::isfinite(projectedX) &&
                std::isfinite(projectedY) &&
                std::isfinite(projectedZ) &&
                std::hypot(rawX - projectedX, rawY - projectedY) <=
                    MaximumHorizontalProjection &&
                std::fabs(rawZ - projectedZ) <= MaximumVerticalProjection;
        }

        static bool SafeConnectedGround(bool projectionBounded,
                                        bool hardHazard,
                                        bool completeCorridor,
                                        bool terrainValid,
                                        bool waterRoute,
                                        bool steepFallback)
        {
            return projectionBounded && !hardHazard && completeCorridor &&
                terrainValid && !waterRoute && !steepFallback;
        }
    };
}
