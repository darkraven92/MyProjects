#pragma once
#include "ActiveBotAfkSafeguard.h"
#include <cstdint>

namespace Bot
{
    enum class IdleReason { IntentionalIdle, WaitingForSafeWork, NoEligibleWork,
        ActiveOwner, EvidenceWait, WorldUnavailable, FaultedSubsystem, StuckSubsystem };
    enum class IdleDecision { None, Due, BlockedUnprovenAction };
    struct IdleSample
    {
        IdleReason reason=IdleReason::WorldUnavailable;
        bool healthyWorld=false, meaningfulActivity=false;
        bool combat=false, navigation=false, interaction=false, dead=false, vendor=false,
             trainer=false, looting=false, stuckRecovery=false, plannerOwner=false;
        bool provenSafeAction=false;
    };
    class IdleActivityPolicy
    {
        std::uint64_t threshold_, since_=0, blockedUntil_=0;
        bool armed_=false;
    public:
        // Use the existing 14K diagnostic interval until client timeout is proven.
        explicit IdleActivityPolicy(std::uint64_t ticks=ActiveBotAfkSafeguard::TriggerAfterTicks())
            : threshold_(ticks ? ticks : ActiveBotAfkSafeguard::TriggerAfterTicks()) {}
        static bool Safe(const IdleSample& s)
        {
            return s.healthyWorld && !s.meaningfulActivity && !s.combat && !s.navigation &&
                !s.interaction && !s.dead && !s.vendor && !s.trainer && !s.looting &&
                !s.stuckRecovery && !s.plannerOwner &&
                (s.reason==IdleReason::IntentionalIdle || s.reason==IdleReason::WaitingForSafeWork ||
                 s.reason==IdleReason::NoEligibleWork);
        }
        IdleDecision Update(const IdleSample& s,std::uint64_t tick)
        {
            if(!Safe(s)) { armed_=false; blockedUntil_=0; return IdleDecision::None; }
            if(!armed_ || tick<since_) { armed_=true; since_=tick; blockedUntil_=0; return IdleDecision::None; }
            if(tick-since_<threshold_ || tick<blockedUntil_) return IdleDecision::None;
            if(!s.provenSafeAction)
            { blockedUntil_=tick+threshold_; return IdleDecision::BlockedUnprovenAction; }
            return IdleDecision::Due;
        }
        // Call only AFTER a successful issue; a request never fabricates activity.
        void ActionIssued(std::uint64_t tick) { since_=tick; armed_=true; blockedUntil_=0; }
        void ActionBlocked(std::uint64_t tick) { blockedUntil_=tick+threshold_; }
        std::uint64_t Age(std::uint64_t tick) const { return armed_ && tick>=since_ ? tick-since_ : 0; }
    };
}
