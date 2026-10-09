#include "../src/Bot/QuestClassificationPolicy.h"
#include "../src/Bot/QuestProfileMergePolicy.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace Bot;
    QuestProfile p;
    p.questId = 12345; p.title = "Synthetic";
    p.objective = {QuestObjectiveType::KillMob, 50, 0, 0, 3, "target"};
    p.destination = {true, 1, 1, 2, 3, 5, "fixture"};
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::KnownExecutable);
    p.objective.type = QuestObjectiveType::CollectItemFromMob;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::MissingData);
    p.objective.itemId = 60;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::KnownExecutable);
    p.objective.type = QuestObjectiveType::CollectWorldItem; p.objective.objectEntry = 70;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::KnownExecutable);
    p.objective.itemId = 0;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::MissingData);
    p.objective.itemId = 60;
    auto inferredReport = p;
    inferredReport.databaseDerived = true;
    inferredReport.objective.type = QuestObjectiveType::TravelReport;
    inferredReport.turnInEntry = 80;
    assert(QuestClassificationPolicy::Classify(inferredReport).support == QuestRuntimeSupport::Ambiguous);
    inferredReport.handwrittenOverride = true;
    assert(QuestClassificationPolicy::Classify(inferredReport).support == QuestRuntimeSupport::KnownExecutable);
    p.objective.type = QuestObjectiveType::ExploreOrAreaTrigger;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::KnownSemanticButUnsupported);
    p.objective.type = QuestObjectiveType::Unknown;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::MissingData);
    p.semanticAmbiguous = true;
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::Ambiguous);
    p.semanticAmbiguous = false; p.objective.type = QuestObjectiveType::KillMob;
    auto manual = p;
    manual.destination.valid = false;
    auto merged = QuestProfileMergePolicy::Merge(manual, p, true);
    assert(merged.destination.valid && merged.handwrittenOverride);
    p.objective.targetEntry = 99;
    merged = QuestProfileMergePolicy::Merge(manual, p, true);
    assert(merged.objective.targetEntry == 50 && !merged.destination.valid);
    assert(!merged.metadataConflicts.empty());
    p.semanticAmbiguous = true;
    merged = QuestProfileMergePolicy::Merge(manual, p, true);
    assert(!merged.semanticAmbiguous && merged.objective.targetEntry == 50);
    merged = QuestProfileMergePolicy::Merge(manual, p, false);
    assert(merged.semanticAmbiguous && merged.objective.targetEntry == 99);
    p.semanticAmbiguous = false;
    p.destination.z = std::numeric_limits<float>::quiet_NaN();
    assert(QuestClassificationPolicy::Classify(p).support == QuestRuntimeSupport::MissingData);
    // Every executable catalogue classification must actually dispatch.
    for (const auto& profile : ValleyOfTrialsProfiles::All())
        if (QuestClassificationPolicy::Classify(profile).support == QuestRuntimeSupport::KnownExecutable)
            assert(QuestObjectiveDispatchPolicy::SupportsAllSteps(profile));
}
