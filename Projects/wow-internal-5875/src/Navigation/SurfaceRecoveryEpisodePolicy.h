#pragma once
#include <cmath>

namespace Navigation
{
    enum class SurfaceRecoveryQuality { Progress, Neutral, Regression };

    struct SurfaceRecoveryEpisodePolicy
    {
        // A waypoint reached by ordinary corridor following is stronger
        // evidence than movement to a lateral recovery target or a new path.
        static SurfaceRecoveryQuality Assess(float before, float after,
            float requiredDestinationGain, bool crossedOrdinaryPortal)
        {
            if (!std::isfinite(before) || !std::isfinite(after))
                return SurfaceRecoveryQuality::Neutral;
            if (after > before)
                return SurfaceRecoveryQuality::Regression;
            if (crossedOrdinaryPortal &&
                after + requiredDestinationGain < before)
                return SurfaceRecoveryQuality::Progress;
            return SurfaceRecoveryQuality::Neutral;
        }

        static bool SameLocalTarget(float firstX, float firstY,
            float nextX, float nextY, float arrivalDistance)
        {
            return std::isfinite(firstX) && std::isfinite(firstY) &&
                std::isfinite(nextX) && std::isfinite(nextY) &&
                std::isfinite(arrivalDistance) && arrivalDistance > 0 &&
                std::hypot(firstX-nextX,firstY-nextY) <= 2*arrivalDistance;
        }
    };
}
