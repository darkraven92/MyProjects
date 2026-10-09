#pragma once

#include "GrindTargetPolicy.h"
#include "PullSafetyPolicy.h"

#include "../Objects/WorldState.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Bot
{
    // The WorldState adapter is deliberately separate from the pure pull
    // policy. Existing quest/grind eligibility stays with its current owner.
    struct SafeTargetSnapshot
    {
        std::vector<PullSafetyUnit> units;
        std::vector<const Objects::UnitState*> worldUnits;

        const Objects::UnitState* At(std::size_t index) const
        {
            return index < worldUnits.size() ? worldUnits[index] : nullptr;
        }

        const Objects::UnitState* ByGuid(std::uint64_t guid) const
        {
            for (const auto* unit : worldUnits)
                if (unit != nullptr && unit->guid == guid)
                    return unit;
            return nullptr;
        }
    };

    class SafeTargetSelector
    {
    public:
        template <typename Eligible, typename Aggressor>
        static SafeTargetSnapshot Capture(
            const Objects::WorldState& world,
            Eligible eligible, Aggressor aggressor)
        {
            SafeTargetSnapshot result{};
            result.units.reserve(world.units.size());
            result.worldUnits.reserve(world.units.size());
            for (const auto& unit : world.units)
            {
                const bool live = unit.valid && unit.guid != 0 &&
                    unit.health > 0 && unit.maxHealth > 0;
                result.units.push_back(PullSafetyUnit{
                    unit.guid, unit.entryId, unit.x, unit.y, unit.z,
                    unit.distance, 0.0f, live,
                    live && eligible(unit),
                    live && GrindTargetPolicy::LooksLikeCombatCreature(unit),
                    live && aggressor(unit)});
                result.worldUnits.push_back(&unit);
            }
            return result;
        }
    };
}
