#include "../src/Bot/ObjectiveAnchorSelectionPolicy.h"
#include "../src/Bot/QuestPlanner.h"
#include "../src/Bot/SummonedQuestObjectivePolicy.h"

#include <cassert>
#include <set>
#include <string>

using namespace Bot;

int main()
{
    const QuestProfile* summoned = nullptr;
    const QuestProfile* prowlers = nullptr;
    for (const auto& candidate : ValleyOfTrialsProfiles::All())
    {
        if (candidate.questId == 881) summoned = &candidate;
        if (candidate.questId == 903) prowlers = &candidate;
    }
    assert(summoned != nullptr && prowlers != nullptr);
    assert(std::string(summoned->title) == "Echeyakee");
    assert(summoned->giverEntry == 3338 && summoned->turnInEntry == 3338);
    assert(summoned->previousQuestId == 903);
    assert(summoned->expectedObjectiveCount == 1);
    assert(summoned->questUseItemId == 10327); // horn, not the loot item
    assert(summoned->questUseMinimumIntervalTicks == 128);
    assert(summoned->objective.itemId == 5100); // hide, not the use item
    assert(summoned->objective.targetEntry == 3475);
    assert(summoned->objective.requiredCount == 1);
    assert(summoned->objective.type == QuestObjectiveType::UseQuestItemAtLocation);
    assert(SummonedQuestObjectivePolicy::Supports(*summoned));
    assert(summoned->automatable && summoned->preferNearestObjectiveAnchor);
    assert(summoned->objectives.size() == 1);
    assert(summoned->objectives[0].leaderboardIndex == 0);
    assert(summoned->searchDestinations.size() == 2);
    assert(summoned->destination.mapId == 1);
    assert(summoned->giverDestination.valid && summoned->turnInDestination.valid);
    assert(prowlers->objective.type == QuestObjectiveType::CollectItemFromMob);
    assert(!SummonedQuestObjectivePolicy::Supports(*prowlers));

    // Source-data anchors are profile-only and bounded; the nearest valid
    // same-map Echeyakee's Lair record wins deterministically.
    const auto anchor = ObjectiveAnchorSelectionPolicy::Select(
        *summoned, 458.2f, -3035.9f, 91.7f);
    assert(anchor.valid && anchor.index == 1);
    assert(anchor.directDistance < 1.0f);
    assert(summoned->searchDestinations.size() <=
        ObjectiveAnchorSelectionPolicy::MaximumCandidates);

    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.playerLevel = 19;
    PlannerQuestLogEntry unrelated{};
    unrelated.title = "Egg Hunt";
    unrelated.objectiveCount = 1;
    unrelated.objectiveComplete = {false};
    snapshot.quests.push_back(unrelated);
    const auto originalUnrelated = snapshot.quests[0];

    auto plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 881);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);
    assert(plan.primary == nullptr);
    assert(QuestFocusPolicy::Allows(881, 881));
    assert(!QuestFocusPolicy::Allows(881, 903));
    assert(QuestFocusPolicy::Allows(0, 903));

    PlannerQuestLogEntry active{};
    active.title = "Echeyakee";
    active.objectiveCount = 1;
    active.objectiveComplete = {false};
    snapshot.quests.push_back(active);
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 881);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == summoned && plan.activeObjectiveIndex == 0);
    assert(!QuestFocusPolicy::NeedsPickupAudit(881, plan.action, true));
    assert(snapshot.quests[0].title == originalUnrelated.title);
    assert(snapshot.quests[0].objectiveComplete ==
        originalUnrelated.objectiveComplete);

    // A loot/kill event is not authoritative. Only the live quest state is.
    snapshot.quests[1].objectiveComplete[0] = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 881);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    snapshot.quests[1].complete = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 881);
    assert(plan.action == QuestPlannerAction::TurnIn);
    assert(plan.primary == summoned);
    snapshot.quests.pop_back();
    const std::set<int> rewarded{881};
    plan = QuestPlanner::Evaluate(snapshot, nullptr, &rewarded, 881);
    assert(plan.primary == nullptr);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);

    // The same pure policy gates all item-use preconditions and retry bounds.
    const auto mayUse = [](bool present, bool combatIdle, int stable,
                           int attempts, float distance)
    {
        return SummonedQuestObjectivePolicy::MayUse(
            true, true, true, combatIdle, present, distance,
            stable, attempts, 20, 18);
    };
    assert(mayUse(true, true, 2, 0, 5.0f));
    assert(!mayUse(false, true, 2, 0, 5.0f));
    assert(!mayUse(true, false, 2, 0, 5.0f));
    assert(!mayUse(true, true, 1, 0, 5.0f));
    assert(!mayUse(true, true, 2, 0, 9.0f));
    assert(!mayUse(true, true, 2,
        SummonedQuestObjectivePolicy::MaximumUseAttempts, 5.0f));
    assert(!SummonedQuestObjectivePolicy::MayUse(
        false, true, true, true, true, 5.0f, 2, 0, 20, 18));
    assert(!SummonedQuestObjectivePolicy::MayUse(
        true, false, true, true, true, 5.0f, 2, 0, 20, 18));
    assert(!SummonedQuestObjectivePolicy::MayUse(
        true, true, false, true, true, 5.0f, 2, 0, 20, 18));
    assert(!SummonedQuestObjectivePolicy::MayUse(
        true, true, true, true, true, 5.0f, 2, 0, 20, 21));
    assert(!SummonedQuestObjectivePolicy::MayUse(
        true, true, true, true, true, 5.0f, 2, 1, 127,
        summoned->questUseMinimumIntervalTicks));
    assert(SummonedQuestObjectivePolicy::MayUse(
        true, true, true, true, true, 5.0f, 2, 1, 128,
        summoned->questUseMinimumIntervalTicks));
    assert(SummonedQuestObjectivePolicy::StableSample(0.1f));
    assert(!SummonedQuestObjectivePolicy::StableSample(2.0f));

    assert(SummonedQuestObjectivePolicy::ExpectedSpawn(
        3475, 3475, true, false, 100, 10.0f));
    assert(!SummonedQuestObjectivePolicy::ExpectedSpawn(
        3425, 3475, true, false, 100, 10.0f));
    assert(!SummonedQuestObjectivePolicy::ExpectedSpawn(
        3475, 3475, true, false, 0, 10.0f));
    assert(!SummonedQuestObjectivePolicy::ExpectedSpawn(
        3475, 3475, true, false, 100, 90.0f));
    assert(!SummonedQuestObjectivePolicy::ExpectedSpawn(
        3475, 3475, true, true, 100, 10.0f));
    assert(!SummonedQuestObjectivePolicy::SpawnWaitExpired(47, 0));
    assert(SummonedQuestObjectivePolicy::SpawnWaitExpired(48, 0));
    assert(!SummonedQuestObjectivePolicy::ObjectiveTimedOut(1599, 0));
    assert(SummonedQuestObjectivePolicy::ObjectiveTimedOut(1600, 0));
}
