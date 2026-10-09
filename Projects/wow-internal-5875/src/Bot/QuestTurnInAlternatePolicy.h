#pragma once

#include "../Navigation/NavigationInitTelemetryPolicy.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Bot
{
    // Strategic turn-in recovery is separate from a follower's ordinary
    // replan budget. Each accepted point is a short, independently verified
    // leg; the original NPC destination remains unchanged.
    class QuestTurnInAlternatePolicy
    {
    public:
        static constexpr int MaximumStrategicLegs = 2;
        static constexpr std::uint64_t MaximumProbeTicks = 80;
        static constexpr float EgressDistance = 32.0f;
        static constexpr float MinimumDisplacement = 18.0f;
        static constexpr float MaximumEgressRouteCost = 180.0f;
        static constexpr float MaximumAdditionalHazardRisk = 1.0f;
        static constexpr std::size_t MaximumCandidates = 4;

        static bool MayStartLeg(int completedLegs)
        {
            return completedLegs >= 0 &&
                completedLegs < MaximumStrategicLegs;
        }

        static bool MayRecover(
            bool followerFailed,
            Navigation::NavigationPlanFailure reason,
            int completedLegs)
        {
            // Health-safety aborts and unknown failures must not be turned
            // into fresh autonomous movement by a strategic quest owner.
            return followerFailed && MayStartLeg(completedLegs) &&
                (reason == Navigation::NavigationPlanFailure::
                    ReplanBudgetExhausted ||
                 reason == Navigation::NavigationPlanFailure::
                    SurfaceRecoveryExhausted);
        }

        template <typename Point>
        static float Distance2D(const Point& a, const Point& b)
        {
            return std::hypot(a.x - b.x, a.y - b.y);
        }

        template <typename Point>
        static std::array<Point, MaximumCandidates> Generate(
            const Point& origin, const Point& destination)
        {
            float dx = destination.x - origin.x;
            float dy = destination.y - origin.y;
            const float length = std::hypot(dx, dy);
            if (!std::isfinite(length) || length < 1.0f)
                return {};
            dx /= length;
            dy /= length;
            // Two lateral exits followed by two rear-lateral exits. These
            // leave the failed local passage without assuming a zone axis.
            return {{
                {origin.x - dy * EgressDistance,
                 origin.y + dx * EgressDistance, origin.z},
                {origin.x + dy * EgressDistance,
                 origin.y - dx * EgressDistance, origin.z},
                {origin.x + (-dy - dx) * EgressDistance * 0.70710678f,
                 origin.y + (dx - dy) * EgressDistance * 0.70710678f,
                 origin.z},
                {origin.x + (dy - dx) * EgressDistance * 0.70710678f,
                 origin.y + (-dx - dy) * EgressDistance * 0.70710678f,
                 origin.z}
            }};
        }

        template <typename Point>
        static bool CanGenerate(const Point& origin,
                                const Point& destination)
        {
            return std::isfinite(origin.x) && std::isfinite(origin.y) &&
                std::isfinite(origin.z) && std::isfinite(destination.x) &&
                std::isfinite(destination.y) &&
                std::isfinite(destination.z) &&
                Distance2D(origin, destination) >= 1.0f;
        }

        template <typename Point>
        static bool Accept(
            const Point& failurePosition,
            const Point& candidate,
            float routeCost,
            bool completeRoute,
            bool crossesFailedTransition,
            float currentRisk,
            float candidateRisk,
            const std::vector<Point>& previouslyTried)
        {
            if (!completeRoute || crossesFailedTransition ||
                !std::isfinite(failurePosition.x) ||
                !std::isfinite(failurePosition.y) ||
                !std::isfinite(candidate.x) ||
                !std::isfinite(candidate.y) ||
                !std::isfinite(candidate.z) ||
                !std::isfinite(routeCost) || routeCost < MinimumDisplacement ||
                routeCost > MaximumEgressRouteCost ||
                !std::isfinite(candidateRisk) || !std::isfinite(currentRisk) ||
                candidateRisk > currentRisk + MaximumAdditionalHazardRisk ||
                Distance2D(failurePosition, candidate) < MinimumDisplacement)
                return false;

            for (const auto& prior : previouslyTried)
                if (Distance2D(prior, candidate) < MinimumDisplacement)
                    return false;
            return true;
        }

        template <typename Point>
        static bool StageReached(const Point& failurePosition,
                                 const Point& current)
        {
            return Distance2D(failurePosition, current) >=
                MinimumDisplacement;
        }
    };
}
