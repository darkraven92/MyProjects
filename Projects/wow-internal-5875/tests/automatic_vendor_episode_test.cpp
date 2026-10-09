#include "../src/Bot/AutomaticVendorEpisodePolicy.h"
#include "../src/Bot/ServiceHubBackoffPolicy.h"
#include "../src/Bot/QuestMaintenancePolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include "../src/Navigation/LocalRecoveryLimits.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path); assert(file);
    return {std::istreambuf_iterator<char>(file), {}};
}

int main()
{
    using namespace Bot;
    using D = MaintenanceWaitDecision;
    AutomaticVendorEpisodePolicy episode;
    static_assert(AutomaticVendorEpisodePolicy::MaximumAttemptTicks == 480);
    static_assert(AutomaticVendorEpisodePolicy::MaximumAttempts == 3);
    static_assert(UnattendedMaintenanceWaitPolicy::MaximumRetries == 2);
    static_assert(QuestMaintenancePolicy::RetryTicks == 2400);
    static_assert(Navigation::LocalRecoveryLimits::MaximumSurfaceRecoveryAttempts == 4);
    static_assert(Navigation::LocalRecoveryLimits::MaximumLastSafeBacktracks == 2);
    assert(!episode.Expired(9999)); // manual/quest controller not opted in
    assert(episode.Begin(100));
    episode.Select(10);
    assert(!episode.Expired(200)); // short successful trip
    episode.Complete();
    assert(!episode.Active());
    assert(episode.Begin(300));
    episode.Select(10);
    episode.Failure();
    episode.Select(20); // one failure can choose an alternate within the budget
    assert(!episode.Expired(400));
    assert(episode.Changes() == 1 && episode.Failures() == 1);
    // Re-created intents, changed fallback tiers, vendor substates and combat
    // interruptions all retain the same attempt; repeated Start cannot refill it.
    for (unsigned tick=401; tick<780; ++tick)
    {
        assert(!episode.Begin(tick));
        if (tick%20 == 0) episode.Select(tick);
        assert(!episode.Expired(tick));
    }
    assert(episode.Expired(780));
    assert(episode.Exhaust(780));
    assert(!episode.Exhaust(781)); // transition emitted once
    assert(episode.Waiting() && episode.ServiceProofRequired());
    assert(!episode.Begin(100000)); // no same-tick or delayed ordinary reacquisition

    UnattendedMaintenanceWaitPolicy wait;
    wait.FailedTrip(780+2400); // existing non-urgent maintenance cooldown
    auto read = [&](std::uint64_t tick, bool fresh, bool known, int slots,
                    bool serviceSatisfied, bool safe=true)
    {
        return wait.Observe(tick, fresh, known, slots, 1, safe,
            episode.CanRetry(), serviceSatisfied);
    };
    assert(read(788,true,true,12,false) == D::Wait);
    assert(read(796,true,true,12,false) == D::Wait); // free bags cannot restart food search
    assert(read(3179,true,true,12,false) == D::Wait);
    assert(read(3180,false,true,12,false) == D::Wait);
    assert(read(3180,true,true,12,false,false) == D::Wait);
    assert(read(3180,true,true,12,false) == D::RetryVendor);
    assert(episode.Retry(3180));
    assert(episode.Attempts() == 2 && !episode.Waiting());
    assert(episode.ServiceProofRequired()); // survives retry/controller Reset
    assert(!episode.Expired(3659) && episode.Expired(3660));
    assert(episode.Exhaust(3660));
    wait.FailedTrip(3660+480); // existing bag/urgent cooldown
    assert(read(4139,true,true,0,false) == D::Wait);
    assert(read(4140,true,true,0,false) == D::RetryVendor);
    assert(episode.Retry(4140));
    episode.Select(30);
    assert(episode.Exhaust(4620));
    wait.FailedTrip(5100);
    assert(read(5100,true,true,12,false) == D::MaintenanceBlocked);
    assert(read(100000,true,true,12,false) == D::MaintenanceBlocked);
    assert(!episode.CanRetry() && !episode.Retry(100000));
    assert(wait.Retries() == 2 && episode.Attempts() == 3);
    assert(read(100008,true,true,12,true) == D::Wait);
    assert(read(100009,false,true,12,true) == D::Wait);
    wait.InvalidateSpaceProof(); // world gap
    assert(read(100016,true,true,12,true) == D::Wait);
    assert(read(100024,true,false,12,true) == D::MaintenanceBlocked);
    assert(read(100032,true,true,12,true) == D::Wait);
    assert(read(100040,true,true,12,true) == D::Resume);
    episode.Complete(); wait.Reset();
    assert(episode.Begin(100041));
    assert(episode.Expired(100040)); // clock regression fails closed

    MaintenanceNeed food{}; food.food=true;
    MaintenanceSnapshot inventory{}; inventory.valid=true;
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    inventory.foodCountKnown=true; inventory.foodCount=12;
    assert(AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(false,12,1,inventory,food));
    inventory.valid=false;
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    inventory.valid=true; food.repair=true;
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    inventory.durabilityKnown=true;
    assert(AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));

    // Only requested services own this latch. A bag-only episode cannot
    // inherit a later food/repair shortage (or require unrelated metadata).
    inventory.foodCount=0; inventory.durableItems=1;
    inventory.minimumDurabilityPercent=10;
    assert(AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,{}));
    inventory.valid=false;
    assert(AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,{}));
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(false,12,1,inventory,{}));
    inventory.valid=true; inventory.foodCount=5; food.repair=false;
    assert(AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    food.repair=true;
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    inventory.minimumDurabilityPercent=100; inventory.durabilityKnown=false;
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,food));
    MaintenanceNeed drink{}; drink.drink=true;
    inventory.drinkCount=4;
    assert(!AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,drink));
    inventory.drinkCount=5;
    assert(AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,drink));

    wait.Reset(); wait.FailedTrip(2000);
    inventory.foodCount=0;
    const bool bagOnlyResolved=AutomaticVendorEpisodePolicy::RequirementsSatisfied(true,12,1,inventory,{});
    assert(wait.Observe(100,true,true,12,1,true,true,bagOnlyResolved) == D::Wait);
    assert(wait.Observe(108,true,true,12,1,true,true,bagOnlyResolved) == D::Resume);

    // Expiry still terminates outbound, service and return states. The
    // production state classifier attributes a penalty only to unfinished
    // candidate work, never to completed service followed by return travel.
    for (auto state : {VendorState::NavigatingVendorAnchor, VendorState::Maintaining,
                       VendorState::ReturningToGrind, VendorState::Done})
    {
        AutomaticVendorEpisodePolicy trip;
        ServiceHubBackoffPolicy failures;
        assert(trip.Begin(100)); trip.Select(20);
        assert(trip.Exhaust(580));
        if (AutomaticVendorEpisodePolicy::RejectCandidateOnExpiry(state))
        {
            trip.Failure();
            failures.Reject(20,580,QuestMaintenancePolicy::RetryTicks);
        }
        const bool outbound=state==VendorState::NavigatingVendorAnchor || state==VendorState::Maintaining;
        assert(failures.Blocked(20,580)==outbound);
        assert(trip.Failures()==(outbound ? 1u : 0u));
        assert(trip.Waiting() && trip.Expired(581)); // no return-route escape from bound
    }
    assert(!AutomaticVendorEpisodePolicy::RejectCandidateOnExpiry(VendorState::SelectingHub));

    // Replay the policies used by repeated aggressor preemption -> bag proof
    // -> resumed vendor starts. These are resumes, not explicit retry attempts.
    AutomaticVendorEpisodePolicy interrupted;
    assert(interrupted.Begin(100));
    UnattendedMaintenanceWaitPolicy interruptions;
    for (auto tick : {150u,250u,350u})
    {
        interrupted.Select(tick);
        interruptions.FailedTrip(tick+480);
        assert(interruptions.Observe(tick+1,true,true,12,1,false,true)==D::Wait);
        assert(interruptions.Observe(tick+8,true,true,12,1,true,true)==D::Wait);
        assert(interruptions.Observe(tick+16,true,true,12,1,true,true)==D::Resume);
        interruptions.Reset();
        assert(!interrupted.Begin(tick+17));
        assert(interrupted.Attempts()==1);
    }
    assert(interrupted.Exhaust(580));
    interruptions.FailedTrip(1060);
    assert(interruptions.Observe(1059,true,true,0,1,true,true,false)==D::Wait);
    assert(interruptions.Observe(1060,true,true,0,1,true,true,false)==D::RetryVendor);
    assert(interrupted.Retry(1060)); // startup fails; both counters remain consumed
    interruptions.FailedTrip(1540);
    assert(interruptions.Retries()==1 && interrupted.Attempts()==2);
    assert(interruptions.Observe(1060,true,true,0,1,true,true,false)==D::Wait);
    assert(interruptions.Observe(1540,true,true,0,1,true,true,false)==D::RetryVendor);
    assert(interrupted.Retry(1540));
    // Successful retry: terminal read is not a scheduled release observation.
    interrupted.Complete(); interruptions.FailedTrip(2040);
    assert(interruptions.Observe(1560,false,true,12,1,true,true)==D::MaintenanceBlocked);
    assert(interruptions.Observe(1568,true,true,12,1,true,true)==D::Wait);
    interruptions.InvalidateSpaceProof();
    assert(interruptions.Observe(1576,true,true,12,1,true,true)==D::Wait);
    assert(interruptions.Observe(1584,true,true,12,1,true,true)==D::Resume);

    ServiceHubBackoffPolicy backoff;
    backoff.Reject(20,400,QuestMaintenancePolicy::RetryTicks);
    assert(backoff.Blocked(20,2799) && !backoff.Blocked(20,2800));
    // Episode rejection is additionally retained across controller Reset/Start,
    // even once this existing timed backoff expires.
    const auto vendor = Read("src/Bot/VendorController.h");
    const auto expiry=vendor.substr(vendor.find("void ExhaustAutomaticEpisode"),
        vendor.find("void CompleteAutomaticEpisode")-vendor.find("void ExhaustAutomaticEpisode"));
    assert(expiry.find("AutomaticVendorEpisodePolicy::RejectCandidateOnExpiry(state_)") != std::string::npos);
    assert(expiry.find("if (penalizeCandidate)") < expiry.find("BackOffCandidate(vendorEntry_"));
    const auto start = vendor.substr(vendor.find("        bool Start("),
        vendor.find("        void Update(") - vendor.find("        bool Start("));
    const auto reset = vendor.substr(vendor.find("        void Reset()"));
    for (const auto* block : {&start, &reset})
    {
        assert(block->find("if (!automaticEpisode_.Active()) rejectedServiceEntries_.clear();") != std::string::npos);
        assert(block->find("automaticEpisode_.Complete()") == std::string::npos);
        assert(block->find("automaticEpisode_.Retry(") == std::string::npos);
    }
    assert(start.find("automaticEpisode_.Expired(tick)") != std::string::npos);
    const auto grind = Read("src/Bot/GrindModeController.h");
    const auto routing = grind.substr(grind.find("            if (state_ == GrindModeState::Vendoring)"));
    const auto guard = routing.find("if (vendor_.AutomaticEpisode().Expired(tick))");
    assert(guard < routing.find("vendor_.Update(world, tick)"));
    const auto branch = routing.substr(guard, routing.find("vendor_.Update(world, tick)")-guard);
    assert(branch.find("EnterVendorEpisodeWait(world, tick);") != std::string::npos);
    assert(branch.find("return;") != std::string::npos);
    const auto hold = grind.substr(grind.find("void EnterVendorEpisodeWait"),
        grind.find("void ProbeBags")-grind.find("void EnterVendorEpisodeWait"));
    for (const auto* token : {"vendor_.Reset();", "HoldPosition(world.player)",
             "unattendedMaintenanceWait_.FailedTrip", "WaitingForManualVendor"})
        assert(hold.find(token) != std::string::npos);
    assert(grind.find("vendor_.RetryAutomaticEpisode(tick) &&") != std::string::npos);
    assert(grind.find("vendor_.AutomaticEpisode().CanRetry(), releaseAllowed") != std::string::npos);
    assert(grind.find("maintenance_.valid = false;") != std::string::npos);
    assert(grind.find("VendorController::ProbeMaintenance(completedMaintenance) &&") != std::string::npos);
    assert(grind.find("VendorTriggerFreeSlots, completedMaintenance,") != std::string::npos);
    assert(grind.find("!unattendedMaintenanceWait_.Active() &&") != std::string::npos);
    assert(grind.find("resolvedBags.valid, resolvedBags.freeSlots, VendorTriggerFreeSlots,") != std::string::npos);
    assert(hold.find("std::max(tick, unattendedMaintenanceWait_.RetryAt())") != std::string::npos);
    assert(hold.find("state_ != GrindModeState::Vendoring && previousRetryAt != 0") != std::string::npos);

    AfkSafety safe; safe.healthyIdle = true;
    AfkObservation o{true,false,false,240000,0,300000};
    AfkProductionPolicy afk;
    assert(afk.Update(o,safe,1,false,false).action == AfkAction::InputPulse);
    for (auto field : {&AfkSafety::combat,&AfkSafety::vendor,&AfkSafety::water,&AfkSafety::death})
    {
        auto blocked=safe; blocked.*field=true;
        assert(afk.Update(o,blocked,1,false,false).action == AfkAction::None);
    }
    assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(1,false));
    assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(0x00200000,false));
    for (auto age : {239999u,240000u,269999u,270000u,299999u,300000u})
    {
        o.clientNow=age;
        const auto expected=age>=300000 ? AfkProductionBand::ThresholdCrossed :
            age>=270000 ? AfkProductionBand::Overdue : age>=240000 ?
            AfkProductionBand::Due : AfkProductionBand::Recent;
        assert(AfkProductionPolicy::Band(o) == expected);
    }
    AfkProductionPolicy delivery;
    assert(delivery.Update(o,safe,1,false,false).action == AfkAction::InputPulse);
    delivery.Issued(AfkAction::InputPulse,o,1);
    assert(delivery.Update(o,safe,2,false,false).result == AfkResult::Pending);
    o.lastInput=o.clientNow;
    assert(delivery.Update(o,safe,3,false,false).result == AfkResult::Confirmed);
}
