#pragma once
#include "AfkProtectionPolicy.h"
#include <string>
#include <cmath>

namespace Bot
{
    struct AfkCandidateScene
    {
        bool known=false;
        float x=0,y=0,z=0,facing=0;
        std::uint64_t target=0;
        std::uint32_t movementFlags=0;
        static bool Unchanged(const AfkCandidateScene& a,const AfkCandidateScene& b)
        {
            return a.known && b.known && std::isfinite(a.x) && std::isfinite(a.y) &&
                std::isfinite(a.z) && std::isfinite(a.facing) &&
                a.x==b.x && a.y==b.y && a.z==b.z && a.facing==b.facing &&
                a.target==b.target && a.movementFlags==b.movementFlags;
        }
    };
    enum class AfkQualificationState { Disabled, Requested, Held, Aborted, Complete };

    // Diagnostic ownership, not movement ownership. Normal workload acquisition
    // must be behind this gate. Survival/unsafe evidence always releases it.
    class AfkQualificationHold
    {
        AfkQualificationState state_;
        std::string reason_="not_requested";
    public:
        explicit AfkQualificationHold(bool requested=false)
            : state_(requested ? AfkQualificationState::Requested : AfkQualificationState::Disabled) {}
        bool Requested() const
        { return state_==AfkQualificationState::Requested || state_==AfkQualificationState::Held; }
        bool InhibitsWorkloadAcquisition() const { return state_==AfkQualificationState::Held; }
        AfkQualificationState State() const { return state_; }
        const char* Reason() const { return reason_.c_str(); }
        bool Advance(const char* unsafeReason)
        {
            if (!Requested()) return false;
            if (unsafeReason) { Abort(unsafeReason); return false; }
            state_=AfkQualificationState::Held; reason_="safe_stationary_world";
            return true;
        }
        void Abort(const char* reason)
        { if(Requested()) { state_=AfkQualificationState::Aborted; reason_=reason; } }
        void Complete()
        { if(InhibitsWorkloadAcquisition()) { state_=AfkQualificationState::Complete; reason_="two_windows"; } }
    };

    enum class AfkWorkloadSafety { SafeIdle, BenignWork, Unsafe };
    struct AfkWorkloadSafetyPolicy
    {
        // Classification is NOT permission to inject input. Until harmless F12
        // is runtime-qualified, production still requires AfkSafety::Safe().
        static AfkWorkloadSafety Classify(const AfkSafety& s, bool benignWorkEvidence)
        {
            if (s.combat || s.death || s.recovery || s.water || s.dialog || s.vendor ||
                s.trainer || s.talents || s.equipment || s.loot || s.fault)
                return AfkWorkloadSafety::Unsafe;
            if (s.Safe()) return AfkWorkloadSafety::SafeIdle;
            return benignWorkEvidence ? AfkWorkloadSafety::BenignWork : AfkWorkloadSafety::Unsafe;
        }
    };
}
