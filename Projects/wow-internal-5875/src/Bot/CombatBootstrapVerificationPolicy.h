#pragma once

#include "CombatLivenessPolicy.h"

namespace Bot
{
    enum class CombatBootstrapResult { None, Pending, Confirmed, Failed };

    // One guarded first-start attempt. Unknown input/action evidence cannot
    // freeze its observation deadline or authorize another offensive command.
    class CombatBootstrapVerificationPolicy
    {
        std::uint64_t guid_=0, startedMs_=0, windowMs_=0;
        bool active_=false, issued_=false;
    public:
        void Reset() { *this={}; }
        void Begin(std::uint64_t guid, std::uint64_t now,
            std::uint32_t attackPeriodMs, bool issued)
        {
            guid_=guid; startedMs_=now; issued_=issued; active_=true;
            windowMs_=std::max<std::uint64_t>(8000,2ull*attackPeriodMs+1000);
        }
        CombatBootstrapResult Observe(const CombatLivenessSample& s, bool damage)
        {
            if (!active_) return CombatBootstrapResult::None;
            if (s.targetGuid!=guid_ || !s.alive || !s.targetValid)
            { Reset(); return CombatBootstrapResult::None; }
            const bool proof=issued_ && s.fresh && s.selectionKnown &&
                s.selectedGuid==guid_ && (damage || (s.actionKnown && s.attackActive));
            if (proof)
            { active_=false; return CombatBootstrapResult::Confirmed; }
            if (!issued_ || s.nowMs<startedMs_ || s.nowMs-startedMs_>=windowMs_)
            { active_=false; return CombatBootstrapResult::Failed; }
            return CombatBootstrapResult::Pending;
        }
    };
}
