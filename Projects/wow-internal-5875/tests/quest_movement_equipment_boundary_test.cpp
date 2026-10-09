#include "../src/Bot/QuestPlanner.h"
#include "../src/Navigation/LocalRecoveryExhaustionPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path); assert(file);
    return {std::istreambuf_iterator<char>(file),{}};
}
int main()
{
    using namespace Bot;
    // Existing commitment gates: progress does not authorize a new candidate.
    const QuestRevisitBoundary owners[]={{true,false,false,false,false},
        {false,true,false,false,false},{false,false,true,false,false},
        {false,false,false,true,false},{false,false,false,false,true}};
    for(const auto& owner:owners)
        for(int tick=0;tick<100;++tick) assert(!QuestPlanner::MayReplaceSelection(owner));
    assert(QuestPlanner::MayReplaceSelection({})); // terminal owner release
    assert(!QuestPlanner::MayReplaceSelection({},true)); // death suspension
    using Navigation::LocalRecoveryExhaustionPolicy;
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(100,100,4));
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(110,100,4)); // backtrack is not forward progress
    assert(LocalRecoveryExhaustionPolicy::Assess(4,4,false,0,1)==
        Navigation::LocalRecoveryExhaustionDecision::FailExhausted);

    // Windows/game-thread controllers are covered by source-wiring contracts,
    // not represented as simulated runtime success by these native tests.
    const auto runtime=Read("src/Bot/QuestPlannerRuntimeController.h");
    assert(runtime.find("if(equipment_.Pending()) return;")!=std::string::npos);
    const auto owns=runtime.substr(runtime.find("bool OwnsControl() const"));
    assert(owns.find("equipment_.Pending() ||")!=std::string::npos);
    const auto controller=Read("src/Bot/EquipmentUpgradeController.h");
    assert(controller.find("tick-pendingSince_>=40")!=std::string::npos);
    assert(controller.find("combat.LockedGuid()!=0")!=std::string::npos);
    const auto relocation=Read("src/Bot/RegionalQuestRelocationController.h");
    assert(relocation.find("if(Active()) return false;")<relocation.find("selected_=candidate;"));
    assert(relocation.find("else if(navigator_->Failed()) Finish")!=std::string::npos);
    assert(relocation.find("defense_.Update(world,combat,navigator_.get()")!=std::string::npos);
    const auto maintenance=Read("src/Bot/QuestMaintenanceController.h");
    assert(maintenance.find("tick<nextTrip_")<maintenance.find("vendor_->Start("));
    assert(maintenance.find("Cancel(); nextTrip_=tick+QuestMaintenancePolicy::RetryTicks;")!=std::string::npos);
    const auto nav=Read("src/Navigation/GenericNavMeshPathFollower.h");
    const auto start=nav.substr(nav.find("bool Start("));
    assert(start.find("if(!options.planningOnly)")<start.find("intentId_=++nextIntentId_"));
    assert(nav.find("MOVEMENT INTENT RELEASE")!=std::string::npos);
    assert(nav.find("MOVEMENT INTENT SUSPEND")!=std::string::npos);
    assert(nav.find("MOVEMENT INTENT RESUME")!=std::string::npos);
    assert(nav.find("MaximumPathLength =\n            2000.0f")!=std::string::npos);
    const auto discovery=Read("src/Bot/GenericQuestDiscoveryController.h");
    assert(discovery.find("QuestOfferResolutionPolicy::Evaluate(profile,node,context)")!=std::string::npos);
    assert(discovery.find("if(confirmedEmpty) confirmedCheckedGiverEntries_.insert(giverEntry_)")!=std::string::npos);
    const auto persistence=Read("src/Bot/PersistentQuestState.h");
    assert(persistence.find("completedQuestIds.insert(fileCompleted.begin(), fileCompleted.end())")<
        persistence.find("QuestOfferResolutionPolicy::RestoreEmptyCache(giverAuditVersion)"));
}
