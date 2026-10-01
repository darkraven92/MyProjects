#include "../src/Navigation/NavigationInitTelemetryPolicy.h"

#include <cassert>
#include <string_view>

int main()
{
    using Navigation::NavigationInitTelemetryPolicy;
    using Navigation::NavigationInitTier;
    using Navigation::NavigationPlanFailure;

    // Instrumentation describes the existing route -> expanded -> full-map
    // sequence without changing its caller-controlled fallback gate.
    static_assert(NavigationInitTelemetryPolicy::NextTier(
        NavigationInitTier::Route, false) == NavigationInitTier::Expanded);
    static_assert(NavigationInitTelemetryPolicy::NextTier(
        NavigationInitTier::Route, true) == NavigationInitTier::Expanded);
    static_assert(NavigationInitTelemetryPolicy::NextTier(
        NavigationInitTier::Expanded, false) == NavigationInitTier::None);
    static_assert(NavigationInitTelemetryPolicy::NextTier(
        NavigationInitTier::Expanded, true) == NavigationInitTier::FullMap);
    static_assert(NavigationInitTelemetryPolicy::NextTier(
        NavigationInitTier::FullMap, true) == NavigationInitTier::None);

    // Only source-explicit missing corridor/ground is called no_path.
    static_assert(NavigationInitTelemetryPolicy::QueryFailure(
        "Detour could not build a polygon path.") == NavigationPlanFailure::NoPath);
    static_assert(NavigationInitTelemetryPolicy::QueryFailure(
        "No ground polygon was found near the destination.") ==
        NavigationPlanFailure::NoPath);
    static_assert(NavigationInitTelemetryPolicy::QueryFailure(
        "Persistent navigation hazard memory rejected every safe corridor.") ==
        NavigationPlanFailure::OtherUnknown);

    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::InitializationFailed)) == "initialization_failed");
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::NoPath)) == "no_path");
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::PathValidationFailed)) == "path_validation_failed");
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::PathLengthExceeded)) == "path_length_exceeded");
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::PartialOrUnusablePath)) == "partial_or_unusable_path");
    assert(std::string_view(NavigationInitTelemetryPolicy::TierName(
        NavigationInitTier::FullMap)) == "full_map");
}
