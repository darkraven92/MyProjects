#pragma once

#include "QuestPlannerTypes.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Bot
{
    // Pure decisions shared by every profile-driven resource + GO use step.
    // Completion is never inferred from an item dispatch or a GO click.
    struct MultiLocationItemUsePolicy
    {
        static constexpr int MaximumUseAttempts = 3;
        static constexpr std::uint64_t ProgressWaitTicks = 32;
        static constexpr std::uint64_t MaximumObjectiveTicks = 1200;
        static constexpr float GameObjectUseDistance = 5.5f;
        static constexpr float AnchorIdentityDistance = 30.0f;
        static constexpr float StableDisplacement = 0.35f;
        static constexpr int RequiredStableSamples = 2;

        static bool Supports(const QuestProfile& profile)
        {
            if (profile.objective.type !=
                    QuestObjectiveType::UseItemAtGameObject ||
                profile.objective.objectEntry == 0 ||
                profile.questUseItemId == 0 ||
                !profile.destination.valid ||
                profile.activeLeaderboardIndex < 0 ||
                profile.questResourceSources.empty())
                return false;
            for (const auto& source : profile.questResourceSources)
                if (source.creatureEntry != 0 && source.destination.valid)
                    return true;
            return false;
        }

        static int NearestResourceSource(
            const QuestProfile& profile, float x, float y)
        {
            int best = -1;
            float bestDistance = std::numeric_limits<float>::infinity();
            for (std::size_t i = 0; i < profile.questResourceSources.size(); ++i)
            {
                const auto& source = profile.questResourceSources[i];
                if (source.creatureEntry == 0 || !source.destination.valid ||
                    source.destination.mapId != profile.destination.mapId)
                    continue;
                const float distance = std::hypot(
                    x - source.destination.x, y - source.destination.y);
                if (!std::isfinite(distance))
                    continue;
                if (distance < bestDistance ||
                    (distance == bestDistance &&
                     (best < 0 || source.creatureEntry <
                         profile.questResourceSources[static_cast<std::size_t>(best)].creatureEntry)))
                {
                    best = static_cast<int>(i);
                    bestDistance = distance;
                }
            }
            return best;
        }

        static bool MatchesGameObject(
            const QuestProfile& profile, std::uint32_t entry,
            float objectX, float objectY, float objectZ,
            float playerDistance)
        {
            return entry == profile.objective.objectEntry &&
                std::isfinite(playerDistance) &&
                playerDistance <= GameObjectUseDistance &&
                std::hypot(objectX - profile.destination.x,
                    objectY - profile.destination.y,
                    objectZ - profile.destination.z) <=
                    AnchorIdentityDistance;
        }

        static bool MayIssueUse(
            bool questActive, bool rowIncomplete, bool playerAlive,
            bool combatIdle, bool itemPresent, bool matchingObject,
            int stableSamples, int attempts, std::uint64_t tick,
            std::uint64_t nextUseTick)
        {
            return questActive && rowIncomplete && playerAlive &&
                combatIdle && itemPresent && matchingObject &&
                stableSamples >= RequiredStableSamples &&
                attempts < MaximumUseAttempts && tick >= nextUseTick;
        }

        static bool ProgressConfirmed(
            const PlannerQuestLogEntry& entry, const QuestProfile& profile)
        {
            return SelectedObjectiveComplete(entry, profile);
        }
    };
}
