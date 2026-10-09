#pragma once

#include "RuntimeRobustnessSupervisor.h"
#include "../Navigation/NavigationInitTelemetryPolicy.h"

#include <cstdint>

namespace Bot
{
    enum class GrindInterruptionReason
    {
        NoTarget,
        TargetUnreachable,
        NavigationFailure,
        LocalRecoveryExhausted,
        CombatHardStall,
        LootFailure,
        RecoveryBlocked,
        DeathRecoveryFailure,
        ManualVendorRequired,
        IntentionalWait,
        UnexpectedIdle,
        OtherUnknown
    };

    struct GrindInterruptionFacts
    {
        bool manualVendorWait = false;
        bool deathRecoveryFailed = false;
        bool recoveryBlocked = false;
        bool lootFailed = false;
        bool noTarget = false;
        bool intentionalWait = false;
        bool approachFailure = false;
        bool navigationFailure = false;
        Navigation::NavigationPlanFailure navigationReason =
            Navigation::NavigationPlanFailure::None;
        RuntimeRobustnessReason watchdogReason = RuntimeRobustnessReason::None;
    };

    struct GrindEnduranceEdges
    {
        bool combatStarted = false;
        bool deathStarted = false;
        bool manualVendorEntered = false;
        bool manualVendorCleared = false;
    };

    class GrindEnduranceEdgeTracker
    {
    private:
        bool combatActive_ = false;
        bool deathOwned_ = false;
        bool manualVendorWaiting_ = false;

    public:
        GrindEnduranceEdges Observe(
            bool combatActive,
            bool deathOwned,
            bool manualVendorWaiting)
        {
            const GrindEnduranceEdges edges{
                combatActive && !combatActive_,
                deathOwned && !deathOwned_,
                manualVendorWaiting && !manualVendorWaiting_,
                !manualVendorWaiting && manualVendorWaiting_
            };
            combatActive_ = combatActive;
            deathOwned_ = deathOwned;
            manualVendorWaiting_ = manualVendorWaiting;
            return edges;
        }
    };

    class GrindEnduranceDiagnosticPolicy
    {
    public:
        static constexpr GrindInterruptionReason Classify(
            const GrindInterruptionFacts& facts)
        {
            if (facts.manualVendorWait)
                return GrindInterruptionReason::ManualVendorRequired;
            if (facts.deathRecoveryFailed)
                return GrindInterruptionReason::DeathRecoveryFailure;
            if (facts.recoveryBlocked ||
                facts.watchdogReason == RuntimeRobustnessReason::RecoveryOwnerTimeout ||
                facts.watchdogReason == RuntimeRobustnessReason::FirstAidOwnerTimeout)
                return GrindInterruptionReason::RecoveryBlocked;
            if (facts.lootFailed)
                return GrindInterruptionReason::LootFailure;
            if (facts.navigationFailure)
            {
                if (facts.navigationReason ==
                    Navigation::NavigationPlanFailure::SurfaceRecoveryExhausted)
                    return GrindInterruptionReason::LocalRecoveryExhausted;
                if (facts.navigationReason == Navigation::NavigationPlanFailure::None ||
                    facts.navigationReason == Navigation::NavigationPlanFailure::OtherUnknown)
                    return GrindInterruptionReason::OtherUnknown;
                return facts.approachFailure
                    ? GrindInterruptionReason::TargetUnreachable
                    : GrindInterruptionReason::NavigationFailure;
            }
            if (facts.intentionalWait)
                return GrindInterruptionReason::IntentionalWait;
            if (facts.watchdogReason == RuntimeRobustnessReason::CombatOwnerTimeout)
                return GrindInterruptionReason::CombatHardStall;
            if (facts.watchdogReason == RuntimeRobustnessReason::MovementOwnerTimeout ||
                facts.watchdogReason == RuntimeRobustnessReason::VendorOwnerTimeout)
                return GrindInterruptionReason::NavigationFailure;
            if (facts.noTarget)
                return GrindInterruptionReason::NoTarget;
            if (facts.watchdogReason == RuntimeRobustnessReason::IdleDeadlock ||
                facts.watchdogReason == RuntimeRobustnessReason::UnexpectedSeatedIdle ||
                facts.watchdogReason == RuntimeRobustnessReason::RepeatedActivityLoop)
                return GrindInterruptionReason::UnexpectedIdle;
            return GrindInterruptionReason::OtherUnknown;
        }

        static constexpr const char* Name(GrindInterruptionReason reason)
        {
            switch (reason)
            {
                case GrindInterruptionReason::NoTarget: return "no_target";
                case GrindInterruptionReason::TargetUnreachable: return "target_unreachable";
                case GrindInterruptionReason::NavigationFailure: return "navigation_failure";
                case GrindInterruptionReason::LocalRecoveryExhausted: return "local_recovery_exhausted";
                case GrindInterruptionReason::CombatHardStall: return "combat_hard_stall";
                case GrindInterruptionReason::LootFailure: return "loot_failure";
                case GrindInterruptionReason::RecoveryBlocked: return "recovery_blocked";
                case GrindInterruptionReason::DeathRecoveryFailure: return "death_recovery_failure";
                case GrindInterruptionReason::ManualVendorRequired: return "manual_vendor_required";
                case GrindInterruptionReason::IntentionalWait: return "intentional_wait";
                case GrindInterruptionReason::UnexpectedIdle: return "unexpected_idle";
                case GrindInterruptionReason::OtherUnknown:
                default: return "other_unknown";
            }
        }

        static constexpr const char* WatchdogName(RuntimeRobustnessReason reason)
        {
            switch (reason)
            {
                case RuntimeRobustnessReason::TacticalNoProgress: return "tactical_no_progress";
                case RuntimeRobustnessReason::StrategicNoOutcome: return "strategic_no_outcome";
                case RuntimeRobustnessReason::IdleDeadlock: return "idle_deadlock";
                case RuntimeRobustnessReason::UnexpectedSeatedIdle: return "unexpected_seated_idle";
                case RuntimeRobustnessReason::RepeatedActivityLoop: return "repeated_activity_loop";
                case RuntimeRobustnessReason::RecoveryOwnerTimeout: return "recovery_owner_timeout";
                case RuntimeRobustnessReason::FirstAidOwnerTimeout: return "first_aid_owner_timeout";
                case RuntimeRobustnessReason::VendorOwnerTimeout: return "vendor_owner_timeout";
                case RuntimeRobustnessReason::MovementOwnerTimeout: return "movement_owner_timeout";
                case RuntimeRobustnessReason::CombatOwnerTimeout: return "combat_owner_timeout";
                case RuntimeRobustnessReason::None:
                default: return "none";
            }
        }

        static constexpr bool CountsAsUnexpectedIdle(GrindInterruptionReason reason)
        {
            return reason == GrindInterruptionReason::UnexpectedIdle;
        }
    };
}
