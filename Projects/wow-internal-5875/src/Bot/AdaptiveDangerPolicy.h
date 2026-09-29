#pragma once

#include <cstdint>

namespace Bot
{
    class AdaptiveDangerPolicy
    {
    public:
        static constexpr int DeathsBeforeQuarantine = 2;

        static bool ShouldQuarantine(int deathsAtCurrentLevel)
        {
            return deathsAtCurrentLevel >= DeathsBeforeQuarantine;
        }

        static bool RemainsQuarantined(
            std::uint32_t blockedAtLevel,
            std::uint32_t currentLevel)
        {
            return blockedAtLevel != 0 && currentLevel <= blockedAtLevel;
        }

        static float LevelRiskFactor(
            std::uint32_t observationLevel,
            std::uint32_t currentLevel)
        {
            if (observationLevel == 0 || currentLevel <= observationLevel)
                return 1.0f;

            const std::uint32_t delta = currentLevel - observationLevel;
            if (delta == 1)
                return 0.35f;
            if (delta == 2)
                return 0.10f;
            return 0.025f;
        }
    };
}
