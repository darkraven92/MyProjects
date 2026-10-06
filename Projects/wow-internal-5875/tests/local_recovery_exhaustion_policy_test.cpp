#include "../src/Navigation/CompleteLongStagePolicy.h"
#include "../src/Navigation/LocalRecoveryExhaustionPolicy.h"
#include "../src/Navigation/NavigationInitTelemetryPolicy.h"

#include <cassert>
#include <string_view>

int main()
{
    using Navigation::LocalRecoveryExhaustionDecision;
    using Navigation::LocalRecoveryExhaustionPolicy;

    // Mathematical replanning does not change the physical recovery budget,
    // regardless of whether the owner has a complete-long stage.
    const int attemptsAfterSuccessfulDetourReplan = 4;
    assert(LocalRecoveryExhaustionPolicy::Assess(
        attemptsAfterSuccessfulDetourReplan, 4, true, 0, 2) ==
        LocalRecoveryExhaustionDecision::TryLastSafeBacktrack);
    assert(LocalRecoveryExhaustionPolicy::Assess(
        attemptsAfterSuccessfulDetourReplan, 4, true, 0, 2) ==
        LocalRecoveryExhaustionDecision::TryLastSafeBacktrack);

    for (int attempts = 0; attempts < 4; ++attempts)
        assert(LocalRecoveryExhaustionPolicy::Assess(
            attempts, 4, true, 0, 2) ==
            LocalRecoveryExhaustionDecision::NotExhausted);
    assert(LocalRecoveryExhaustionPolicy::Assess(
        4, 4, false, 0, 2) ==
        LocalRecoveryExhaustionDecision::FailExhausted);
    assert(LocalRecoveryExhaustionPolicy::Assess(
        4, 4, true, 2, 2) ==
        LocalRecoveryExhaustionDecision::FailExhausted);
    // Ordinary bounded navigation now has the same bounded escalation.
    assert(LocalRecoveryExhaustionPolicy::Assess(
        4, 4, true, 0, 2) ==
        LocalRecoveryExhaustionDecision::TryLastSafeBacktrack);
    // Preserve the existing strict 4-unit destination-progress reset gate.
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(
        99.0f, 100.0f, 4.0f));
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(
        96.0f, 100.0f, 4.0f));
    assert(LocalRecoveryExhaustionPolicy::EarnedProgressReset(
        95.0f, 100.0f, 4.0f));
    // Only such a reset can re-arm four attempts; a plan alone cannot.
    assert(LocalRecoveryExhaustionPolicy::Assess(
        0, 4, true, 1, 2) ==
        LocalRecoveryExhaustionDecision::NotExhausted);

    using Navigation::NavigationInitTelemetryPolicy;
    using Navigation::NavigationPlanFailure;
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::SurfaceRecoveryExhausted)) ==
        "surface_recovery_exhausted");
    assert(NavigationInitTelemetryPolicy::QueryFailure(
        "Detour could not build a polygon path.") ==
        NavigationPlanFailure::NoPath);
    assert(NavigationInitTelemetryPolicy::QueryFailure(
        "Detour could not build a polygon path.") !=
        NavigationPlanFailure::SurfaceRecoveryExhausted);

    using Navigation::CompleteLongStagePolicy;
    using Navigation::CompleteLongPlanTarget;
    assert(CompleteLongStagePolicy::PlanTarget(false, false) ==
        CompleteLongPlanTarget::FinalDestination);
    assert(CompleteLongStagePolicy::StageIndexAfterAcceptedPlan(0, false) == 0);
    assert(CompleteLongStagePolicy::PlanTarget(true, false) ==
        CompleteLongPlanTarget::ActiveStage);
    assert(CompleteLongStagePolicy::StageIndexAfterAcceptedPlan(1, false) == 1);
    assert(!CompleteLongStagePolicy::FinalArrivalAllowed(true, false));
}
