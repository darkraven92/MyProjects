#include "../src/Bot/QuestDeferPolicy.h"
#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <set>

using namespace Bot;

int main()
{
    constexpr int quest = 42;
    QuestDeferPolicy policy;

    // Death attribution requires recent objective ownership plus a verified
    // quest target. An unrelated defensive fight or a stale selection fails
    // closed even when the planner happened to display a quest.
    QuestDeathContext context{};
    context.questObjectiveOwned = true;
    context.recentlyActive = true;
    context.lockedQuestTarget = true;
    assert(ShouldAttributeQuestDeath(context));
    context.unrelatedDefensiveCombat = true;
    assert(!ShouldAttributeQuestDeath(context));
    context.unrelatedDefensiveCombat = false;
    context.recentlyActive = false;
    assert(!ShouldAttributeQuestDeath(context));
    context.recentlyActive = true;
    context.otherOwner = true;
    assert(!ShouldAttributeQuestDeath(context));
    context.otherOwner = false;

    assert(!policy.ObserveDeath(quest, false, 10, 10, 0));
    assert(!policy.ObserveDeath(quest, true, 10, 10, 0));
    assert(!policy.IsDeferred(quest));
    assert(policy.Find(quest)->reason == QuestDeferReason::None);
    assert(policy.Find(quest)->deferTick == 0);
    assert(!policy.RevisitEligible(quest, 10000, 60, 100));
    assert(!policy.Revisit(quest, 10000, 60, 100));
    assert(policy.Find(quest)->deaths == 1);
    assert(policy.ObserveDeath(quest, true, 20, 10, 0));
    assert(policy.IsDeferred(quest));
    assert(policy.Find(quest)->reason == QuestDeferReason::RepeatedDeaths);
    assert(!policy.RevisitEligible(quest, 21, 11, 0));
    assert(policy.RevisitEligible(quest, 180, 11, 0));
    assert(policy.Revisit(quest, 180, 11, 0));
    assert(!policy.IsDeferred(quest));
    assert(policy.Find(quest)->deaths == 0);

    // The death window is bounded; old incidents do not accumulate forever.
    assert(!policy.ObserveDeath(quest, true, 200, 11, 0));
    assert(!policy.ObserveDeath(quest, true,
        200 + QuestDeferPolicy::DeathEvidenceWindowTicks + 1, 11, 0));

    QuestDeferPolicy noSafe;
    assert(!noSafe.ObserveNoSafe(quest, 10, 10, 0));
    assert(noSafe.Find(quest)->noSafe == 1);
    assert(!noSafe.IsDeferred(quest));
    assert(noSafe.Find(quest)->reason == QuestDeferReason::None);
    assert(noSafe.Find(quest)->deferTick == 0);
    assert(!noSafe.RevisitEligible(quest, 10000, 60, 100));
    assert(!noSafe.ObserveNoSafe(quest, 11, 10, 0));
    assert(noSafe.Find(quest)->noSafe == 1);
    assert(!noSafe.ObserveNoSafe(quest, 30, 10, 0));
    assert(noSafe.ObserveNoSafe(quest, 50, 10, 0));
    assert(noSafe.Find(quest)->reason ==
        QuestDeferReason::PersistentNoSafeTarget);
    assert(!noSafe.RevisitEligible(quest, 51, 10, 0));
    assert(noSafe.ObserveSafeTarget(quest));
    assert(noSafe.Find(quest)->noSafe == 0);
    assert(noSafe.RevisitEligible(quest, 210, 10, 0));
    assert(noSafe.Revisit(quest, 210, 10, 0));
    assert(!noSafe.IsDeferred(quest));
    assert(!noSafe.ObserveNoSafe(quest, 220, 10, 0));
    assert(!noSafe.ObserveNoSafe(quest, 240, 10, 0));
    assert(!noSafe.ObserveNoSafe(quest, 240 +
        QuestDeferPolicy::NoSafeEvidenceWindowTicks + 1, 10, 0));
    assert(noSafe.Find(quest)->noSafe == 1);

    QuestDeferPolicy failures;
    assert(!MeaningfulObjectiveFailure(nullptr));
    assert(!MeaningfulObjectiveFailure("player_dead"));
    assert(!MeaningfulObjectiveFailure("item_inventory_unavailable"));
    assert(!MeaningfulObjectiveFailure("temporary_spawn_absent"));
    assert(!MeaningfulObjectiveFailure(
        "all configured world-item search points were exhausted without quest completion."));
    assert(!MeaningfulObjectiveFailure("navigation_initialization_pending"));
    assert(MeaningfulObjectiveFailure("all_gameobject_approaches_failed"));
    assert(MeaningfulObjectiveFailure("generic NavMesh follower failed."));
    assert(!failures.ObserveObjectiveFailure(quest, false, 10, 10, 0));
    assert(!failures.ObserveObjectiveFailure(quest, true, 10, 10, 0));
    assert(!failures.ObserveObjectiveFailure(quest, true, 20, 10, 0));
    assert(failures.ObserveObjectiveFailure(quest, true, 30, 10, 0));
    assert(failures.Find(quest)->reason ==
        QuestDeferReason::RepeatedObjectiveFailure);
    assert(failures.ObserveProgress(quest));
    assert(!failures.IsDeferred(quest));
    assert(failures.Find(quest) == nullptr);

    // Live quest identity/progress, not an in-memory defer, remains authority.
    QuestDeferPolicy reconcile;
    assert(!reconcile.ObserveDeath(quest, true, 1, 10, 0));
    assert(reconcile.ObserveDeath(quest, true, 2, 10, 0));
    reconcile.ReconcilePresent({quest});
    assert(reconcile.IsDeferred(quest));
    reconcile.ReconcilePresent({});
    assert(reconcile.Find(quest) == nullptr);

    // Existing score/order is unchanged; the new set only filters current
    // eligibility. A focused deferred quest never spills into another quest.
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
    const auto ordinary = QuestPlanner::Evaluate(snapshot);
    assert(ordinary.action == QuestPlannerAction::ExecuteObjective);
    assert(ordinary.primary != nullptr);
    const int primaryId = ordinary.primary->questId;
    const std::set<int> deferred{primaryId};
    const auto alternate = QuestPlanner::Evaluate(snapshot,
        nullptr, nullptr, 0, &deferred);
    assert(alternate.action == QuestPlannerAction::ExecuteObjective);
    assert(alternate.primary != nullptr);
    assert(alternate.primary->questId != primaryId);
    const auto focused = QuestPlanner::Evaluate(snapshot,
        nullptr, nullptr, primaryId, &deferred);
    assert(focused.action == QuestPlannerAction::Deferred);
    assert(focused.primary == nullptr);
    std::set<int> allDeferred{primaryId, alternate.primary->questId};
    const auto allHeld = QuestPlanner::Evaluate(snapshot,
        nullptr, nullptr, 0, &allDeferred);
    assert(allHeld.action == QuestPlannerAction::Deferred);
    assert(allHeld.primary == nullptr);
    const std::set<int> temporarilyBlocked{primaryId,
        alternate.primary->questId};
    const auto shortHold = QuestPlanner::Evaluate(snapshot,
        &temporarilyBlocked);
    assert(shortHold.action == QuestPlannerAction::TemporarilyBlocked);
    assert(shortHold.primary == nullptr);
    assert(shortHold.unknownActiveTitles.empty());
    first.complete = true;
    snapshot.quests[0] = first;
    const auto turnIn = QuestPlanner::Evaluate(snapshot,
        nullptr, nullptr, 0, &allDeferred);
    assert(turnIn.action == QuestPlannerAction::TurnIn);
    return 0;
}
