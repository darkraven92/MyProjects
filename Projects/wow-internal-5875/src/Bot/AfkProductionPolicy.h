#pragma once
#include "AfkQualificationHold.h"

namespace Bot
{
    enum class AfkProductionPhase { Recent, Due, Deferred, VerifyingInput, RecoveringAfk, VerifyingClear, Faulted };
    enum class AfkProductionBand { Recent, Due, Overdue, ThresholdCrossed };

    // Implementation qualification is a reviewed release fact, not a volatile
    // session bit. P0.0.6 user evidence: two qualification windows + first
    // harmless Grinding pulse. Every new command still needs live verification.
    class AfkProductionPolicy
    {
        AfkProductionPhase phase_=AfkProductionPhase::Recent;
        bool due_=false, haveClock_=false, delivered_=false;
        std::uint32_t observedClock_=0, issuedClock_=0;
        std::uint64_t issuedAt_=0;
        const char* failure_="production_fault";
        AfkDecision Fail(const char* reason)
        {
            phase_=AfkProductionPhase::Faulted; failure_=reason;
            return {AfkStatus::Fault,AfkAction::None,AfkResult::Failed,reason};
        }
    public:
        static constexpr bool ImplementationQualified=true;
        static constexpr std::uint64_t VerificationMs=AfkProtectionPolicy::VerificationMs;
        static AfkProductionBand Band(const AfkObservation& o)
        {
            const auto age=std::uint32_t(o.clientNow-o.lastInput);
            if (age>=o.thresholdMs) return AfkProductionBand::ThresholdCrossed;
            if (age>=o.thresholdMs-o.thresholdMs/10) return AfkProductionBand::Overdue;
            if (age>=o.thresholdMs-o.thresholdMs/5) return AfkProductionBand::Due;
            return AfkProductionBand::Recent;
        }
        static const char* Blocker(const AfkSafety& s,bool benign)
        {
            if (s.fault) return AfkRuntimeFaultPolicy::ReasonName(s.faultReason);
            if (s.death) return "death_recovery";
            if (s.combat) return "combat_or_ability_owner";
            if (s.recovery) return "recovery_owner";
            if (s.water) return "water_or_unsafe_native_movement";
            if (s.dialog) return "dialog";
            if (s.vendor) return "vendor";
            if (s.trainer) return "trainer";
            if (s.talents) return "talents";
            if (s.equipment) return "equipment_or_explicit_input";
            if (s.loot) return "loot";
            return AfkWorkloadSafetyPolicy::Classify(s,benign)==AfkWorkloadSafety::Unsafe ?
                "unclassified_owner" : nullptr;
        }
        AfkProductionPhase Phase() const { return phase_; }
        static const char* PhaseName(AfkProductionPhase phase)
        {
            switch (phase)
            {
            case AfkProductionPhase::Recent: return "Recent";
            case AfkProductionPhase::Due: return "Due";
            case AfkProductionPhase::Deferred: return "Deferred";
            case AfkProductionPhase::VerifyingInput: return "VerifyingInput";
            case AfkProductionPhase::RecoveringAfk: return "RecoveringAfk";
            case AfkProductionPhase::VerifyingClear: return "VerifyingClear";
            case AfkProductionPhase::Faulted: return "Faulted";
            }
            return "Unknown";
        }
        bool Due() const { return due_; }
        bool SessionDeliveryVerified() const { return delivered_; }
        AfkAction PendingAction() const
        {
            return phase_==AfkProductionPhase::VerifyingInput ? AfkAction::InputPulse :
                phase_==AfkProductionPhase::VerifyingClear ? AfkAction::NativeAutoClear : AfkAction::None;
        }
        AfkDecision Update(const AfkObservation& o,const AfkSafety& s,std::uint64_t now,
            bool observeOnly=false,bool benign=false,AfkAutoClearSetting setting=AfkAutoClearSetting::Unknown)
        {
            if (phase_==AfkProductionPhase::Faulted) return Fail(failure_);
            const bool pending=phase_==AfkProductionPhase::VerifyingInput ||
                phase_==AfkProductionPhase::RecoveringAfk || phase_==AfkProductionPhase::VerifyingClear;
            if (!AfkProtectionPolicy::Valid(o))
                return pending ? Fail("production_observation_lost") : AfkDecision{};
            const bool active=o.clientAfk || o.serverAfk;
            if (observeOnly) return {active ? AfkStatus::AfkDetected : AfkStatus::Active,
                AfkAction::None,AfkResult::None,"observe_only"};
            if (pending)
            {
                if (now<issuedAt_ || now-issuedAt_>=VerificationMs)
                    return Fail(phase_==AfkProductionPhase::VerifyingInput ? "production_delivery_timeout" :
                        o.clientAfk!=o.serverAfk ? "mixed_afk_requires_safe_reconciliation" : "production_clear_timeout");
                if (phase_==AfkProductionPhase::VerifyingInput)
                {
                    if (o.lastInput==issuedClock_ || std::uint32_t(o.clientNow-o.lastInput)>VerificationMs)
                        return {AfkStatus::ClearingAfk,AfkAction::None,AfkResult::Pending,"waiting_native_input_clock"};
                    delivered_=true; due_=false; observedClock_=o.lastInput; haveClock_=true;
                    phase_=active ? AfkProductionPhase::RecoveringAfk : AfkProductionPhase::Recent;
                    return {active ? AfkStatus::AfkDetected : AfkStatus::VerifiedClear,
                        AfkAction::None,AfkResult::Confirmed,"client_input_clock_advanced"};
                }
                if (!active)
                {
                    phase_=AfkProductionPhase::Recent;
                    return {AfkStatus::VerifiedClear,AfkAction::None,AfkResult::Confirmed,"client_and_server_afk_clear"};
                }
                if (phase_==AfkProductionPhase::VerifyingClear)
                    return {AfkStatus::ClearingAfk,AfkAction::None,AfkResult::Pending,"waiting_client_and_server_clear"};
                if (const auto block=Blocker(s,benign))
                    return {AfkStatus::Blocked,AfkAction::None,AfkResult::Pending,block};
                // Native 5EB830 returns when client=false, and sends a SERVER
                // TOGGLE when client=true. Qualification of both-active does
                // not make either mixed state safe to dispatch. Allow bounded
                // authoritative convergence; otherwise terminal fault, no loop.
                if (o.clientAfk!=o.serverAfk)
                    return {AfkStatus::ClearingAfk,AfkAction::None,AfkResult::Pending,"reconciling_mixed_afk_no_safe_toggle"};
                if (setting!=AfkAutoClearSetting::Enabled)
                    return Fail(setting==AfkAutoClearSetting::Disabled ? "auto_clear_afk_disabled" : "auto_clear_afk_unknown");
                return {AfkStatus::ClearingAfk,AfkAction::NativeAutoClear,AfkResult::Pending,"verified_input_requires_native_auto_clear"};
            }
            // Only an authoritative input-clock change can retire deferred due.
            if (haveClock_ && observedClock_!=o.lastInput) due_=false;
            observedClock_=o.lastInput; haveClock_=true;
            due_=due_ || Band(o)!=AfkProductionBand::Recent;
            if (!due_ && !active)
            {
                phase_=AfkProductionPhase::Recent;
                return {AfkStatus::Active,AfkAction::None,AfkResult::None,"input_clock_recent"};
            }
            if (const auto block=Blocker(s,benign))
            {
                phase_=AfkProductionPhase::Deferred;
                return {AfkStatus::Blocked,AfkAction::None,AfkResult::None,block};
            }
            phase_=AfkProductionPhase::Due;
            return {AfkStatus::ApproachingThreshold,AfkAction::InputPulse,AfkResult::None,
                active ? "production_afk_recovery_due" : "client_input_age_due"};
        }
        void Issued(AfkAction action,const AfkObservation& o,std::uint64_t now)
        {
            if (action==AfkAction::InputPulse && phase_==AfkProductionPhase::Due)
            { issuedAt_=now; issuedClock_=o.lastInput; phase_=AfkProductionPhase::VerifyingInput; }
            else if (action==AfkAction::NativeAutoClear && phase_==AfkProductionPhase::RecoveringAfk)
                phase_=AfkProductionPhase::VerifyingClear; // no reset of input deadline
        }
        void DispatchFailed() { Fail("production_dispatch_or_scene_failed"); }
        void Reset() { *this=AfkProductionPolicy{}; }
    };
}
