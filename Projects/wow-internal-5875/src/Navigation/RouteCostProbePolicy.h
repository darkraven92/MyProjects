#pragma once

#include "NavigationInitTelemetryPolicy.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace Navigation
{
    enum class RouteCostProbeStatus
    {
        Pending,
        Reachable,
        Unreachable
    };

    struct RouteCostProbeResult
    {
        RouteCostProbeStatus status = RouteCostProbeStatus::Pending;
        float pathLength = 0.0f;
        NavigationPlanFailure failure = NavigationPlanFailure::None;
    };

    struct RouteCostProbePolicy
    {
        static std::uint64_t HazardFingerprint(std::vector<std::uint64_t> refs)
        {
            std::sort(refs.begin(), refs.end());
            refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
            std::uint64_t hash = 14695981039346656037ull;
            for (auto ref : refs)
                for (unsigned i = 0; i < 8; ++i, ref >>= 8)
                    hash = (hash ^ (ref & 255u)) * 1099511628211ull;
            return hash;
        }
        static constexpr bool MayIssueMovement(bool planningOnly)
        {
            return !planningOnly;
        }

        static constexpr RouteCostProbeResult Ready(float pathLength)
        {
            return {RouteCostProbeStatus::Reachable, pathLength,
                    NavigationPlanFailure::None};
        }

        static constexpr RouteCostProbeResult Failed(
            NavigationPlanFailure reason)
        {
            return {RouteCostProbeStatus::Unreachable, 0.0f, reason};
        }
    };
}
