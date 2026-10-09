#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <set>
#include <string>
#include <vector>

using namespace Bot;

int main()
{
    // Structured enrichment supplies the missing destination without changing
    // this authored kill mechanic. The generic executor can now dispatch it.
    const QuestProfile* cuttingTeeth = nullptr;
    for (const auto& profile : ValleyOfTrialsProfiles::All())
        if (profile.questId == 788)
            cuttingTeeth = &profile;
    assert(cuttingTeeth != nullptr);
    assert(std::string(cuttingTeeth->title) == "Cutting Teeth");
    assert(cuttingTeeth->giverEntry == 3143);
    assert(cuttingTeeth->turnInEntry == 3143);
    assert(cuttingTeeth->automatable);
    assert(cuttingTeeth->objective.type == QuestObjectiveType::KillMob);
    assert(cuttingTeeth->objective.targetEntry == 3098);
    assert(cuttingTeeth->objective.requiredCount == 10);
    assert(QuestObjectiveDispatchPolicy::SupportsStep(*cuttingTeeth, -1));
    auto missingDestination = *cuttingTeeth;
    missingDestination.destination = {};
    missingDestination.objectives.clear();
    assert(!QuestObjectiveDispatchPolicy::SupportsStep(missingDestination, -1));
    assert(QuestFocusPolicy::Allows(0, 788));
    assert(QuestFocusPolicy::Allows(788, 788));
    assert(!QuestFocusPolicy::Allows(788, 789));
    assert(!QuestFocusPolicy::Allows(-1, 788));

    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.playerLevel = 1;

    // Absent from the live log means pickup discovery, not an invented
    // accepted quest. Discovery must still verify the actual offer in UI.
    auto plan = QuestPlanner::Evaluate(snapshot);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);
    assert(plan.primary == nullptr);
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 788);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);

    PlannerQuestLogEntry live{};
    live.title = "Cutting Teeth";
    live.objectiveCount = 1;
    live.objectiveComplete = {false};
    snapshot.quests.push_back(live);
    plan = QuestPlanner::Evaluate(snapshot);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == cuttingTeeth);
    assert(plan.nonExecutableQuestIds.empty());
    PlannerQuestLogEntry unrelated{};
    unrelated.title = "Sting of the Scorpid";
    unrelated.objectiveCount = 1;
    unrelated.objectiveComplete = {false};
    snapshot.quests.push_back(unrelated);
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 788);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == cuttingTeeth);
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 789);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary && plan.primary->questId == 789);
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 999999);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);
    assert(plan.primary == nullptr);
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, -1);
    assert(plan.primary == nullptr);
    snapshot.quests.pop_back();

    // A leaderboard bit alone does not claim whole-quest completion for this
    // legacy profile; a click or kill is not a turn-in confirmation.
    snapshot.quests[0].objectiveComplete[0] = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 788);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    snapshot.quests[0].complete = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 788);
    assert(plan.action == QuestPlannerAction::TurnIn);
    assert(plan.primary != nullptr && plan.primary->questId == 788);

    // The existing turn-in executor must observe quest-log removal after
    // reward action before the runtime records completion in its ledger.
    snapshot.quests.clear();
    const std::set<int> rewarded{788};
    plan = QuestPlanner::Evaluate(snapshot, nullptr, &rewarded);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);
    assert(plan.primary == nullptr);

    snapshot.valid = false;
    plan = QuestPlanner::Evaluate(snapshot);
    assert(plan.action == QuestPlannerAction::None);
}
