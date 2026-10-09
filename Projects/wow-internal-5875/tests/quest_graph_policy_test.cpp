#include "../src/Bot/QuestGraph.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <cassert>

int main()
{
    using namespace Bot;
    QuestProfile p;
    p.questId = 10001; p.title = "graph fixture";
    p.minimumLevel = 10; p.questLevel = 12;
    p.objective = {QuestObjectiveType::KillMob, 17, 0, 0, 2, "creature"};
    p.destination = {true, 1, 1, 2, 3, 5, "fixture"};
    QuestEligibilityContext c; c.level = 10;
    const auto classify = [&](const QuestProfile& profile) { return QuestClassificationPolicy::Classify(profile); };
    const auto evaluate = [&]() { return QuestEligibilityPolicy::Evaluate(p, classify(p), c); };
    assert(evaluate() == QuestEligibility::Eligible);
    c.level = 9; assert(evaluate() == QuestEligibility::TooLowLevel); c.level = 10;
    p.requiredRaceMask = 2; c.raceMask = 2; assert(evaluate() == QuestEligibility::Eligible);
    c.raceMask = 1; assert(evaluate() == QuestEligibility::RaceMismatch);
    c.raceMask.reset(); assert(evaluate() == QuestEligibility::UnknownRestrictions); c.raceMask = 2;
    p.requiredClassMask = 1; c.classMask = QuestEligibilityPolicy::ClassMask("WARRIOR");
    assert(evaluate() == QuestEligibility::Eligible);
    c.classMask = 2; assert(evaluate() == QuestEligibility::ClassMismatch); c.classMask = 1;
    p.requiredFactionMask = 2; c.factionMask = 1;
    assert(evaluate() == QuestEligibility::FactionMismatch); c.factionMask = 2;
    p.previousQuestId = 10000;
    assert(evaluate() == QuestEligibility::PrerequisiteUnknown);
    c.knownMissingPrerequisites.insert(10000); assert(evaluate() == QuestEligibility::MissingPrerequisite);
    c.completed.insert(10000); assert(evaluate() == QuestEligibility::Eligible);
    c.exclusiveConflict = true; assert(evaluate() == QuestEligibility::ExclusiveConflict); c.exclusiveConflict = false;
    c.active = true; c.level = 1; c.completed.clear();
    assert(evaluate() == QuestEligibility::AlreadyActive); // live beats static prerequisites/level
    c.blocked = true; assert(evaluate() == QuestEligibility::TemporarilyBlocked);
    c.deferred = true; assert(evaluate() == QuestEligibility::Deferred);
    c.liveComplete = true; assert(evaluate() == QuestEligibility::ReadyForTurnIn);
    c = {}; c.completed.insert(p.questId); assert(evaluate() == QuestEligibility::Completed);
    c = {}; c.liveOffer = true; assert(evaluate() == QuestEligibility::Eligible);
    p.objective.type = QuestObjectiveType::TalkToNpc; assert(evaluate() == QuestEligibility::Unsupported);
    p.objective.type = QuestObjectiveType::KillMob; p.destination.valid = false;
    assert(evaluate() == QuestEligibility::NonExecutable);
    p.semanticAmbiguous = true; assert(evaluate() == QuestEligibility::AmbiguousMetadata);
    p.semanticAmbiguous = false; p.destination.valid = true;
    p.previousQuestId = 0; p.nextInChainQuestId = 10002;
    auto second = p; second.questId = 10002; second.previousQuestId = p.questId; second.nextInChainQuestId = 0;
    std::vector<QuestProfile> profiles{p, second};
    QuestGraph graph(profiles);
    assert(graph.Nodes().size() == 2 && graph.Edges().size() == 2 && graph.Issues().empty());
    assert(graph.Find(p.questId)->classification.support == QuestRuntimeSupport::KnownExecutable);
    profiles[0].previousQuestId = second.questId;
    QuestGraph cycle(profiles);
    assert(!cycle.Issues().empty());
    assert(!cycle.Find(p.questId)->structurallyValid);
    assert(!cycle.Find(second.questId)->structurallyValid);
    profiles[0].previousQuestId = p.questId;
    QuestGraph self(profiles); assert(!self.Issues().empty());
    profiles[0].previousQuestId = 99999; profiles[1].requiredRaceMask = 0x100;
    QuestGraph bad(profiles); assert(bad.Issues().size() >= 2);
    assert(bad.Find(second.questId)->classification.support == QuestRuntimeSupport::Ambiguous);
    profiles = {p, p};
    QuestGraph duplicate(profiles);
    assert(!duplicate.Find(p.questId)->structurallyValid);
    profiles = {p}; profiles[0].minimumLevel = 61;
    QuestGraph badLevel(profiles);
    assert(!badLevel.Find(p.questId)->structurallyValid);
    QuestGraph catalogue(ValleyOfTrialsProfiles::All());
    assert(!catalogue.Nodes().empty());
    // The new optional M records round-trip through the real TSV loader.
    for (const auto& profile : VanillaQuestDatabase::Instance().Profiles())
    {
        assert(profile.requiredRaceMask.has_value());
        assert(profile.requiredClassMask.has_value());
        assert(profile.requiredCondition.has_value());
        assert(profile.zoneOrSort.has_value());
        assert(profile.nextQuestId.has_value());
        assert(!profile.requiredFactionMask.has_value()); // not in this data pack
    }
}
