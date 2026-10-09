#pragma once
#include "RegionalQuestRelocationPolicy.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include <memory>

namespace Bot
{
    class RegionalQuestRelocationController
    {
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        ObjectiveDefensiveCombatGuard defense_{};
        std::uint64_t started_ = 0;
        bool arrived_ = false;
        RegionalQuestDestination selected_{};
        std::map<std::uint32_t,std::uint64_t> backoff_{};
    public:
        bool Active() const { return navigator_ != nullptr; }
        bool Arrived() const { return arrived_; }
        const auto& Backoff() const { return backoff_; }
        void Cancel() { navigator_.reset(); arrived_=false; }
        bool Start(const RegionalQuestDestination& candidate,const Objects::WorldState& world,std::uint64_t tick)
        {
            if(Active()) return false;
            selected_=candidate; started_=tick; arrived_=false;
            defense_.Reset(world,"regional quest relocation");
            navigator_=std::make_unique<Navigation::GenericNavMeshPathFollower>();
            const auto& d=candidate.destination;
            Debug::Logger::Info("QUEST REGIONAL SELECT quest="+std::to_string(candidate.questId)+
                " giverEntry="+std::to_string(candidate.giverEntry)+" distance="+std::to_string(candidate.distance)+
                " usefulQuests="+std::to_string(candidate.usefulQuests)+" reason=local_work_exhausted auditOnly=yes");
            if(navigator_->Start(world.player,tick,{d.x,d.y,d.z},d.mapId,5.0f,"regional quest giver audit")) return true;
            Finish(world,tick,false,"navigation_start_failed");
            return false;
        }
        void Finish(const Objects::WorldState& world,std::uint64_t tick,bool arrived,const char* reason)
        {
            MovementController::HoldPosition(world.player);
            backoff_[selected_.giverEntry]=tick+QuestAcquisitionPolicy::ReauditTicks;
            // Bounded by static catalogue actors; expire evidence on each trip.
            for(auto it=backoff_.begin();it!=backoff_.end();)
                if(it->second<=tick) it=backoff_.erase(it); else ++it;
            navigator_.reset(); arrived_=arrived;
            Debug::Logger::Info("QUEST REGIONAL RESULT quest="+std::to_string(selected_.questId)+
                " result="+(arrived?"arrived":"failed")+" reason="+reason+
                " revisitAfter="+std::to_string(tick+QuestAcquisitionPolicy::ReauditTicks));
        }
        void Update(const Objects::WorldState& world,CombatController& combat,std::uint64_t tick)
        {
            if(!Active()) return;
            const auto defense=defense_.Update(world,combat,navigator_.get(),0,tick);
            if(defense==ObjectiveDefenseUpdate::OwnsControl) return;
            if(defense==ObjectiveDefenseUpdate::Failed)
            { Finish(world,tick,false,"defense_failed"); return; }
            if(tick-started_>=RegionalQuestRelocationPolicy::JourneyTicks)
            { Finish(world,tick,false,"bounded_journey_deadline"); return; }
            navigator_->Update(world.player,tick);
            if(navigator_->Arrived()) Finish(world,tick,true,"validated_route_arrival");
            else if(navigator_->Failed()) Finish(world,tick,false,"navigation_failed");
        }
    };
}
