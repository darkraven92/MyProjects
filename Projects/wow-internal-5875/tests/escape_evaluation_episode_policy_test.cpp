#include "../src/Bot/EscapeEvaluationEpisodePolicy.h"
#include "../src/Bot/EscapeRouteSelectionPolicy.h"
#include "../src/Navigation/RouteCostProbePolicy.h"

#include <cassert>

using namespace Bot;

static EscapeEvaluationEpisodeInput Valid(
    EscapeDecision decision = EscapeDecision::EscapeCandidate,
    std::uint64_t targetGuid = 42,
    float healthPct = 3.5f)
{
    EscapeEvaluationEpisodeInput input{};
    input.worldValid = true;
    input.playerAlive = true;
    input.combatOwned = true;
    input.targetGuid = targetGuid;
    input.targetPresent = true;
    input.targetAlive = true;
    input.targetVitalsKnown = true;
    input.playerHealthKnown = true;
    input.playerHealthPct = healthPct;
    input.decision = decision;
    return input;
}

int main()
{
    using Kind = EscapeEvaluationEpisodeEventKind;
    using Reason = EscapeEvaluationEpisodeReason;
    EscapeEvaluationEpisodePolicy episode{};

    // Warning alone cannot start planning. A candidate starts once.
    assert(episode.Observe(Valid(EscapeDecision::Warning)).kind == Kind::None);
    auto event = episode.Observe(Valid());
    assert(event.kind == Kind::Start);
    assert(event.reason == Reason::TriggerEscapeCandidate);
    assert(episode.Active());
    assert(episode.TargetGuid() == 42);

    // The hard-stall pulse clears, but low-HP combat with the same live
    // target still owns the already-started bounded evaluation.
    event = episode.Observe(Valid(EscapeDecision::Warning));
    assert(event.kind == Kind::Keep);
    assert(event.reason == Reason::TransientCandidateCleared);
    assert(episode.Active());
    for (int i = 0; i < 20; ++i)
        assert(episode.Observe(Valid(EscapeDecision::Warning)).kind == Kind::None);
    assert(episode.Active());
    assert(episode.Complete().kind == Kind::Complete);
    assert(episode.Closed());
    // A later candidate pulse in the same fight cannot restart the probes.
    for (int i = 0; i < 20; ++i)
        assert(episode.Observe(Valid()).kind == Kind::None);

    // Real combat exit clears episode identity for a subsequent fight.
    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    auto ended = Valid(EscapeDecision::Continue);
    ended.combatOwned = false;
    event = episode.Observe(ended);
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::CombatEnded);
    assert(!episode.Active());

    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    event = episode.Observe(Valid(EscapeDecision::EscapeCandidate, 43));
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::TargetChanged);
    assert(!episode.Active());
    // Never silently transfer the old route to the new target on that tick.
    assert(episode.Observe(Valid(EscapeDecision::Warning, 43)).kind == Kind::None);
    assert(episode.Observe(Valid(EscapeDecision::EscapeCandidate, 43)).kind ==
           Kind::Start);

    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    auto missing = Valid();
    missing.targetPresent = false;
    event = episode.Observe(missing);
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::TargetLost);
    assert(episode.Observe(Valid()).kind == Kind::None); // closed same target

    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    auto deadTarget = Valid();
    deadTarget.targetAlive = false;
    event = episode.Observe(deadTarget);
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::TargetDead);

    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    auto deadPlayer = Valid();
    deadPlayer.playerAlive = false;
    event = episode.Observe(deadPlayer);
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::PlayerDead);

    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    auto invalid = Valid();
    invalid.worldValid = false;
    event = episode.Observe(invalid);
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::WorldInvalid);
    assert(episode.Observe(Valid()).kind == Kind::None);

    episode.Reset();
    assert(episode.Observe(Valid()).kind == Kind::Start);
    invalid = Valid();
    invalid.targetVitalsKnown = false;
    event = episode.Observe(invalid);
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::EvidenceInvalid);

    // 15C.0's existing two-sample recovery semantics determine the release:
    // a first safe-health sample retains Candidate; Continue then cancels.
    EscapeDecisionPolicy decision{};
    EscapeDecisionInput decisionInput{};
    decisionInput.combatOwned = true;
    decisionInput.targetGuid = 42;
    decisionInput.playerHealthKnown = true;
    decisionInput.playerHealthPct = 3.5f;
    decisionInput.targetHealthKnown = true;
    decisionInput.targetHealthPct = 92.0f;
    decisionInput.aggressorsKnown = true;
    decisionInput.observedDirectAggressors = 1;
    decisionInput.combatHardStall = true;
    episode.Reset();
    assert(episode.Observe(Valid(decision.Observe(decisionInput).decision)).kind ==
           Kind::Start);
    decisionInput.combatHardStall = false;
    decisionInput.playerHealthPct = 75.0f;
    const auto firstSafe = decision.Observe(decisionInput);
    assert(firstSafe.decision == EscapeDecision::EscapeCandidate);
    assert(episode.Observe(Valid(firstSafe.decision, 42, 75.0f)).kind == Kind::None);
    const auto secondSafe = decision.Observe(decisionInput);
    assert(secondSafe.decision == EscapeDecision::Continue);
    event = episode.Observe(Valid(secondSafe.decision, 42, 75.0f));
    assert(event.kind == Kind::Cancel);
    assert(event.reason == Reason::HealthRecovered);

    // The per-candidate bound is unchanged. The same 160 ticks now bound
    // the whole evaluation, rather than allowing three sequential 160-tick
    // probes. Clock rollback remains a terminal timeout.
    assert(!EscapeEvaluationEpisodePolicy::ProbeTimedOut(10, 169));
    assert(EscapeEvaluationEpisodePolicy::ProbeTimedOut(10, 170));
    assert(EscapeEvaluationEpisodePolicy::ProbeTimedOut(10, 9));
    assert(EscapeEvaluationEpisodePolicy::MaximumEpisodeTicks ==
           EscapeEvaluationEpisodePolicy::MaximumProbeTicks);
    assert(!EscapeEvaluationEpisodePolicy::EpisodeTimedOut(10, 169));
    assert(EscapeEvaluationEpisodePolicy::EpisodeTimedOut(10, 170));
    assert(EscapeEvaluationEpisodePolicy::EpisodeTimedOut(10, 9));
    assert(EscapeRouteSelectionPolicy::MaximumCandidates == 3);

    // All terminal route outcomes close the same diagnostic episode even if
    // hardStall has cleared. No outcome dispatches movement or changes combat.
    for (auto outcome : {EscapeRouteDecisionKind::CandidateAvailable,
                         EscapeRouteDecisionKind::NoReachableCandidate,
                         EscapeRouteDecisionKind::InsufficientData})
    {
        (void)outcome;
        episode.Reset();
        assert(episode.Observe(Valid()).kind == Kind::Start);
        assert(episode.Observe(Valid(EscapeDecision::Warning)).kind == Kind::Keep);
        assert(episode.Complete().kind == Kind::Complete);
        assert(episode.Complete().kind == Kind::None);
        assert(episode.Observe(Valid()).kind == Kind::None);
    }
    assert(!Navigation::RouteCostProbePolicy::MayIssueMovement(true));
}
