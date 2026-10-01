#include "../src/Navigation/CorridorReplanHysteresisPolicy.h"
#include "../src/Navigation/SteeringSelectionPolicy.h"

#include <cassert>
#include <cstdint>

int main()
{
    using Navigation::CorridorFailureRecord;
    using Navigation::CorridorReplanDecision;
    using Navigation::CorridorReplanHysteresisPolicy;
    using Navigation::CorridorReplanReason;

    constexpr float meaningfulProgress = 4.0f;
    constexpr std::uint64_t failedFingerprint = 123;
    constexpr std::uint64_t alternateFingerprint = 456;

    CorridorFailureRecord failure{};
    auto assess = [&](std::uint64_t fingerprint, float x, float y,
                      bool recoveryAvailable = true,
                      bool candidateKnown = false,
                      std::uint64_t fromPoly = 0,
                      std::uint64_t toPoly = 0)
    {
        return CorridorReplanHysteresisPolicy::Assess(
            failure, fingerprint, x, y, meaningfulProgress,
            recoveryAvailable, candidateKnown, fromPoly, toPoly);
    };

    // D: a previously successful matching fingerprint is not a failure.
    assert(assess(failedFingerprint, 0.0f, 0.0f).decision ==
        CorridorReplanDecision::Allow);

    CorridorReplanHysteresisPolicy::RecordFailure(
        failure, failedFingerprint, 10.0f, 20.0f, meaningfulProgress,
        true, 11, 22);
    // A: only a recent failed corridor at the same physical anchor is blocked.
    assert(assess(failedFingerprint, 10.0f, 20.0f).decision ==
        CorridorReplanDecision::SuppressRepeated);
    assert(assess(failedFingerprint, 10.0f, 20.0f).reason ==
        CorridorReplanReason::ExactFingerprint);

    // B: the same directed local transition catches a changed path hash.
    const auto equivalent = assess(
        alternateFingerprint, 10.0f, 20.0f, true, true, 11, 22);
    assert(equivalent.decision == CorridorReplanDecision::SuppressRepeated);
    assert(equivalent.reason == CorridorReplanReason::LocalTransitionMatch);
    assert(equivalent.localTransitionMatch);

    // C/F/G: either endpoint of the directed pair changing is a new exit.
    assert(assess(alternateFingerprint, 10.0f, 20.0f,
        true, true, 11, 33).reason ==
        CorridorReplanReason::DifferentLocalTransition);
    assert(assess(alternateFingerprint, 10.0f, 20.0f,
        true, true, 33, 22).decision == CorridorReplanDecision::Allow);

    // H: an edge present only later in the corridor is not the local pair.
    const std::uint64_t corridor[] = {11, 33, 11, 22};
    assert(corridor[2] == 11 && corridor[3] == 22);
    assert(assess(alternateFingerprint, 10.0f, 20.0f,
        true, true, corridor[0], corridor[1]).decision ==
        CorridorReplanDecision::Allow);

    // D: an unresolved transition cannot be guessed from proximity.
    assert(assess(alternateFingerprint, 10.0f, 20.0f).reason ==
        CorridorReplanReason::TransitionUnknown);
    assert(assess(alternateFingerprint, 10.0f, 20.0f,
        true, true, 0, 0).reason == CorridorReplanReason::TransitionUnknown);
    CorridorFailureRecord unresolved{};
    CorridorReplanHysteresisPolicy::RecordFailure(
        unresolved, failedFingerprint, 10.0f, 20.0f,
        meaningfulProgress, true, 0, 0);
    assert(!unresolved.transitionKnown);
    assert(CorridorReplanHysteresisPolicy::Assess(
        unresolved, alternateFingerprint, 10.0f, 20.0f,
        meaningfulProgress, true, true, 11, 22).reason ==
        CorridorReplanReason::TransitionUnknown);
    // F/G: suppression never accepts/commits a steering index or replans.
    std::size_t pointIndex = 2;
    int recursiveReplans = 0;
    if (assess(failedFingerprint, 10.0f, 20.0f).decision ==
        CorridorReplanDecision::Allow)
    {
        pointIndex = Navigation::SteeringSelectionPolicy::CommitIndex(
            pointIndex, 5, true, true);
        ++recursiveReplans;
    }
    assert(pointIndex == 2 && recursiveReplans == 0);

    // A repeated failure cannot shift the anchor and postpone progress reset.
    CorridorReplanHysteresisPolicy::RecordFailure(
        failure, alternateFingerprint, 12.0f, 20.0f, meaningfulProgress,
        true, 11, 22);
    assert(failure.anchorX == 10.0f);
    assert(failure.fingerprint == failedFingerprint);
    // C/I: a different corridor is allowed even without movement.
    assert(assess(alternateFingerprint, 10.0f, 20.0f).decision ==
        CorridorReplanDecision::Allow);
    assert(assess(alternateFingerprint, 10.0f, 20.0f,
        true, true, 11, 33).decision == CorridorReplanDecision::Allow);
    assert(assess(failedFingerprint, 10.0f, 20.0f).decision ==
        CorridorReplanDecision::SuppressRepeated);

    // H: repeated bounded recovery choices eventually reach escalation.
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        assert(assess(failedFingerprint, 10.0f, 20.0f, true).decision ==
            CorridorReplanDecision::SuppressRepeated);
    }
    assert(assess(failedFingerprint, 10.0f, 20.0f, false).decision ==
        CorridorReplanDecision::Escalate);
    assert(assess(alternateFingerprint, 10.0f, 20.0f,
        false, true, 11, 22).decision == CorridorReplanDecision::Escalate);
    // B/J/L: real physical progress permits even the same corpse corridor.
    const auto progressed = assess(alternateFingerprint, 14.0f, 20.0f,
        true, true, 11, 22);
    assert(progressed.decision == CorridorReplanDecision::AllowAfterProgress);
    assert(progressed.reason == CorridorReplanReason::PhysicalProgress);
    assert(progressed.anchorDistance >= meaningfulProgress);
    failure = CorridorFailureRecord{}; // caller clears after observed progress
    assert(assess(failedFingerprint, 14.0f, 20.0f).decision ==
        CorridorReplanDecision::Allow);

    // E/M: a new destination episode does not inherit a previous failure.
    CorridorReplanHysteresisPolicy::RecordFailure(
        failure, failedFingerprint, 14.0f, 20.0f, meaningfulProgress);
    failure = CorridorFailureRecord{};
    assert(assess(failedFingerprint, 14.0f, 20.0f).decision ==
        CorridorReplanDecision::Allow);

    // A later real failure can resolve unknown refs without moving its anchor.
    CorridorReplanHysteresisPolicy::RecordFailure(
        unresolved, failedFingerprint, 11.0f, 20.0f,
        meaningfulProgress, true, 11, 22);
    assert(unresolved.anchorX == 10.0f && unresolved.transitionKnown);

    // K: the independent 14N.1 minimum clearance contract is unchanged.
    assert(Navigation::SteeringSelectionPolicy::MinimumWallClearance == 0.70f);
    assert(!Navigation::SteeringSelectionPolicy::MeetsMinimumClearance(0.699f));
    assert(Navigation::SteeringSelectionPolicy::MeetsMinimumClearance(0.70f));
}
