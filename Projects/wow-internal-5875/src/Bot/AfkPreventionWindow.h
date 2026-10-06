#pragma once
#include "AfkProtectionPolicy.h"

namespace Bot
{
    // Count only a continuously observed, clear and safe interval following a
    // verified input/clear. An AFK event or observation gap invalidates it.
    class AfkPreventionWindow
    {
        bool armed_=false, pending_=false;
    public:
        void Observe(const AfkObservation& o, bool safe)
        {
            if (!AfkProtectionPolicy::Valid(o) || o.clientAfk || o.serverAfk || !safe)
                armed_=pending_=false;
        }
        void Issued(const AfkObservation& o)
        {
            pending_=armed_ && AfkProtectionPolicy::Valid(o) &&
                !o.clientAfk && !o.serverAfk &&
                std::uint32_t(o.clientNow-o.lastInput)>=o.thresholdMs-o.thresholdMs/5;
            armed_=false;
        }
        bool Confirmed(AfkAction action, const AfkObservation& o)
        {
            const bool clear=AfkProtectionPolicy::Valid(o) && !o.clientAfk && !o.serverAfk;
            const bool complete=action==AfkAction::InputPulse && pending_ && clear;
            pending_=false;
            armed_=clear;
            return complete;
        }
    };
}
