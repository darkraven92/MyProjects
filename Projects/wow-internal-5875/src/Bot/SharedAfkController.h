#pragma once
#include "AfkClient5875.h"
#include "AfkPreventionWindow.h"
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
        std::string reason="not_observed";
    };
    class SharedAfkController
    {
        AfkProtectionPolicy policy_{};
        AfkRuntimeStatus status_{};
        AfkObservation previous_{};
        std::uint64_t playerGuid_=0, nextDispatchProbe_=0;
        bool observeOnly_=false, qualification_=false, baselineComplete_=false;
        bool havePrevious_=false, safeBaseline_=false;
        std::uint64_t qualificationStart_=0;
        unsigned preventionWindows_=0;
        AfkPreventionWindow preventionWindow_{};
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
            qualification_=mode && std::string(mode)=="qualify";
            // Unknown explicit mode must never silently enable input.
            if (mode && *mode && std::string(mode)!="protect" &&
                std::string(mode)!="observe" && std::string(mode)!="qualify")
                observeOnly_=true;
            Debug::Logger::Info(std::string("AFK CONFIG mode=")+
                (qualification_ ? "qualify" : observeOnly_ ? "observe" : "protect")+
                " candidate=unbound_F12 inputPath=targeted_win32_messages runtimeVerified=no");
        }
        bool ControlledIdleRequested() const { return qualification_; }
        void EndControlledIdle(const char* reason)
        {
            if (qualification_)
                Debug::Logger::Info(std::string("AFK QUALIFICATION stopped reason=")+reason);
            qualification_=false;
        }
        void Reset()
        {
            if (havePrevious_)
                Debug::Logger::Info("AFK SESSION RESET reason=world_gap_or_stop pendingEvidence=discarded");
            policy_.Reset(); status_={}; previous_={}; playerGuid_=0;
            havePrevious_=false; safeBaseline_=false; baselineComplete_=false;
            qualificationStart_=0; preventionWindows_=0; preventionWindow_={};
            nextDispatchProbe_=0; lastDecision_.clear(); lastDispatchBlock_.clear();
        }
        void Update(const Objects::PlayerState& player, std::uint64_t playerGuid,
            const AfkSafety& safety, std::uint64_t now, const char* owner)
        {
            if (playerGuid_ && playerGuid_!=playerGuid) Reset();
            playerGuid_=playerGuid;
            const auto o=AfkClient5875::Read(player);
            const bool valid=AfkProtectionPolicy::Valid(o);
            if (qualification_)
            {
                if (!qualificationStart_) qualificationStart_=now;
                if (!valid) EndControlledIdle("afk_observation_unavailable");
                else if (now-qualificationStart_ > std::uint64_t(o.thresholdMs)*4+30000)
                    EndControlledIdle("bounded_qualification_deadline");
                else if (baselineComplete_ && havePrevious_ && o.lastInput!=previous_.lastInput &&
                    policy_.PendingAction()==AfkAction::None)
                    EndControlledIdle("unattributed_input_during_qualification");
            }
            if (!havePrevious_ || valid!=status_.known ||
                o.serverAfk!=previous_.serverAfk || o.clientAfk!=previous_.clientAfk)
                Debug::Logger::Info("AFK STATE known="+std::string(valid ? "yes" : "no")+
                    " active="+(valid ? (o.serverAfk ? std::string("yes") : "no") : "unknown")+
                    " clientActive="+(valid ? (o.clientAfk ? std::string("yes") : "no") : "unknown")+
                    " source=5875_signature_and_server_player_flags");
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
                if (qualification_ && safeBaseline_)
                    baselineComplete_=true;
            }
            status_.known=valid;
            status_.active=valid && (o.serverAfk || o.clientAfk);
            status_.inputAgeMs=valid ? std::uint32_t(o.clientNow-o.lastInput) : 0;
            status_.sourceThresholdMs=valid ? o.thresholdMs : 0;
            const bool observe=observeOnly_ || (qualification_ && !baselineComplete_);
            auto checkedSafety=safety;
            if (!AfkClient5875::StationaryAliveLand(player)) checkedSafety.healthyIdle=false;
            preventionWindow_.Observe(o,checkedSafety.Safe());
            const auto pendingAction=policy_.PendingAction();
            const auto decision=policy_.Update(o,checkedSafety,now,observe);
            status_.status=decision.status; status_.reason=decision.reason;
            LogDecision(decision,owner);
            if (decision.result==AfkResult::Confirmed)
            {
                const bool preventionComplete=preventionWindow_.Confirmed(pendingAction,o);
                if (std::string(decision.reason)=="client_input_clock_advanced")
                {
                    ++status_.verifiedInputs;
                    if (qualification_ && preventionComplete)
                    {
                        ++preventionWindows_;
                        Debug::Logger::Info("AFK PREVENTION WINDOW window="+std::to_string(preventionWindows_)+
                            " result=observed_clear inputClockAdvanced=yes pairedRelease=yes");
                        if (preventionWindows_>=2)
                            EndControlledIdle("two_prevention_windows_observed");
                    }
                }
                Debug::Logger::Info("AFK ACTION RESULT result=confirmed reason="+
                    std::string(decision.reason)+" verifiedInputs="+std::to_string(status_.verifiedInputs));
            }
            if (decision.status==AfkStatus::Fault)
                EndControlledIdle("verification_failed");
            if (decision.action!=AfkAction::None && now>=nextDispatchProbe_)
            {
                nextDispatchProbe_=now+10000; // UI guard backoff; not an input interval.
                const auto dispatch=AfkClient5875::Dispatch(player,decision.action,o);
                const std::string action=decision.action==AfkAction::InputPulse ? "unbound_F12" : "clear_existing_afk";
                if (dispatch.issued)
                {
                    Debug::Logger::Info("AFK ACTION action="+action+
                        " inputPath="+(decision.action==AfkAction::InputPulse ?
                            std::string("targeted_win32_messages") : "game_thread_lua")+
                        " reason="+decision.reason);
                    if (decision.action==AfkAction::InputPulse)
                        Debug::Logger::Info("AFK INPUT RELEASE action=unbound_F12 result="+
                            std::string(dispatch.releaseDelivered ? "paired_message_delivered" : "failed"));
                    Debug::Logger::Info("AFK ACTION RESULT action="+action+" result=pending reason="+dispatch.reason);
                    policy_.Issued(decision.action,dispatch.before,now);
                    if (decision.action==AfkAction::InputPulse)
                        preventionWindow_.Issued(dispatch.before);
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
