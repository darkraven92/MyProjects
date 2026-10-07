#pragma once
#include "AfkPreventionWindow.h"

namespace Bot
{
    enum class AfkQualificationPhase
    { Initial, AwaitingQuiescence, Synchronizing, Baseline, CandidateReady, Delivery, Clear, Prevention, Complete, Failed };

    // Diagnostic flow only. The original input-only candidate remains testable;
    // the new explicit composite uses the client's non-forced auto-clear path.
    // Neither candidate inherits production's explicit AFK-chat toggle.
    class AfkQualificationPolicy
    {
        AfkQualificationPhase phase_=AfkQualificationPhase::Initial;
        AfkProtectionPolicy input_{};
        AfkPreventionWindow window_{};
        std::uint64_t syncStart_=0, issuedAt_=0;
        std::uint64_t quiescenceStart_=0, quietSince_=0;
        std::uint32_t observedInput_=0;
        bool baselineCaptured_=false;
        AfkObservation baseline_{};
        unsigned windows_=0;
        bool deliveryVerified_=false, clearVerified_=false;
        bool preventionCandidate_=false;
        bool nativeCandidate_=false, nativeClearIssued_=false, preventionStarted_=false;
        const char* failure_="qualification_failed";
        AfkDecision Fail(const char* reason)
        {
            phase_=AfkQualificationPhase::Failed; failure_=reason;
            input_.Reset(); window_={};
            return {AfkStatus::Fault,AfkAction::None,AfkResult::Failed,reason};
        }
    public:
        AfkQualificationPolicy() = default;
        explicit AfkQualificationPolicy(bool nativeCandidate) : nativeCandidate_(nativeCandidate) {}
        bool PreventionStarted() const { return preventionStarted_; }
        // A command is not confirmation. Keep the ORIGINAL input deadline.
        void NativeClearIssued()
        {
            if (nativeCandidate_ && phase_==AfkQualificationPhase::Clear && deliveryVerified_)
                nativeClearIssued_=true;
        }
        // Startup noise may reset only the quiet interval, never the deadline.
        // Tick-driven diagnostic timing; neither constant changes WoW's clock.
        static constexpr std::uint64_t QuietIntervalMs=1500, MaximumQuiescenceMs=10000;
        bool BaselineCaptured() const { return baselineCaptured_; }
        const AfkObservation& BaselineObservation() const { return baseline_; }
        std::uint32_t ObservedInput() const { return observedInput_; }
        std::uint64_t QuietSince() const { return quietSince_; }
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
            std::uint64_t now, AfkAutoClearSetting autoClear=AfkAutoClearSetting::Unknown)
        {
            if (phase_==AfkQualificationPhase::Failed) return Fail(failure_);
            if (!AfkProtectionPolicy::Valid(o)) return Fail("qualification_observation_unknown");
            if (!safe) return Fail("qualification_unsafe_state");
            if (baselineCaptured_ && input_.PendingAction()==AfkAction::None && o.lastInput!=observedInput_)
                return Fail("unattributed_input_during_qualification");
            const bool active=o.clientAfk || o.serverAfk;
            const auto age=std::uint32_t(o.clientNow-o.lastInput);
            if (phase_==AfkQualificationPhase::Initial)
            {
                quiescenceStart_=quietSince_=now;
                observedInput_=o.lastInput;
                phase_=AfkQualificationPhase::AwaitingQuiescence;
            }
            if (phase_==AfkQualificationPhase::AwaitingQuiescence)
            {
                if (now<quietSince_) return Fail("qualification_clock_reversed");
                if (now-quiescenceStart_>=MaximumQuiescenceMs) return Fail("quiescence_not_reached");
                if (o.lastInput!=observedInput_)
                {
                    observedInput_=o.lastInput;
                    quietSince_=now;
                }
                if (now-quietSince_<QuietIntervalMs)
                    return {active ? AfkStatus::AfkDetected : AfkStatus::Active,
                        AfkAction::None,AfkResult::Pending,"awaiting_input_quiescence"};
                baseline_=o;
                baselineCaptured_=true;
            }
            observedInput_=o.lastInput;
            if (phase_==AfkQualificationPhase::AwaitingQuiescence)
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
                if (active && nativeCandidate_ && !nativeClearIssued_ && o.clientAfk && o.serverAfk)
                {
                    if (autoClear!=AfkAutoClearSetting::Enabled)
                        return Fail(autoClear==AfkAutoClearSetting::Disabled ?
                            "auto_clear_afk_disabled" : "auto_clear_afk_unknown");
                    return {AfkStatus::ClearingAfk,AfkAction::NativeAutoClear,AfkResult::Pending,
                        "verified_input_requires_native_auto_clear"};
                }
                if (active)
                    return {AfkStatus::ClearingAfk,AfkAction::None,AfkResult::Pending,"waiting_client_and_server_clear"};
                clearVerified_=true;
                preventionStarted_=true;
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
            if (o.lastInput!=observedInput_)
            {
                Fail("unattributed_input_before_candidate_dispatch");
                return;
            }
            if (phase_==AfkQualificationPhase::CandidateReady && !o.clientAfk && !o.serverAfk)
            {
                Fail("afk_cleared_before_candidate_dispatch");
                return;
            }
            window_.Issued(o);
            preventionCandidate_=phase_==AfkQualificationPhase::Prevention;
            input_.Issued(AfkAction::InputPulse,o,now);
            issuedAt_=now; deliveryVerified_=clearVerified_=false;
            nativeClearIssued_=false;
            phase_=AfkQualificationPhase::Delivery;
        }
    };
}
