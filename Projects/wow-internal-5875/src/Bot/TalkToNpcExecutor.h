#pragma once
#include "GenericQuestTurnInExecutor.h"
#include "IObjectiveExecutor.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "QuestObjectiveDispatchPolicy.h"
#include "ValleyOfTrialsProfiles.h"

namespace Bot
{
    class TalkToNpcExecutor final : public IObjectiveExecutor
    {
        enum class Phase { Idle, Routing, Opening, AwaitingProgress, Done, Failed };
        Phase phase_ = Phase::Idle;
        const QuestProfile* profile_ = nullptr;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        ObjectiveDefensiveCombatGuard defense_{};
        std::uint64_t started_ = 0, phaseTick_ = 0, npcGuid_ = 0;
        unsigned attempts_ = 0;
        bool refined_ = false;
        std::string failure_{};
        Objects::PlayerState lastPlayer_{};

        void Set(Phase phase, std::uint64_t tick)
        {
            if (phase_ == phase) return;
            phase_ = phase; phaseTick_ = tick;
            Debug::Logger::Info("TALK NPC OBJECTIVE quest=" + std::to_string(profile_->questId) +
                " npcEntry=" + std::to_string(profile_->objective.targetEntry) + " state=" + StateName() +
                " attempts=" + std::to_string(attempts_) + " result=" + (failure_.empty() ? "pending" : failure_));
        }
        void Fail(const char* reason, std::uint64_t tick)
        {
            failure_=reason;
            if(navigator_ && lastPlayer_.valid) navigator_->PauseForCombat(lastPlayer_,"talk objective stop");
            navigator_.reset(); Set(Phase::Failed,tick);
        }
        const Objects::UnitState* FindNpc(const Objects::WorldState& world) const
        {
            const Objects::UnitState* best=nullptr;
            for(const auto& unit:world.units)
                if(unit.valid && unit.guid && unit.health && unit.entryId==profile_->objective.targetEntry &&
                    std::isfinite(unit.distance) && unit.distance>=0 &&
                    (!best || unit.distance<best->distance || (unit.distance==best->distance && unit.guid<best->guid))) best=&unit;
            return best;
        }
        bool Route(const Objects::WorldState& world, std::uint64_t tick, const Objects::UnitState* npc)
        {
            navigator_=std::make_unique<Navigation::GenericNavMeshPathFollower>();
            const auto& d=profile_->destination;
            return navigator_->Start(world.player,tick,{npc?npc->x:d.x,npc?npc->y:d.y,npc?npc->z:d.z},
                d.mapId,3.5f,"quest talk NPC approach");
        }
    public:
        bool Supports(const QuestProfile& p) const override { return TalkToNpcPolicy::Supports(p); }
        bool Start(const QuestProfile& p, const Objects::WorldState& world, CombatController& combat, std::uint64_t tick) override
        {
            if(phase_!=Phase::Idle || !Supports(p) || !world.player.valid || !world.player.health) return false;
            profile_=&p; started_=tick; lastPlayer_=world.player; combat.ClearPlannerQuestTarget(); defense_.Reset(world,"talk NPC objective");
            if(!Route(world,tick,FindNpc(world))) { Fail("npc_route_start_failed",tick); return false; }
            Set(Phase::Routing,tick); return true;
        }
        void Update(const QuestPlannerSnapshot& snapshot,const Objects::WorldState& world,CombatController& combat,std::uint64_t tick) override
        {
            if(!OwnsControl() || !world.player.valid || !world.player.health || !snapshot.valid) return;
            lastPlayer_=world.player;
            const PlannerQuestLogEntry* live=nullptr;
            for(const auto& entry:snapshot.quests)
            {
                const auto* p=ValleyOfTrialsProfiles::Find(entry,snapshot.classToken);
                if(p && p->questId==profile_->questId) { if(live) { Fail("ambiguous_live_identity",tick); return; } live=&entry; }
            }
            if(!live) { Fail("live_quest_identity_lost",tick); return; }
            if(SelectedObjectiveComplete(*live,*profile_))
            {
                if(navigator_) navigator_->PauseForCombat(world.player,"talk live progress confirmed");
                navigator_.reset(); Set(Phase::Done,tick); return;
            }
            if(TalkToNpcPolicy::TimedOut(tick,started_)) { Fail("talk_objective_timeout",tick); return; }
            const auto defense=defense_.Update(world,combat,navigator_.get(),0,tick);
            if(defense!=ObjectiveDefenseUpdate::Clear) return;
            const auto* npc=FindNpc(world);
            if(phase_==Phase::Routing)
            {
                if(npc && TalkToNpcPolicy::MayInteract(true,false,true,false,true,npc->distance,attempts_))
                {
                    if(!navigator_->PauseForCombat(world.player,"talk interaction hold")) { Fail("interaction_hold_failed",tick); return; }
                    // Keep the paused follower for defensive-combat ownership/resume.
                    ++attempts_;
                    if(!GenericQuestTurnInExecutor::InteractUnitGuid(npc->guid)) { Fail("npc_interaction_dispatch_failed",tick); return; }
                    npcGuid_=npc->guid; Set(Phase::Opening,tick); return;
                }
                navigator_->Update(world.player,tick);
                if(navigator_->Failed()) { Fail("npc_route_failed",tick); return; }
                if(navigator_->Arrived())
                {
                    if(npc && !refined_) { refined_=true; if(!Route(world,tick,npc)) Fail("npc_refinement_failed",tick); }
                    else Fail("npc_not_in_interaction_range",tick);
                }
                return;
            }
            if(phase_==Phase::Opening && tick>=phaseTick_+TalkToNpcPolicy::DialogWaitTicks)
            {
                if(!npc || npc->guid!=npcGuid_ || npc->distance>TalkToNpcPolicy::InteractionRange)
                { Fail("npc_identity_or_range_changed",tick); return; }
                std::string result;
                if(!GenericQuestTurnInExecutor::RunQuestLua(TalkToNpcPolicy::CreditScript(profile_->gossipCreditText),
                        "WOW_INTERNAL_TALK_RESULT",result) || result!="issued")
                { Fail("source_backed_gossip_option_not_observed",tick); return; }
                Set(Phase::AwaitingProgress,tick); return;
            }
            if(phase_==Phase::AwaitingProgress && TalkToNpcPolicy::ProgressWaitExpired(tick,phaseTick_))
            {
                if(attempts_>=TalkToNpcPolicy::MaximumAttempts) { Fail("talk_no_live_progress",tick); return; }
                if(!Route(world,tick,npc)) { Fail("npc_retry_route_failed",tick); return; }
                Set(Phase::Routing,tick);
            }
        }
        bool OwnsControl() const override { return phase_!=Phase::Idle && phase_!=Phase::Done && phase_!=Phase::Failed; }
        ObjectiveExecutorState State() const override
        {
            if(phase_==Phase::Failed) return ObjectiveExecutorState::Failed;
            if(phase_==Phase::Done) return ObjectiveExecutorState::ReadyForTurnIn;
            if(phase_==Phase::Idle) return ObjectiveExecutorState::Idle;
            return phase_==Phase::Routing?ObjectiveExecutorState::Navigating:ObjectiveExecutorState::Executing;
        }
        const char* StateName() const override
        {
            switch(phase_) { case Phase::Idle:return "Idle"; case Phase::Routing:return "RoutingToNpc";
                case Phase::Opening:return "OpeningGossip"; case Phase::AwaitingProgress:return "AwaitingProgress";
                case Phase::Done:return "LiveProgressConfirmed"; case Phase::Failed:return "Failed"; }
            return "Failed";
        }
        const char* FailureReason() const override
        {
            if(failure_=="talk_no_live_progress") return "live_row_progress_not_observed";
            if(failure_=="npc_route_failed") return "generic NavMesh follower failed.";
            if(failure_=="talk_objective_timeout") return "objective_timeout";
            return failure_.c_str();
        }
    };
}
