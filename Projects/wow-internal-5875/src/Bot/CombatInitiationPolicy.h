#pragma once

#include "CombatFacingPolicy.h"

#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class CombatInitiationPhase { ChargeFacing, Chase, Melee };
    enum class CombatInitiationReason
    {
        Ready, TargetEvidenceStale, SelectionUnknown, SelectionMismatch, ActionWait,
        InputBlocked, ActionEvidenceUnknown
    };
    inline const char* CombatInitiationReasonName(CombatInitiationReason reason)
    {
        switch (reason)
        {
            case CombatInitiationReason::Ready: return "ready";
            case CombatInitiationReason::TargetEvidenceStale: return "target_evidence_stale";
            case CombatInitiationReason::SelectionUnknown: return "selection_unknown";
            case CombatInitiationReason::SelectionMismatch: return "selection_mismatch";
            case CombatInitiationReason::ActionWait: return "legitimate_action_wait";
            case CombatInitiationReason::InputBlocked: return "input_blocked";
            case CombatInitiationReason::ActionEvidenceUnknown: return "action_evidence_unknown";
        }
        return "unknown";
    }
    struct CombatInitiationEvidence
    {
        bool targetFresh=false, selectionKnown=false, selectionMatches=false;
        bool actionKnown=false, actionInputSafe=false, actionWait=false;
        bool alternateKnown=false, alternateInputSafe=false, alternateWait=false;
    };
    struct CombatInitiationDecision
    {
        bool stateProgressAllowed=false;
        bool offensiveInputAllowed=false;
        CombatInitiationReason reason=CombatInitiationReason::TargetEvidenceStale;
    };
    class CombatInitiationPolicy
    {
    public:
        static constexpr float MeleeEnterDistance=5.0f;
        static constexpr float ChaseResumeDistance=6.0f;
        static constexpr float ChargeMinimumDistance=8.0f;
        static constexpr float ChargeMaximumDistance=25.0f;
        static bool EnterMelee(float distance)
        { return std::isfinite(distance) && distance<=MeleeEnterDistance; }
        static bool ResumeChase(float distance)
        { return !std::isfinite(distance) || distance>ChaseResumeDistance; }
        static bool ChargeRangeEligible(float distance)
        { return std::isfinite(distance) && distance>=ChargeMinimumDistance &&
            distance<=ChargeMaximumDistance; }

        // Melee-liveness readback is not the owner of Charge or chase. A
        // missing Attack-action readback cannot freeze their bounded FSMs.
        // Actual offensive input still requires a positive input-safety probe.
        static CombatInitiationDecision Decide(CombatInitiationPhase phase,
            const CombatInitiationEvidence& e)
        {
            if (!e.targetFresh)
                return {false,false,CombatInitiationReason::TargetEvidenceStale};
            if (!e.selectionKnown)
                return {false,false,CombatInitiationReason::SelectionUnknown};
            if (e.actionKnown && e.actionWait)
                return {false,false,CombatInitiationReason::ActionWait};
            if (!e.actionKnown && e.alternateKnown && e.alternateWait)
                return {false,false,CombatInitiationReason::ActionWait};
            if (e.actionKnown && !e.actionInputSafe)
                return {false,false,CombatInitiationReason::InputBlocked};
            if (!e.actionKnown && e.alternateKnown && !e.alternateInputSafe)
                return {false,false,CombatInitiationReason::InputBlocked};
            if (!e.selectionMatches)
                return {phase!=CombatInitiationPhase::Melee,false,
                    CombatInitiationReason::SelectionMismatch};
            const bool inputSafe=e.actionKnown ? e.actionInputSafe :
                (e.alternateKnown && e.alternateInputSafe);
            if (phase==CombatInitiationPhase::Melee && !inputSafe)
                return {false,false,CombatInitiationReason::ActionEvidenceUnknown};
            return {true,inputSafe,inputSafe ? CombatInitiationReason::Ready :
                CombatInitiationReason::ActionEvidenceUnknown};
        }

        // A pre-selection alignment never earns a Charge confirmation. The
        // delayed native UI selection begins a fresh two-snapshot sequence.
        static std::uint32_t ConfirmChargeFacing(std::uint32_t previous,
            bool selectedOwnGuid, bool aligned)
        {
            if (!selectedOwnGuid || !aligned) return 0;
            return previous<CombatFacingPolicy::StableSnapshotsRequired ?
                previous+1 : previous;
        }
    };
}
