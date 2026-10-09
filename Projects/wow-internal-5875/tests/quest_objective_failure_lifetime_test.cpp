#include "../src/Bot/QuestDeferPolicy.h"
#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <cstdint>
#include <set>

using namespace Bot;

int main()
{
    constexpr int questId = 903;
    QuestDeferPolicy policy;
    QuestDeferPolicy::ObjectiveFailureObservation observation{};

    // Route, expanded and full-map are one completed executor attempt.
    assert(!policy.ObserveObjectiveFailure(questId, true, 500, 18, 0,
        std::uint64_t{100}, &observation));
    assert(observation.previousCount == 0 && observation.newCount == 1);
    assert(!policy.IsDeferred(questId));
    assert(policy.Find(questId)->reason == QuestDeferReason::None);
    assert(policy.Find(questId)->deferTick == 0);
    assert(!policy.RevisitEligible(questId, 5000, 99, 99));
    assert(!policy.Revisit(questId, 5000, 99, 99));
    assert(policy.Find(questId)->objectiveFailures == 1);
    assert(!policy.ObserveObjectiveFailure(questId, true, 501, 18, 0,
        std::uint64_t{100}, &observation));
    assert(observation.duplicateAttempt && observation.newCount == 1);

    // Executor restart and planner re-resolution are not progress. A long
    // active route may exceed 320 ticks; only the idle gap before its start
    // determines whether earlier completed-cycle evidence expired.
    policy.ReconcilePresent({questId});
    assert(policy.Find(questId)->objectiveFailures == 1);
    assert(!policy.ObserveObjectiveFailure(questId, true, 988, 18, 0,
        std::uint64_t{508}, &observation));
    assert(!observation.expired && observation.failureGapTicks == 488);
    assert(observation.idleGapTicks == 8);
    assert(observation.previousCount == 1 && observation.newCount == 2);
    assert(policy.Find(questId)->objectiveFailures == 2);
    assert(!policy.IsDeferred(questId));
    assert(policy.Find(questId)->reason == QuestDeferReason::None);
    assert(policy.Find(questId)->deferTick == 0);
    assert(!policy.RevisitEligible(questId, 5000, 99, 99));
    assert(policy.Find(questId)->objectiveFailures == 2);
    policy.ReconcilePresent({questId}); // normal live refresh/quarantine
    assert(policy.Find(questId)->objectiveFailures == 2);

    assert(policy.ObserveObjectiveFailure(questId, true, 1476, 18, 0,
        std::uint64_t{996}, &observation));
    assert(observation.previousCount == 2 && observation.newCount == 3);
    assert(policy.IsDeferred(questId));
    assert(policy.Find(questId)->reason ==
        QuestDeferReason::RepeatedObjectiveFailure);
    assert(policy.Find(questId)->deferTick == 1476);
    assert(policy.Find(questId)->noSafe == 0);
    assert(policy.Find(questId)->deaths == 0);
    assert(!policy.RevisitEligible(questId, 1477, 19, 0));
    assert(!policy.RevisitEligible(questId, 1635, 19, 0));
    assert(policy.RevisitEligible(questId, 1636, 19, 0));

    QuestRevisitBoundary activeBoundary{};
    activeBoundary.objectiveOwned = true;
    assert(!CanEvaluateQuestRevisit(activeBoundary));
    activeBoundary = {};
    activeBoundary.objectiveStartPending = true;
    assert(!CanEvaluateQuestRevisit(activeBoundary));
    activeBoundary = {};
    activeBoundary.objectiveRetryPending = true;
    assert(!CanEvaluateQuestRevisit(activeBoundary));
    activeBoundary = {};
    activeBoundary.discoveryOwned = true;
    assert(!CanEvaluateQuestRevisit(activeBoundary));
    activeBoundary = {};
    activeBoundary.turnInOwned = true;
    assert(!CanEvaluateQuestRevisit(activeBoundary));
    assert(CanEvaluateQuestRevisit({}));

    // Planner defers applying the already-eligible revisit until a safe
    // ownership boundary; merely checking that boundary never resets it.
    assert(policy.Find(questId)->objectiveFailures == 3);
    assert(policy.Revisit(questId, 1636, 19, 0));
    assert(!policy.IsDeferred(questId));

    QuestDeferPolicy idleExpiry;
    assert(!idleExpiry.ObserveObjectiveFailure(questId, true, 500, 18, 0,
        std::uint64_t{100}));
    assert(!idleExpiry.ObserveObjectiveFailure(questId, true, 1300, 18, 0,
        std::uint64_t{821}, &observation));
    assert(observation.expired && observation.idleGapTicks == 321);
    assert(observation.previousCount == 1 && observation.newCount == 1);
    assert(!idleExpiry.IsDeferred(questId));

    // Missing or inconsistent attempt ticks fail closed to the original
    // failure-to-failure window, including a zero-valued first failure tick.
    QuestDeferPolicy invalidClock;
    assert(!invalidClock.ObserveObjectiveFailure(questId, true, 0, 18, 0));
    assert(!invalidClock.ObserveObjectiveFailure(questId, true, 321, 18, 0,
        std::uint64_t{400}, &observation));
    assert(observation.expired && observation.failureGapTicks == 321);
    assert(invalidClock.Find(questId)->objectiveFailures == 1);

    // Only authoritative progress/removal clears the accumulated record.
    assert(idleExpiry.ObserveProgress(questId));
    assert(idleExpiry.Find(questId) == nullptr);
    assert(!idleExpiry.ObserveObjectiveFailure(questId, true, 1400, 18, 0));
    idleExpiry.ReconcilePresent({});
    assert(idleExpiry.Find(questId) == nullptr);

    // Planner filtering after defer remains ordinary multiquest behavior;
    // focus remains exclusive and neither path marks the quest unsupported.
    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.playerLevel = 18;
    PlannerQuestLogEntry first{};
    first.title = "Prowlers of the Barrens";
    first.objectiveCount = 1;
    first.objectiveComplete = {false};
    PlannerQuestLogEntry second{};
    second.title = "Echeyakee";
    second.objectiveCount = 1;
    second.objectiveComplete = {false};
    snapshot.quests = {first, second};
    const std::set<int> deferred{questId};
    const auto multi = QuestPlanner::Evaluate(snapshot, nullptr, nullptr,
        0, &deferred);
    assert(multi.action == QuestPlannerAction::ExecuteObjective);
    assert(multi.primary != nullptr && multi.primary->questId != questId);
    const auto focused = QuestPlanner::Evaluate(snapshot, nullptr, nullptr,
        questId, &deferred);
    assert(focused.action == QuestPlannerAction::Deferred);
    assert(focused.primary == nullptr);
}
