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
        LivingRecoveryIdentity identity_{};
        std::uint32_t map_=1;
        const char* lastEvent_=nullptr;
        bool needsStop_=false;
        bool defenseNeedsReconcile_=false;
        bool evidenceGap_=false;
        std::string evidenceLog_;

        static LivingRecoveryIdentity Identity(const Objects::WorldState& world)
        { return {world.activePlayerGuid,world.manager,world.localPlayer,world.player.descriptors}; }
        bool SameWorld(const Objects::WorldState& world) const
        { return identity_.Matches(Identity(world)); }
        P::Sample Sample(const Objects::WorldState& world,
            const CombatClientEvidence5875::LivingEvidence& e,
            const WaterEvidenceSnapshot5875& w, bool defense=false) const
        {
            P::Sample s{};
            s.now=GetTickCount64(); s.guid=world.activePlayerGuid; s.map=map_;
            s.identity=e.identity && SameWorld(world); s.alive=e.Living();
            s.evidenceKnown=e.Complete(); s.combat=e.Positive(); s.aggressor=e.aggressor;
            s.defense=defense; s.movementKnown=w.movementKnown;
            s.swimming=(w.movementFlags&WaterEvidenceTracker5875::SwimmingMask)!=0;
            s.hp=e.healthKnown ? e.hp : 0; s.maxHp=e.healthKnown ? e.maxHp : 0;
            s.position={world.player.x,world.player.y,world.player.z};
            return s;
        }

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
            const auto e=ReadEvidence(world);
            const auto w=WaterEvidence5875::Read(world);
            const bool same=e.identity && SameWorld(world);
            const bool allowed=policy_.Observe(Sample(world,e,w)) &&
                (!release || (e.maxHp && double(e.hp)/e.maxHp>=0.95 &&
                    P::Distance(policy_.Origin(),{world.player.x,world.player.y,world.player.z})>=P::MinimumDisplacement)) &&
                w.movementKnown && !(w.movementFlags&WaterEvidenceTracker5875::SwimmingMask);
            if (!allowed)
            {
                if (same && e.Living()) Stop(world.player);
                else
                {
                    ForgetRoute(); recovery_.Reset(); needsStop_=true;
                    if (!same) InvalidateWorld();
                }
            }
            return allowed;
        }
    public:
        static CombatClientEvidence5875::LivingEvidence ReadEvidence(const Objects::WorldState& world)
        {
            CombatClientEvidence5875::LivingEvidence e{};
            GameThreadDispatcher::Invoke([&]
            {
                e=CombatClientEvidence5875::Living(world);
                if (!e.Living()) return;
                // Existing targetless IsAttackAction/IsCurrentAction read. No
                // selection, toggling, input dispatch or new client API.
                e.attackKnown=AutoAttackController::ProbeLivingAttack(e.attackActive);
                std::uint64_t selected=0, guid=0;
                std::uint32_t descriptors=0;
                if (!CombatClientEvidence5875::Selection(world,selected) ||
                    !Core::Memory::Read(world.localPlayer+0x30,guid) || guid!=world.activePlayerGuid ||
                    !Core::Memory::Read(world.localPlayer+0x08,descriptors) || descriptors!=world.player.descriptors)
                    e={};
            });
            return e;
        }
        bool Owns() const { return policy_.Owns(); }
        bool Recovering() const { return recovery_.IsActive(); }
        bool Matches(std::uint64_t guid) const { return policy_.SamePlayer(guid); }
        bool SameWorldIdentity(const Objects::WorldState& world) const { return SameWorld(world); }
        bool ConsumeEvidenceGap()
        { const bool result=evidenceGap_; evidenceGap_=false; return result; }
        bool AllowsWaterHandoff(const Objects::WorldState& world)
        {
            if (!Owns()) return true;
            const auto e=ReadEvidence(world);
            policy_.Observe(Sample(world,e,WaterEvidence5875::Read(world)));
            if (!e.identity || !SameWorld(world)) InvalidateWorld();
            return SameWorld(world) && e.QuietObservation();
        }
        void Begin(const Objects::WorldState& world, std::uint32_t map, std::uint64_t now)
        {
            Reset(); map_=map; identity_=Identity(world);
            policy_.Begin(true,world.activePlayerGuid,map,
                {world.player.x,world.player.y,world.player.z},now);
            if (!identity_.Valid()) policy_.Block("living_identity_lost");
            needsStop_=true;
            Debug::Logger::Info("DEATH LIVING entered=automatic_alive_confirmed playerGuid="+
                std::to_string(world.activePlayerGuid)+" deadlineMs="+std::to_string(P::DeadlineMs)+
                " candidateLimit="+std::to_string(P::MaximumCandidates)+
                " objective=bounded_displacement_not_safety");
            Log("waiting");
        }
        void Reset()
        { route_.reset(); recovery_.Reset(); transitions_.Reset(); policy_.Reset(); identity_={}; lastEvent_=nullptr; needsStop_=defenseNeedsReconcile_=evidenceGap_=false; evidenceLog_.clear(); }
        void InvalidateWorld()
        {
            if (!Owns()) return;
            policy_.WorldGap(); route_.reset(); recovery_.Reset(); transitions_.Reset();
            evidenceGap_=true;
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
        bool Update(const Objects::WorldState& world, CombatController& combat, std::uint64_t tick)
        {
            const auto e=ReadEvidence(world);
            const auto w=WaterEvidence5875::Read(world);
            const bool same=e.identity && SameWorld(world);
            const bool calm=same && e.QuietObservation();
            if (defenseNeedsReconcile_ && same && e.Living())
            {
                combat.UpdateLivingDefense(world,tick,0,true);
                defenseNeedsReconcile_=false;
            }
            evidenceLog_=" playerGuid="+std::to_string(world.activePlayerGuid)+
                " healthKnown="+(e.healthKnown ? "yes" : "no")+
                " lifeKnown="+(e.lifeKnown ? "yes" : "no")+
                " danger="+(e.Danger()==LivingDanger::Observed ? "observed" : "unknown")+
                " combat="+(!e.combatKnown ? "unknown" : e.combat ? "yes" : "no")+
                " attack="+(!e.attackKnown ? "unknown" : e.attackActive ? "active" : "inactive")+
                " directEvidence="+(e.aggressor ? "yes" : e.scanComplete ? "not_observed" : "unknown")+
                " attackerGuid="+std::to_string(e.attacker)+
                " readComplete="+(e.Complete() ? "yes" : "no")+
                " hp="+std::to_string(e.hp)+"/"+std::to_string(e.maxHp);
            if (combat.Failed()) policy_.Block("defense_terminal_failure");
            if (same && e.alive && calm && combat.LivingDefenseActive())
                combat.UpdateLivingDefense(world,tick,0,true);
            const auto s=Sample(world,e,w,combat.LivingDefenseActive());
            const auto action=policy_.Update(s);
            if (tick%20==0)
                Debug::Logger::Info("DEATH LIVING STATUS tick="+std::to_string(tick)+
                    " state="+std::to_string(int(policy_.Current()))+
                    " attempts="+std::to_string(policy_.Attempts())+
                    " navigation="+(route_ ? "yes" : "no")+
                    " normalModeBlocked="+(Owns() ? "yes" : "no")+
                    " reason="+policy_.Reason()+evidenceLog_);
            if (!same || !e.Living())
            {
                if (!same) InvalidateWorld();
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
