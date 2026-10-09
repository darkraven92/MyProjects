#include "../src/Bot/QuestPlanner.h"
#include <cassert>
int main()
{
    using namespace Bot;
    QuestProfile p; p.questId=10001; p.title="Synthetic talk credit";
    p.expectedObjectiveCount=1; p.objective={QuestObjectiveType::TalkToNpc,17,0,0,1,"NPC"};
    p.destination={true,1,1,2,3,3.5f,"source"}; p.gossipCreditText="Verified option";
    assert(TalkToNpcPolicy::Supports(p));
    assert(QuestClassificationPolicy::Classify(p).support==QuestRuntimeSupport::KnownExecutable);
    assert(QuestObjectiveDispatchPolicy::SupportsAllSteps(p));
    auto missing=p; missing.objective.targetEntry=0;
    assert(QuestClassificationPolicy::Classify(missing).support==QuestRuntimeSupport::MissingData);
    missing=p; missing.gossipCreditText.clear();
    assert(QuestClassificationPolicy::Classify(missing).support==QuestRuntimeSupport::KnownSemanticButUnsupported);
    assert(!TalkToNpcPolicy::MayInteract(true,false,true,false,false,1,0));
    assert(!TalkToNpcPolicy::MayInteract(true,false,true,false,true,5,0));
    assert(!TalkToNpcPolicy::MayInteract(true,false,true,true,true,1,0));
    assert(TalkToNpcPolicy::MayInteract(true,false,true,false,true,4,0));
    assert(!TalkToNpcPolicy::MayInteract(true,false,true,false,true,4,3));
    assert(!TalkToNpcPolicy::ProgressWaitExpired(39,0));
    assert(TalkToNpcPolicy::ProgressWaitExpired(40,0));
    assert(TalkToNpcPolicy::TimedOut(1200,0));
    PlannerQuestLogEntry live{p.title,false,1,{false}};
    p.activeLeaderboardIndex=0;
    assert(!SelectedObjectiveComplete(live,p)); // dispatch/arrival grants no credit
    live.objectiveComplete[0]=true; assert(SelectedObjectiveComplete(live,p));
    QuestPlannerSnapshot snapshot; snapshot.valid=true; snapshot.classToken="WARRIOR"; snapshot.playerLevel=20;
    snapshot.quests={{p.title,false,1,{false}}};
    std::vector<QuestProfile> profiles{p};
    assert(QuestPlanner::Evaluate(snapshot,nullptr,nullptr,0,nullptr,&profiles).action==QuestPlannerAction::ExecuteObjective);
    profiles[0].gossipCreditText.clear();
    assert(QuestPlanner::Evaluate(snapshot,nullptr,nullptr,p.questId,nullptr,&profiles).action==QuestPlannerAction::UnsupportedActiveQuest);
    auto script=TalkToNpcPolicy::CreditScript("'\\\n");
    assert(script.find("\\039\\092\\010")!=std::string::npos);
    assert(script.find("table.getn(a)==2")!=std::string::npos);
}
