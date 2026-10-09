#pragma once
#include "QuestGraph.h"
#include "ObjectiveAnchorSelectionPolicy.h"
#include <cmath>

namespace Bot
{
    enum class QuestAcquisitionResult
    {
        EligibleForPickup, AlreadyActive, KnownCompleted, PrerequisiteUnknown,
        MissingPrerequisite, WrongLevel, WrongRace, WrongClass, UnknownRestrictions,
        UnsupportedObjective, AmbiguousObjective, MissingGiver, MissingGiverLocation,
        DiscoveryBackoff, InvalidGraph, ExclusiveConflict, UnsupportedActor
    };
    struct QuestAcquisitionPolicy
    {
        // Local UI confirmation, not a navigation/recovery budget.
        static constexpr std::uint64_t AcceptanceVerificationTicks=40;
        static constexpr std::uint64_t ReauditTicks=900;
        static std::optional<std::uint32_t> RaceMask(const std::string& token)
        {
            const char* races[]={"Human","Orc","Dwarf","NightElf","Scourge","Tauren","Gnome","Troll"};
            for(std::uint32_t i=0;i<8;++i) if(token==races[i]) return 1u<<i;
            return std::nullopt;
        }
        static bool ValidDestination(const ObjectiveDestination& d)
        { return d.valid && std::isfinite(d.x) && std::isfinite(d.y) && std::isfinite(d.z); }
        static QuestAcquisitionResult Evaluate(const QuestProfile& p, const QuestGraphNode* node,
            QuestEligibilityContext context, bool backoff=false, bool visibleMatchingActor=false)
        {
            using R=QuestAcquisitionResult;
            if(!node || !node->structurallyValid) return R::InvalidGraph;
            if(context.active || context.activeQuestIds.count(p.questId)) return R::AlreadyActive;
            if(context.completed.count(p.questId)) return R::KnownCompleted;
            if(backoff) return R::DiscoveryBackoff;
            if(p.giverIsGameObject || p.turnInIsGameObject) return R::UnsupportedActor;
            if(!p.giverEntry) return R::MissingGiver;
            if(!visibleMatchingActor && !ValidDestination(p.giverDestination)) return R::MissingGiverLocation;
            // A live offer can resolve incomplete history, but must not bypass
            // a known conflict after accepting another offer from this actor.
            if(context.liveOffer)
            {
                if(context.level<p.minimumLevel ||
                   (p.maximumLevel && *p.maximumLevel>0 && context.level>*p.maximumLevel)) return R::WrongLevel;
                if(p.requiredRaceMask && *p.requiredRaceMask && context.raceMask &&
                   !(*p.requiredRaceMask & *context.raceMask)) return R::WrongRace;
                if(p.requiredClassMask && *p.requiredClassMask && context.classMask &&
                   !(*p.requiredClassMask & *context.classMask)) return R::WrongClass;
                if(p.relationshipMetadataKnown)
                    for(int peer:p.exclusivePeers)
                        if(context.activeQuestIds.count(peer) || context.completed.count(peer))
                            return R::ExclusiveConflict;
            }
            switch(QuestEligibilityPolicy::Evaluate(p,node->classification,context))
            {
                case QuestEligibility::Eligible:return R::EligibleForPickup;
                case QuestEligibility::AlreadyActive:return R::AlreadyActive;
                case QuestEligibility::Completed:return R::KnownCompleted;
                case QuestEligibility::PrerequisiteUnknown:return R::PrerequisiteUnknown;
                case QuestEligibility::MissingPrerequisite:return R::MissingPrerequisite;
                case QuestEligibility::TooLowLevel:case QuestEligibility::TooHighLevel:return R::WrongLevel;
                case QuestEligibility::RaceMismatch:case QuestEligibility::FactionMismatch:return R::WrongRace;
                case QuestEligibility::ClassMismatch:return R::WrongClass;
                case QuestEligibility::AmbiguousMetadata:return R::AmbiguousObjective;
                case QuestEligibility::Unsupported:case QuestEligibility::NonExecutable:return R::UnsupportedObjective;
                case QuestEligibility::ExclusiveConflict:return R::ExclusiveConflict;
                case QuestEligibility::Deferred:case QuestEligibility::TemporarilyBlocked:return R::DiscoveryBackoff;
                default:return R::UnknownRestrictions;
            }
        }
        static const char* Name(QuestAcquisitionResult r)
        {
            switch(r)
            {
#define ACQ_NAME(x) case QuestAcquisitionResult::x:return #x;
                ACQ_NAME(EligibleForPickup) ACQ_NAME(AlreadyActive) ACQ_NAME(KnownCompleted)
                ACQ_NAME(PrerequisiteUnknown) ACQ_NAME(MissingPrerequisite) ACQ_NAME(WrongLevel)
                ACQ_NAME(WrongRace) ACQ_NAME(WrongClass) ACQ_NAME(UnknownRestrictions)
                ACQ_NAME(UnsupportedObjective) ACQ_NAME(AmbiguousObjective) ACQ_NAME(MissingGiver)
                ACQ_NAME(MissingGiverLocation) ACQ_NAME(DiscoveryBackoff) ACQ_NAME(InvalidGraph)
                ACQ_NAME(ExclusiveConflict) ACQ_NAME(UnsupportedActor)
#undef ACQ_NAME
            }
            return "InvalidGraph";
        }
        static ObjectiveDestination SelectDestination(const ObjectiveDestination& primary,
            const std::vector<ObjectiveDestination>& candidates, float x,float y,float z)
        {
            if(candidates.empty()) return primary;
            QuestProfile proxy; proxy.destination=primary; proxy.searchDestinations=candidates;
            const auto selected=ObjectiveAnchorSelectionPolicy::Select(proxy,x,y,z);
            return selected.valid?candidates[selected.index]:ObjectiveDestination{};
        }
        static bool Better(const QuestProfile& a,const QuestProfile& b)
        { return a.priority!=b.priority?a.priority>b.priority:a.questId<b.questId; }
        static bool AcceptanceExpired(std::uint64_t tick,std::uint64_t issued)
        { return tick>=issued && tick-issued>=AcceptanceVerificationTicks; }
        static bool ReauditDue(bool sweepComplete,bool idle,bool safeBoundary,std::uint64_t tick,
            std::uint64_t due,int oldLevel,int level)
        { return sweepComplete && idle && safeBoundary && ((due!=0 && tick>=due) || (oldLevel!=0 && level>oldLevel)); }
        static bool TitleStillPresent(const QuestProfile& p,const QuestPlannerSnapshot& snapshot)
        {
            for(const auto& live:snapshot.quests) if(p.title && live.title==p.title) return true;
            return false;
        }
        static bool VerifiedAcceptance(const QuestProfile& p,const PlannerQuestLogEntry& live,
            std::uint32_t giver,bool issued,bool absentBefore)
        {
            return issued && absentBefore && giver==p.giverEntry && p.title && live.title==p.title &&
                (p.expectedObjectiveCount<0 || live.objectiveCount==p.expectedObjectiveCount);
        }
        static bool VerifiedTurnIn(bool validSnapshot,bool absent,bool rewardAction)
        { return validSnapshot && absent && rewardAction; }
    };
}
