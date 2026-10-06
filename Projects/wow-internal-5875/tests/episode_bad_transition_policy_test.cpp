#include "../src/Navigation/EpisodeBadTransitionPolicy.h"

#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using Navigation::DirectedPolyTransition;
    using Navigation::EpisodeBadTransitionPolicy;
    using Decision = EpisodeBadTransitionPolicy::AlternativeDecision;

    constexpr DirectedPolyTransition bad{10, 20};
    constexpr DirectedPolyTransition reverse{20, 10};
    EpisodeBadTransitionPolicy episode{};

    // One unsafe portal can recover successfully; it is not proof of a bad
    // edge and must not create an avoidance record.
    episode.ObserveDispatch(bad, true);
    assert(!episode.ExhaustionProvesEdge(bad, 1, 8));
    assert(!episode.BoundaryEvidence(true, true, true).Valid());
    assert(episode.Size() == 0);
    episode.ClearObservation(); // Meaningful progress earned a new budget.
    assert(!episode.ExhaustionProvesEdge(bad, 8, 8));
    assert(!episode.BoundaryEvidence(true, true, true).Valid());

    // A terminal replan boundary is evidence only for repeated stalled
    // recoveries of one currently observed directed edge. It can occur well
    // before the separate 8/8 vertical-recovery exhaustion boundary.
    episode.ObserveDispatch(bad, true);
    assert(episode.ObserveStalled() == bad);
    assert(episode.StalledObservations() == 1);
    assert(!episode.BoundaryEvidence(true, true, true).Valid());
    episode.ObserveDispatch(bad, true); // Internal replan keeps the observation.
    assert(episode.ObserveStalled() == bad);
    assert(episode.StalledObservations() == 2);
    assert(!episode.ExhaustionProvesEdge(bad, 2, 8));
    assert(!episode.BoundaryEvidence(false, true, true).Valid());
    assert(!episode.BoundaryEvidence(true, false, true).Valid());
    assert(!episode.BoundaryEvidence(true, true, false).Valid());
    assert(episode.BoundaryEvidence(true, true, true) == bad);
    episode.ClearStallEvidence(); // Genuine progress invalidates old stalls.
    assert(!episode.BoundaryEvidence(true, true, true).Valid());

    // Alternating edges do not combine their stalled observations.
    constexpr DirectedPolyTransition other{30, 40};
    episode.ObserveDispatch(bad, true);
    assert(episode.ObserveStalled() == bad);
    episode.ObserveDispatch(other, true);
    assert(episode.ObserveStalled() == other);
    assert(!episode.BoundaryEvidence(true, true, true).Valid());
    episode.ObserveDispatch(bad, true);
    assert(episode.ObserveStalled() == bad);
    assert(!episode.BoundaryEvidence(true, true, true).Valid());
    episode.ClearObservation();
    assert(!episode.BoundaryEvidence(true, true, true).Valid());

    // Eight accepted recovery commands still exhaust the follower's safety
    // budget, but issuance without stalls cannot prove an unusable edge.
    for (int i = 0; i < 8; ++i)
        episode.ObserveDispatch(bad, true);
    assert(!episode.ExhaustionProvesEdge(bad, 7, 8));
    assert(!episode.ExhaustionProvesEdge(bad, 8, 8));
    episode.ClearObservation();
    for (int i = 0; i < 8; ++i)
    {
        episode.ObserveDispatch(bad, true);
        assert(episode.ObserveStalled() == bad);
    }
    assert(episode.ExhaustionProvesEdge(bad, 8, 8));

    // The runtime pattern: earlier stalls, then two locally successful
    // recovery steps. The attempt budget remains exhausted, but old stalls
    // cannot turn that safety stop into a learned bad edge.
    EpisodeBadTransitionPolicy progressAtExhaustion{};
    for (int i = 0; i < 3; ++i)
    {
        progressAtExhaustion.ObserveDispatch(bad, true);
        progressAtExhaustion.ClearStallEvidence();
    }
    for (int i = 0; i < 3; ++i)
    {
        progressAtExhaustion.ObserveDispatch(bad, true);
        assert(progressAtExhaustion.ObserveStalled() == bad);
    }
    for (int i = 0; i < 2; ++i)
    {
        progressAtExhaustion.ObserveDispatch(bad, true);
        progressAtExhaustion.ClearStallEvidence();
    }
    assert(progressAtExhaustion.ConsecutiveIssued() == 8);
    assert(progressAtExhaustion.StalledObservations() == 0);
    assert(!progressAtExhaustion.ExhaustionProvesEdge(bad, 8, 8));
    assert(!progressAtExhaustion.BoundaryEvidence(true, true, true).Valid());

    // New same-edge failures after a progress reset may independently prove
    // the edge; the three older stalls do not contribute to the new count.
    EpisodeBadTransitionPolicy stallsAfterProgress{};
    for (int i = 0; i < 4; ++i)
    {
        stallsAfterProgress.ObserveDispatch(bad, true);
        assert(stallsAfterProgress.ObserveStalled() == bad);
    }
    stallsAfterProgress.ClearStallEvidence();
    for (int i = 0; i < 2; ++i)
    {
        stallsAfterProgress.ObserveDispatch(bad, true);
        stallsAfterProgress.ClearStallEvidence();
    }
    assert(stallsAfterProgress.StalledObservations() == 0);
    assert(!stallsAfterProgress.ExhaustionProvesEdge(bad, 6, 8));
    for (int i = 0; i < 2; ++i)
    {
        stallsAfterProgress.ObserveDispatch(bad, true);
        assert(stallsAfterProgress.ObserveStalled() == bad);
    }
    assert(stallsAfterProgress.StalledObservations() == 2);
    assert(stallsAfterProgress.ExhaustionProvesEdge(bad, 8, 8));

    // An edge change cannot borrow either issuance or stalls from another
    // directed edge, even when the follower's independent budget reaches 8.
    EpisodeBadTransitionPolicy alternatingAtExhaustion{};
    for (int i = 0; i < 3; ++i)
    {
        alternatingAtExhaustion.ObserveDispatch(bad, true);
        assert(alternatingAtExhaustion.ObserveStalled() == bad);
    }
    alternatingAtExhaustion.ObserveDispatch(other, true);
    assert(alternatingAtExhaustion.ObserveStalled() == other);
    for (int i = 0; i < 4; ++i)
    {
        alternatingAtExhaustion.ObserveDispatch(bad, true);
        assert(alternatingAtExhaustion.ObserveStalled() == bad);
    }
    assert(!alternatingAtExhaustion.ExhaustionProvesEdge(bad, 8, 8));
    assert(!alternatingAtExhaustion.ExhaustionProvesEdge(other, 8, 8));

    EpisodeBadTransitionPolicy rejectedDispatch{};
    for (int i = 0; i < 7; ++i)
    {
        rejectedDispatch.ObserveDispatch(bad, true);
        assert(rejectedDispatch.ObserveStalled() == bad);
    }
    rejectedDispatch.ObserveDispatch(bad, false); // Attempt 8 rejected CTM.
    assert(rejectedDispatch.ConsecutiveIssued() == 0);
    assert(rejectedDispatch.StalledObservations() == 0);
    assert(!rejectedDispatch.ExhaustionProvesEdge(bad, 8, 8));

    assert(episode.Learn(bad));
    assert(!episode.Learn(bad));
    assert(episode.Learned(bad));
    assert(!episode.Learned(reverse));
    assert(episode.Size() == 1);

    const std::vector<std::uint64_t> original{1, 10, 20, 30};
    const std::vector<std::uint64_t> alternative{1, 10, 25, 30};
    const std::vector<std::uint64_t> reverseRoute{1, 20, 10, 30};
    assert(episode.FirstMatch(original) == bad);
    assert(!episode.FirstMatch(reverseRoute).Valid());
    assert(episode.MatchedTargetPolygons(original) ==
           std::vector<std::uint64_t>{20});
    assert(episode.AssessAlternative(original, true, alternative) ==
           Decision::UseAlternative);
    assert(episode.AssessAlternative(original, false, {}) ==
           Decision::NoAlternative);
    assert(episode.AssessAlternative(original, true, original) ==
           Decision::NoAlternative);
    assert(episode.AssessAlternative(reverseRoute, false, {}) ==
           Decision::KeepOriginal);

    // The same alternate-corridor gate is used after boundary learning;
    // absence of an alternate cannot silently accept the failed edge.
    EpisodeBadTransitionPolicy atBoundary{};
    atBoundary.ObserveDispatch(bad, true);
    assert(atBoundary.ObserveStalled() == bad);
    atBoundary.ObserveDispatch(bad, true);
    assert(atBoundary.ObserveStalled() == bad);
    const DirectedPolyTransition boundaryEdge =
        atBoundary.BoundaryEvidence(true, true, true);
    assert(boundaryEdge == bad);
    assert(atBoundary.Learn(boundaryEdge));
    assert(atBoundary.AssessAlternative(original, true, alternative) ==
           Decision::UseAlternative);
    assert(atBoundary.AssessAlternative(original, false, {}) ==
           Decision::NoAlternative);
    assert(!atBoundary.FirstMatch(reverseRoute).Valid());
    assert(!atBoundary.BoundaryEvidence(true, true, false).Valid());
    atBoundary.Reset();
    assert(!atBoundary.FirstMatch(original).Valid());
    assert(!atBoundary.BoundaryEvidence(true, true, true).Valid());

    // An internal replan does not reset this follower-owned policy.
    assert(episode.FirstMatch(original) == bad);
    episode.Reset(); // Start() for a genuinely new destination episode.
    assert(episode.Size() == 0);
    assert(!episode.FirstMatch(original).Valid());
    assert(episode.ConsecutiveIssued() == 0);

    // Bounded memory and no silent reverse-edge equivalence.
    for (std::uint64_t i = 1; i <= 12; ++i)
        assert(episode.Learn({100 + i, 200 + i}));
    assert(episode.Size() == EpisodeBadTransitionPolicy::MaximumLearned);
    assert(!episode.Learned({101, 201}));
    assert(episode.Learned({112, 212}));
    assert(!episode.Learn({0, 20}));
    assert(!episode.Learn({10, 10}));

    episode.Reset();
    episode.ObserveDispatch(bad, true);
    episode.ObserveDispatch(reverse, true);
    assert(!episode.ExhaustionProvesEdge(bad, 8, 8));
    assert(!episode.ExhaustionProvesEdge(reverse, 8, 8));
}
