#pragma once
#include "QuestPlannerTypes.h"
namespace Bot
{
    struct QuestSourceSemanticPolicy
    {
        static bool ReportOnlyObjective(const QuestProfile& p)
        {
            return p.sourceObjectiveCount == 0 && p.expectedObjectiveCount == 0 &&
                p.objectives.empty() && !p.objective.itemId && !p.objective.objectEntry &&
                p.objective.targetEntry == p.turnInEntry &&
                (p.objective.type == QuestObjectiveType::TalkToNpc ||
                 p.objective.type == QuestObjectiveType::TravelReport) && PlainReport(p);
        }

        static bool PlainReport(const QuestProfile& p)
        {
            if (!p.sourceMetadata) return false;
            const auto& m = *p.sourceMetadata;
            return m.method == 2 && m.specialFlags == 0 && m.flags &&
                ((*m.flags & 0x400) == 0) && m.startScript == 0 && m.completeScript == 0 &&
                m.timeLimit == 0 && m.reputationObjective == 0 &&
                m.rewardOrRequiredMoney && *m.rewardOrRequiredMoney >= 0 &&
                p.areaTriggerIds.empty() && !p.turnInIsGameObject && p.turnInEntry != 0;
        }
    };
}
