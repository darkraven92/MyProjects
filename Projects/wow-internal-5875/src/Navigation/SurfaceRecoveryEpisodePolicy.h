#pragma once
#include <cmath>
#include <cstdint>

namespace Navigation
{
    enum class SurfaceRecoveryQuality { Progress, Neutral, Regression };

    struct SurfaceRecoveryEpisodePolicy
    {
        static bool VerifiedPortalStageProgress(bool interiorStage,
            std::uint64_t issuedFrom, std::uint64_t issuedTo,
            std::uint64_t initialPoly, std::uint64_t observedPoly,
            std::uint64_t rayLastPoly, bool sameRouteGeneration,
            bool rayComplete, bool terrainSafe, float initialDistance,
            float observedDistance, float requiredGain)
        {
            return interiorStage && issuedFrom != 0 && issuedTo != 0 &&
                issuedFrom != issuedTo && initialPoly == issuedFrom &&
                observedPoly == issuedTo && rayLastPoly == issuedTo &&
                sameRouteGeneration && rayComplete && terrainSafe &&
                std::isfinite(initialDistance) &&
                std::isfinite(observedDistance) &&
                observedDistance + requiredGain < initialDistance;
        }

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
