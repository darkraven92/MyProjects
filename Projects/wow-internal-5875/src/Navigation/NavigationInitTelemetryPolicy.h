#pragma once

#include <string_view>

namespace Navigation
{
    enum class NavigationInitTier
    {
        None,
        Route,
        Expanded,
        FullMap
    };

    enum class NavigationPlanFailure
    {
        None,
        InitializationFailed,
        NoPath,
        PathValidationFailed,
        PathLengthExceeded,
        PartialOrUnusablePath,
        OtherUnknown
    };

    struct NavigationInitTelemetryPolicy
    {
        static constexpr NavigationPlanFailure QueryFailure(
            std::string_view error)
        {
            return error == "Detour could not build a polygon path." ||
                error == "No ground polygon was found near the start position." ||
                error == "No ground polygon was found near the destination."
                ? NavigationPlanFailure::NoPath
                : NavigationPlanFailure::OtherUnknown;
        }

        static constexpr NavigationInitTier NextTier(
            NavigationInitTier current, bool allowFullMapFallback)
        {
            if (current == NavigationInitTier::Route)
                return NavigationInitTier::Expanded;
            if (current == NavigationInitTier::Expanded && allowFullMapFallback)
                return NavigationInitTier::FullMap;
            return NavigationInitTier::None;
        }

        static constexpr const char* TierName(NavigationInitTier tier)
        {
            switch (tier)
            {
                case NavigationInitTier::Route: return "route";
                case NavigationInitTier::Expanded: return "expanded";
                case NavigationInitTier::FullMap: return "full_map";
                default: return "none";
            }
        }

        static constexpr const char* ReasonName(NavigationPlanFailure reason)
        {
            switch (reason)
            {
                case NavigationPlanFailure::None: return "none";
                case NavigationPlanFailure::InitializationFailed: return "initialization_failed";
                case NavigationPlanFailure::NoPath: return "no_path";
                case NavigationPlanFailure::PathValidationFailed: return "path_validation_failed";
                case NavigationPlanFailure::PathLengthExceeded: return "path_length_exceeded";
                case NavigationPlanFailure::PartialOrUnusablePath: return "partial_or_unusable_path";
                default: return "other_unknown";
            }
        }
    };
}
