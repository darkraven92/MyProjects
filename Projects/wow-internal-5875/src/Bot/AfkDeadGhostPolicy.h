#pragma once
#include "AfkProtectionPolicy.h"

namespace Bot
{
    enum class AfkLifeState { Unknown, Alive, Dead, Ghost };
    enum class AfkDeathGap { Unknown, Idle, RoutingToCorpse, WaitingForReclaim, Failed, CommandInFlight };

    // Prevention-only session evidence. Dead and ghost are qualified separately.
    // This never authorizes native auto-clear or changes death ownership.
    class AfkDeadGhostPolicy
    {
        bool deadQualified_=false, ghostQualified_=false, failed_=false, pending_=false;
        AfkLifeState issuedLife_=AfkLifeState::Unknown;
        std::uint32_t before_=0;
        std::uint64_t issuedAt_=0;
    public:
        static bool DeadOrGhost(AfkLifeState life)
        { return life==AfkLifeState::Dead || life==AfkLifeState::Ghost; }
        static const char* LifeName(AfkLifeState life)
        {
            return life==AfkLifeState::Dead ? "dead" : life==AfkLifeState::Ghost ? "ghost" :
                life==AfkLifeState::Alive ? "alive" : "unknown";
        }
        bool Qualified(AfkLifeState life) const
        { return !failed_ && (life==AfkLifeState::Dead ? deadQualified_ : life==AfkLifeState::Ghost && ghostQualified_); }
        bool Failed() const { return failed_; }
        bool Pending() const { return pending_; }
        const char* Blocker(AfkLifeState life,AfkDeathGap gap,const AfkObservation& o) const
        {
            if (failed_) return "dead_ghost_candidate_failed_session_latched";
            if (!DeadOrGhost(life) || !AfkProtectionPolicy::Valid(o)) return "dead_ghost_observation_unknown";
            if (gap==AfkDeathGap::Unknown) return "death_recovery_gap_unknown";
            if (gap==AfkDeathGap::CommandInFlight) return "death_recovery_command_in_flight";
            if (o.clientAfk || o.serverAfk || std::uint32_t(o.clientNow-o.lastInput)>=o.thresholdMs)
                return "dead_ghost_afk_recovery_not_qualified";
            return nullptr;
        }
        void Fail() { failed_=true; pending_=false; }
        void Issued(AfkLifeState life,const AfkObservation& before,std::uint64_t now,
            bool paired,bool sceneSame,bool recoverySame)
        {
            if (failed_ || pending_ || Blocker(life,AfkDeathGap::Idle,before) ||
                !paired || !sceneSame || !recoverySame)
            { Fail(); return; }
            issuedLife_=life; before_=before.lastInput; issuedAt_=now; pending_=true;
        }
        AfkResult Verify(const AfkObservation& o,std::uint64_t now)
        {
            if (failed_) return AfkResult::Failed;
            if (!pending_) return AfkResult::None;
            if (!AfkProtectionPolicy::Valid(o) || o.clientAfk || o.serverAfk ||
                now<issuedAt_ || now-issuedAt_>=AfkProtectionPolicy::VerificationMs)
            { Fail(); return AfkResult::Failed; }
            if (o.lastInput==before_ || std::uint32_t(o.clientNow-o.lastInput)>AfkProtectionPolicy::VerificationMs)
                return AfkResult::Pending;
            pending_=false;
            if (issuedLife_==AfkLifeState::Dead) deadQualified_=true;
            else ghostQualified_=true;
            return AfkResult::Confirmed;
        }
    };
}
