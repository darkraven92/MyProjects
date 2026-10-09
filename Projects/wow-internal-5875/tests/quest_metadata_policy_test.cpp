#include "../src/Bot/QuestGraph.h"
#include "../src/Bot/VanillaQuestDatabase.h"
#include <cassert>
int main()
{
    using namespace Bot;
    QuestProfile p;
    p.questId=10001; p.title="metadata test";
    p.objective={QuestObjectiveType::KillMob, 17, 0, 0, 1, "fixture"};
    p.destination={true,1,1,2,3,5,"fixture"};
    p.relationshipMetadataKnown=true;
    p.prerequisiteAlternatives={{false,true,{10002,10003}}, {true,true,{10004}}};
    QuestEligibilityContext c; c.level=20;
    auto eval=[&]{ return QuestEligibilityPolicy::Evaluate(p, QuestClassificationPolicy::Classify(p),c); };
    assert(eval()==QuestEligibility::PrerequisiteUnknown);
    c.completed={10002}; assert(eval()==QuestEligibility::PrerequisiteUnknown);
    c.completed.insert(10003); assert(eval()==QuestEligibility::Eligible);
    c.completed.clear(); c.activeQuestIds.insert(10004); assert(eval()==QuestEligibility::Eligible);
    p.exclusivePeers={10005}; c.activeQuestIds.insert(10005); assert(eval()==QuestEligibility::ExclusiveConflict);
    c.activeQuestIds.clear(); c.historyComplete=true; c.activeHistoryComplete=true; assert(eval()==QuestEligibility::MissingPrerequisite);
    p.prerequisiteAlternatives[0].resolved=false; assert(eval()==QuestEligibility::PrerequisiteUnknown);
    c.liveOffer=true; assert(eval()==QuestEligibility::Eligible);
    // SQL -> SQLite -> generated TSV -> actual runtime loader coverage.
    bool enriched=false, hasCondition=false, hasGroup=false, hasArea=false, hasAmbiguity=false;
    for(const auto& row : VanillaQuestDatabase::Instance().Profiles())
    {
        if(!row.sourceMetadata) continue;
        enriched=true;
        assert(row.requiredRaceMask && row.requiredClassMask && row.requiredCondition);
        assert(row.sourceMetadata->method && row.sourceMetadata->flags && row.sourceMetadata->specialFlags);
        assert(row.sourceMetadata->requiredSkill && row.sourceMetadata->requiredSkillValue);
        assert(row.sourceMetadata->startScript && row.sourceMetadata->completeScript);
        assert(row.sourceMetadata->timeLimit && row.sourceMetadata->reputationObjective);
        assert(row.relationshipMetadataKnown);
        assert(!row.sourceMetadata->breadcrumb); // absent from retained SQL schema
        hasCondition |= *row.requiredCondition!=0;
        hasGroup |= row.sourceMetadata->exclusiveGroup && *row.sourceMetadata->exclusiveGroup!=0;
        hasArea |= !row.areaTriggers.empty();
        if(row.semanticAmbiguous)
        {
            hasAmbiguity=true;
            assert(QuestClassificationPolicy::Classify(row).support==QuestRuntimeSupport::Ambiguous);
        }
    }
    assert(enriched && hasGroup && hasArea && hasAmbiguity);
    assert(!hasCondition); // current pack has zero condition IDs; fixture covers nonzero
}
