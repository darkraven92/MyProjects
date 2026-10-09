#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <cassert>
#include <fstream>
#include <iterator>

int main()
{
    using namespace Bot;
    QuestEligibilityContext fresh;
    fresh.level=1; fresh.raceMask=2; fresh.classMask=1; fresh.activeHistoryComplete=true;
    int checked=0, eligible=0, prerequisite=0, aboveLevel=0, reports=0;
    for (const auto& manual : ValleyOfTrialsProfiles::HandAuthored())
    {
        const auto* source=VanillaQuestDatabase::Instance().SourceProfile(manual.questId);
        assert(source && source->sourceMetadata); // actual TSV, not synthetic IDs
        const auto* node=ValleyOfTrialsProfiles::Graph().Find(manual.questId);
        assert(node && node->structurallyValid);
        const auto& merged=*node->profile;
        assert(merged.minimumLevel==source->minimumLevel);
        assert(merged.requiredRaceMask==source->requiredRaceMask);
        assert(merged.requiredClassMask==source->requiredClassMask);
        assert(merged.requiredCondition==source->requiredCondition);
        assert(merged.prerequisiteAlternatives.size()==source->prerequisiteAlternatives.size());
        assert(merged.previousQuestId==source->previousQuestId);
        assert(merged.objective.type==manual.objective.type);
        assert(merged.objective.targetEntry==manual.objective.targetEntry);
        assert(merged.objective.itemId==manual.objective.itemId);
        assert(merged.objective.objectEntry==manual.objective.objectEntry);
        if (manual.destination.valid) assert(merged.destination.x==manual.destination.x);
        auto result=QuestAcquisitionPolicy::Evaluate(merged,node,fresh);
        eligible+=result==QuestAcquisitionResult::EligibleForPickup;
        prerequisite+=result==QuestAcquisitionResult::PrerequisiteUnknown;
        aboveLevel+=result==QuestAcquisitionResult::WrongLevel;
        if (QuestSourceSemanticPolicy::ReportOnlyObjective(merged))
        {
            ++reports;
            assert(QuestObjectiveDispatchPolicy::Executor(merged)==QuestExecutorKind::TurnIn);
            assert(QuestClassificationPolicy::Classify(merged).executor==QuestExecutorKind::TurnIn);
            assert(QuestObjectiveDispatchPolicy::SupportsAllSteps(merged));
        }
        ++checked;
    }
    assert(checked>0 && eligible>0 && prerequisite>0 && aboveLevel>0 && reports>0);
    // Conflicting restrictions use structured evidence, never generated mechanics.
    auto manual=ValleyOfTrialsProfiles::HandAuthored().front();
    auto source=*VanillaQuestDatabase::Instance().SourceProfile(manual.questId);
    source.minimumLevel=9; source.requiredRaceMask=1; source.requiredClassMask=2;
    source.requiredCondition=17; source.objective.type=QuestObjectiveType::Unknown;
    auto merged=QuestProfileMergePolicy::Merge(manual,source,true);
    assert(merged.minimumLevel==9 && merged.requiredRaceMask==1 && merged.requiredClassMask==2);
    assert(merged.objective.type==manual.objective.type && !merged.metadataConflicts.empty());
    auto classification=QuestClassificationPolicy::Classify(merged);
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::TooLowLevel);
    fresh.level=9;
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::RaceMismatch);
    fresh.raceMask=1;
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::ClassMismatch);
    fresh.classMask=2;
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::UnknownRestrictions);
    fresh.liveOffer=true;
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::Eligible);
    fresh.liveOffer=false; merged.requiredCondition=0;
    merged.prerequisiteAlternatives={{false,true,{100001}}};
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::PrerequisiteUnknown);
    fresh.knownMissingPrerequisites.insert(100001);
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::MissingPrerequisite);
    fresh.completed.insert(100001);
    assert(QuestEligibilityPolicy::Evaluate(merged,classification,fresh)==QuestEligibility::Eligible);
    // Plain report proof must not become a general gossip-credit bypass.
    assert(QuestSourceSemanticPolicy::ReportOnlyObjective(merged));
    merged.sourceObjectiveCount=1; assert(!QuestSourceSemanticPolicy::ReportOnlyObjective(merged));
    merged.sourceObjectiveCount=0; merged.sourceMetadata->specialFlags=2;
    assert(!QuestSourceSemanticPolicy::ReportOnlyObjective(merged));
    merged.sourceMetadata->specialFlags=0; merged.sourceMetadata->completeScript=7;
    assert(!QuestSourceSemanticPolicy::ReportOnlyObjective(merged));
    std::ifstream runtime("src/Bot/QuestPlannerRuntimeController.h");
    const std::string text{std::istreambuf_iterator<char>(runtime),{}};
    assert(text.find("QuestObjectiveDispatchPolicy::Executor(*plan_.primary) == QuestExecutorKind::TurnIn")!=std::string::npos);
}
