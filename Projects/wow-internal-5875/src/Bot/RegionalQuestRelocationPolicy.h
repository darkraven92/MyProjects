#pragma once
#include "QuestAcquisitionPolicy.h"
#include <algorithm>
#include <map>
#include <set>

namespace Bot
{
    struct RegionalQuestDestination
    {
        int questId = 0;
        std::uint32_t giverEntry = 0;
        ObjectiveDestination destination{};
        float distance = 0;
        std::size_t usefulQuests = 0;
    };

    struct RegionalQuestRelocationPolicy
    {
        // Same scope as a local discovery sweep. Relocation is a separate
        // bounded audit journey, never authority to accept a quest.
        static constexpr float LocalRadius = 300.0f;
        static constexpr float MaximumJourneyDistance = 2000.0f;
        static constexpr std::uint64_t JourneyTicks = 900; // existing giver leg convention
        static bool IdleAction(QuestPlannerAction action)
        {
            return action == QuestPlannerAction::DiscoverPickup ||
                action == QuestPlannerAction::Deferred || action == QuestPlannerAction::UnsupportedActiveQuest;
        }
        static std::vector<RegionalQuestDestination> Candidates(
            const std::vector<QuestProfile>& profiles, const QuestGraph& graph,
            QuestEligibilityContext context, const QuestPlannerSnapshot& snapshot,
            std::uint32_t mapId, float x, float y, float z, std::uint64_t tick,
            const std::map<std::uint32_t,std::uint64_t>& giverBackoff)
        {
            std::vector<RegionalQuestDestination> result;
            if (!snapshot.valid || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return result;
            for (const auto& p : profiles)
            {
                context.active = QuestAcquisitionPolicy::TitleStillPresent(p,snapshot);
                if (!p.sourceMetadata || (p.hubUnlockQuestId != 0 &&
                    !context.completed.count(p.hubUnlockQuestId))) continue;
                const auto blocked = giverBackoff.find(p.giverEntry);
                if (QuestAcquisitionPolicy::Evaluate(p,graph.Find(p.questId),context,
                    blocked!=giverBackoff.end() && tick<blocked->second) != QuestAcquisitionResult::EligibleForPickup)
                    continue;
                std::vector<ObjectiveDestination> anchors = p.giverDestinations;
                if (anchors.empty()) anchors.push_back(p.giverDestination);
                for (const auto& d : anchors)
                {
                    if (!QuestAcquisitionPolicy::ValidDestination(d) || d.mapId!=mapId) continue;
                    const float distance=std::hypot(d.x-x,d.y-y,d.z-z);
                    if (distance<=LocalRadius || distance>MaximumJourneyDistance) continue;
                    result.push_back({p.questId,p.giverEntry,d,distance,0});
                }
            }
            for (auto& candidate : result)
            {
                std::set<int> useful;
                for (const auto& other : result)
                    if (std::hypot(candidate.destination.x-other.destination.x,
                            candidate.destination.y-other.destination.y,
                            candidate.destination.z-other.destination.z)<=LocalRadius)
                        useful.insert(other.questId);
                candidate.usefulQuests=useful.size();
            }
            std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
                // Shortest source-backed audit first; density resolves equal
                // distances. Reachability is proved only by the real follower.
                if(a.distance!=b.distance) return a.distance<b.distance;
                if(a.usefulQuests!=b.usefulQuests) return a.usefulQuests>b.usefulQuests;
                if(a.giverEntry!=b.giverEntry) return a.giverEntry<b.giverEntry;
                if(a.questId!=b.questId) return a.questId<b.questId;
                if(a.destination.x!=b.destination.x) return a.destination.x<b.destination.x;
                if(a.destination.y!=b.destination.y) return a.destination.y<b.destination.y;
                return a.destination.z<b.destination.z;
            });
            return result;
        }
    };
}
