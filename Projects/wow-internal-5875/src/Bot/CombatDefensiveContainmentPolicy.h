#pragma once

#include "CombatLivenessPolicy.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <memory> // CombatController's containment follower owns its route

namespace Bot
{
    enum class DefensiveContainmentAction
    {
        Observe, VerifiedDisengagement, Reengage, DeathHandoff, Fail
    };

    struct DefensiveContainmentSample
    {
        std::uint64_t nowMs=0, guid=0;
        std::uint32_t targetHp=0;
        bool playerAlive=false, targetValid=false, evidenceKnown=false;
        bool hostileEngaged=true, reengageReady=false;
        bool routeFailed=false, routeArrived=false, waterBlocked=false;
    };

    struct DefensiveContainmentDecision
    {
        DefensiveContainmentAction action=DefensiveContainmentAction::Observe;
        const char* reason="defensive_route_in_progress";
    };

    struct DefensiveThreatPosition { float x=0.0f, y=0.0f; };
    struct DefensiveEscapePoint { float x=0.0f, y=0.0f, z=0.0f; };

    class CombatDefensiveEscapePolicy
    {
    public:
        static constexpr unsigned MaximumThreats=8;
        static constexpr float LocalEscapeDistance=24.0f; // existing Grind handoff scale
        static bool AwayPoint(const DefensiveEscapePoint& origin,
            const std::array<DefensiveThreatPosition,MaximumThreats>& threats,
            unsigned count, DefensiveEscapePoint& result)
        {
            result={};
            if (!count || count>MaximumThreats || !std::isfinite(origin.x) ||
                !std::isfinite(origin.y) || !std::isfinite(origin.z)) return false;
            float dx=0.0f,dy=0.0f;
            for (unsigned i=0; i<count; ++i)
            {
                const float ux=origin.x-threats[i].x;
                const float uy=origin.y-threats[i].y;
                const float distance=std::hypot(ux,uy);
                if (!std::isfinite(distance) || distance<0.05f) return false;
                dx+=ux/distance; dy+=uy/distance;
            }
            const float length=std::hypot(dx,dy);
            if (!std::isfinite(length) || length<0.05f) return false;
            result={origin.x+dx/length*LocalEscapeDistance,
                origin.y+dy/length*LocalEscapeDistance,origin.z};
            return std::isfinite(result.x) && std::isfinite(result.y);
        }
    };

    // One target-bound defensive episode. Route dispatch and distance alone
    // never prove escape: authoritative combat/aggressor evidence must stay
    // clear for the existing structural verification window.
    class CombatDefensiveContainmentPolicy
    {
        std::uint64_t guid_=0, startedMs_=0, disengagedSinceMs_=0;
        std::uint32_t targetHp_=0;
        bool active_=false, disengagementObserved_=false;
    public:
        static constexpr std::uint64_t MaximumDurationMs=30000;
        static constexpr std::uint64_t DisengagementVerificationMs=
            CombatLivenessPolicy::StructuralVerificationMs;

        void Reset() { *this=CombatDefensiveContainmentPolicy{}; }
        bool Active() const { return active_; }
        bool Expired(std::uint64_t nowMs) const
        {
            return !active_ || nowMs<startedMs_ || nowMs-startedMs_>=MaximumDurationMs;
        }
        void Begin(std::uint64_t guid, std::uint32_t hp, std::uint64_t nowMs)
        {
            Reset();
            if (!guid || !hp) return;
            active_=true; guid_=guid; targetHp_=hp; startedMs_=nowMs;
        }
        DefensiveContainmentDecision Observe(const DefensiveContainmentSample& s)
        {
            if (!active_ || !guid_ || s.guid!=guid_ || s.nowMs<startedMs_)
                return {DefensiveContainmentAction::Fail,"containment_identity_invalid"};
            if (!s.playerAlive)
                return {DefensiveContainmentAction::DeathHandoff,"player_dead"};
            if (s.waterBlocked)
                return {DefensiveContainmentAction::Fail,"living_water_blocked"};
            if (s.nowMs-startedMs_>=MaximumDurationMs)
                return {DefensiveContainmentAction::Fail,"containment_timeout"};
            if (s.evidenceKnown && !s.hostileEngaged)
            {
                if (!disengagementObserved_)
                {
                    disengagementObserved_=true;
                    disengagedSinceMs_=s.nowMs;
                }
                if (s.nowMs-disengagedSinceMs_>=DisengagementVerificationMs)
                    return {DefensiveContainmentAction::VerifiedDisengagement,
                        "combat_and_aggressors_verified_clear"};
                return {DefensiveContainmentAction::Observe,"verifying_disengagement"};
            }
            disengagementObserved_=false;
            if (s.evidenceKnown && s.targetValid && s.targetHp<targetHp_ &&
                s.reengageReady)
                return {DefensiveContainmentAction::Reengage,"verified_target_damage"};
            if (s.routeFailed)
                return {DefensiveContainmentAction::Fail,"defensive_route_failed"};
            if (s.routeArrived)
                return {DefensiveContainmentAction::Observe,
                    "route_arrived_waiting_for_combat_clear"};
            if (!s.evidenceKnown)
                return {DefensiveContainmentAction::Observe,"waiting_for_hostile_evidence"};
            return {};
        }
    };
}
