#include "../src/Bot/QuestOfferResolutionPolicy.h"
#include "../src/Bot/QuestGiverOfferCycle.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <cassert>

int main()
{
    using namespace Bot;
    using R=QuestAcquisitionResult;
    QuestProfile first; first.questId=10001; first.title="Shared offer";
    first.minimumLevel=1; first.giverEntry=17; first.turnInEntry=18;
    first.objective={QuestObjectiveType::KillMob,19,0,0,1,"fixture"};
    first.destination={true,1,1,2,3,5,"fixture"}; first.giverDestination=first.destination;
    first.expectedObjectiveCount=1;
    auto followup=first; followup.questId=10002; followup.previousQuestId=first.questId;
    QuestEligibilityContext context; context.level=4; context.classMask=1; context.raceMask=2;
    context.liveOffer=true; // Deliberately prove that the UI cannot supply missing history.
    auto eval=[&](const QuestProfile& p) {
        QuestGraphNode node{&p,QuestClassificationPolicy::Classify(p),true};
        return QuestOfferResolutionPolicy::Evaluate(p,&node,context);
    };
    assert(eval(first)==R::EligibleForPickup);
    assert(eval(followup)==R::PrerequisiteUnknown);
    QuestOfferResolutionPolicy resolution;
    resolution.Observe(first,eval(first)); resolution.Observe(followup,eval(followup));
    assert(resolution.candidateCount==2 && resolution.eligibleCount==1 && resolution.Resolved()==&first);
    assert(context.completed.empty()); // Excluding unknown automation work invents no completion history.
    context.historyComplete=true; assert(eval(followup)==R::MissingPrerequisite);
    context.completed.insert(first.questId);
    assert(eval(first)==R::KnownCompleted && eval(followup)==R::EligibleForPickup);
    resolution={}; resolution.Observe(first,eval(first)); resolution.Observe(followup,eval(followup));
    assert(resolution.Resolved()==&followup);
    context.completed.clear(); context.historyComplete=false; followup.previousQuestId=0;
    resolution={}; resolution.Observe(first,eval(first)); resolution.Observe(followup,eval(followup));
    assert(resolution.eligibleCount==2 && !resolution.Resolved());
    assert(!QuestOfferResolutionPolicy::ConfirmEmpty(2));
    assert(!QuestOfferResolutionPolicy::ConfirmEmpty(1));
    assert(QuestOfferResolutionPolicy::ConfirmEmpty(0));
    assert(!QuestOfferResolutionPolicy::RestoreEmptyCache(0));
    assert(QuestOfferResolutionPolicy::RestoreEmptyCache(QuestOfferResolutionPolicy::EmptyCacheVersion));
    followup.previousQuestId=10003;
    resolution={}; resolution.Observe(followup,eval(followup)); assert(!resolution.Resolved());
    context.activeQuestIds.insert(first.questId); assert(eval(first)==R::AlreadyActive);
    context.activeQuestIds.clear(); first.requiredClassMask=2; assert(eval(first)==R::WrongClass);
    first.requiredClassMask=1; first.requiredRaceMask=1; assert(eval(first)==R::WrongRace);
    first.requiredRaceMask=2; context.level=0; assert(eval(first)==R::WrongLevel); context.level=4;
    first.requiredCondition=1; assert(eval(first)==R::UnknownRestrictions); first.requiredCondition=0;
    first.relationshipMetadataKnown=true; first.exclusivePeers={10004};
    context.activeQuestIds.insert(10004); assert(eval(first)==R::ExclusiveConflict);
    context.activeQuestIds.clear();
    resolution={}; resolution.Observe(first,eval(first),false); assert(!resolution.Resolved()); // focus/scope
    PlannerQuestLogEntry live{"Shared offer",false,1,{false}};
    assert(!QuestAcquisitionPolicy::VerifiedAcceptance(first,live,17,false,true));
    assert(QuestAcquisitionPolicy::VerifiedAcceptance(first,live,17,true,true));
    live.title="Other"; assert(!QuestAcquisitionPolicy::VerifiedAcceptance(first,live,17,true,true));

    // Real loaded metadata, not generic quest-specific runtime behavior.
    const QuestProfile *starter=nullptr,*report=nullptr;
    for(const auto& p:ValleyOfTrialsProfiles::All())
    { if(p.questId==790) starter=&p; if(p.questId==804) report=&p; }
    assert(starter && report && std::string(starter->title)==report->title && starter->giverEntry==report->giverEntry);
    assert(eval(*starter)==R::EligibleForPickup && eval(*report)==R::PrerequisiteUnknown);
    context.completed.insert(starter->questId);
    assert(eval(*starter)==R::KnownCompleted && eval(*report)==R::EligibleForPickup);
}
