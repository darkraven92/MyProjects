#pragma once

#include "CombatHealthTrendPolicy.h"

#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class EscapeDecision
    {
        Continue,
        Warning,
        EscapeCandidate
    };

    enum class EscapeDecisionReason
    {
        OutsideCombat,
        UnknownInput,
        Healthy,
        LowHealthOnly,
        MultiAggroPending,
        LowHealthMultiAggro,
        LowHealthHardStall,
        LowHealthDeteriorating,
        ClearHysteresis
    };

    struct EscapeDecisionInput
    {
        bool combatOwned = false;
        std::uint64_t playerGuid = 0;
        std::uint64_t lifeEpisode = 0;
        std::uint64_t targetGuid = 0;
        std::uint64_t observedAtMs = 0;
        bool playerHealthKnown = false;
        float playerHealthPct = 0.0f;
        bool targetHealthKnown = false;
        float targetHealthPct = 0.0f;
        bool aggressorsKnown = false;
        unsigned observedDirectAggressors = 0;
        bool combatHardStall = false;
    };

    struct EscapeDecisionAssessment
    {
        EscapeDecision decision = EscapeDecision::Continue;
        EscapeDecisionReason reason = EscapeDecisionReason::OutsideCombat;
        bool changed = false;
        unsigned corroboratedSamples = 0;
    };

    // Diagnostic only. In particular, no result authorizes movement, target
    // abandonment, or combat cancellation. The 20% threshold matches the
    // existing active-combat emergency-health observation.
    class EscapeDecisionPolicy
    {
    public:
        static constexpr float EmergencyHealthPercent = 20.0f;
        // The combat controller already uses an 8-yard local aggressor
        // radius for multi-target pressure (Thunder Clap ownership).
        static constexpr float LocalAggressorRadius = 8.0f;
        static constexpr unsigned RequiredMultiAggroSamples = 3;
        static constexpr unsigned RequiredClearSamples = 2;
        // A target above half health is materially alive; the verified 1v1
        // had 83% remaining. Near-dead finisher targets are excluded.
        static constexpr float TrendTargetHealthFloorPercent = 50.0f;

        static const char* DecisionName(EscapeDecision decision)
        {
            switch (decision)
            {
                case EscapeDecision::Continue: return "continue";
                case EscapeDecision::Warning: return "warning";
                case EscapeDecision::EscapeCandidate: return "escape_candidate";
            }
            return "continue";
        }

        static const char* ReasonName(EscapeDecisionReason reason)
        {
            switch (reason)
            {
                case EscapeDecisionReason::OutsideCombat: return "outside_combat";
                case EscapeDecisionReason::UnknownInput: return "unknown_input";
                case EscapeDecisionReason::Healthy: return "healthy";
                case EscapeDecisionReason::LowHealthOnly: return "low_health_only";
                case EscapeDecisionReason::MultiAggroPending: return "multi_aggro_pending";
                case EscapeDecisionReason::LowHealthMultiAggro: return "low_health_multi_aggro";
                case EscapeDecisionReason::LowHealthHardStall: return "low_health_hard_stall";
                case EscapeDecisionReason::LowHealthDeteriorating: return "low_health_deteriorating";
                case EscapeDecisionReason::ClearHysteresis: return "clear_hysteresis";
            }
            return "unknown_input";
        }

        void Reset()
        {
            decision_ = EscapeDecision::Continue;
            targetGuid_ = 0;
            corroboratedSamples_ = 0;
            clearSamples_ = 0;
            healthTrend_.Reset();
        }

        CombatHealthTrendAssessment HealthTrend() const
        {
            return healthTrend_.Current();
        }

        EscapeDecisionAssessment Observe(const EscapeDecisionInput& input)
        {
            const EscapeDecision prior = decision_;
            if (!input.combatOwned || input.targetGuid == 0)
            {
                Reset();
                return {decision_, EscapeDecisionReason::OutsideCombat,
                        decision_ != prior, 0};
            }

            if (!input.playerHealthKnown || !input.targetHealthKnown ||
                !input.aggressorsKnown ||
                !std::isfinite(input.playerHealthPct) ||
                !std::isfinite(input.targetHealthPct) ||
                input.playerHealthPct < 0.0f ||
                input.playerHealthPct > 100.0f ||
                input.targetHealthPct < 0.0f ||
                input.targetHealthPct > 100.0f)
            {
                Reset();
                return {decision_, EscapeDecisionReason::UnknownInput,
                        decision_ != prior, 0};
            }

            if (targetGuid_ != input.targetGuid)
            {
                Reset();
                targetGuid_ = input.targetGuid;
            }

            const CombatHealthTrendAssessment trend = healthTrend_.Observe({
                input.combatOwned, input.playerGuid, input.lifeEpisode,
                input.targetGuid, input.playerHealthKnown,
                input.playerHealthPct, input.observedAtMs});

            const bool lowHealth =
                input.playerHealthPct <= EmergencyHealthPercent;
            const bool multiAggro = input.observedDirectAggressors >= 2;
            if (lowHealth && input.combatHardStall &&
                input.observedDirectAggressors >= 1)
            {
                decision_ = EscapeDecision::EscapeCandidate;
                corroboratedSamples_ = RequiredMultiAggroSamples;
                clearSamples_ = 0;
                return {decision_, EscapeDecisionReason::LowHealthHardStall,
                        decision_ != prior, corroboratedSamples_};
            }

            if (lowHealth && multiAggro)
            {
                if (decision_ == EscapeDecision::EscapeCandidate)
                    corroboratedSamples_ = RequiredMultiAggroSamples;
                else if (corroboratedSamples_ < RequiredMultiAggroSamples)
                    ++corroboratedSamples_;
                clearSamples_ = 0;
                decision_ = corroboratedSamples_ >= RequiredMultiAggroSamples
                    ? EscapeDecision::EscapeCandidate
                    : EscapeDecision::Warning;
                return {decision_, decision_ == EscapeDecision::EscapeCandidate
                        ? EscapeDecisionReason::LowHealthMultiAggro
                        : EscapeDecisionReason::MultiAggroPending,
                        decision_ != prior, corroboratedSamples_};
            }

            if (lowHealth && input.observedDirectAggressors >= 1 &&
                input.targetHealthPct >= TrendTargetHealthFloorPercent &&
                trend.deteriorating)
            {
                decision_ = EscapeDecision::EscapeCandidate;
                clearSamples_ = 0;
                corroboratedSamples_ = 0;
                return {decision_, EscapeDecisionReason::LowHealthDeteriorating,
                        decision_ != prior, trend.declineObservations};
            }

            corroboratedSamples_ = 0;
            if (decision_ != EscapeDecision::Continue &&
                (!lowHealth || decision_ == EscapeDecision::EscapeCandidate) &&
                ++clearSamples_ < RequiredClearSamples)
            {
                return {decision_, EscapeDecisionReason::ClearHysteresis,
                        false, 0};
            }

            clearSamples_ = 0;
            decision_ = lowHealth
                ? EscapeDecision::Warning : EscapeDecision::Continue;
            return {decision_, lowHealth
                    ? EscapeDecisionReason::LowHealthOnly
                    : EscapeDecisionReason::Healthy,
                    decision_ != prior, 0};
        }

    private:
        EscapeDecision decision_ = EscapeDecision::Continue;
        std::uint64_t targetGuid_ = 0;
        unsigned corroboratedSamples_ = 0;
        unsigned clearSamples_ = 0;
        CombatHealthTrendPolicy healthTrend_{};
    };
}
