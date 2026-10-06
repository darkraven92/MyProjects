#pragma once
#include "AfkPreventionWindow.h"

namespace Bot
{
    enum class AfkQualificationPhase
    { Initial, Synchronizing, Baseline, CandidateReady, Delivery, Clear, Prevention, Complete, Failed };

    // Diagnostic flow only. Never use the production AFK-chat toggle here:
    // qualification must establish whether the paired candidate ALONE works.
    class AfkQualificationPolicy
    {
        AfkQualificationPhase phase_=AfkQualificationPhase::Initial;
        AfkProtectionPolicy input_{};
        AfkPreventionWindow window_{};
        std::uint64_t syncStart_=0, issuedAt_=0;
        unsigned windows_=0;
        bool deliveryVerified_=false, clearVerified_=false;
        bool preventionCandidate_=false;
        const char* failure_="qualification_failed";
        AfkDecision Fail(const char* reason)
        {
            phase_=AfkQualificationPhase::Failed; failure_=reason;
            input_.Reset(); window_={};
            return {AfkStatus::Fault,AfkAction::None,AfkResult::Failed,reason};
        }
    public:
        // A bounded startup observation grace, NOT a measured propagation delay.
        static constexpr std::uint64_t SynchronizationMs=1000;
        AfkQualificationPhase Phase() const { return phase_; }
        bool DeliveryVerified() const { return deliveryVerified_; }
        bool ClearVerified() const { return clearVerified_; }
        unsigned Windows() const { return windows_; }
        std::uint64_t IssuedAt() const { return issuedAt_; }
        bool Verifying() const
        { return phase_==AfkQualificationPhase::Delivery || phase_==AfkQualificationPhase::Clear; }
        AfkAction PendingAction() const { return input_.PendingAction(); }

        AfkDecision Update(const AfkObservation& o, bool safe, bool unchanged,
            std::uint64_t now)
        {
            if (phase_==AfkQualificationPhase::Failed) return Fail(failure_);
            if (!AfkProtectionPolicy::Valid(o)) return Fail("qualification_observation_unknown");
            if (!safe) return Fail("qualification_unsafe_state");
            const bool active=o.clientAfk || o.serverAfk;
            const auto age=std::uint32_t(o.clientNow-o.lastInput);
            if (phase_==AfkQualificationPhase::Initial)
            {
                syncStart_=now;
                phase_=!active ? AfkQualificationPhase::Baseline :
                    age<=AfkProtectionPolicy::VerificationMs ? AfkQualificationPhase::Synchronizing :
                    AfkQualificationPhase::CandidateReady;
            }
            if (phase_==AfkQualificationPhase::Synchronizing)
            {
                if (!active) phase_=AfkQualificationPhase::Baseline;
                else if (now<syncStart_) return Fail("qualification_clock_reversed");
                else if (now-syncStart_>=SynchronizationMs) phase_=AfkQualificationPhase::CandidateReady;
                else return {AfkStatus::AfkDetected,AfkAction::None,AfkResult::Pending,"initial_state_synchronizing"};
            }
            if (Verifying())
            {
                if (!unchanged) return Fail("candidate_scene_or_ui_changed");
                if (preventionCandidate_ && active) return Fail("afk_during_prevention_window");
                if (now<issuedAt_ || now-issuedAt_>=AfkProtectionPolicy::VerificationMs)
                    return Fail(deliveryVerified_ ? "candidate_afk_clear_timeout" : "candidate_delivery_timeout");
                if (phase_==AfkQualificationPhase::Delivery)
                {
                    AfkSafety idle; idle.healthyIdle=true;
                    const auto result=input_.Update(o,idle,now);
                    if (result.result==AfkResult::Confirmed)
                    {
                        deliveryVerified_=true;
                        phase_=AfkQualificationPhase::Clear;
                    }
                    return result; // delivery alone never completes a window
                }
                if (active)
                    return {AfkStatus::ClearingAfk,AfkAction::None,AfkResult::Pending,"waiting_client_and_server_clear"};
                clearVerified_=true;
                if (window_.Confirmed(AfkAction::InputPulse,o)) ++windows_;
                phase_=windows_==2 ? AfkQualificationPhase::Complete : AfkQualificationPhase::Prevention;
                return {AfkStatus::VerifiedClear,AfkAction::None,AfkResult::Confirmed,"candidate_clear_confirmed"};
            }
            if (phase_==AfkQualificationPhase::Complete)
                return {AfkStatus::VerifiedClear,AfkAction::None,AfkResult::None,"qualification_complete"};
            // A natural clear before dispatch is not evidence that our key
            // cleared AFK. Return to the ordinary baseline path.
            if (phase_==AfkQualificationPhase::CandidateReady && !active)
                phase_=AfkQualificationPhase::Baseline;
            if (phase_==AfkQualificationPhase::Baseline)
            {
                if (!active)
                    return {AfkStatus::Active,AfkAction::None,AfkResult::None,"observe_baseline"};
                phase_=AfkQualificationPhase::CandidateReady;
            }
            if (phase_==AfkQualificationPhase::Prevention)
            {
                if (active) return Fail("afk_during_prevention_window");
                if (age<o.thresholdMs-o.thresholdMs/5)
                    return {AfkStatus::Active,AfkAction::None,AfkResult::None,"observe_prevention_window"};
            }
            return {AfkStatus::ApproachingThreshold,AfkAction::InputPulse,AfkResult::None,"qualification_candidate_due"};
        }
        void Issued(const AfkObservation& o, std::uint64_t now)
        {
            if (phase_!=AfkQualificationPhase::CandidateReady && phase_!=AfkQualificationPhase::Prevention)
                return;
            if (phase_==AfkQualificationPhase::CandidateReady && !o.clientAfk && !o.serverAfk)
            {
                Fail("afk_cleared_before_candidate_dispatch");
                return;
            }
            window_.Issued(o);
            preventionCandidate_=phase_==AfkQualificationPhase::Prevention;
            input_.Issued(AfkAction::InputPulse,o,now);
            issuedAt_=now; deliveryVerified_=clearVerified_=false;
            phase_=AfkQualificationPhase::Delivery;
        }
    };
}
