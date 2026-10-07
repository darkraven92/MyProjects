#pragma once

#include <string_view>
#include <cstddef>
#include <cstdint>

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
        SurfaceRecoveryExhausted,
        ReplanBudgetExhausted,
        HardStallExhausted,
        OtherUnknown,
        StartProjectionFailed,
        DestinationProjectionFailed,
        AvoidanceProjectionUnresolved,
        PersistentHazardRejected
    };

    // Snapshot of actual incremental work, not a route generation or CTM
    // dispatch. The same intent survives internal tier changes.
    struct NavigationInitializationObservation
    {
        bool pending = false;
        std::uint64_t intent = 0;
        NavigationInitTier tier = NavigationInitTier::None;
        std::size_t tilesProcessed = 0;
        std::size_t tilesTotal = 0;

        bool Valid() const
        {
            return pending && intent != 0 && tier != NavigationInitTier::None &&
                tilesTotal != 0 && tilesProcessed <= tilesTotal;
        }
    };

    struct NavigationInitTelemetryPolicy
    {
        static constexpr NavigationPlanFailure TerminalFailure(
            NavigationPlanFailure reason)
        {
            // A terminal follower failure must never be reported as success.
            return reason == NavigationPlanFailure::None
                ? NavigationPlanFailure::OtherUnknown : reason;
        }

        static constexpr NavigationPlanFailure QueryFailure(
            std::string_view error)
        {
            if (error == "No ground polygon was found near the start position.")
                return NavigationPlanFailure::StartProjectionFailed;
            if (error == "No ground polygon was found near the destination.")
                return NavigationPlanFailure::DestinationProjectionFailed;
            if (error == "Could not resolve start/end polygon for blocked-route query.")
                return NavigationPlanFailure::AvoidanceProjectionUnresolved;
            if (error == "Persistent navigation hazard memory rejected every safe corridor.")
                return NavigationPlanFailure::PersistentHazardRejected;
            return error == "Detour could not build a polygon path."
                ? NavigationPlanFailure::NoPath : NavigationPlanFailure::OtherUnknown;
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

        static constexpr bool RetainIntentForFallback(
            NavigationInitTier current, bool allowFullMapFallback)
        {
            return NextTier(current, allowFullMapFallback) !=
                NavigationInitTier::None;
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
                case NavigationPlanFailure::SurfaceRecoveryExhausted: return "surface_recovery_exhausted";
                case NavigationPlanFailure::ReplanBudgetExhausted: return "replan_budget_exhausted";
                case NavigationPlanFailure::HardStallExhausted: return "hard_stall_exhausted";
                case NavigationPlanFailure::StartProjectionFailed: return "start_projection_failed";
                case NavigationPlanFailure::DestinationProjectionFailed: return "destination_projection_failed";
                case NavigationPlanFailure::AvoidanceProjectionUnresolved: return "avoidance_projection_unresolved";
                case NavigationPlanFailure::PersistentHazardRejected: return "persistent_hazard_rejected";
                default: return "other_unknown";
            }
        }
    };
}
