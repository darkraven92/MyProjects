#pragma once

#include "LivingDeathRecoveryPolicy.h"
#include "CombatController.h"
#include "CombatClientEvidence5875.h"
#include "WaterEvidence5875.h"
#include "PlayerPostureController.h"
#include "../Navigation/DeathRouteTransitionMemory.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include <memory>

namespace Bot
{
    class LivingDeathRecoveryController
    {
        using P=LivingDeathRecoveryPolicy;
        P policy_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> route_;
        Navigation::DeathRouteTransitionMemory transitions_{};
        RecoveryController recovery_{};
        std::uint32_t map_=1;
        const char* lastEvent_=nullptr;
        bool needsStop_=false;
        bool defenseNeedsReconcile_=false;
        std::string evidenceLog_;

        void Log(const char* event)
        {
            if (lastEvent_ && std::string(lastEvent_)==event) return;
            lastEvent_=event;
            Debug::Logger::Info("DEATH LIVING state="+std::string(event)+
                " attempts="+std::to_string(policy_.Attempts())+
                " reason="+policy_.Reason()+
                " normalModeBlocked="+(policy_.Owns() ? "yes" : "no")+
                " threatCoverage=unknown"+evidenceLog_);
        }
        void ForgetRoute()
        {
            if (!route_) return;
            const auto e=route_->FailureEvidence();
            transitions_.ObserveGeneration(e.meshGeneration);
            for (const auto edge:e.learnedTransitions) transitions_.Learn(edge);
            transitions_.Learn(e.learnedTransition);
            route_.reset();
        }
        void Stop(const Objects::PlayerState& player)
        {
            const bool hadRoute=bool(route_);
            ForgetRoute();
            recovery_.Reset();
            if ((hadRoute || needsStop_) && player.valid)
            {
                if (!MovementController::HoldPosition(player))
                    policy_.Block("living_stop_dispatch_failed");
                needsStop_=false;
            }
        }
        Navigation::GenericNavMeshStartOptions Options(bool planning) const
        {
            Navigation::GenericNavMeshStartOptions options{};
            options.allowFullMapFallback=false;
            options.planningOnly=planning;
            options.waterTraversal=Navigation::WaterTraversalMode::AvoidUntilQualified;
            options.initialAvoidedTransitions=transitions_.Transitions();
            options.initialTransitionMeshGeneration=transitions_.Generation();
            return options;
        }
        bool CommandWorld(Objects::WorldState& world, bool release=false)
        {
            if (!Objects::WorldStateReader::Read(world))
            { InvalidateWorld(); return false; }
            const auto e=CombatClientEvidence5875::Living(world);
            const auto w=WaterEvidence5875::Read(world);
            policy_.ObserveDeadline(GetTickCount64());
            const bool same=e.identity && policy_.SamePlayer(world.activePlayerGuid);
            const bool allowed=policy_.Current()!=P::State::Blocked && same &&
                e.identity && e.known && e.alive && e.scanComplete && !e.combat && !e.aggressor &&
                (!policy_.LastHealth() || e.hp>=policy_.LastHealth()) &&
                (!release || (e.maxHp && double(e.hp)/e.maxHp>=0.95 &&
                    P::Distance(policy_.Origin(),{world.player.x,world.player.y,world.player.z})>=P::MinimumDisplacement)) &&
                w.movementKnown && !(w.movementFlags&WaterEvidenceTracker5875::SwimmingMask);
            if (!allowed)
            {
                if (same && e.known && e.alive) Stop(world.player);
                else
                {
                    ForgetRoute(); recovery_.Reset(); needsStop_=true;
                    if (!same) policy_.Block("living_identity_lost");
                }
            }
            return allowed;
        }
    public:
        bool Owns() const { return policy_.Owns(); }
        bool Recovering() const { return recovery_.IsActive(); }
        bool Matches(std::uint64_t guid) const { return policy_.SamePlayer(guid); }
        bool AllowsWaterHandoff(const Objects::WorldState& world) const
        {
            if (!Owns()) return true;
            const auto e=CombatClientEvidence5875::Living(world);
            return Matches(world.activePlayerGuid) && e.identity && e.known && e.alive &&
                e.scanComplete && !e.combat && !e.aggressor && !Recovering();
        }
        void Begin(const Objects::WorldState& world, std::uint32_t map, std::uint64_t now)
        {
            Reset(); map_=map;
            policy_.Begin(true,world.activePlayerGuid,map,
                {world.player.x,world.player.y,world.player.z},now);
            needsStop_=true;
            Debug::Logger::Info("DEATH LIVING entered=automatic_alive_confirmed playerGuid="+
                std::to_string(world.activePlayerGuid)+" deadlineMs="+std::to_string(P::DeadlineMs)+
                " candidateLimit="+std::to_string(P::MaximumCandidates)+
                " objective=bounded_displacement_not_safety");
            Log("waiting");
        }
        void Reset()
        { route_.reset(); recovery_.Reset(); transitions_.Reset(); policy_.Reset(); lastEvent_=nullptr; needsStop_=defenseNeedsReconcile_=false; evidenceLog_.clear(); }
        void InvalidateWorld()
        {
            if (!Owns()) return;
            policy_.WorldGap(); route_.reset(); recovery_.Reset(); transitions_.Reset();
            needsStop_=defenseNeedsReconcile_=true; Log("blocked_manual_recovery");
        }
        void ObserveDeadline(std::uint64_t now) { policy_.ObserveDeadline(now); }
        void YieldWater(const Objects::PlayerState& player)
        {
            if (!Owns()) return;
            Stop(player); policy_.Interrupt(); Log("water_handoff");
        }
        // Called only after the monitor gives death and living-water their turns.
        // True means fresh completion; all other outcomes keep normal modes held.
        bool Update(const Objects::WorldState& world, CombatController& combat,
            std::uint64_t tick, const CombatClientEvidence5875::LivingEvidence& e)
        {
            const auto w=WaterEvidence5875::Read(world);
            const bool same=e.identity && policy_.SamePlayer(world.activePlayerGuid);
            const bool calm=same && e.known && e.alive && e.scanComplete && !e.combat && !e.aggressor;
            if (defenseNeedsReconcile_ && same && e.known && e.alive)
            {
                combat.UpdateLivingDefense(world,tick,0,true);
                defenseNeedsReconcile_=false;
            }
            evidenceLog_=" playerGuid="+std::to_string(world.activePlayerGuid)+
                " combat="+(e.combat ? "yes" : "no")+
                " directEvidence="+(e.aggressor ? "yes" : "no")+
                " attackerGuid="+std::to_string(e.attacker)+
                " readComplete="+(e.known && e.scanComplete ? "yes" : "no")+
                " hp="+std::to_string(e.hp)+"/"+std::to_string(e.maxHp);
            if (combat.Failed()) policy_.Block("defense_terminal_failure");
            if (same && e.alive && calm && combat.LivingDefenseActive())
                combat.UpdateLivingDefense(world,tick,0,true);
            P::Sample s{};
            s.now=GetTickCount64(); s.guid=world.activePlayerGuid; s.map=map_;
            s.identity=same; s.alive=e.alive; s.evidenceKnown=e.known && e.scanComplete;
            s.combat=e.combat; s.aggressor=e.aggressor; s.defense=combat.LivingDefenseActive();
            s.movementKnown=w.movementKnown;
            s.swimming=(w.movementFlags&WaterEvidenceTracker5875::SwimmingMask)!=0;
            s.hp=e.hp; s.maxHp=e.maxHp;
            s.position={world.player.x,world.player.y,world.player.z};
            const auto action=policy_.Update(s);
            if (tick%20==0)
                Debug::Logger::Info("DEATH LIVING STATUS tick="+std::to_string(tick)+
                    " state="+std::to_string(int(policy_.Current()))+
                    " attempts="+std::to_string(policy_.Attempts())+
                    " navigation="+(route_ ? "yes" : "no")+
                    " normalModeBlocked="+(Owns() ? "yes" : "no")+
                    " reason="+policy_.Reason()+evidenceLog_);
            if (!same || !e.known || !e.alive)
            {
                ForgetRoute(); recovery_.Reset(); needsStop_=true;
                Log(policy_.Current()==P::State::Blocked ? "blocked_manual_recovery" : "waiting_evidence");
                return false; // no command to a foreign/unknown player or unknown life state
            }
            if (action==P::Action::Defense)
            {
                Stop(world.player);
                if (same && e.alive)
                {
                    PlayerPostureController::EnsureStanding(world.player,"living_recovery_defense");
                    combat.UpdateLivingDefense(world,tick,e.attacker,false);
                }
                Log("defense_handoff"); return false;
            }
            if (action==P::Action::Block || action==P::Action::Hold || action==P::Action::Water)
            {
                Stop(world.player);
                Log(action==P::Action::Block ? "blocked_manual_recovery" : "waiting_evidence");
                return false;
            }
            if (action==P::Action::Release)
            {
                Objects::WorldState fresh;
                if (!CommandWorld(fresh,true))
                { policy_.Interrupt(); return false; }
                recovery_.Reset();
                if (!PlayerPostureController::EnsureStanding(fresh.player,"living_recovery_complete"))
                    policy_.Block("posture_unknown");
                policy_.ObserveDeadline(GetTickCount64());
                if (!policy_.Complete()) { Log("blocked_manual_recovery"); return false; }
                Log("complete"); return true;
            }
            if (action==P::Action::Recover)
            {
                Objects::WorldState fresh;
                if (!CommandWorld(fresh))
                { policy_.Interrupt(); return false; }
                if (!recovery_.IsActive()) recovery_.Start(fresh.player,tick);
                else recovery_.Update(fresh.player,tick);
                Log("settling"); return false;
            }
            if (action==P::Action::Plan)
            {
                Stop(world.player);
                Objects::WorldState fresh;
                if (!CommandWorld(fresh)) { policy_.RouteFailed(); return false; }
                const auto origin=policy_.Origin();
                const float angle=fresh.player.rotation+(policy_.Attempts()-1)*1.570796327f;
                const Navigation::NavPoint destination{origin.x+P::CandidateDistance*std::cos(angle),
                    origin.y+P::CandidateDistance*std::sin(angle),origin.z};
                route_=std::make_unique<Navigation::GenericNavMeshPathFollower>();
                if (!route_->Start(fresh.player,tick,destination,map_,2.0f,
                    "post resurrection egress preflight",false,Options(true)))
                { ForgetRoute(); policy_.RouteFailed(); }
                policy_.ObserveDeadline(GetTickCount64());
                if (policy_.Current()==P::State::Blocked) Stop(fresh.player);
                Log("planning"); return false;
            }
            if (action==P::Action::Navigate && route_)
            {
                Objects::WorldState fresh;
                if (!CommandWorld(fresh))
                { policy_.Interrupt(); return false; }
                route_->Update(fresh.player,tick);
                policy_.ObserveDeadline(GetTickCount64());
                if (policy_.Current()==P::State::Blocked)
                { Stop(fresh.player); Log("blocked_manual_recovery"); return false; }
                if (route_->Failed() || route_->LifetimeReplans()>=int(P::MaximumReplans))
                { Stop(fresh.player); policy_.RouteFailed(); Log("route_rejected"); return false; }
                if (policy_.Current()==P::State::Planning &&
                    route_->PlanningOnlyResult().status==Navigation::RouteCostProbeStatus::Reachable)
                {
                    const auto destination=route_->PlanningOnlyProjectedDestination();
                    const bool ready=policy_.PlanReady(route_->PlanningOnlyReachedDestination(),
                        {destination.x,destination.y,destination.z});
                    ForgetRoute();
                    if (!ready) return false;
                    if (!CommandWorld(fresh)) { policy_.RouteFailed(); return false; }
                    if (!PlayerPostureController::EnsureStanding(fresh.player,"living_recovery_egress"))
                    { policy_.Block("posture_unknown"); return false; }
                    route_=std::make_unique<Navigation::GenericNavMeshPathFollower>();
                    if (!route_->Start(fresh.player,tick,destination,map_,2.0f,
                        "post resurrection living egress",false,Options(false)))
                    { Stop(fresh.player); policy_.RouteFailed(); }
                    policy_.ObserveDeadline(GetTickCount64());
                    if (policy_.Current()==P::State::Blocked) Stop(fresh.player);
                    Log("routing");
                }
                else if (route_->Arrived())
                {
                    policy_.Arrived({fresh.player.x,fresh.player.y,fresh.player.z});
                    Stop(fresh.player); Log("egress_arrived");
                }
            }
            return false;
        }
    };
}
