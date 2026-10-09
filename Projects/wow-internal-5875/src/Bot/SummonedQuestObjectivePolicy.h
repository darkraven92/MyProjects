#pragma once

#include "QuestPlannerTypes.h"

#include <cmath>
#include <cstdint>

namespace Bot
{
    // Pure bounds/identity rules for item-at-location -> expected spawn ->
    // existing collect-from-mob executor. No client calls or movement here.
    struct SummonedQuestObjectivePolicy
    {
        static constexpr int MaximumUseAttempts = 3;
        static constexpr std::uint64_t SpawnWaitTicks = 48;
        static constexpr std::uint64_t RetryDelayTicks = 8;
        static constexpr std::uint64_t MaximumObjectiveTicks = 1600;
        static constexpr float UseRadius = 8.0f;
        static constexpr float MaximumSpawnDistance = 55.0f;
        static constexpr float StableDisplacement = 0.5f;
        static constexpr int RequiredStableSamples = 2;

        static bool Supports(const QuestProfile& profile)
        {
            return profile.objective.type ==
                    QuestObjectiveType::UseQuestItemAtLocation &&
                profile.questUseItemId != 0 &&
                profile.objective.targetEntry != 0 &&
                profile.objective.itemId != 0 &&
                profile.objective.requiredCount > 0 &&
                profile.destination.valid;
        }

        static bool ExpectedSpawn(
            std::uint32_t entry, std::uint32_t expectedEntry,
            bool valid, bool pet, std::uint32_t health,
            float distanceFromUseLocation)
        {
            return valid && !pet && health > 0 &&
                entry == expectedEntry &&
                std::isfinite(distanceFromUseLocation) &&
                distanceFromUseLocation <= MaximumSpawnDistance;
        }

        static bool StableSample(float displacement)
        {
            return std::isfinite(displacement) &&
                displacement <= StableDisplacement;
        }

        static bool MayUse(
            bool activeQuest, bool objectiveIncomplete,
            bool alive, bool combatIdle, bool itemPresent,
            float distanceToUseLocation, int stableSamples,
            int attempts, std::uint64_t tick,
            std::uint64_t nextUseTick)
        {
            return activeQuest && objectiveIncomplete && alive &&
                combatIdle && itemPresent &&
                std::isfinite(distanceToUseLocation) &&
                distanceToUseLocation <= UseRadius &&
                stableSamples >= RequiredStableSamples &&
                attempts < MaximumUseAttempts && tick >= nextUseTick;
        }

        static bool SpawnWaitExpired(
            std::uint64_t tick, std::uint64_t useTick)
        {
            return tick >= useTick && tick - useTick >= SpawnWaitTicks;
        }

        static bool ObjectiveTimedOut(
            std::uint64_t tick, std::uint64_t startTick)
        {
            return tick >= startTick &&
                tick - startTick >= MaximumObjectiveTicks;
        }
    };
}
