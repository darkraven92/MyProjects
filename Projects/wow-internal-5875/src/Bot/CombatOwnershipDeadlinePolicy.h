#pragma once
#include "CombatLivenessPolicy.h"

namespace Bot
{
    // For an active locked target, unknown input, stale samples, selection,
    // facing, latch and chase transitions cannot retain active ownership
    // forever. This is a terminal handoff, never permission for another input.
    class CombatOwnershipDeadlinePolicy
    {
        std::uint64_t guid_=0, progressMs_=0, tick_=0;
        std::uint32_t object_=0, hp_=0;
        bool active_=false, sampled_=false;
    public:
        // Existing 160-tick owner timeout scale at the 250-ms monitor cadence.
        static constexpr std::uint64_t MaximumNoProgressMs=40000;
        void Reset() { *this={}; }
        void Pause(std::uint64_t now)
        { if (active_) progressMs_=now; sampled_=false; }
        bool Observe(const CombatLivenessSample& s)
        {
            if (!s.alive || !s.targetValid || !s.targetGuid) { Reset(); return false; }
            if (guid_ && guid_!=s.targetGuid) Reset();
            if (!active_)
            {
                active_=true; guid_=s.targetGuid; progressMs_=s.nowMs;
            }
            if (s.nowMs<progressMs_) return true;
            // Never compare HP across a freshness gap or object replacement.
            // Neither gap refunds the absolute no-progress allowance.
            if (s.fresh && s.selectionKnown && s.selectedGuid==guid_ &&
                s.targetHp && s.sampleTick>tick_)
            {
                if (sampled_ && object_==s.targetObject && s.targetHp<hp_)
                    progressMs_=s.nowMs;
                object_=s.targetObject; hp_=s.targetHp; tick_=s.sampleTick; sampled_=true;
            }
            else sampled_=false;
            return s.nowMs-progressMs_>=MaximumNoProgressMs;
        }
    };
}
