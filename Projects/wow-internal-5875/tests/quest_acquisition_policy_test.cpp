#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <cassert>
int main()
{
    using namespace Bot;
    using R=QuestAcquisitionResult;
    QuestProfile p; p.questId=10001; p.title="Fixture"; p.minimumLevel=3;
    p.giverEntry=17; p.turnInEntry=18;
    p.objective={QuestObjectiveType::KillMob,19,0,0,1,"fixture"};
    p.destination={true,1,1,2,3,5,"fixture"}; p.giverDestination=p.destination;
    QuestGraphNode n{&p,QuestClassificationPolicy::Classify(p),true};
    QuestEligibilityContext c; c.level=10; c.raceMask=2; c.classMask=1;
    auto eval=[&]{n.classification=QuestClassificationPolicy::Classify(p);return QuestAcquisitionPolicy::Evaluate(p,&n,c);};
    assert(eval()==R::EligibleForPickup);
    c.active=true; assert(eval()==R::AlreadyActive); c.active=false;
    c.completed.insert(p.questId); assert(eval()==R::KnownCompleted); c.completed.clear();
    p.previousQuestId=10002; assert(eval()==R::PrerequisiteUnknown);
    c.historyComplete=true; assert(eval()==R::MissingPrerequisite);
    c.completed.insert(10002); assert(eval()==R::EligibleForPickup); p.previousQuestId=0;
    c.level=2; assert(eval()==R::WrongLevel); c.level=10;
    p.requiredRaceMask=1; assert(eval()==R::WrongRace); p.requiredRaceMask=2;
    p.requiredClassMask=2; assert(eval()==R::WrongClass); p.requiredClassMask=1;
    c.raceMask.reset(); assert(eval()==R::UnknownRestrictions); c.raceMask=2;
    p.semanticAmbiguous=true; assert(eval()==R::AmbiguousObjective); p.semanticAmbiguous=false;
    p.giverEntry=0; assert(eval()==R::MissingGiver); p.giverEntry=17;
    p.giverDestination.valid=false; assert(eval()==R::MissingGiverLocation); p.giverDestination.valid=true;
    p.giverIsGameObject=true; assert(eval()==R::UnsupportedActor); p.giverIsGameObject=false;
    n.structurallyValid=false; assert(eval()==R::InvalidGraph); n.structurallyValid=true;
    assert(QuestAcquisitionPolicy::Evaluate(p,&n,c,true)==R::DiscoveryBackoff);
    p.objective.type=QuestObjectiveType::TalkToNpc; assert(eval()==R::UnsupportedObjective); p.objective.type=QuestObjectiveType::KillMob;
    p.relationshipMetadataKnown=true; p.prerequisiteAlternatives={{true,true,{10003}}};
    assert(eval()==R::PrerequisiteUnknown); c.activeQuestIds.insert(10003); assert(eval()==R::EligibleForPickup);
    p.exclusivePeers={10004}; c.activeQuestIds.insert(10004); assert(eval()==R::ExclusiveConflict);
    c.liveOffer=true; assert(eval()==R::ExclusiveConflict);
    p.exclusivePeers.clear();
    p.requiredClassMask=2; assert(eval()==R::WrongClass); p.requiredClassMask=1;
    p.requiredRaceMask=1; assert(eval()==R::WrongRace); p.requiredRaceMask=2;
    c.level=2; assert(eval()==R::WrongLevel); c.level=10;
    c.liveOffer=false;
    p.exclusivePeers.clear(); p.prerequisiteAlternatives.clear();
    auto other=p; other.questId++; assert(QuestAcquisitionPolicy::Better(p,other));
    other.priority++; assert(QuestAcquisitionPolicy::Better(other,p));
    std::vector<ObjectiveDestination> anchors={p.giverDestination,p.giverDestination};
    anchors[0].x=100; anchors[1].x=4;
    assert(QuestAcquisitionPolicy::SelectDestination(p.giverDestination,anchors,0,2,3).x==4);
    anchors[1].mapId=0; assert(QuestAcquisitionPolicy::SelectDestination(p.giverDestination,anchors,0,2,3).x==100);
    p.expectedObjectiveCount=1;
    PlannerQuestLogEntry live{"Fixture",false,1,{false}};
    assert(QuestAcquisitionPolicy::VerifiedAcceptance(p,live,17,true,true));
    assert(!QuestAcquisitionPolicy::VerifiedAcceptance(p,live,17,false,true));
    assert(!QuestAcquisitionPolicy::VerifiedAcceptance(p,live,17,true,false));
    assert(!QuestAcquisitionPolicy::VerifiedAcceptance(p,live,18,true,true));
    live.title="Other"; assert(!QuestAcquisitionPolicy::VerifiedAcceptance(p,live,17,true,true));
    assert(!QuestAcquisitionPolicy::AcceptanceExpired(39,0)); assert(QuestAcquisitionPolicy::AcceptanceExpired(40,0));
    assert(!QuestAcquisitionPolicy::VerifiedTurnIn(true,true,false));
    assert(!QuestAcquisitionPolicy::VerifiedTurnIn(false,true,true));
    assert(!QuestAcquisitionPolicy::VerifiedTurnIn(true,false,true));
    assert(QuestAcquisitionPolicy::VerifiedTurnIn(true,true,true));
    assert(QuestAcquisitionPolicy::RaceMask("Orc")==2);
    assert(!QuestAcquisitionPolicy::RaceMask("UNKNOWN"));
    assert(!QuestAcquisitionPolicy::ReauditDue(true,true,true,899,900,20,20));
    assert(QuestAcquisitionPolicy::ReauditDue(true,true,true,900,900,20,20));
    assert(!QuestAcquisitionPolicy::ReauditDue(true,true,false,900,900,20,20));
    assert(!QuestAcquisitionPolicy::ReauditDue(true,false,true,900,900,20,20));
    assert(QuestAcquisitionPolicy::ReauditDue(true,true,true,200,900,20,21));
    QuestPlannerSnapshot snapshot; snapshot.valid=true;
    snapshot.quests={{"Fixture",false,999,{}}};
    assert(QuestAcquisitionPolicy::TitleStillPresent(p,snapshot)); // mismatching rows are NOT removal
    snapshot.quests.clear(); assert(!QuestAcquisitionPolicy::TitleStillPresent(p,snapshot));
    for(const auto& row:VanillaQuestDatabase::Instance().Profiles())
        assert(row.giverDestinations.size()<=8 && row.turnInDestinations.size()<=8);
}
