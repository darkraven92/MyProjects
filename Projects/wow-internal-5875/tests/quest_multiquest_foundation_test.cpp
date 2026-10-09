#include "../src/Bot/QuestPlanner.h"
#include <cassert>

int main()
{
    using namespace Bot;
    QuestProfile first;
    first.questId = 10001; first.title = "First synthetic quest";
    first.expectedObjectiveCount = 1;
    first.priority = 10; first.giverEntry = 31; first.turnInEntry = 31;
    first.objective = {QuestObjectiveType::KillMob, 51, 0, 0, 1, "target"};
    first.destination = {true, 1, 10, 20, 30, 5, "synthetic"};
    first.routeGroup = QuestRouteGroup::ValleyExterior;
    auto second = first; second.questId = 10002; second.title = "Second synthetic quest";
    std::vector<QuestProfile> profiles{second, first}; // reverse insertion order
    QuestPlannerSnapshot snapshot;
    snapshot.valid = true; snapshot.classToken = "WARRIOR"; snapshot.playerLevel = 20;
    snapshot.quests = {{second.title, false, 1, {false}}, {first.title, false, 1, {false}}};
    std::set<int> deferred, blocked, completed;
    auto evaluate = [&](int focus = 0) {
        return QuestPlanner::Evaluate(snapshot, &blocked, &completed, focus, &deferred, &profiles);
    };
    auto plan = evaluate();
    assert(plan.primary && plan.primary->questId == first.questId);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.routeBundle.size() == 2 && plan.routeBundle[0]->questId == first.questId);
    for (int i = 0; i < 20; ++i) assert(evaluate().primary->questId == first.questId);
    assert(plan.candidateEvaluations[0].eligibility == QuestEligibility::AlreadyActive);

    QuestDeferPolicy difficulty;
    assert(!difficulty.ObserveObjectiveFailure(first.questId, true, 469, 20, 0, std::uint64_t{3}));
    assert(!difficulty.ObserveObjectiveFailure(first.questId, true, 944, 20, 0, std::uint64_t{478}));
    assert(difficulty.ObserveObjectiveFailure(first.questId, true, 1419, 20, 0, std::uint64_t{953}));
    deferred = difficulty.DeferredIds();
    plan = evaluate();
    assert(plan.primary->questId == second.questId);
    assert(profiles[1].automatable);
    assert(snapshot.quests.size() == 2); // no abandonment / synthetic completion
    assert(evaluate(first.questId).action == QuestPlannerAction::Deferred);
    assert(evaluate(first.questId).primary == nullptr);
    assert(!difficulty.RevisitEligible(first.questId, 1420, 20, 0));
    assert(difficulty.RevisitEligible(first.questId, 1800, 20, 0));
    const QuestRevisitBoundary boundaries[] = {
        {true, false, false, false, false}, {false, true, false, false, false},
        {false, false, true, false, false}, {false, false, false, true, false},
        {false, false, false, false, true}};
    for (const auto& boundary : boundaries)
    {
        assert(!QuestPlanner::MayReplaceSelection(boundary));
        auto held = plan;
        if (QuestPlanner::MayReplaceSelection(boundary)) held = evaluate();
        assert(held.primary == plan.primary && difficulty.IsDeferred(first.questId));
    }
    assert(!QuestPlanner::MayReplaceSelection({}, true));
    assert(QuestPlanner::MayReplaceSelection({}));
    assert(difficulty.Revisit(first.questId, 1800, 20, 0));
    deferred = difficulty.DeferredIds();
    assert(evaluate().primary->questId == first.questId);
    blocked.insert(first.questId);
    assert(evaluate(first.questId).action == QuestPlannerAction::TemporarilyBlocked);
    assert(evaluate().primary->questId == second.questId);
    blocked.clear();
    profiles[1].objective.type = QuestObjectiveType::ExploreOrAreaTrigger;
    plan = evaluate();
    assert(plan.primary->questId == second.questId);
    assert(evaluate(first.questId).action == QuestPlannerAction::UnsupportedActiveQuest);
    assert(evaluate(first.questId).primary == nullptr);
    assert(evaluate(first.questId).candidateEvaluations[0].eligibility == QuestEligibility::Unsupported);
    profiles[1].semanticAmbiguous = true;
    assert(evaluate(first.questId).candidateEvaluations[0].eligibility == QuestEligibility::AmbiguousMetadata);
    profiles[1].semanticAmbiguous = false;
    // Completion still routes to turn-in even if the objective semantic has
    // no executor: live state is stronger than an objective classification.
    snapshot.quests[1].complete = true;
    assert(evaluate(first.questId).action == QuestPlannerAction::TurnIn);
    assert(evaluate(first.questId).candidateEvaluations[0].eligibility == QuestEligibility::ReadyForTurnIn);
    difficulty.ReconcilePresent({second.questId});
    assert(difficulty.Find(first.questId) == nullptr);
    assert(QuestFocusPolicy::ParseConfiguration("0").questId == 0);
    assert(QuestFocusPolicy::ParseConfiguration("-1").questId < 0);
}
