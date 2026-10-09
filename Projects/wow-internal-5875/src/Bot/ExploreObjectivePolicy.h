#pragma once
#include "QuestPlannerTypes.h"
#include <cmath>
namespace Bot
{
    struct ExploreObjectivePolicy
    {
        static constexpr std::uint64_t MaximumObjectiveTicks=1200;
        static constexpr std::uint64_t ProgressWaitTicks=40;
        static bool Valid(const QuestAreaTrigger& t)
        {
            return t.clientGeometryVerified && t.id>0 && t.build>0 && t.build<=5875 &&
                t.center.valid && std::isfinite(t.radius) && t.radius>0 &&
                std::isfinite(t.center.x) && std::isfinite(t.center.y) && std::isfinite(t.center.z);
        }
        static bool Supports(const QuestProfile& p)
        {
            if(p.databaseDerived && (!p.sourceMetadata || p.sourceMetadata->startScript!=0 ||
                p.sourceMetadata->completeScript!=0)) return false;
            if(p.objective.type!=QuestObjectiveType::ExploreOrAreaTrigger || !p.objectives.empty() ||
                p.areaTriggers.empty() || p.areaTriggers.size()>8 || !p.destination.valid) return false;
            for(const auto& t:p.areaTriggers) if(!Valid(t) || t.center.mapId!=p.destination.mapId) return false;
            return true;
        }
        static bool Inside(const QuestProfile& p, std::uint32_t map, float x,float y,float z)
        {
            for(const auto& t:p.areaTriggers)
                if(Valid(t) && t.center.mapId==map && std::hypot(x-t.center.x,y-t.center.y,z-t.center.z)<=t.radius) return true;
            return false;
        }
        static bool Complete(const PlannerQuestLogEntry& live) { return live.complete; }
        static bool WaitExpired(std::uint64_t tick,std::uint64_t entered)
        { return tick>=entered && tick-entered>=ProgressWaitTicks; }
    };
}
