#pragma once
#include "AfkProtectionPolicy.h"
#include <string>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Bot
{
    struct AfkCandidateScene
    {
        bool known=false;
        float x=0,y=0,z=0,facing=0;
        std::uint64_t target=0;
        std::uint32_t movementFlags=0;
        // Numerical read/angle-wrap tolerance only: not permission to move.
        static constexpr float PositionTolerance=0.01f, FacingTolerance=0.001f;
        static bool Unchanged(const AfkCandidateScene& a,const AfkCandidateScene& b)
        {
            const float dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z;
            return a.known && b.known && std::isfinite(a.x) && std::isfinite(a.y) &&
                std::isfinite(a.z) && std::isfinite(a.facing) &&
                std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z) && std::isfinite(b.facing) &&
                dx*dx+dy*dy+dz*dz<=PositionTolerance*PositionTolerance &&
                std::fabs(std::remainder(a.facing-b.facing,6.283185307179586f))<=FacingTolerance &&
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
    struct AfkMovementEvidence
    {
        bool known=false;
        std::uint32_t flags=0,allowedMask=0,unsupportedBits=0;
        std::string Fields() const
        {
            if (!known) return "movementKnown=no";
            std::ostringstream text;
            text<<"movementFlags=0x"<<std::hex<<std::setfill('0')<<std::setw(8)<<flags
                <<" allowedMask=0x"<<std::setw(8)<<allowedMask
                <<" unsupportedBits=0x"<<std::setw(8)<<unsupportedBits;
            return text.str();
        }
    };
    struct AfkWorkloadSafetyPolicy
    {
        static AfkMovementEvidence MovementEvidence(std::uint32_t flags,bool ordinaryLandMovement)
        {
            const auto mask=ordinaryLandMovement ? 0x13fu : 0x100u;
            return {true,flags,mask,flags & ~mask};
        }
        static bool LandMovementAllowed(std::uint32_t flags,bool ordinaryLandMovement)
        {
            // 1.12.1 forward/backward/strafe/turn and walk preference only.
            return MovementEvidence(flags,ordinaryLandMovement).unsupportedBits==0;
        }
        // Classification is separate from implementation qualification and each
        // action's native/UI/input/scene guards. An owner name is not evidence.
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
