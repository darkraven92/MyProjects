#pragma once
#include "IObjectiveExecutor.h"
#include "ExploreObjectivePolicy.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "ValleyOfTrialsProfiles.h"
namespace Bot
{
    class ExploreObjectiveExecutor final : public IObjectiveExecutor
    {
        ObjectiveExecutorState state_=ObjectiveExecutorState::Idle;
        const QuestProfile* profile_=nullptr;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        ObjectiveDefensiveCombatGuard defense_{};
        Objects::PlayerState lastPlayer_{};
        std::uint64_t started_=0, entered_=0;
        std::string failure_{};
        void Set(ObjectiveExecutorState state)
        {
            if(state_==state) return;
            state_=state;
            Debug::Logger::Info("EXPLORE OBJECTIVE quest="+std::to_string(profile_->questId)+
                " state="+StateName()+" result="+(failure_.empty()?"pending":failure_));
        }
        void Stop()
        {
            if(navigator_ && lastPlayer_.valid) navigator_->PauseForCombat(lastPlayer_,"explore objective hold");
        }
        void Fail(const char* reason)
        { failure_=reason; Stop(); navigator_.reset(); Set(ObjectiveExecutorState::Failed); }
    public:
        bool Supports(const QuestProfile& p) const override { return ExploreObjectivePolicy::Supports(p); }
        bool Start(const QuestProfile& p,const Objects::WorldState& world,CombatController& combat,std::uint64_t tick) override
        {
            if(state_!=ObjectiveExecutorState::Idle || !Supports(p) || !world.player.valid || !world.player.health) return false;
            profile_=&p; started_=tick; lastPlayer_=world.player;
            combat.ClearPlannerQuestTarget(); defense_.Reset(world,"explore objective");
            navigator_=std::make_unique<Navigation::GenericNavMeshPathFollower>();
            const auto& d=p.destination;
            if(!navigator_->Start(world.player,tick,{d.x,d.y,d.z},d.mapId,d.arrivalDistance,d.label))
            { Fail("generic NavMesh route failed to start."); return false; }
            Debug::Logger::Info("EXPLORE OBJECTIVE quest="+std::to_string(p.questId)+
                " destination="+std::to_string(d.x)+","+std::to_string(d.y)+","+std::to_string(d.z)+" source=SQL_DBC");
            Set(ObjectiveExecutorState::Navigating); return true;
        }
        void Update(const QuestPlannerSnapshot& snapshot,const Objects::WorldState& world,CombatController& combat,std::uint64_t tick) override
        {
            if(!OwnsControl() || !snapshot.valid || !world.player.valid || !world.player.health) return;
            lastPlayer_=world.player;
            const PlannerQuestLogEntry* live=nullptr;
            for(const auto& entry:snapshot.quests)
            {
                const auto* p=ValleyOfTrialsProfiles::Find(entry,snapshot.classToken);
                if(p && p->questId==profile_->questId)
                { if(live) { Fail("ambiguous_live_identity"); return; } live=&entry; }
            }
            if(!live) { Fail("live_quest_identity_lost"); return; }
            if(ExploreObjectivePolicy::Complete(*live))
            { Stop(); navigator_.reset(); Set(ObjectiveExecutorState::ReadyForTurnIn); return; }
            if(tick>=started_ && tick-started_>=ExploreObjectivePolicy::MaximumObjectiveTicks)
            { Fail("objective_timeout"); return; }
            if(defense_.Update(world,combat,navigator_.get(),0,tick)!=ObjectiveDefenseUpdate::Clear) return;
            if(state_==ObjectiveExecutorState::Executing)
            {
                if(ExploreObjectivePolicy::WaitExpired(tick,entered_)) Fail("live_quest_completion_not_observed");
                return;
            }
            if(ExploreObjectivePolicy::Inside(*profile_,profile_->destination.mapId,world.player.x,world.player.y,world.player.z))
            { Stop(); entered_=tick; Set(ObjectiveExecutorState::Executing); return; }
            navigator_->Update(world.player,tick);
            if(navigator_->Failed()) { Fail("generic NavMesh follower failed."); return; }
            if(navigator_->Arrived()) Fail("validated_route_did_not_enter_trigger_sphere");
        }
        bool OwnsControl() const override
        { return state_==ObjectiveExecutorState::Navigating || state_==ObjectiveExecutorState::Executing; }
        ObjectiveExecutorState State() const override { return state_; }
        const char* StateName() const override
        {
            switch(state_) { case ObjectiveExecutorState::Idle:return "Idle";
                case ObjectiveExecutorState::Navigating:return "RoutingToTrigger";
                case ObjectiveExecutorState::Executing:return "AwaitingLiveCompletion";
                case ObjectiveExecutorState::ReadyForTurnIn:return "LiveCompletionConfirmed";
                case ObjectiveExecutorState::Failed:return "Failed"; }
            return "Failed";
        }
        const char* FailureReason() const override { return failure_.c_str(); }
    };
}
