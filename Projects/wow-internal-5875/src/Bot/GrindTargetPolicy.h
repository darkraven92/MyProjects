#pragma once

#include "GrindLevelPolicy.h"

#include "../Objects/WorldState.h"

#include <cstdint>

namespace Bot
{
    class GrindTargetPolicy
    {
    private:
        static constexpr std::uint32_t UnitFlagNonAttackable = 0x00000002u;
        static constexpr std::uint32_t UnitFlagPlayerControlled = 0x00000008u;
        static constexpr std::uint32_t UnitFlagImmuneToPlayer = 0x00000100u;
        static constexpr std::uint32_t UnitFlagNonAttackable2 = 0x00010000u;
        static constexpr std::uint32_t UnitFlagNotSelectable = 0x02000000u;

        static constexpr std::uint32_t UnsafeUnitFlags =
            UnitFlagNonAttackable |
            UnitFlagPlayerControlled |
            UnitFlagImmuneToPlayer |
            UnitFlagNonAttackable2 |
            UnitFlagNotSelectable;

        static constexpr std::uint32_t MinimumNormalMobHealth = 10;

    public:
        static bool LooksLikeCombatCreature(
            const Objects::UnitState& unit)
        {
            if (
                !unit.valid ||
                unit.guid == 0 ||
                unit.entryId == 0 ||
                unit.isPet ||
                unit.health == 0 ||
                unit.maxHealth < MinimumNormalMobHealth ||
                unit.level == 0)
            {
                return false;
            }

            // Vendors, quest givers, trainers, flight masters, gossip NPCs,
            // etc. expose NPC flags. Normal outdoor grind mobs normally do not.
            if (unit.npcFlags != 0)
                return false;

            if ((unit.unitFlags & UnsafeUnitFlags) != 0)
                return false;

            return true;
        }

        static bool IsLevelSuitable(
            const Objects::WorldState& world,
            const Objects::UnitState& unit)
        {
            return GrindLevelPolicy::IsSafeXpTarget(
                world.player.level,
                unit.level,
                1);
        }

        static bool IsGrayCreature(
            const Objects::WorldState& world,
            const Objects::UnitState& unit)
        {
            return
                LooksLikeCombatCreature(unit) &&
                GrindLevelPolicy::IsGray(
                    world.player.level,
                    unit.level);
        }

        static bool IsPotentialTarget(
            const Objects::WorldState& world,
            const Objects::UnitState& unit)
        {
            if (!LooksLikeCombatCreature(unit))
                return false;

            if (!IsLevelSuitable(world, unit))
                return false;

            // Do not deliberately steal a unit already engaged with another
            // player/NPC. A unit targeting us remains valid and is handled with
            // even higher priority by the existing multi-aggro path.
            if (
                unit.targetGuid != 0 &&
                unit.targetGuid != world.activePlayerGuid)
            {
                return false;
            }

            return true;
        }
    };
}
