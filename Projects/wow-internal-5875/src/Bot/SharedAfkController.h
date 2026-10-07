#pragma once
#include "AfkClient5875.h"
#include "AfkPreventionWindow.h"
#include "AfkQualificationHold.h"
#include "AfkQualificationPolicy.h"
#include "AfkProductionPolicy.h"
#include "../Debug/Logger.h"
#include <cstdlib>
#include <string>
#include <functional>
#include <sstream>

namespace Bot
{
    struct AfkRuntimeStatus
    {
        AfkStatus status=AfkStatus::Unknown;
        AfkAgreementState agreement=AfkAgreementState::Unknown;
        bool known=false, active=false, thresholdMeasured=false;
        std::uint32_t inputAgeMs=0, sourceThresholdMs=0, observedThresholdMs=0;
        unsigned verifiedInputs=0;
        bool candidateDeliveryVerified=false, candidateClearVerified=false;
        unsigned preventionWindows=0;
        std::string reason="not_observed";
    };
    class SharedAfkController
    {
        AfkProductionPolicy policy_{};
        AfkDeadGhostPolicy deadGhost_{};
        std::string lastDeathStatus_{};
        std::string lastDeathMovementBlock_{};
        bool deadQualificationAnnounced_=false,ghostQualificationAnnounced_=false;
        AfkProductionBand productionBand_=AfkProductionBand::Recent;
        bool productionDeferred_=false;
        std::string productionBlock_{};
        AfkRuntimeStatus status_{};
        AfkObservation previous_{};
        std::uint64_t playerGuid_=0, nextDispatchProbe_=0;
        bool observeOnly_=false;
        AfkQualificationHold qualification_{};
        AfkQualificationPolicy qualificationFlow_{true};
        AfkAutoClearSetting previousAutoClear_=AfkAutoClearSetting::Unknown;
        std::uint64_t nextQualificationUiProbe_=0;
        std::uint32_t candidateInputBefore_=0;
        bool candidateAfkBefore_=false;
        AfkCandidateScene candidateSceneBefore_{};
        AfkCandidateScene baselineScene_{};
        bool havePrevious_=false, safeBaseline_=false;
        std::uint64_t qualificationStart_=0;
        std::string lastDecision_{}, lastDispatchBlock_{};
        bool mixedObserved_=false, mixedResultLogged_=false;
        std::uint64_t mixedObservedAt_=0, nextMixedClockProbe_=0;
        AfkAgreementState previousAgreement_=AfkAgreementState::Unknown;

        void LogDecision(const AfkDecision& d, const char* owner)
        {
            const std::string key=std::to_string(static_cast<int>(d.status))+":"+d.reason;
            if (key==lastDecision_) return;
            lastDecision_=key;
            Debug::Logger::Info("AFK PROTECTION state="+std::to_string(static_cast<int>(d.status))+
                " owner="+owner+" reason="+d.reason);
        }
        void LogDeadMovementBlock(AfkLifeState life,const char* deathState,
            const AfkMovementEvidence& evidence)
        {
            const std::string key=std::string("life=")+AfkDeadGhostPolicy::LifeName(life)+
                " deathRecoveryState="+deathState+" "+evidence.Fields();
            if (key!=lastDeathMovementBlock_)
                Debug::Logger::Info("AFK DEAD/GHOST MOVEMENT BLOCK "+key);
            lastDeathMovementBlock_=key;
        }
        void LogReconciliation(const AfkObservation& o,const AfkDecision& decision,
            const Objects::PlayerState& player,AfkLifeState life,const char* owner,
            const char* deathState,std::uint64_t now)
        {
            const auto state=AfkAgreementPolicy::Classify(o);
            if (AfkAgreementPolicy::Mixed(state))
            {
                const bool starting=!mixedObserved_;
                if (starting)
                {
                    mixedObserved_=true; mixedResultLogged_=false; mixedObservedAt_=now;
                    Debug::Logger::Info(std::string("AFK RECONCILIATION START state=")+
                        AfkAgreementPolicy::Name(state)+" policy=observe_only_no_safe_toggle");
                }
                // Read-only comparison probe: flag changes and native-clock
                // advances (including human input) are logged, never credited
                // as reconciliation without both authoritative flags agreeing.
                if (starting || state!=previousAgreement_ ||
                    (now>=nextMixedClockProbe_ && o.lastInput!=previous_.lastInput))
                {
                    nextMixedClockProbe_=now+1000; // bound read-only probes during continuous human input
                    std::uint32_t flags=0;
                    const bool flagsKnown=player.valid && player.descriptors &&
                        Core::Memory::Read(player.descriptors+0x2f8,flags);
                    const auto scene=AfkClient5875::ReadScene(player);
                    std::ostringstream fields;
                    fields << "AFK RECONCILIATION OBSERVE state=" << AfkAgreementPolicy::Name(state)
                        << " clientActive=" << (o.clientAfk ? "yes" : "no")
                        << " serverActive=" << (o.serverAfk ? "yes" : "no")
                        << " elapsed=" << (now>=mixedObservedAt_ ? now-mixedObservedAt_ : 0)
                        << " inputClock=" << o.lastInput << " clientNow=" << o.clientNow
                        << " inputAdvanced=" << (havePrevious_ && o.lastInput!=previous_.lastInput ? "yes" : "no")
                        << " life=" << AfkDeadGhostPolicy::LifeName(life) << " owner=" << owner
                        << " deathRecoveryState=" << deathState
                        << " playerFlagsKnown=" << (flagsKnown ? "yes" : "no")
                        << " playerFlags=0x" << std::hex << flags
                        << " movementKnown=" << (scene.known ? "yes" : "no")
                        << " movementFlags=0x" << scene.movementFlags;
                    Debug::Logger::Info(fields.str());
                }
                if (decision.status==AfkStatus::Fault && !mixedResultLogged_)
                {
                    mixedResultLogged_=true;
                    Debug::Logger::Info(std::string("AFK RECONCILIATION RESULT state=")+
                        AfkAgreementPolicy::Name(state)+" result=unsupported reason="+decision.reason+
                        " verificationBudgetMs="+std::to_string(AfkProductionPolicy::VerificationMs));
                }
            }
            else if (mixedObserved_)
            {
                Debug::Logger::Info(std::string("AFK RECONCILIATION RESULT result=")+
                    (state==AfkAgreementState::BothClear ? "converged_clear" :
                     state==AfkAgreementState::BothActive ? "converged_active" : "observation_lost")+
                    " state="+AfkAgreementPolicy::Name(state)+
                    " inputClock="+std::to_string(o.lastInput)+
                    " source=authoritative_flags commandSent=no policyReason="+decision.reason);
                mixedObserved_=false;
            }
            previousAgreement_=state;
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
                (qualification_.Requested() ?
                    " candidate=paired_F12_then_native_auto_clear inputPath=targeted_win32_and_native_5875 implementationQualification=runtime_pass_user_reported sessionDeliveryVerified=no actionVerified=no" :
                    " candidate=paired_F12_then_native_auto_clear implementationQualification=runtime_pass_user_reported sessionDeliveryVerified=no actionVerified=no"));
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
            if (qualificationFlow_.PreventionStarted())
                Debug::Logger::Info("AFK PREVENTION WINDOW index="+std::to_string(qualificationFlow_.Windows()+1)+
                    " result=fail reason="+reason);
            else
                Debug::Logger::Info(std::string("AFK QUALIFICATION PREREQUISITE result=failed preventionWindows=0 reason=")+reason);
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
            deadGhost_={}; lastDeathStatus_.clear();
            lastDeathMovementBlock_.clear();
            deadQualificationAnnounced_=ghostQualificationAnnounced_=false;
            havePrevious_=false; safeBaseline_=false;
            qualificationStart_=0; qualificationFlow_=AfkQualificationPolicy(true);
            previousAutoClear_=AfkAutoClearSetting::Unknown;
            productionBand_=AfkProductionBand::Recent; productionDeferred_=false; productionBlock_.clear();
            baselineScene_={};
            nextDispatchProbe_=0; lastDecision_.clear(); lastDispatchBlock_.clear();
            mixedObserved_=mixedResultLogged_=false; mixedObservedAt_=nextMixedClockProbe_=0;
            previousAgreement_=AfkAgreementState::Unknown;
        }
        void Update(const Objects::PlayerState& player, std::uint64_t playerGuid,
            const AfkSafety& safety, std::uint64_t now, const char* owner,
            bool benignWorkEvidence=false, AfkDeathGap deathGap=AfkDeathGap::Unknown,
            const char* deathState="Unknown", const std::function<bool()>& recoveryUnchanged={})
        {
            if (playerGuid_ && playerGuid_!=playerGuid) Reset();
            playerGuid_=playerGuid;
            const auto o=AfkClient5875::Read(player);
            const auto autoClear=AfkClient5875::ReadAutoClearSetting();
            if (!havePrevious_ || autoClear!=previousAutoClear_)
                Debug::Logger::Info(std::string("AFK AUTO CLEAR setting=")+
                    (autoClear==AfkAutoClearSetting::Unknown ? "unknown" :
                     autoClear==AfkAutoClearSetting::Enabled ? "enabled" : "disabled")+
                    " source=5875_cvar_pointer_read_only modified=no");
            previousAutoClear_=autoClear;
            const bool valid=AfkProtectionPolicy::Valid(o);
            if (qualification_.InhibitsWorkloadAcquisition())
            {
                if (!qualificationStart_) qualificationStart_=now;
                if (!valid) EndControlledIdle("afk_observation_unavailable");
                else if (now-qualificationStart_ > std::uint64_t(o.thresholdMs)*4+30000)
                    EndControlledIdle("bounded_qualification_deadline");
                // Input attribution belongs to the qualification state machine:
                // startup noise resets quiescence, post-baseline input fails.
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
                if (qualification_.InhibitsWorkloadAcquisition() &&
                    qualificationFlow_.BaselineCaptured() && safeBaseline_)
                {
                    Debug::Logger::Info("AFK BASELINE elapsed="+std::to_string(age)+
                        " clientAfk="+(o.clientAfk ? std::string("yes") : "no")+
                        " serverAfk="+(o.serverAfk ? std::string("yes") : "no")+
                        " inputClock="+std::to_string(o.lastInput));
                }
            }
            status_.known=valid;
            status_.agreement=AfkAgreementPolicy::Classify(o);
            status_.active=valid && (o.serverAfk || o.clientAfk);
            status_.inputAgeMs=valid ? std::uint32_t(o.clientNow-o.lastInput) : 0;
            status_.sourceThresholdMs=valid ? o.thresholdMs : 0;
            auto checkedSafety=safety;
            const bool qualifying=qualification_.InhibitsWorkloadAcquisition();
            const auto life=AfkClient5875::ReadLife(player);
            const bool deadContext=!qualifying && !observeOnly_ && AfkDeadGhostPolicy::DeadOrGhost(life);
            const auto deathBlock=deadContext ? deadGhost_.Blocker(life,deathGap,o) : nullptr;
            const bool deadPermit=deadContext && !deathBlock && bool(recoveryUnchanged);
            const bool lifeQualificationRequested=deadContext && deadGhost_.NeedsQualification(life);
            // Only the death-owner gate is specialized. Combat, transactions,
            // input faults, water, and independent recovery remain hard gates.
            if (deadPermit) checkedSafety.death=false;
            if (deadContext)
            {
                owner="DeathRecovery";
                const std::string key=std::string(AfkDeadGhostPolicy::LifeName(life))+":"+deathState+
                    (deadGhost_.Qualified(life) ? ":yes" : ":no");
                if (key!=lastDeathStatus_)
                    Debug::Logger::Info("AFK DEAD/GHOST STATUS qualified="+
                        std::string(deadGhost_.Qualified(life) ? "yes" : "no")+
                        " state="+AfkDeadGhostPolicy::LifeName(life)+" deathRecoveryState="+deathState);
                lastDeathStatus_=key;
                bool& announced=life==AfkLifeState::Dead ? deadQualificationAnnounced_ : ghostQualificationAnnounced_;
                if (lifeQualificationRequested && !announced)
                {
                    announced=true;
                    nextDispatchProbe_=0;
                    Debug::Logger::Info("AFK DEAD/GHOST QUALIFICATION DUE reason=natural_life_state_observed state="+
                        std::string(AfkDeadGhostPolicy::LifeName(life))+" inputAge="+
                        std::to_string(status_.inputAgeMs)+" deathRecoveryState="+deathState);
                }
            }
            const bool benign=!qualifying && AfkProductionPolicy::ImplementationQualified && (benignWorkEvidence || deadPermit);
            AfkMovementEvidence movementEvidence;
            const auto nativeBlock=AfkClient5875::StationarySafetyReason(player,benign,deadPermit,&movementEvidence);
            if (deadContext && nativeBlock && std::strcmp(nativeBlock,"movement_or_transport_flags")==0)
                LogDeadMovementBlock(life,deathState,movementEvidence);
            else
                lastDeathMovementBlock_.clear();
            if (nativeBlock) checkedSafety.water=true;
            const auto beforePhase=qualificationFlow_.Phase();
            const bool hadBaseline=qualificationFlow_.BaselineCaptured();
            const auto beforeClock=qualificationFlow_.ObservedInput();
            const auto beforeQuietSince=qualificationFlow_.QuietSince();
            const auto beforeWindows=qualificationFlow_.Windows();
            const auto pendingAction=qualifying ? qualificationFlow_.PendingAction() : policy_.PendingAction();
            // Check the candidate scene throughout delivery AND asynchronous AFK
            // propagation, not merely on the first clock-advance snapshot.
            bool sceneSame=true;
            AfkCandidateScene sceneNow{};
            if (qualifying)
            {
                sceneNow=AfkClient5875::ReadScene(player);
                if (!AfkCandidateScene::Unchanged(sceneNow,sceneNow)) checkedSafety.healthyIdle=false;
            }
            if (qualifying && qualificationFlow_.Verifying())
                sceneSame=AfkCandidateScene::Unchanged(candidateSceneBefore_,sceneNow) &&
                    AfkClient5875::SafeInputReason()=="ready";
            const auto priorProductionPhase=policy_.Phase();
            if (!qualifying && deadGhost_.Pending())
            {
                const bool qualifyingLife=!deadGhost_.Qualified(deadGhost_.IssuedLife());
                const bool lifeSame=life==deadGhost_.IssuedLife();
                const auto result=deadGhost_.Verify(o,now,life);
                if (result!=AfkResult::Pending)
                {
                    Debug::Logger::Info("AFK DEAD/GHOST VERIFY inputClockBefore="+std::to_string(candidateInputBefore_)+
                        " inputClockAfter="+std::to_string(o.lastInput)+
                        " advanced="+(valid && o.lastInput!=candidateInputBefore_ ? "yes" : "no")+
                        " lifeStateUnchanged="+(lifeSame ? "yes" : "no")+
                        " sceneUnchanged=yes recoveryStateUnchangedOrValid=yes result="+
                        (result==AfkResult::Confirmed ? "pass" : "fail")+
                        " purpose="+(qualifyingLife ? "life_state_qualification" : "prevention"));
                    if (result==AfkResult::Failed) policy_.DispatchFailed();
                    else Debug::Logger::Info("AFK PRODUCTION VERIFY advanced=yes sceneUnchanged=yes result=confirmed context=dead_ghost");
                }
            }
            auto decision=qualifying ? qualificationFlow_.Update(o,checkedSafety.Safe(),sceneSame,now,autoClear) :
                policy_.Update(o,checkedSafety,now,observeOnly_,benign,autoClear,lifeQualificationRequested);
            if (lifeQualificationRequested && decision.action==AfkAction::InputPulse)
                decision.reason=life==AfkLifeState::Dead ? "first_safe_dead_gap" : "first_safe_ghost_gap";
            // Production scene/UI proof brackets the synchronous messages.
            // Combat or a legitimate dialog beginning on the NEXT gameplay
            // tick blocks further actions; it is not proof of a key side effect.
            if (decision.status==AfkStatus::Blocked && nativeBlock) decision.reason=nativeBlock;
            if (decision.status==AfkStatus::Blocked && deathBlock) decision.reason=deathBlock;
            if (!qualifying && !observeOnly_)
                LogReconciliation(o,decision,player,life,owner,deathState,now);
            if (!qualifying && !observeOnly_ && valid)
            {
                const auto band=AfkProductionPolicy::Band(o);
                if (band!=productionBand_ && band!=AfkProductionBand::Recent)
                    Debug::Logger::Info(std::string("AFK PRODUCTION ")+
                        (band==AfkProductionBand::Due ? "DUE" : band==AfkProductionBand::Overdue ? "OVERDUE" : "THRESHOLD CROSSED")+
                        " inputAge="+std::to_string(status_.inputAgeMs)+" blockedReason="+decision.reason);
                productionBand_=band;
                const bool blocked=decision.status==AfkStatus::Blocked;
                if (blocked && (!productionDeferred_ || productionBlock_!=decision.reason))
                    Debug::Logger::Info(std::string(lifeQualificationRequested ?
                        "AFK DEAD/GHOST QUALIFICATION DEFER inputAge=" : "AFK PRODUCTION DEFER inputAge=")+
                        std::to_string(status_.inputAgeMs)+
                        " owner="+owner+" reason="+decision.reason);
                if (productionDeferred_ && decision.action==AfkAction::InputPulse)
                {
                    Debug::Logger::Info(std::string(lifeQualificationRequested ?
                        "AFK DEAD/GHOST QUALIFICATION RESUME inputAge=" : "AFK PRODUCTION RESUME inputAge=")+
                        std::to_string(status_.inputAgeMs)+" owner="+owner+
                        (deadPermit ? " reason=death_recovery_safe_gap" : " reason=eligible_workload"));
                    nextDispatchProbe_=0; // first eligible opportunity, no stale UI backoff
                }
                productionDeferred_=blocked;
                productionBlock_=decision.reason;
                if (priorProductionPhase!=policy_.Phase() &&
                    (o.clientAfk || o.serverAfk || priorProductionPhase==AfkProductionPhase::VerifyingClear ||
                     priorProductionPhase==AfkProductionPhase::RecoveringAfk))
                    Debug::Logger::Info(std::string("AFK PRODUCTION RECOVERY phase=")+AfkProductionPolicy::PhaseName(policy_.Phase())+
                        " clientActive="+(o.clientAfk ? "yes" : "no")+" serverActive="+(o.serverAfk ? "yes" : "no")+
                        " reason="+decision.reason);
            }
            status_.status=decision.status; status_.reason=decision.reason;
            if (qualifying)
            {
                status_.candidateDeliveryVerified=qualificationFlow_.DeliveryVerified();
                status_.candidateClearVerified=qualificationFlow_.ClearVerified();
                status_.preventionWindows=qualificationFlow_.Windows();
                const auto phase=qualificationFlow_.Phase();
                if (beforePhase==AfkQualificationPhase::Initial && phase==AfkQualificationPhase::AwaitingQuiescence)
                    Debug::Logger::Info("AFK QUIESCENCE START inputClock="+std::to_string(o.lastInput));
                if (beforePhase==AfkQualificationPhase::AwaitingQuiescence &&
                    phase==AfkQualificationPhase::AwaitingQuiescence && beforeClock!=o.lastInput)
                    Debug::Logger::Info("AFK QUIESCENCE RESET oldClock="+std::to_string(beforeClock)+
                        " newClock="+std::to_string(o.lastInput)+
                        " quietMsBeforeReset="+std::to_string(now-beforeQuietSince));
                if (!hadBaseline && qualificationFlow_.BaselineCaptured())
                {
                    baselineScene_=sceneNow;
                    const auto& baseline=qualificationFlow_.BaselineObservation();
                    Debug::Logger::Info("AFK QUIESCENCE COMPLETE stableMs="+
                        std::to_string(now-qualificationFlow_.QuietSince())+
                        " baselineInputClock="+std::to_string(baseline.lastInput));
                    Debug::Logger::Info("AFK QUALIFICATION BASELINE inputClock="+std::to_string(baseline.lastInput)+
                        " afkState="+(baseline.clientAfk || baseline.serverAfk ? "active" : "clear")+
                        " sceneKnown="+(baselineScene_.known ? "yes" : "no"));
                }
                if (beforePhase==AfkQualificationPhase::Initial ||
                    (!hadBaseline && qualificationFlow_.BaselineCaptured()) ||
                    (beforePhase==AfkQualificationPhase::Synchronizing && phase!=beforePhase))
                    Debug::Logger::Info("AFK QUALIFICATION INITIAL state="+
                        std::string(!valid ? "unknown" : o.clientAfk || o.serverAfk ? "active" : "clear")+
                        " inputAge="+std::to_string(status_.inputAgeMs)+" decision="+
                        (phase==AfkQualificationPhase::AwaitingQuiescence ? "await_quiescence" :
                         phase==AfkQualificationPhase::Synchronizing ? "synchronize" :
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
                    Debug::Logger::Info("AFK PRODUCTION GATE activeWork=classified_benign_only reason=reviewed_qualified_implementation");
                }
            }
            LogDecision(decision,owner);
            if (pendingAction==AfkAction::InputPulse &&
                (decision.result==AfkResult::Confirmed || decision.result==AfkResult::Failed))
            {
                Debug::Logger::Info("AFK CANDIDATE TEST action=F12 phase=verify");
                Debug::Logger::Info("AFK CANDIDATE DELIVERY baselineClock="+
                    std::to_string(qualificationFlow_.BaselineObservation().lastInput)+
                    " inputClockBefore="+std::to_string(candidateInputBefore_)+
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
            const bool nativeContinuation=decision.action==AfkAction::NativeAutoClear;
            if (decision.action!=AfkAction::None && (nativeContinuation || now>=nextDispatchProbe_))
            {
                nextDispatchProbe_=now+(qualifying ? 10000 : 1000); // guard probe, not an input interval
                const auto dispatch=AfkClient5875::Dispatch(player,decision.action,o,benign,deadPermit);
                const std::string action=decision.action==AfkAction::InputPulse ? "unbound_F12" :
                    nativeContinuation ? "native_auto_clear" : "clear_existing_afk";
                if (dispatch.issued)
                {
                    if (deadPermit)
                    {
                        const auto pulseLife=dispatch.lifeBefore; // fresh command-time state, not the earlier monitor snapshot
                        const auto& movement=dispatch.movementEvidence;
                        if (pulseLife==AfkLifeState::Ghost && movement.known &&
                            movement.unsupportedBits==0 &&
                            (movement.flags&AfkWorkloadSafetyPolicy::GhostWaterWalk)!=0)
                            Debug::Logger::Info("AFK DEAD/GHOST MOVEMENT ELIGIBLE life=ghost "+
                                movement.Fields()+" reason=source_verified_ghost_water_walk");
                        Debug::Logger::Info("AFK DEAD/GHOST "+std::string(deadGhost_.Qualified(pulseLife) ? "ACTION" : "QUALIFICATION")+
                            " phase=dispatch action=F12 inputAge="+std::to_string(status_.inputAgeMs)+
                            " deathRecoveryState="+deathState+" state="+AfkDeadGhostPolicy::LifeName(pulseLife)+
                            " inputClockBefore="+std::to_string(dispatch.before.lastInput)+
                            " position="+std::to_string(dispatch.sceneBefore.x)+","+
                                std::to_string(dispatch.sceneBefore.y)+","+std::to_string(dispatch.sceneBefore.z)+
                            " facing="+std::to_string(dispatch.sceneBefore.facing)+
                            " target="+std::to_string(dispatch.sceneBefore.target)+
                            " movementFlags="+std::to_string(dispatch.sceneBefore.movementFlags)+
                            " ui=ready releaseDelivered="+(dispatch.releaseDelivered ? "yes" : "no"));
                        // DeathRecovery runs on this monitor thread. Its update
                        // is complete before this synchronous game-thread pulse;
                        // compare its state again before any next owner update.
                        const bool recoverySame=recoveryUnchanged && recoveryUnchanged() && dispatch.lifeVerified && pulseLife==life;
                        deadGhost_.Issued(pulseLife,dispatch.before,now,dispatch.releaseDelivered,
                            dispatch.sceneVerified,recoverySame);
                        if (deadGhost_.Failed())
                        {
                            policy_.DispatchFailed();
                            Debug::Logger::Info("AFK DEAD/GHOST VERIFY advanced=unknown sceneUnchanged="+
                                std::string(dispatch.sceneVerified ? "yes" : "no")+
                                " recoveryStateUnchangedOrValid="+(recoverySame ? "yes" : "no")+
                                " result=fail reason=paired_scene_or_recovery_evidence");
                            previous_=o; havePrevious_=true; return;
                        }
                    }
                    if (decision.action==AfkAction::InputPulse)
                    {
                        Debug::Logger::Info(std::string("AFK CANDIDATE SCENE positionFacingTargetMovementUnchanged=")+
                            (dispatch.sceneVerified ? "yes" : "no")+" sceneWindow=paired_game_thread_messages ui="+
                            (dispatch.sceneVerified ? "ready" : "failed"));
                        if (!dispatch.sceneVerified)
                        {
                            policy_.DispatchFailed(); EndControlledIdle("candidate_scene_or_ui_changed");
                            Debug::Logger::Info("AFK ACTION RESULT result=failed reason=candidate_scene_or_ui_changed");
                            previous_=o; havePrevious_=true; return;
                        }
                    }
                    if (!nativeContinuation)
                    {
                        candidateInputBefore_=dispatch.before.lastInput;
                        candidateAfkBefore_=dispatch.before.clientAfk || dispatch.before.serverAfk;
                        candidateSceneBefore_=dispatch.sceneBefore;
                    }
                    Debug::Logger::Info("AFK ACTION action="+action+
                        " inputPath="+(decision.action==AfkAction::InputPulse ?
                            std::string("targeted_win32_messages") : nativeContinuation ? "game_thread_native_5875" : "game_thread_lua")+
                        " reason="+decision.reason);
                    if (decision.action==AfkAction::InputPulse)
                        Debug::Logger::Info("AFK INPUT RELEASE action=unbound_F12 result="+
                            std::string(dispatch.releaseDelivered ? "paired_message_delivered" : "failed"));
                    Debug::Logger::Info("AFK ACTION RESULT action="+action+" result=pending reason="+dispatch.reason);
                    if (nativeContinuation && qualifying) qualificationFlow_.NativeClearIssued();
                    else if (qualifying) qualificationFlow_.Issued(dispatch.before,now);
                    else policy_.Issued(decision.action,dispatch.before,now);
                }
                else if (!dispatch.blocked || nativeContinuation)
                {
                    if (deadPermit)
                    {
                        deadGhost_.Fail();
                        Debug::Logger::Info("AFK DEAD/GHOST VERIFY result=fail reason=dispatch_or_release_failed");
                    }
                    policy_.DispatchFailed();
                    Debug::Logger::Info("AFK ACTION RESULT action="+action+" result=failed reason="+dispatch.reason);
                    EndControlledIdle("dispatch_failed");
                }
                else
                {
                    if (deadContext && dispatch.reason=="movement_or_transport_flags")
                        LogDeadMovementBlock(life,deathState,dispatch.movementEvidence);
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
