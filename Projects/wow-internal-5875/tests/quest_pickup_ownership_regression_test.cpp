#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/QuestPlanner.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string Source(const char* path)
{
    std::ifstream input(path); assert(input);
    return {std::istreambuf_iterator<char>(input), {}};
}

int main()
{
    using namespace Bot;
    QuestProfile profile;
    profile.questId=100001; profile.title="Pickup ownership fixture";
    profile.giverEntry=17; profile.turnInEntry=17; profile.expectedObjectiveCount=1;
    profile.objective={QuestObjectiveType::KillMob,19,0,0,1,"Fixture target"};
    profile.destination={true,1,1,2,3,5,"Fixture destination"};
    const std::vector<QuestProfile> catalogue{profile};
    QuestPlannerSnapshot snapshot;
    snapshot.valid=true; snapshot.playerLevel=1; snapshot.classToken="WARRIOR";
    auto evaluate=[&] {return QuestPlanner::Evaluate(snapshot,nullptr,nullptr,0,nullptr,&catalogue);};

    // Empty live log after a previous turn-in opens discovery; it must retain
    // acquisition ownership even before the new sweep has started.
    assert(evaluate().action==QuestPlannerAction::DiscoverPickup);
    PlannerQuestLogEntry accepted{profile.title,false,1,{false}};
    assert(!QuestAcquisitionPolicy::VerifiedAcceptance(profile,accepted,17,false,true));
    assert(QuestAcquisitionPolicy::VerifiedAcceptance(profile,accepted,17,true,true));
    snapshot.quests.push_back(accepted);
    const auto plan=evaluate();
    assert(plan.action==QuestPlannerAction::ExecuteObjective);
    assert(plan.primary && QuestObjectiveDispatchPolicy::SupportsAllSteps(*plan.primary));
    // Completion/removal and future acquisition still use the existing plan.
    snapshot.quests.front().complete=true;
    assert(evaluate().action==QuestPlannerAction::TurnIn);
    snapshot.quests.clear();
    assert(evaluate().action==QuestPlannerAction::DiscoverPickup);

    // Native tests cannot instantiate the Windows/game-thread controllers.
    // Bind the sequence above to the production ownership/handoff boundaries;
    // these checks fail against both proven pre-fix paths.
    const auto runtime=Source("src/Bot/QuestPlannerRuntimeController.h");
    const auto owns=runtime.substr(runtime.find("bool OwnsControl() const"));
    assert(owns.find("valleyDiscoverySweepComplete_ && plan_.action == QuestPlannerAction::DiscoverPickup")==std::string::npos);
    assert(owns.find("plan_.action == QuestPlannerAction::DiscoverPickup ||")!=std::string::npos);
    assert(owns.find("objectiveDirector_.OwnsControl()")!=std::string::npos);

    const auto combat=Source("src/Bot/CombatController.h");
    const auto begin=combat.find("void SetPlannerQuestTarget(");
    const auto end=combat.find("void ClearPlannerQuestTarget()",begin);
    const auto handoff=combat.substr(begin,end-begin);
    const auto reset=handoff.find("questPickup_.Reset();");
    assert(reset!=std::string::npos);
    assert(handoff.find("if (state_ == CombatState::QuestPickup)")<reset);
    const auto acquire=handoff.find("SetState(CombatState::AcquiringTarget);");
    assert(acquire!=std::string::npos && acquire>reset);
    assert(handoff.find("questPickupAttempted_ = true;")!=std::string::npos);
    assert(handoff.find("MarkQuestActive")==std::string::npos); // never fake acceptance
    assert(handoff.find("questPickup_.Update")==std::string::npos);
    assert(handoff.find("questPickup_.Start")==std::string::npos);

    const auto discovery=Source("src/Bot/GenericQuestDiscoveryController.h");
    const auto confirmed=discovery.substr(discovery.find("void MarkKnownQuestActive("));
    assert(confirmed.find("selectedProfile_ = nullptr;")!=std::string::npos);
    assert(confirmed.find("giverNavigator_.reset();")!=std::string::npos);
    const auto executor=Source("src/Bot/CollectItemFromMobExecutor.h");
    assert(executor.find("combat.SetPlannerQuestTarget(")!=std::string::npos);
    assert(executor.find("combat.Update(")!=std::string::npos);
    const auto world=Source("src/Bot/WorldMonitor.h");
    const auto update=world.find("questPlannerRuntime.Update(");
    const auto ownership=world.find("questPlannerRuntime.\n                        OwnsControl()");
    assert(update!=std::string::npos && ownership!=std::string::npos && update<ownership);
}
