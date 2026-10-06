#include "../src/Navigation/NavigationInitTelemetryPolicy.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
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
    static_assert(NavigationInitTelemetryPolicy::RetainIntentForFallback(
        NavigationInitTier::Route, false));
    static_assert(NavigationInitTelemetryPolicy::RetainIntentForFallback(
        NavigationInitTier::Expanded, true));
    static_assert(!NavigationInitTelemetryPolicy::RetainIntentForFallback(
        NavigationInitTier::Expanded, false));
    static_assert(!NavigationInitTelemetryPolicy::RetainIntentForFallback(
        NavigationInitTier::FullMap, true));

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
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::ReplanBudgetExhausted)) == "replan_budget_exhausted");
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::HardStallExhausted)) == "hard_stall_exhausted");
    assert(NavigationInitTelemetryPolicy::TerminalFailure(
        NavigationPlanFailure::None) == NavigationPlanFailure::OtherUnknown);
    assert(NavigationInitTelemetryPolicy::TerminalFailure(
        NavigationPlanFailure::NoPath) == NavigationPlanFailure::NoPath);
    assert(std::string_view(NavigationInitTelemetryPolicy::TierName(
        NavigationInitTier::FullMap)) == "full_map");

    // Initial planning failures must not publish a terminal movement release
    // before the already-supported next loading tier has been tried.
    std::ifstream followerFile("src/Navigation/GenericNavMeshPathFollower.h");
    assert(followerFile.good());
    const std::string follower{
        std::istreambuf_iterator<char>(followerFile),
        std::istreambuf_iterator<char>()};
    assert(follower.find("if (next == GenericNavMeshFollowState::Failed &&\n"
                         "                retainIntentForInitialFallback_)") !=
           std::string::npos);
    assert(follower.find("retainIntentForInitialFallback_ = ready &&") !=
           std::string::npos);
    assert(follower.find("const bool planReady = ready && PlanFrom(") !=
           std::string::npos);
    assert(follower.find("retainIntentForInitialFallback_ = false;") !=
           std::string::npos);
    assert(follower.find("lastPlanFailure_ = "
                         "NavigationPlanFailure::HardStallExhausted;") !=
           std::string::npos);
}
