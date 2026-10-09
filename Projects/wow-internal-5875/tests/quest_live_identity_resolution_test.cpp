#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <set>
#include <string>

int main()
{
    using namespace Bot;
    const auto& catalogue = ValleyOfTrialsProfiles::All();
    std::set<int> sameTitle;
    for (const auto& profile : catalogue)
        if (std::string(profile.title) == "Vile Familiars")
            sameTitle.insert(profile.questId);
    assert((sameTitle == std::set<int>{792, 1485, 1499}));

    const auto* family = ValleyOfTrialsProfiles::Graph().Find(792);
    const auto* classQuest = ValleyOfTrialsProfiles::Graph().Find(1485);
    const auto* report = ValleyOfTrialsProfiles::Graph().Find(1499);
    assert(family && classQuest && report);
    assert(family->profile->expectedObjectiveCount == 1);
    assert(family->profile->sourceObjectiveCount == 1);
    assert(classQuest->profile->expectedObjectiveCount == 1);
    assert(classQuest->profile->requiredClassMask == 256u);
    assert(report->profile->expectedObjectiveCount == 0);

    PlannerQuestLogEntry live{"Vile Familiars", true, 1, {true}};
    const auto* resolved = ValleyOfTrialsProfiles::Find(live, "WARRIOR");
    assert(resolved == family->profile);
    assert(ValleyOfTrialsProfiles::Find(live, "WARRIOR", nullptr, 0, &catalogue) == resolved);
    assert(ValleyOfTrialsProfiles::Find(live, "") == nullptr);
    assert(ValleyOfTrialsProfiles::Find(live, "WARLOCK") == nullptr);
    live.objectiveCount = 2;
    assert(ValleyOfTrialsProfiles::Find(live, "WARRIOR") == nullptr);
    live.objectiveCount = 1;
    live.complete = false;
    assert(ValleyOfTrialsProfiles::Find(live, "WARRIOR") == resolved);

    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.raceToken = "Orc";
    snapshot.playerLevel = 4;
    live.complete = true;
    snapshot.quests.push_back(live);
    const auto plan = QuestPlanner::Evaluate(snapshot);
    assert(plan.action == QuestPlannerAction::TurnIn);
    assert(plan.primary == resolved);
    assert(plan.candidateEvaluations.size() == 1);
    assert(plan.candidateEvaluations.front().eligibility == QuestEligibility::ReadyForTurnIn);
    assert(resolved->turnInEntry == 3145);
    assert(resolved->turnInDestination.valid);
    snapshot.classToken.clear();
    const auto unknown = QuestPlanner::Evaluate(snapshot);
    assert(unknown.action == QuestPlannerAction::UnsupportedActiveQuest);
    assert(unknown.primary == nullptr);

    // Once the verified turn-in ledger advances, a separate eligible quest
    // is still selected through the same graph/acquisition policy, never by
    // inferring an offer from the old quest's title.
    const auto* next = ValleyOfTrialsProfiles::Graph().Find(790);
    assert(next && next->structurallyValid);
    QuestEligibilityContext context{};
    context.level = 4;
    context.classMask = QuestEligibilityPolicy::ClassMask("WARRIOR");
    context.raceMask = QuestAcquisitionPolicy::RaceMask("Orc");
    context.completed.insert(792);
    context.activeHistoryComplete = true;
    assert(QuestAcquisitionPolicy::Evaluate(*next->profile, next, context) ==
           QuestAcquisitionResult::EligibleForPickup);
    PlannerQuestLogEntry nextLive{"Sarkoth", false, 1, {false}};
    assert(ValleyOfTrialsProfiles::Find(nextLive, "WARRIOR") == next->profile);
    nextLive.objectiveCount = 0;
    assert(ValleyOfTrialsProfiles::Find(nextLive, "WARRIOR")->questId == 804);
}
