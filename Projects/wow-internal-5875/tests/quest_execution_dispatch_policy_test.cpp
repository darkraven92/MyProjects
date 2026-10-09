#include "../src/Bot/QuestPlanner.h"

#include <cassert>
#include <limits>
#include <string>

using namespace Bot;

int main()
{
    using Kind = QuestFocusConfigurationKind;
    const auto multi = QuestFocusPolicy::ParseConfiguration("0");
    assert(multi.configured && multi.kind == Kind::Multiquest &&
        multi.questId == 0);
    assert(QuestFocusPolicy::ParseConfiguration(nullptr).kind ==
        Kind::Multiquest);
    const auto focused = QuestFocusPolicy::ParseConfiguration("826");
    assert(focused.kind == Kind::Focused && focused.questId == 826);
    assert(QuestFocusPolicy::ParseConfiguration("").kind == Kind::Invalid);
    assert(QuestFocusPolicy::ParseConfiguration("-1").kind == Kind::Invalid);
    assert(QuestFocusPolicy::ParseConfiguration("826x").kind == Kind::Invalid);
    assert(QuestFocusPolicy::ParseConfiguration("99999999999999999999").kind ==
        Kind::Invalid);

    const QuestProfile* killProfile = nullptr;
    const QuestProfile* collectionProfile = nullptr;
    const QuestProfile* nonExecutableProfile = nullptr;
    for (const auto& profile : ValleyOfTrialsProfiles::All())
    {
        if (profile.questId == 826) killProfile = &profile;
        if (profile.questId == 903) collectionProfile = &profile;
        if (profile.questId == 4641) nonExecutableProfile = &profile;
    }
    assert(killProfile != nullptr && collectionProfile != nullptr &&
        nonExecutableProfile != nullptr);
    // The real starter report now has structured proof. Preserve the negative
    // regression using a dialogue fixture deliberately lacking that proof.
    auto unproven = *nonExecutableProfile;
    unproven.questId = 100001;
    unproven.title = "Unproven dialogue fixture";
    unproven.sourceObjectiveCount.reset();
    unproven.sourceMetadata.reset();
    std::vector<QuestProfile> catalogue{*killProfile, *collectionProfile, unproven};
    killProfile = &catalogue[0]; collectionProfile = &catalogue[1];
    nonExecutableProfile = &catalogue[2];
    assert(killProfile->support == QuestExecutionSupport::ProfiledPlannerOnly);
    assert(killProfile->objectives.size() == 3);
    assert(QuestObjectiveDispatchPolicy::SupportsStep(*killProfile, 0));
    assert(QuestObjectiveDispatchPolicy::SupportsAllSteps(*killProfile));
    assert(QuestObjectiveDispatchPolicy::SupportsAllSteps(*collectionProfile));
    assert(!QuestObjectiveDispatchPolicy::SupportsStep(
        *nonExecutableProfile, -1));
    assert(!QuestObjectiveDispatchPolicy::SupportsAllSteps(
        *nonExecutableProfile));

    QuestPlannerSnapshot snapshot{};
    snapshot.valid = true;
    snapshot.classToken = "WARRIOR";
    snapshot.playerLevel = 18;
    PlannerQuestLogEntry unsupported{};
    unsupported.title = nonExecutableProfile->title;
    unsupported.objectiveCount = 0;
    snapshot.quests.push_back(unsupported);
    PlannerQuestLogEntry executable{};
    executable.title = killProfile->title;
    executable.objectiveCount = 3;
    executable.objectiveComplete = {false, false, false};
    snapshot.quests.push_back(executable);

    auto evaluate = [&](int focus = 0) {
        return QuestPlanner::Evaluate(snapshot, nullptr, nullptr, focus, nullptr, &catalogue);
    };
    auto plan = evaluate();
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == killProfile);
    assert(plan.activeObjectiveIndex == 0);
    assert(plan.nonExecutableQuestIds.size() == 1);
    assert(plan.nonExecutableQuestIds[0] == nonExecutableProfile->questId);
    assert(plan.waveDeferredCount == 0);

    plan = evaluate(nonExecutableProfile->questId);
    assert(plan.action == QuestPlannerAction::UnsupportedActiveQuest);
    assert(plan.primary == nullptr);
    assert(plan.waveDeferredCount == 0);

    plan = evaluate(killProfile->questId);
    assert(plan.action == QuestPlannerAction::ExecuteObjective);
    assert(plan.primary == killProfile);

    snapshot.quests[1].complete = true;
    plan = evaluate(killProfile->questId);
    assert(plan.action == QuestPlannerAction::TurnIn);
    assert(plan.primary == killProfile);
}
