#pragma once

#include "EscapeDecisionPolicy.h"

#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class EscapeEvaluationEpisodeEventKind
    {
        None,
        Start,
        Keep,
        Cancel,
        Complete
    };

    enum class EscapeEvaluationEpisodeReason
    {
        None,
        TriggerEscapeCandidate,
        TransientCandidateCleared,
        WorldInvalid,
        PlayerDead,
        CombatEnded,
        TargetChanged,
        TargetLost,
        TargetDead,
        EvidenceInvalid,
        HealthRecovered,
        CandidateCleared,
        TerminalRouteResult
    };

    struct EscapeEvaluationEpisodeInput
    {
        bool worldValid = false;
        bool playerAlive = false;
        bool combatOwned = false;
        std::uint64_t targetGuid = 0;
        bool targetPresent = false;
        bool targetAlive = false;
        bool targetVitalsKnown = false;
        bool playerHealthKnown = false;
        float playerHealthPct = 0.0f;
        EscapeDecision decision = EscapeDecision::Continue;
    };

    struct EscapeEvaluationEpisodeEvent
    {
        EscapeEvaluationEpisodeEventKind kind =
            EscapeEvaluationEpisodeEventKind::None;
        EscapeEvaluationEpisodeReason reason =
            EscapeEvaluationEpisodeReason::None;
    };

    // A route evaluation is triggered by 15C.0, but is not owned by its
    // instantaneous classification. One evaluation is permitted per combat
    // target episode; completion/cancellation closes it until that identity
    // ends. No decision here authorizes flee movement or combat handoff.
    class EscapeEvaluationEpisodePolicy
    {
    private:
        enum class State { Idle, Evaluating, Closed };
        State state_ = State::Idle;
        std::uint64_t targetGuid_ = 0;
        EscapeDecision previousDecision_ = EscapeDecision::Continue;

        EscapeEvaluationEpisodeEvent Close(
            EscapeEvaluationEpisodeReason reason)
        {
            const bool wasEvaluating = state_ == State::Evaluating;
            if (state_ != State::Idle)
                state_ = State::Closed;
            return wasEvaluating
                ? EscapeEvaluationEpisodeEvent{
                    EscapeEvaluationEpisodeEventKind::Cancel, reason}
                : EscapeEvaluationEpisodeEvent{};
        }

    public:
        // Unchanged per-probe limit. The same existing bound now also caps
        // the entire diagnostic episode, so three sequential probes cannot
        // multiply emergency-evaluation latency by three.
        static constexpr std::uint64_t MaximumProbeTicks = 160;
        static constexpr std::uint64_t MaximumEpisodeTicks =
            MaximumProbeTicks;

        static bool ProbeTimedOut(std::uint64_t startedTick,
                                  std::uint64_t currentTick)
        {
            return currentTick < startedTick ||
                currentTick - startedTick >= MaximumProbeTicks;
        }

        static bool EpisodeTimedOut(std::uint64_t startedTick,
                                    std::uint64_t currentTick)
        {
            return currentTick < startedTick ||
                currentTick - startedTick >= MaximumEpisodeTicks;
        }

        static const char* ReasonName(EscapeEvaluationEpisodeReason reason)
        {
            switch (reason)
            {
                case EscapeEvaluationEpisodeReason::None: return "none";
                case EscapeEvaluationEpisodeReason::TriggerEscapeCandidate:
                    return "trigger_escape_candidate";
                case EscapeEvaluationEpisodeReason::TransientCandidateCleared:
                    return "transient_candidate_cleared";
                case EscapeEvaluationEpisodeReason::WorldInvalid:
                    return "world_invalid";
                case EscapeEvaluationEpisodeReason::PlayerDead:
                    return "player_dead";
                case EscapeEvaluationEpisodeReason::CombatEnded:
                    return "combat_ended";
                case EscapeEvaluationEpisodeReason::TargetChanged:
                    return "target_changed";
                case EscapeEvaluationEpisodeReason::TargetLost:
                    return "target_lost";
                case EscapeEvaluationEpisodeReason::TargetDead:
                    return "target_dead";
                case EscapeEvaluationEpisodeReason::EvidenceInvalid:
                    return "evidence_invalid";
                case EscapeEvaluationEpisodeReason::HealthRecovered:
                    return "health_recovered";
                case EscapeEvaluationEpisodeReason::CandidateCleared:
                    return "candidate_cleared";
                case EscapeEvaluationEpisodeReason::TerminalRouteResult:
                    return "terminal_route_result";
            }
            return "none";
        }

        bool Active() const { return state_ == State::Evaluating; }
        bool Closed() const { return state_ == State::Closed; }
        std::uint64_t TargetGuid() const { return targetGuid_; }

        void Reset()
        {
            state_ = State::Idle;
            targetGuid_ = 0;
            previousDecision_ = EscapeDecision::Continue;
        }

        EscapeEvaluationEpisodeEvent Observe(
            const EscapeEvaluationEpisodeInput& input)
        {
            if (!input.worldValid)
                return Close(EscapeEvaluationEpisodeReason::WorldInvalid);
            if (!input.playerAlive)
            {
                const auto event = Close(
                    EscapeEvaluationEpisodeReason::PlayerDead);
                Reset();
                return event;
            }
            if (!input.combatOwned || input.targetGuid == 0)
            {
                const auto event = Close(
                    EscapeEvaluationEpisodeReason::CombatEnded);
                Reset();
                return event;
            }
            if (state_ != State::Idle && input.targetGuid != targetGuid_)
            {
                const auto event = Close(
                    EscapeEvaluationEpisodeReason::TargetChanged);
                Reset();
                return event; // never transfer an evaluation on this tick.
            }
            if (!input.targetPresent)
                return Close(EscapeEvaluationEpisodeReason::TargetLost);
            if (!input.targetAlive)
                return Close(EscapeEvaluationEpisodeReason::TargetDead);
            if (!input.targetVitalsKnown || !input.playerHealthKnown ||
                !std::isfinite(input.playerHealthPct) ||
                input.playerHealthPct < 0.0f ||
                input.playerHealthPct > 100.0f)
                return Close(EscapeEvaluationEpisodeReason::EvidenceInvalid);

            if (state_ == State::Closed)
                return {};
            if (state_ == State::Idle)
            {
                if (input.decision != EscapeDecision::EscapeCandidate)
                    return {};
                state_ = State::Evaluating;
                targetGuid_ = input.targetGuid;
                previousDecision_ = input.decision;
                return {EscapeEvaluationEpisodeEventKind::Start,
                        EscapeEvaluationEpisodeReason::TriggerEscapeCandidate};
            }

            if (input.decision == EscapeDecision::Continue)
                return Close(input.playerHealthPct >
                        EscapeDecisionPolicy::EmergencyHealthPercent
                    ? EscapeEvaluationEpisodeReason::HealthRecovered
                    : EscapeEvaluationEpisodeReason::CandidateCleared);

            const bool warningAfterCandidate =
                previousDecision_ == EscapeDecision::EscapeCandidate &&
                input.decision == EscapeDecision::Warning;
            previousDecision_ = input.decision;
            return warningAfterCandidate
                ? EscapeEvaluationEpisodeEvent{
                    EscapeEvaluationEpisodeEventKind::Keep,
                    EscapeEvaluationEpisodeReason::TransientCandidateCleared}
                : EscapeEvaluationEpisodeEvent{};
        }

        EscapeEvaluationEpisodeEvent Complete()
        {
            if (state_ != State::Evaluating)
                return {};
            state_ = State::Closed;
            return {EscapeEvaluationEpisodeEventKind::Complete,
                    EscapeEvaluationEpisodeReason::TerminalRouteResult};
        }
    };
}
