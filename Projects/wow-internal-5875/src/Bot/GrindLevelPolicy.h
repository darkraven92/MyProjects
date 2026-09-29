#pragma once

#include <cstdint>

namespace Bot
{
    class GrindLevelPolicy
    {
    public:
        // Vanilla mob gray-level boundary. Mobs at or below this level
        // award no normal kill XP to the character.
        static std::uint32_t GrayLevel(std::uint32_t playerLevel)
        {
            if (playerLevel <= 5)
                return 0;

            if (playerLevel <= 39)
                return playerLevel - (playerLevel / 10) - 5;

            if (playerLevel <= 59)
                return playerLevel - (playerLevel / 5) - 1;

            // Vanilla level-cap behavior. At 60 the gray boundary jumps to 51.
            return playerLevel > 9 ? playerLevel - 9 : 0;
        }

        static bool IsGray(
            std::uint32_t playerLevel,
            std::uint32_t mobLevel)
        {
            if (playerLevel == 0 || mobLevel == 0)
                return false;

            return mobLevel <= GrayLevel(playerLevel);
        }

        static bool IsSafeXpTarget(
            std::uint32_t playerLevel,
            std::uint32_t mobLevel,
            std::uint32_t maximumLevelsAbove = 1)
        {
            if (playerLevel == 0 || mobLevel == 0)
                return false;

            if (mobLevel > playerLevel + maximumLevelsAbove)
                return false;

            return !IsGray(playerLevel, mobLevel);
        }
    };
}
