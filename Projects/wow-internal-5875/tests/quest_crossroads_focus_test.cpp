#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <set>
#include <string>
#include <vector>

using namespace Bot;

int main()
{
    const QuestProfile* profile = nullptr;
    for (const auto& candidate : ValleyOfTrialsProfiles::All())
        if (candidate.questId == 903)
            profile = &candidate;
    assert(profile != nullptr);
    assert(std::string(profile->title) == "Prowlers of the Barrens");
    assert(profile->giverEntry == 3338 && profile->turnInEntry == 3338);
    assert(profile->objective.type == QuestObjectiveType::CollectItemFromMob);
    assert(profile->objective.targetEntry == 3425);
    assert(profile->objective.itemId == 5096);
    assert(profile->objective.requiredCount == 7);
    assert(profile->expectedObjectiveCount == 1);
    assert(profile->destination.valid && profile->turnInDestination.valid);
    assert(profile->searchDestinations.size() == 3);
    assert(profile->automatable);
    assert(profile->preferNearestObjectiveAnchor);
    assert(profile->objectives.size() == 1);
    assert(profile->objectives[0].leaderboardIndex == 0);

    // Existing executor/CombatController filtering is by exact creature
    // entry. The profile must not accidentally describe arbitrary Barrens
    // units as objective targets.
    assert(profile->objective.targetEntry == 3425);
    assert(profile->objective.targetEntry != 3426);

    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.playerLevel = 18;
    const std::vector<std::string> titles{
        "The Forgotten Pools", "Prowlers of the Barrens", "Stolen Silver",
        "The Warsong Reports", "Consumed by Hatred", "Egg Hunt"
    };
    for (const auto& title : titles)
    {
        PlannerQuestLogEntry entry{};
        entry.title = title;
        entry.objectiveCount = 1;
        entry.objectiveComplete = {false};
        snapshot.quests.push_back(entry);
    }
    PlannerQuestLogEntry higherPriorityUnrelated{};
    higherPriorityUnrelated.title = "Zalazane";
    higherPriorityUnrelated.objectiveCount = 3;
    higherPriorityUnrelated.objectiveComplete = {false, false, false};
    snapshot.quests.push_back(higherPriorityUnrelated);
    const auto originalQuests = snapshot.quests;

    auto plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 903);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == profile);
    assert(plan.activeObjectiveIndex == 0);
    assert(plan.unknownActiveTitles.empty());
    const auto normalPlan = QuestPlanner::Evaluate(
        snapshot, nullptr, nullptr, 0);
    assert(normalPlan.action == QuestPlannerAction::ExecuteObjective);
    assert(normalPlan.primary != nullptr);
    assert(normalPlan.waveActiveQuestCount > 1);
    assert(!QuestFocusPolicy::NeedsPickupAudit(903, plan.action, true));
    assert(QuestFocusPolicy::NeedsPickupAudit(903,
        QuestPlannerAction::DiscoverPickup, false));
    assert(QuestFocusPolicy::NeedsPickupAudit(0, plan.action, true));

    // No live title means focus ID alone cannot invent an active quest.
    QuestPlannerSnapshot absent = snapshot;
    absent.quests.erase(absent.quests.begin() + 1);
    auto absentPlan = QuestPlanner::Evaluate(absent, nullptr, nullptr, 903);
    assert(absentPlan.action == QuestPlannerAction::DiscoverPickup);
    assert(absentPlan.primary == nullptr);

    // The other five active titles cannot satisfy or redirect focus 903.
    for (const auto& entry : absent.quests)
        assert(entry.title != "Prowlers of the Barrens");

    // Existing duplicate-title quest identity is not guessed from a focus ID.
    QuestPlannerSnapshot ambiguous{};
    ambiguous.valid = true;
    ambiguous.classToken = "WARRIOR";
    PlannerQuestLogEntry sarkoth{};
    sarkoth.title = "Sarkoth";
    sarkoth.objectiveCount = 2; // neither profiled identity matches
    ambiguous.quests.push_back(sarkoth);
    const auto ambiguousPlan = QuestPlanner::Evaluate(
        ambiguous, nullptr, nullptr, 790);
    assert(ambiguousPlan.primary == nullptr);
    assert(ambiguousPlan.action == QuestPlannerAction::DiscoverPickup);

    // One objective row reporting done does not claim quest completion; the
    // whole live quest-complete flag is authoritative for turn-in.
    snapshot.quests[1].objectiveComplete[0] = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 903);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    snapshot.quests[1].complete = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 903);
    assert(plan.action == QuestPlannerAction::TurnIn);
    assert(plan.primary == profile);
    assert(!QuestFocusPolicy::NeedsPickupAudit(903, plan.action, true));

    // Focused planning observes but does not mutate the other five quests.
    for (std::size_t i = 0; i < snapshot.quests.size(); ++i)
    {
        if (i == 1)
            continue;
        assert(snapshot.quests[i].title == originalQuests[i].title);
        assert(snapshot.quests[i].complete == originalQuests[i].complete);
        assert(snapshot.quests[i].objectiveComplete ==
            originalQuests[i].objectiveComplete);
    }

    snapshot.quests.erase(snapshot.quests.begin() + 1);
    const std::set<int> rewarded{903};
    plan = QuestPlanner::Evaluate(snapshot, nullptr, &rewarded, 903);
    assert(plan.primary == nullptr);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);
}
