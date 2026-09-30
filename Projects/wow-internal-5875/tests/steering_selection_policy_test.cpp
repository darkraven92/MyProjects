#include "../src/Navigation/SteeringSelectionPolicy.h"
#include "../src/Bot/DeathRecoveryPolicy.h"

#include <cassert>
#include <limits>
#include <vector>

int main()
{
    using Navigation::SteeringCandidateDecision;
    using Navigation::SteeringCandidateEvidence;
    using Navigation::SteeringSelectionPolicy;

    auto safe = [](std::size_t index)
    {
        SteeringCandidateEvidence candidate{};
        candidate.index = index;
        candidate.distanceAndRiseValid = true;
        candidate.raycastValid = true;
        candidate.raycastFraction = 1.0f;
        candidate.clearanceKnown = true;
        candidate.candidateClearance = 0.90f;
        return candidate;
    };

    // A: unsafe far target falls back to the nearer safe corridor point.
    auto far = safe(4);
    far.candidateClearance = 0.404f;
    auto near = safe(2);
    assert(SteeringSelectionPolicy::SelectFarthestSafe({far, near}) == 2);

    // B: a safe far target retains useful lookahead.
    far.candidateClearance = 0.70f;
    assert(SteeringSelectionPolicy::SelectFarthestSafe({far, near}) == 4);

    // C: no unsafe original target is selected when all candidates fail.
    far.candidateClearance = 0.404f;
    near.candidateClearance = 0.60f;
    assert(!SteeringSelectionPolicy::SelectFarthestSafe({far, near}));

    // D/E: an index changes only after both acceptance and successful CTM.
    assert(SteeringSelectionPolicy::CommitIndex(1, 4, false, true) == 1);
    assert(SteeringSelectionPolicy::CommitIndex(1, 4, true, false) == 1);
    assert(SteeringSelectionPolicy::CommitIndex(1, 4, true, true) == 4);
    assert(SteeringSelectionPolicy::CommitIndex(4, 2, true, false) == 4);
    assert(SteeringSelectionPolicy::CommitIndex(4, 2, true, true) == 2);

    // F: failed, incomplete, and unknown raycasts are rejected.
    auto candidate = safe(3);
    candidate.raycastValid = false;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedRaycast);
    candidate.raycastValid = true;
    candidate.raycastFraction = 0.5f;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedRaycast);
    candidate.raycastFraction = std::numeric_limits<float>::quiet_NaN();
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedRaycast);

    // G: a missing or non-finite wall measurement is not assumed safe.
    candidate = safe(3);
    candidate.clearanceKnown = false;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedClearance);
    candidate.clearanceKnown = true;
    candidate.candidateClearance = std::numeric_limits<float>::quiet_NaN();
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedClearance);

    // H: a useful inset gain is insufficient if final clearance stays <0.70.
    candidate = safe(3);
    candidate.candidateClearance = 0.20f;
    candidate.adjusted = true;
    candidate.adjustedClearanceKnown = true;
    candidate.adjustedClearance = 0.60f;
    candidate.adjustedRaycastValid = true;
    candidate.adjustedRaycastFraction = 1.0f;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedClearance);

    // I: the adjusted target needs both final clearance and reachability.
    candidate.adjustedClearance = 0.80f;
    candidate.adjustedClearanceKnown = false;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedClearance);
    candidate.adjustedClearanceKnown = true;
    candidate.adjustedRaycastValid = false;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedRaycast);
    candidate.adjustedRaycastValid = true;
    candidate.adjustedRaycastFraction = 0.5f;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::RejectedRaycast);
    candidate.adjustedRaycastFraction = 1.0f;
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::Accepted);
    candidate.candidateClearance = 0.60f;
    candidate.adjustedClearance = 0.75f; // safe despite <0.30 gain
    assert(SteeringSelectionPolicy::MeetsMinimumClearance(
        candidate.adjustedClearance));
    assert(SteeringSelectionPolicy::Assess(candidate) ==
        SteeringCandidateDecision::Accepted);

    // J: this steering policy does not broaden corpse reclaim/arrival policy.
    assert(Bot::DeathRecoveryPolicy::CanRetrieveCorpse(true, true, 0.0f, 6.709f));
    assert(!Bot::DeathRecoveryPolicy::CanRetrieveCorpse(true, true, 0.0f, 33.0f));
    assert(!Bot::DeathRecoveryPolicy::CanRetrieveCorpse(true, false, 0.0f, 6.709f));
}
