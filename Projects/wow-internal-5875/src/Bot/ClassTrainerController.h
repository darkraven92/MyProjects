#pragma once
#include "ClassTrainerPolicy.h"
#include "ClassTrainerScript.h"
#include "GenericQuestTurnInExecutor.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "QuestMaintenancePolicy.h"
#include "QuestGraph.h"
#include "VendorController.h"
#include <set>

namespace Bot
{
    class ClassTrainerController
    {
        enum class Phase { Idle, Navigate, Frame, Learn, Confirm };
        Phase phase_=Phase::Idle;
        ClassTrainerCatalogue db_{};
        TrainerNeedPolicy need_{};
        bool loaded_=false, available_=false, interrupted_=false;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> nav_{};
        ObjectiveDefensiveCombatGuard defense_{};
        std::uint64_t nextProbe_=0, start_=0, phaseTick_=0, guid_=0, reserve_=0;
        unsigned entry_=0, class_=0, pending_=0, learned_=0, lastHealth_=0;
        std::set<unsigned> unconfirmed_{};
        std::string token_, name_, lastBlock_;
        bool BlockStart(const char* reason)
        {
            if(lastBlock_!=reason) Debug::Logger::Info("TRAINER NEED level="+std::to_string(need_.Level())+
                " result=deferred reason="+reason);
            lastBlock_=reason; return false;
        }
        static bool Lua(const std::string& script,std::string& result)
        {
            return GrindBagMonitor::ExecuteLuaReadback(script,"wow-internal/ClassTrainer.lua",result,"WOW_INTERNAL_TRAINER_RESULT");
        }
        static void Close()
        {
            std::string ignored;
            Lua("if CloseTrainer then CloseTrainer() end; if CloseGossip then CloseGossip() end; WOW_INTERNAL_TRAINER_RESULT='closed'",ignored);
        }
        void Finish(const Objects::WorldState& world,std::uint64_t tick,const std::string& reason,bool complete=false)
        {
            if(nav_) nav_->PauseForCombat(world.player,"trainer release");
            Close(); nav_.reset(); phase_=Phase::Idle;
            if(complete) need_.Complete(); else need_.Defer(tick);
            Debug::Logger::Info("TRAINER COMPLETE level="+std::to_string(need_.Level())+
                " learnedCount="+std::to_string(learned_)+" result="+reason);
        }
    public:
        bool Active() const { return phase_!=Phase::Idle; }
        void Cancel()
        {
            if(Active()) Close();
            nav_.reset(); phase_=Phase::Idle; interrupted_=false;
        }
        void ObserveLevel(unsigned level,const std::string& classToken)
        {
            if(need_.ObserveLevel(level))
            {
                unconfirmed_.clear();
                Debug::Logger::Info("TRAINER NEED level="+std::to_string(level)+" class="+classToken+" reason=level_audit");
            }
        }
        bool TryStart(const Objects::WorldState& world,const CombatController& combat,std::uint64_t tick,
                      const std::string& token)
        {
            if(Active() || !need_.Pending(tick) || tick<nextProbe_ || !world.player.valid || world.player.health<=1 ||
               combat.LockedGuid() || combat.HasDeferredCorpseLootPending() ||
               ObjectiveDefensiveCombatGuard::HasDirectAggressor(world) ||
               (combat.State()!=CombatState::Idle && combat.State()!=CombatState::AcquiringTarget &&
                combat.State()!=CombatState::PostKillDelay)) return false;
            nextProbe_=tick+ClassTrainerPolicy::ProbeTicks;
            if(!loaded_) { loaded_=true; available_=db_.LoadDefault();
                Debug::Logger::Info("TRAINER CATALOGUE result="+std::string(available_?"loaded":"unavailable")+
                    " services="+std::to_string(db_.services.size())+" spawns="+std::to_string(db_.spawns.size())); }
            if(!available_) { need_.Defer(tick); return BlockStart("catalogue_unavailable"); }
            const auto mask=QuestEligibilityPolicy::ClassMask(token);
            if(!mask) return false;
            class_=1; while((1u<<(class_-1))!=*mask && class_<12) ++class_;
            MaintenanceSnapshot m;
            if(!VendorController::ProbeMaintenance(m) || !m.valid || !m.durabilityKnown ||
               !m.foodCountKnown || (AutonomousMaintenancePolicy::IsManaUser(m) && !m.drinkCountKnown) ||
               QuestMaintenancePolicy::Need(m).Any()) return BlockStart("maintenance_pending_or_unresolved");
            reserve_=ClassTrainerPolicy::Reserve(m.money,0,0);
            if(m.money<=reserve_) { need_.Defer(tick); return BlockStart("insufficient_funds_after_reserve"); }
            unsigned faction=0;
            if(!world.player.descriptors || !Core::Memory::Read(world.player.descriptors+0x8Cu,faction)) return false;
            const Objects::UnitState* best=nullptr;
            for(const auto& u:world.units)
            {
                if(!u.valid || !u.guid || !u.health || !(u.npcFlags&16u) || !std::isfinite(u.distance) ||
                   u.distance<0 || u.distance>300 || !db_.actors.contains(u.entryId) ||
                   db_.actors.at(u.entryId).faction!=u.factionTemplate || !db_.Friendly(u.factionTemplate,faction)) continue;
                bool service=false, spawn=false;
                for(const auto& s:db_.services)
                    if(s.entry==u.entryId && s.classId==class_ && s.level<=world.player.level) { service=true; break; }
                for(const auto& s:db_.spawns)
                    if(s.entry==u.entryId && s.map==1 && std::hypot(s.x-u.x,s.y-u.y)<40 && std::abs(s.z-u.z)<10)
                    { spawn=true; break; }
                if(service && spawn && (!best || u.distance<best->distance ||
                   (u.distance==best->distance && u.guid<best->guid))) best=&u;
            }
            // Regional travel is intentionally not guessed: matching visible actor
            // plus source spawn, within the existing local discovery scope.
            if(!best) return BlockStart("no_matching_live_source_trainer_in_local_scope");
            lastBlock_.clear();
            entry_=best->entryId; guid_=best->guid; name_=db_.actors.at(entry_).name; token_=token;
            pending_=learned_=0; start_=phaseTick_=tick; interrupted_=false; lastHealth_=world.player.health;
            Close(); defense_.Reset(world,"class trainer");
            nav_=std::make_unique<Navigation::GenericNavMeshPathFollower>();
            if(!nav_->Start(world.player,tick,{best->x,best->y,best->z},1,3.5f,"class trainer approach"))
            { Finish(world,tick,"route_start_failed"); return false; }
            phase_=Phase::Navigate;
            Debug::Logger::Info("TRAINER SELECT entry="+std::to_string(entry_)+" distance="+
                std::to_string(best->distance)+" reason=matching_class_live_source_spawn");
            return true;
        }
        void Update(const Objects::WorldState& world,CombatController& combat,std::uint64_t tick)
        {
            if(!Active()) return;
            if(!world.player.valid || world.player.health<=1) { Cancel(); need_.Defer(tick); return; }
            if(world.player.health<lastHealth_ || ObjectiveDefensiveCombatGuard::HasDirectAggressor(world))
            {
                if(!interrupted_) { if(nav_) nav_->PauseForCombat(world.player,"trainer defense"); Close(); interrupted_=true; }
            }
            lastHealth_=world.player.health;
            const auto defense=defense_.Update(world,combat,nav_.get(),0,tick);
            if(defense==ObjectiveDefenseUpdate::OwnsControl) return;
            if(defense==ObjectiveDefenseUpdate::Failed || interrupted_)
            { Finish(world,tick,"defense_interrupted"); return; }
            if(tick<start_ || tick-start_>=ClassTrainerPolicy::EpisodeTicks)
            { Finish(world,tick,"episode_timeout"); return; }
            const Objects::UnitState* npc=nullptr;
            for(const auto& u:world.units) if(u.valid && u.guid==guid_ && u.entryId==entry_ && u.health) npc=&u;
            if(!npc) { Finish(world,tick,"actor_lost"); return; }
            if(phase_==Phase::Navigate)
            {
                if(npc->distance<=4.5f)
                {
                    if(!nav_->PauseForCombat(world.player,"trainer interaction") ||
                       !GenericQuestTurnInExecutor::InteractUnitGuid(guid_))
                    { Finish(world,tick,"interaction_failed"); return; }
                    phase_=Phase::Frame; phaseTick_=tick; return;
                }
                nav_->Update(world.player,tick);
                if(nav_->Failed() || nav_->Arrived()) Finish(world,tick,"route_failed_or_actor_out_of_range");
                return;
            }
            if(!std::isfinite(npc->distance) || npc->distance>4.5f)
            { Finish(world,tick,"actor_range_changed"); return; }
            if(tick<phaseTick_+4) return;
            if(phase_==Phase::Frame)
            {
                std::string r;
                const std::string script="WOW_INTERNAL_TRAINER_RESULT='waiting'; if UnitName('npc')=="+
                    TrainerLuaString(name_)+" then if ClassTrainerFrame and ClassTrainerFrame:IsVisible() then "
                    "if SetTrainerServiceTypeFilter and ExpandTrainerSkillLine then "
                    "SetTrainerServiceTypeFilter('available',1); SetTrainerServiceTypeFilter('unavailable',1); "
                    "SetTrainerServiceTypeFilter('used',1); ExpandTrainerSkillLine(0); WOW_INTERNAL_TRAINER_RESULT='ready' end "
                    "end end";
                if(!Lua(script,r)) { Finish(world,tick,"frame_read_failed"); return; }
                if(r=="ready") { phase_=Phase::Learn; phaseTick_=tick;
                    Debug::Logger::Info("TRAINER FRAME result=confirmed"); return; }
                // Do not guess gossip option types/text; direct trainer frames
                // are supported, ambiguous/multiplexed gossip fails boundedly.
                if(tick-phaseTick_>=ClassTrainerPolicy::ConfirmationTicks) Finish(world,tick,"trainer_frame_not_observed");
                return;
            }
            std::string r;
            if(!Lua(TrainerScript(db_,entry_,class_,token_,name_,reserve_,pending_,false),r))
            { Finish(world,tick,"spellbook_read_failed"); return; }
            if(phase_==Phase::Confirm)
            {
                if(r=="confirmed")
                {
                    Debug::Logger::Info("TRAINER LEARN spell="+std::to_string(pending_)+" result=confirmed source=spellbook");
                    unconfirmed_.erase(pending_); pending_=0; ++learned_; phase_=Phase::Learn; phaseTick_=tick;
                }
                else if(tick-phaseTick_>=ClassTrainerPolicy::ConfirmationTicks)
                { Debug::Logger::Info("TRAINER LEARN spell="+std::to_string(pending_)+" result=failed"); Finish(world,tick,"spellbook_confirmation_timeout"); }
                return;
            }
            if(r.rfind("candidate|",0)==0)
            {
                const unsigned spell=static_cast<unsigned>(std::stoul(r.substr(10)));
                const auto separator=r.find('|',10);
                if(separator==std::string::npos) { Finish(world,tick,"invalid_candidate_readback"); return; }
                const auto cost=std::stoul(r.substr(separator+1));
                if(unconfirmed_.contains(spell)) { Finish(world,tick,"unconfirmed_purchase_suppressed_until_level_change"); return; }
                // Re-read maintenance immediately before each monetary command.
                MaintenanceSnapshot m;
                if(!VendorController::ProbeMaintenance(m) || !m.valid || !m.durabilityKnown ||
                   !m.foodCountKnown || (AutonomousMaintenancePolicy::IsManaUser(m) && !m.drinkCountKnown))
                { Finish(world,tick,"maintenance_pending"); return; }
                const TrainerAbilityEvidence evidence{db_.spells.contains(spell),true,true,true,true,false,true,
                    m.money,cost,reserve_,QuestMaintenancePolicy::Need(m).Any()};
                if(ClassTrainerPolicy::Evaluate(evidence)!=TrainerDecision::Learn)
                { Finish(world,tick,"maintenance_or_funds_changed"); return; }
                unconfirmed_.insert(spell); // includes dispatcher/readback ambiguity: never blindly repurchase
                if(!Lua(TrainerScript(db_,entry_,class_,token_,name_,reserve_,0,true,spell),r) || r.rfind("issued|",0)!=0)
                { Finish(world,tick,"purchase_not_issued"); return; }
                pending_=static_cast<unsigned>(std::stoul(r.substr(7))); unconfirmed_.insert(pending_);
                phase_=Phase::Confirm; phaseTick_=tick;
                Debug::Logger::Info("TRAINER ABILITY spell="+std::to_string(pending_)+
                    " rank="+std::to_string(db_.spells.at(pending_).rank)+" cost="+std::to_string(cost)+
                    " decision=purchase result="+r);
                Debug::Logger::Info("TRAINER LEARN spell="+std::to_string(pending_)+" result=pending"); return;
            }
            Finish(world,tick,r,r.rfind("complete|",0)==0);
        }
    };
}
