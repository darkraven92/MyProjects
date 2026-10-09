#include "../src/Bot/QuestPlanner.h"
#include <cassert>
int main()
{
    using namespace Bot;
    QuestProfile p; p.questId=10001; p.title="Synthetic sphere"; p.expectedObjectiveCount=-1;
    p.objective.type=QuestObjectiveType::ExploreOrAreaTrigger;
    p.destination={true,1,1,2,3,2,"source sphere"};
    p.areaTriggerIds={10002};
    assert(QuestClassificationPolicy::Classify(p).support==QuestRuntimeSupport::MissingData);
    p.areaTriggers={{10002,5875,p.destination,4,true}};
    assert(ExploreObjectivePolicy::Supports(p));
    assert(QuestClassificationPolicy::Classify(p).support==QuestRuntimeSupport::KnownExecutable);
    assert(QuestObjectiveDispatchPolicy::SupportsAllSteps(p));
    assert(ExploreObjectivePolicy::Inside(p,1,1,2,3));
    assert(ExploreObjectivePolicy::Inside(p,1,5,2,3));
    assert(!ExploreObjectivePolicy::Inside(p,1,5.1f,2,3));
    assert(!ExploreObjectivePolicy::Inside(p,0,1,2,3));
    PlannerQuestLogEntry live{p.title,false,1,{true}};
    assert(!ExploreObjectivePolicy::Complete(live)); // even a row bit is not the whole event
    assert(ExploreObjectivePolicy::WaitExpired(50,10));
    assert(!ExploreObjectivePolicy::WaitExpired(49,10));
    live.complete=true; assert(ExploreObjectivePolicy::Complete(live));
    p.areaTriggers[0].clientGeometryVerified=false; assert(!ExploreObjectivePolicy::Supports(p));
    p.areaTriggers[0].clientGeometryVerified=true;
    p.areaTriggers[0].build=8606; assert(!ExploreObjectivePolicy::Supports(p));
    p.areaTriggers[0].build=5875;
    QuestPlannerSnapshot snapshot; snapshot.valid=true; snapshot.playerLevel=20; snapshot.classToken="WARRIOR";
    snapshot.quests={{p.title,false,1,{false}}};
    std::vector<QuestProfile> profiles{p};
    assert(QuestPlanner::Evaluate(snapshot,nullptr,nullptr,0,nullptr,&profiles).action==QuestPlannerAction::ExecuteObjective);
}
