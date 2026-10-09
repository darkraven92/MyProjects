#include "../src/Bot/MultiLocationItemUsePolicy.h"
#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <set>
#include <string>

using namespace Bot;

int main()
{
    const QuestProfile* nests = nullptr;
    const QuestProfile* prowlers = nullptr;
    const QuestProfile* echeyakee = nullptr;
    for (const auto& profile : ValleyOfTrialsProfiles::All())
    {
        if (profile.questId == 905) nests = &profile;
        if (profile.questId == 903) prowlers = &profile;
        if (profile.questId == 881) echeyakee = &profile;
    }
    assert(nests != nullptr && prowlers != nullptr && echeyakee != nullptr);
    assert(std::string(nests->title) == "The Angry Scytheclaws");
    assert(nests->previousQuestId == 881);
    assert(nests->giverEntry == 3338 && nests->turnInEntry == 3338);
    assert(nests->expectedObjectiveCount == 3);
    assert(nests->questUseItemId == 5165);
    assert(nests->objectives.size() == 3);
    assert(nests->objectives[0].leaderboardIndex == 0);
    assert(nests->objectives[0].objective.objectEntry == 6907);
    assert(nests->objectives[1].leaderboardIndex == 1);
    assert(nests->objectives[1].objective.objectEntry == 6908);
    assert(nests->objectives[2].leaderboardIndex == 2);
    assert(nests->objectives[2].objective.objectEntry == 6906);
    for (int i = 0; i < 3; ++i)
    {
        const auto selected = MaterializeObjectiveStep(*nests, i);
        assert(selected.activeLeaderboardIndex == i);
        assert(MultiLocationItemUsePolicy::Supports(selected));
        assert(selected.objective.requiredCount == 1);
        assert(selected.destination.valid && selected.destination.mapId == 1);
    }
    assert(nests->questResourceSources.size() == 3);
    assert(nests->questResourceSources[0].creatureEntry == 3256);
    assert(nests->questResourceSources[1].creatureEntry == 3255);
    assert(nests->questResourceSources[2].creatureEntry == 3254);
    assert(MultiLocationItemUsePolicy::NearestResourceSource(
        *nests, -1530.0f, -2680.0f) == 0);
    assert(MultiLocationItemUsePolicy::NearestResourceSource(
        *nests, -1525.0f, -2630.0f) == 1);
    assert(!MultiLocationItemUsePolicy::Supports(*prowlers));
    assert(!MultiLocationItemUsePolicy::Supports(*echeyakee));

    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.playerLevel = 20;
    snapshot.quests.push_back({"Echeyakee", true, 1, {true}});
    auto plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 905);
    assert(plan.action == QuestPlannerAction::DiscoverPickup);
    assert(!QuestFocusPolicy::Allows(905, 881));
    assert(QuestFocusPolicy::Allows(0, 881));

    snapshot.quests.push_back({"The Angry Scytheclaws", false,
        3, {false, false, false}});
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 905);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == nests && plan.activeObjectiveIndex == 0);
    assert(!QuestFocusPolicy::NeedsPickupAudit(905, plan.action, true));
    auto selected = MaterializeObjectiveStep(*nests, plan.activeObjectiveIndex);
    assert(!MultiLocationItemUsePolicy::ProgressConfirmed(snapshot.quests[1], selected));

    snapshot.quests[1].objectiveComplete = {true, false, false};
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 905);
    assert(plan.activeObjectiveIndex == 1);
    snapshot.quests[1].objectiveComplete = {true, true, false};
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 905);
    assert(plan.activeObjectiveIndex == 2);
    snapshot.quests[1].objectiveComplete = {true, true, true};
    snapshot.quests[1].complete = true;
    plan = QuestPlanner::Evaluate(snapshot, nullptr, nullptr, 905);
    assert(plan.action == QuestPlannerAction::TurnIn);
    snapshot.quests.pop_back();
    const std::set<int> rewarded{905};
    plan = QuestPlanner::Evaluate(snapshot, nullptr, &rewarded, 905);
    assert(plan.action != QuestPlannerAction::TurnIn);

    selected = MaterializeObjectiveStep(*nests, 0);
    const auto& anchor = selected.destination;
    assert(MultiLocationItemUsePolicy::MatchesGameObject(selected,
        6907, anchor.x, anchor.y, anchor.z, 3.0f));
    assert(!MultiLocationItemUsePolicy::MatchesGameObject(selected,
        6908, anchor.x, anchor.y, anchor.z, 3.0f));
    assert(!MultiLocationItemUsePolicy::MatchesGameObject(selected,
        6907, anchor.x + 100.0f, anchor.y, anchor.z, 3.0f));
    assert(!MultiLocationItemUsePolicy::MatchesGameObject(selected,
        6907, anchor.x, anchor.y, anchor.z, 9.0f));

    const auto mayUse = [](bool present, bool matching, int stable,
                           int attempts, std::uint64_t tick)
    {
        return MultiLocationItemUsePolicy::MayIssueUse(
            true, true, true, true, present, matching,
            stable, attempts, tick, 48);
    };
    assert(!mayUse(false, true, 2, 0, 48));
    assert(!mayUse(true, false, 2, 0, 48));
    assert(!mayUse(true, true, 1, 0, 48));
    assert(!mayUse(true, true, 2, 0, 47));
    assert(!mayUse(true, true, 2,
        MultiLocationItemUsePolicy::MaximumUseAttempts, 48));
    assert(mayUse(true, true, 2, 0, 48));
    assert(!MultiLocationItemUsePolicy::MayIssueUse(
        true, false, true, true, true, true, 2, 0, 48, 48));
    assert(!MultiLocationItemUsePolicy::MayIssueUse(
        true, true, false, true, true, true, 2, 0, 48, 48));
    assert(!MultiLocationItemUsePolicy::MayIssueUse(
        true, true, true, false, true, true, 2, 0, 48, 48));
}
