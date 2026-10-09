#pragma once

#include "MultiLocationItemUsePolicy.h"
#include "QuestPlannerTypes.h"
#include "SummonedQuestObjectivePolicy.h"
#include "TalkToNpcPolicy.h"
#include "ExploreObjectivePolicy.h"
#include "QuestSourceSemanticPolicy.h"

namespace Bot
{
    enum class QuestExecutorKind
    {
        None, Mob, WorldItem, GameObject, ItemOnUnit, Summoned, MultiLocation, TurnIn, Talk, Explore
    };
    // Single source for planner classification AND executor Supports contracts.
    // ProfiledPlannerOnly describes verification status, not dispatchability.
    struct QuestObjectiveDispatchPolicy
    {
        static QuestExecutorKind Executor(QuestObjectiveType type)
        {
            switch (type)
            {
                case QuestObjectiveType::TalkToNpc: return QuestExecutorKind::Talk;
                case QuestObjectiveType::ExploreOrAreaTrigger: return QuestExecutorKind::Explore;
                case QuestObjectiveType::KillMob:
                case QuestObjectiveType::CollectItemFromMob: return QuestExecutorKind::Mob;
                case QuestObjectiveType::CollectWorldItem: return QuestExecutorKind::WorldItem;
                case QuestObjectiveType::InteractGameObject: return QuestExecutorKind::GameObject;
                case QuestObjectiveType::UseItemOnUnit: return QuestExecutorKind::ItemOnUnit;
                case QuestObjectiveType::UseQuestItemAtLocation: return QuestExecutorKind::Summoned;
                case QuestObjectiveType::UseItemAtGameObject: return QuestExecutorKind::MultiLocation;
                case QuestObjectiveType::TravelReport: return QuestExecutorKind::TurnIn;
                default: return QuestExecutorKind::None;
            }
        }

        static const char* ExecutorName(QuestExecutorKind executor)
        {
            switch (executor)
            {
                case QuestExecutorKind::Talk: return "TalkToNpc";
                case QuestExecutorKind::Explore: return "ExploreOrAreaTrigger";
                case QuestExecutorKind::Mob: return "CollectItemFromMob";
                case QuestExecutorKind::WorldItem: return "CollectWorldItem";
                case QuestExecutorKind::GameObject: return "InteractGameObject";
                case QuestExecutorKind::ItemOnUnit: return "UseItemOnUnit";
                case QuestExecutorKind::Summoned: return "SummonedQuestObjective";
                case QuestExecutorKind::MultiLocation: return "MultiLocationItemUse";
                case QuestExecutorKind::TurnIn: return "GenericQuestTurnIn";
                default: return "none";
            }
        }

        static QuestExecutorKind Executor(const QuestProfile& profile)
        {
            return QuestSourceSemanticPolicy::ReportOnlyObjective(profile)
                ? QuestExecutorKind::TurnIn : Executor(profile.objective.type);
        }

        static bool SupportsStep(const QuestProfile& base, int objectiveIndex)
        {
            const QuestProfile profile =
                MaterializeObjectiveStep(base, objectiveIndex);
            return SupportsMaterialized(profile);
        }

        static bool SupportsMaterialized(const QuestProfile& profile)
        {
            if (QuestSourceSemanticPolicy::ReportOnlyObjective(profile)) return true;
            switch (profile.objective.type)
            {
                case QuestObjectiveType::TalkToNpc: return TalkToNpcPolicy::Supports(profile);
                case QuestObjectiveType::ExploreOrAreaTrigger: return ExploreObjectivePolicy::Supports(profile);
                case QuestObjectiveType::KillMob:
                    return profile.objective.targetEntry != 0 &&
                        profile.destination.valid;
                case QuestObjectiveType::CollectItemFromMob:
                    return profile.objective.targetEntry != 0 &&
                        profile.objective.itemId != 0 &&
                        profile.destination.valid;
                case QuestObjectiveType::CollectWorldItem:
                case QuestObjectiveType::InteractGameObject:
                    return profile.objective.objectEntry != 0;
                case QuestObjectiveType::UseItemOnUnit:
                    return profile.objective.targetEntry != 0 &&
                        profile.objective.itemId != 0;
                case QuestObjectiveType::UseQuestItemAtLocation:
                    return SummonedQuestObjectivePolicy::Supports(profile);
                case QuestObjectiveType::UseItemAtGameObject:
                    return MultiLocationItemUsePolicy::Supports(profile);
                case QuestObjectiveType::TravelReport:
                    return profile.turnInEntry != 0;
                default:
                    return false;
            }
        }

        static bool SupportsAllSteps(const QuestProfile& profile)
        {
            if (!profile.automatable)
                return false;
            if (profile.objectives.empty())
                return SupportsStep(profile, -1);
            for (std::size_t index = 0; index < profile.objectives.size(); ++index)
                if (!SupportsStep(profile, static_cast<int>(index)))
                    return false;
            return true;
        }
    };
}
