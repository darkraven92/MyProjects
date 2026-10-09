#pragma once
#include "QuestMaintenancePolicy.h"
#include "VendorController.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include <memory>

namespace Bot
{
    class QuestMaintenanceController
    {
        std::unique_ptr<VendorController> vendor_{};
        ObjectiveDefensiveCombatGuard defense_{};
        bool active_=false, enabled_=false;
        std::uint64_t nextProbe_=0, nextTrip_=0;
        std::uint32_t lastHealth_=0;
        ServiceHubBackoffPolicy candidateBackoff_{};
        MaintenanceOutcome outcome_=MaintenanceOutcome::Satisfied;
    public:
        bool Active() const { return active_; }
        MaintenanceOutcome Outcome() const { return outcome_; }
        void SetEnabled(bool enabled)
        {
            if (enabled && !enabled_)
            {
                // Explicit operator permission supersedes a disabled-state wait.
                nextProbe_=0;
                nextTrip_=0;
            }
            enabled_=enabled;
        }
        void Cancel()
        {
            if(active_ && outcome_==MaintenanceOutcome::InProgress) outcome_=MaintenanceOutcome::StillRequired;
            if(vendor_) vendor_->Reset();
            vendor_.reset(); active_=false;
        }
        bool TryStart(const Objects::WorldState& world,CombatController& combat,std::uint64_t tick)
        {
            if(active_ || tick<nextProbe_ || tick<nextTrip_ || combat.LockedGuid()!=0 ||
                combat.HasDeferredCorpseLootPending() || !world.player.valid || world.player.health<=1 ||
                (combat.State()!=CombatState::AcquiringTarget && combat.State()!=CombatState::PostKillDelay &&
                 combat.State()!=CombatState::Idle)) return false;
            nextProbe_=tick+QuestMaintenancePolicy::ProbeTicks;
            if(ObjectiveDefensiveCombatGuard::HasDirectAggressor(world)) return false;
            MaintenanceSnapshot maintenance;
            GrindBagMonitor::Snapshot bags;
            if(!VendorController::ProbeMaintenance(maintenance) || !GrindBagMonitor::Read(bags)) return false;
            const bool bagPressure=bags.valid && bags.freeSlots<=QuestMaintenancePolicy::BagTriggerFreeSlots;
            const auto need=QuestMaintenancePolicy::Need(maintenance);
            if(!bagPressure && !need.Any()) { outcome_=MaintenanceOutcome::Satisfied; return false; }
            outcome_=MaintenanceOutcome::StillRequired;
            Debug::Logger::Info("QUEST MAINTENANCE TRIGGER freeSlots="+std::to_string(bags.freeSlots)+
                " minDurability="+std::to_string(maintenance.minimumDurabilityPercent)+
                " durabilityKnown="+(maintenance.durabilityKnown?"yes":"no")+
                " repair="+(need.repair?"yes":"no")+" food="+(need.food?"yes":"no")+
                " drink="+(need.drink?"yes":"no")+" permission="+(enabled_?"enabled":"disabled"));
            if(!enabled_)
            {
                // Preserve the GUI's explicit manual-vendor contract. This
                // diagnostic does not enable selling behind the user's back.
                nextTrip_=tick+QuestMaintenancePolicy::RetryTicks;
                Debug::Logger::Info("QUEST MAINTENANCE result=manual_required reason=vendor_automation_disabled");
                return false;
            }
            defense_.Reset(world,"quest maintenance"); lastHealth_=world.player.health;
            vendor_=std::make_unique<VendorController>();
            vendor_->SetCandidateBackoff(&candidateBackoff_);
            vendor_->ObserveWorld(world);
            active_=vendor_->Start(world,{world.player.x,world.player.y,world.player.z},tick,need,bagPressure,true);
            nextTrip_=tick+QuestMaintenancePolicy::RetryTicks;
            outcome_=active_ ? MaintenanceOutcome::InProgress : MaintenanceOutcome::TemporarilyUnavailable;
            if(!active_) { vendor_->Reset(); vendor_.reset(); }
            Debug::Logger::Info("QUEST MAINTENANCE result="+std::string(active_?"started":"start_failed")+
                " salePolicy=protected_poor_misc_only bagPressure="+(bagPressure?"yes":"no")+
                " repair="+(need.repair?"yes":"no")+" food="+(need.food?"yes":"no")+
                " drink="+(need.drink?"yes":"no"));
            return active_;
        }
        void Update(const Objects::WorldState& world,CombatController& combat,std::uint64_t tick)
        {
            if(!active_) return;
            if(vendor_ && (!enabled_ || world.player.health<lastHealth_ ||
                ObjectiveDefensiveCombatGuard::HasDirectAggressor(world)))
            {
                // Cancel non-combat CTM BEFORE the guard may start a chase.
                MovementController::HoldPosition(world.player);
                vendor_->Reset(); vendor_.reset();
                outcome_=MaintenanceOutcome::StillRequired;
                Debug::Logger::Info("QUEST MAINTENANCE result=interrupted reason=defense_or_control_change");
            }
            lastHealth_=world.player.health;
            const auto defense=defense_.Update(world,combat,nullptr,0,tick);
            if(defense==ObjectiveDefenseUpdate::OwnsControl) return;
            if(defense==ObjectiveDefenseUpdate::Failed || !vendor_)
            { Cancel(); nextTrip_=tick+QuestMaintenancePolicy::RetryTicks; return; }
            vendor_->Update(world,tick);
            if(vendor_->IsDone() || vendor_->Failed())
            {
                outcome_=vendor_->Outcome();
                Debug::Logger::Info("QUEST MAINTENANCE result="+std::string(vendor_->IsDone()?"done":"failed")+
                    " salesObserved="+std::to_string(vendor_->SalesObserved())+
                    " maintenanceUnmet="+(vendor_->MaintenanceUnmet()?"yes":"no")+
                    " outcome="+MaintenanceOutcomePolicy::Name(outcome_)+
                    " retryAfter="+std::to_string(tick+QuestMaintenancePolicy::RetryTicks));
                MovementController::HoldPosition(world.player);
                Cancel(); nextTrip_=tick+QuestMaintenancePolicy::RetryTicks;
            }
        }
    };
}
