#pragma once
#include "AfkClient5875.h"
#include "AfkPreventionWindow.h"
#include "AfkQualificationHold.h"
#include "AfkQualificationPolicy.h"
#include "../Debug/Logger.h"
#include <cstdlib>
#include <string>

namespace Bot
{
    struct AfkRuntimeStatus
    {
        AfkStatus status=AfkStatus::Unknown;
        bool known=false, active=false, thresholdMeasured=false;
        std::uint32_t inputAgeMs=0, sourceThresholdMs=0, observedThresholdMs=0;
        unsigned verifiedInputs=0;
        bool candidateDeliveryVerified=false, candidateClearVerified=false;
        unsigned preventionWindows=0;
        std::string reason="not_observed";
    };
    class SharedAfkController
    {
        AfkProtectionPolicy policy_{};
        AfkRuntimeStatus status_{};
        AfkObservation previous_{};
        std::uint64_t playerGuid_=0, nextDispatchProbe_=0;
        bool observeOnly_=false, baselineComplete_=false;
        AfkQualificationHold qualification_{};
        AfkQualificationPolicy qualificationFlow_{};
        std::uint64_t nextQualificationUiProbe_=0;
        std::uint32_t candidateInputBefore_=0;
        bool candidateAfkBefore_=false;
        AfkCandidateScene candidateSceneBefore_{};
        bool havePrevious_=false, safeBaseline_=false;
        std::uint64_t qualificationStart_=0;
        std::string lastDecision_{}, lastDispatchBlock_{};

        void LogDecision(const AfkDecision& d, const char* owner)
        {
            const std::string key=std::to_string(static_cast<int>(d.status))+":"+d.reason;
            if (key==lastDecision_) return;
            lastDecision_=key;
            Debug::Logger::Info("AFK PROTECTION state="+std::to_string(static_cast<int>(d.status))+
                " owner="+owner+" reason="+d.reason);
        }
    public:
        SharedAfkController()
        {
            const char* mode=std::getenv("WOW_INTERNAL_AFK_MODE");
            observeOnly_=mode && std::string(mode)=="observe";
            qualification_=AfkQualificationHold(mode && std::string(mode)=="qualify");
            // Unknown explicit mode must never silently enable input.
            if (mode && *mode && std::string(mode)!="protect" &&
                std::string(mode)!="observe" && std::string(mode)!="qualify")
                observeOnly_=true;
            Debug::Logger::Info(std::string("AFK CONFIG mode=")+
                (qualification_.Requested() ? "qualify" : observeOnly_ ? "observe" : "protect")+
                " candidate=unbound_F12 inputPath=targeted_win32_messages runtimeVerified=no");
        }
        bool ControlledIdleRequested() const { return qualification_.Requested(); }
        bool AdvanceQualificationHold(const Objects::PlayerState& player,
            const char* unsafeReason, std::uint64_t now)
        {
            if (!qualification_.Requested()) return false;
            if (unsafeReason) { EndControlledIdle(unsafeReason); return false; }
            const bool acquiring=!qualification_.InhibitsWorkloadAcquisition();
            if (acquiring || now>=nextQualificationUiProbe_)
            {
                nextQualificationUiProbe_=now+1000;
                const auto ui=AfkClient5875::SafeInputReason();
                if (ui!="ready") { EndControlledIdle(ui.c_str()); return false; }
            }
            const auto observation=AfkClient5875::Read(player);
            if (!AfkProtectionPolicy::Valid(observation))
            { EndControlledIdle(AfkProtectionPolicy::ObservationReason(observation)); return false; }
            if (qualification_.Advance(nullptr) && acquiring)
            {
                Debug::Logger::Info("AFK QUALIFICATION START reason=safe_world inputClock="+
                    std::to_string(observation.lastInput)+" afkState="+
                    (observation.clientAfk || observation.serverAfk ? "active" : "clear"));
                Debug::Logger::Info("AFK QUALIFICATION HOLD state=acquired reason=safe_stationary_world");
            }
            return qualification_.InhibitsWorkloadAcquisition();
        }
        void EndControlledIdle(const char* reason)
        {
            if (!qualification_.Requested()) return;
            qualification_.Abort(reason);
            Debug::Logger::Info(std::string("AFK QUALIFICATION HOLD state=aborted reason=")+reason);
            Debug::Logger::Info(std::string("AFK QUALIFICATION HOLD state=released reason=")+reason);
            if (baselineComplete_)
                Debug::Logger::Info("AFK PREVENTION WINDOW index="+std::to_string(qualificationFlow_.Windows()+1)+
                    " result=fail reason="+reason);
            // An aborted diagnostic must not silently test its candidate later
            // during normal work. A new bot session explicitly re-arms it.
            observeOnly_=true;
        }
        void Reset()
        {
            EndControlledIdle("world_loss_or_stop");
            if (havePrevious_)
                Debug::Logger::Info("AFK SESSION RESET reason=world_gap_or_stop pendingEvidence=discarded");
            policy_.Reset(); status_={}; previous_={}; playerGuid_=0;
            havePrevious_=false; safeBaseline_=false; baselineComplete_=false;
            qualificationStart_=0; qualificationFlow_={};
            nextDispatchProbe_=0; lastDecision_.clear(); lastDispatchBlock_.clear();
        }
        void Update(const Objects::PlayerState& player, std::uint64_t playerGuid,
            const AfkSafety& safety, std::uint64_t now, const char* owner,
            bool benignWorkEvidence=false)
        {
            if (playerGuid_ && playerGuid_!=playerGuid) Reset();
            playerGuid_=playerGuid;
            const auto o=AfkClient5875::Read(player);
            const bool valid=AfkProtectionPolicy::Valid(o);
            if (qualification_.InhibitsWorkloadAcquisition())
            {
                if (!qualificationStart_) qualificationStart_=now;
                if (!valid) EndControlledIdle("afk_observation_unavailable");
                else if (now-qualificationStart_ > std::uint64_t(o.thresholdMs)*4+30000)
                    EndControlledIdle("bounded_qualification_deadline");
                else if (havePrevious_ && o.lastInput!=previous_.lastInput &&
                    qualificationFlow_.Phase()!=AfkQualificationPhase::Initial &&
                    qualificationFlow_.Phase()!=AfkQualificationPhase::Synchronizing &&
                    qualificationFlow_.PendingAction()==AfkAction::None)
                    EndControlledIdle("unattributed_input_during_qualification");
            }
            if (!havePrevious_ || valid!=status_.known ||
                o.serverAfk!=previous_.serverAfk || o.clientAfk!=previous_.clientAfk)
                Debug::Logger::Info("AFK STATE known="+std::string(valid ? "yes" : "no")+
                    " active="+(valid ? (o.serverAfk ? std::string("yes") : "no") : "unknown")+
                    " clientActive="+(valid ? (o.clientAfk ? std::string("yes") : "no") : "unknown")+
                    " source=5875_signature_and_server_player_flags reason="+
                    AfkProtectionPolicy::ObservationReason(o)+
                    " clientNow="+std::to_string(o.clientNow)+" inputClock="+std::to_string(o.lastInput));
            if (valid && (!havePrevious_ || !status_.known))
                Debug::Logger::Info("AFK TIMER elapsed="+std::to_string(std::uint32_t(o.clientNow-o.lastInput))+
                    " thresholdKnown=yes threshold="+std::to_string(o.thresholdMs)+
                    " source=client_idle_instruction runtimeMeasured=no margin="+std::to_string(o.thresholdMs/5));
            if (!havePrevious_ || (valid && o.lastInput!=previous_.lastInput))
                safeBaseline_=safety.Safe();
            else
                safeBaseline_=safeBaseline_ && safety.Safe() && valid;
            if (valid && havePrevious_ && !previous_.clientAfk && o.clientAfk)
            {
                const auto age=std::uint32_t(o.clientNow-o.lastInput);
                status_.thresholdMeasured=safeBaseline_;
                status_.observedThresholdMs=age;
                Debug::Logger::Info("AFK THRESHOLD OBSERVATION elapsed="+std::to_string(age)+
                    " sourceThreshold="+std::to_string(o.thresholdMs)+
                    " continuousSafeIdle="+(safeBaseline_ ? std::string("yes") : "no"));
                if (qualification_.InhibitsWorkloadAcquisition() && safeBaseline_)
                {
                    baselineComplete_=true;
                    Debug::Logger::Info("AFK BASELINE elapsed="+std::to_string(age)+
                        " clientAfk="+(o.clientAfk ? std::string("yes") : "no")+
                        " serverAfk="+(o.serverAfk ? std::string("yes") : "no")+
                        " inputClock="+std::to_string(o.lastInput));
                }
            }
            status_.known=valid;
            status_.active=valid && (o.serverAfk || o.clientAfk);
            status_.inputAgeMs=valid ? std::uint32_t(o.clientNow-o.lastInput) : 0;
            status_.sourceThresholdMs=valid ? o.thresholdMs : 0;
            auto checkedSafety=safety;
            if (!AfkClient5875::StationaryAliveLand(player)) checkedSafety.healthyIdle=false;
            const bool qualifying=qualification_.InhibitsWorkloadAcquisition();
            const auto beforePhase=qualificationFlow_.Phase();
            const auto beforeWindows=qualificationFlow_.Windows();
            const auto pendingAction=qualifying ? qualificationFlow_.PendingAction() : policy_.PendingAction();
            // Check the candidate scene throughout delivery AND asynchronous AFK
            // propagation, not merely on the first clock-advance snapshot.
            bool sceneSame=true;
            if (qualifying && qualificationFlow_.Verifying())
                sceneSame=AfkCandidateScene::Unchanged(candidateSceneBefore_,AfkClient5875::ReadScene(player)) &&
                    AfkClient5875::SafeInputReason()=="ready";
            auto decision=qualifying ? qualificationFlow_.Update(o,checkedSafety.Safe(),sceneSame,now) :
                policy_.Update(o,checkedSafety,now,observeOnly_);
            if (!qualifying && pendingAction==AfkAction::InputPulse && decision.result==AfkResult::Confirmed)
            {
                const bool sceneSame=AfkCandidateScene::Unchanged(candidateSceneBefore_,AfkClient5875::ReadScene(player));
                const auto ui=AfkClient5875::SafeInputReason();
                Debug::Logger::Info("AFK CANDIDATE SCENE positionFacingTargetMovementUnchanged="+
                    std::string(sceneSame ? "yes" : "no")+" ui="+ui);
                if (!sceneSame || ui!="ready")
                {
                    policy_.DispatchFailed();
                    decision={AfkStatus::Fault,AfkAction::None,AfkResult::Failed,
                        "candidate_scene_or_ui_changed"};
                }
            }
            if (decision.status==AfkStatus::Blocked &&
                AfkWorkloadSafetyPolicy::Classify(safety,benignWorkEvidence)==AfkWorkloadSafety::BenignWork)
                decision.reason="benign_work_awaiting_runtime_verified_noop";
            status_.status=decision.status; status_.reason=decision.reason;
            if (qualifying)
            {
                status_.candidateDeliveryVerified=qualificationFlow_.DeliveryVerified();
                status_.candidateClearVerified=qualificationFlow_.ClearVerified();
                status_.preventionWindows=qualificationFlow_.Windows();
                const auto phase=qualificationFlow_.Phase();
                if (beforePhase==AfkQualificationPhase::Initial ||
                    (beforePhase==AfkQualificationPhase::Synchronizing && phase!=beforePhase))
                    Debug::Logger::Info("AFK QUALIFICATION INITIAL state="+
                        std::string(!valid ? "unknown" : o.clientAfk || o.serverAfk ? "active" : "clear")+
                        " inputAge="+std::to_string(status_.inputAgeMs)+" decision="+
                        (phase==AfkQualificationPhase::Synchronizing ? "synchronize" :
                         phase==AfkQualificationPhase::Baseline ? "baseline" :
                         phase==AfkQualificationPhase::Failed ? "abort" : "candidate_clear")+
                        " reason="+decision.reason);
                if (beforePhase==AfkQualificationPhase::Clear || phase==AfkQualificationPhase::Clear)
                {
                    // Sparse: entering clear verification, flag transitions, or terminal result.
                    if (phase!=beforePhase || o.clientAfk!=previous_.clientAfk || o.serverAfk!=previous_.serverAfk)
                        Debug::Logger::Info("AFK CLEAR VERIFY clientActive="+
                            std::string(valid ? (o.clientAfk ? "yes" : "no") : "unknown")+
                            " serverActive="+(valid ? (o.serverAfk ? "yes" : "no") : "unknown")+
                            " elapsedSinceCandidate="+std::to_string(now-qualificationFlow_.IssuedAt())+
                            " result="+(phase==AfkQualificationPhase::Failed ? "failed" :
                                phase==AfkQualificationPhase::Clear ? "pending" : "confirmed")+
                            " reason="+decision.reason);
                }
                if (std::string(decision.reason)=="candidate_clear_confirmed" && beforeWindows==0 &&
                    qualificationFlow_.Windows()==0)
                {
                    baselineComplete_=true;
                    Debug::Logger::Info("AFK QUALIFICATION BASELINE RESET reason=candidate_clear_confirmed");
                }
                if (qualificationFlow_.Windows()>beforeWindows)
                    Debug::Logger::Info("AFK PREVENTION WINDOW index="+std::to_string(qualificationFlow_.Windows())+
                        " result=pass reason=continuous_clear_and_verified_paired_input");
                if (phase==AfkQualificationPhase::Complete)
                {
                    qualification_.Complete();
                    Debug::Logger::Info("AFK QUALIFICATION COMPLETE windows=2 result=pass");
                    Debug::Logger::Info("AFK QUALIFICATION HOLD state=released reason=two_prevention_windows");
                    Debug::Logger::Info("AFK PRODUCTION GATE activeWork=blocked reason=harmless_input_runtime_review_required");
                }
            }
            LogDecision(decision,owner);
            if (pendingAction==AfkAction::InputPulse &&
                (decision.result==AfkResult::Confirmed || decision.result==AfkResult::Failed))
            {
                Debug::Logger::Info("AFK CANDIDATE TEST action=F12 phase=verify");
                Debug::Logger::Info("AFK CANDIDATE DELIVERY inputClockBefore="+std::to_string(candidateInputBefore_)+
                    " inputClockAfter="+std::to_string(o.lastInput)+" advanced="+
                    (valid && o.lastInput!=candidateInputBefore_ ? std::string("yes") : "no"));
                Debug::Logger::Info("AFK CANDIDATE VERIFY timestampBefore="+std::to_string(candidateInputBefore_)+
                    " timestampAfter="+std::to_string(o.lastInput)+
                    " advanced="+(valid && o.lastInput!=candidateInputBefore_ ? std::string("yes") : "no")+
                    " afkBefore="+(candidateAfkBefore_ ? "yes" : "no")+
                    " afkAfter="+(valid ? (o.clientAfk || o.serverAfk ? "yes" : "no") : "unknown")+
                    " result="+(decision.result==AfkResult::Confirmed ? "input_confirmed" : "failed"));
            }
            if (decision.result==AfkResult::Confirmed)
            {
                if (std::string(decision.reason)=="client_input_clock_advanced")
                {
                    ++status_.verifiedInputs;
                }
                Debug::Logger::Info("AFK ACTION RESULT result=confirmed reason="+
                    std::string(decision.reason)+" verifiedInputs="+std::to_string(status_.verifiedInputs));
            }
            if (decision.status==AfkStatus::Fault)
                EndControlledIdle(decision.reason);
            if (decision.action!=AfkAction::None && now>=nextDispatchProbe_)
            {
                nextDispatchProbe_=now+10000; // UI guard backoff; not an input interval.
                const auto dispatch=AfkClient5875::Dispatch(player,decision.action,o);
                const std::string action=decision.action==AfkAction::InputPulse ? "unbound_F12" : "clear_existing_afk";
                if (dispatch.issued)
                {
                    candidateInputBefore_=dispatch.before.lastInput;
                    candidateAfkBefore_=dispatch.before.clientAfk || dispatch.before.serverAfk;
                    candidateSceneBefore_=dispatch.sceneBefore;
                    Debug::Logger::Info("AFK ACTION action="+action+
                        " inputPath="+(decision.action==AfkAction::InputPulse ?
                            std::string("targeted_win32_messages") : "game_thread_lua")+
                        " reason="+decision.reason);
                    if (decision.action==AfkAction::InputPulse)
                        Debug::Logger::Info("AFK INPUT RELEASE action=unbound_F12 result="+
                            std::string(dispatch.releaseDelivered ? "paired_message_delivered" : "failed"));
                    Debug::Logger::Info("AFK ACTION RESULT action="+action+" result=pending reason="+dispatch.reason);
                    if (qualifying) qualificationFlow_.Issued(dispatch.before,now);
                    else policy_.Issued(decision.action,dispatch.before,now);
                }
                else if (!dispatch.blocked)
                {
                    policy_.DispatchFailed();
                    Debug::Logger::Info("AFK ACTION RESULT action="+action+" result=failed reason="+dispatch.reason);
                    EndControlledIdle("dispatch_failed");
                }
                else
                {
                    const std::string key="dispatch:"+dispatch.reason;
                    if (lastDispatchBlock_!=key)
                        Debug::Logger::Info("AFK ACTION BLOCKED owner="+std::string(owner)+" reason="+dispatch.reason);
                    lastDispatchBlock_=key;
                }
            }
            previous_=o; havePrevious_=true;
        }
        AfkRuntimeStatus Snapshot() const { return status_; }
    };
}
