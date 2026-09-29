#pragma once

#include "../Objects/WorldState.h"

#include <cstdint>

namespace Bot
{
    struct TargetCandidate
    {
        bool found =
            false;

        Objects::UnitState unit{};
    };

    class TargetSelector
    {
    private:
        inline static std::uint32_t
            plannerAllowedEntry_ =
                0;

        inline static bool
            genericGrindEntriesEnabled_ =
                false;

    public:
        static void SetGenericGrindEntriesEnabled(bool enabled)
        {
            genericGrindEntriesEnabled_ = enabled;
        }

        static bool GenericGrindEntriesEnabled()
        {
            return genericGrindEntriesEnabled_;
        }

        static void SetPlannerAllowedEntry(
            std::uint32_t entryId)
        {
            plannerAllowedEntry_ =
                entryId;
        }

        static std::uint32_t PlannerAllowedEntry()
        {
            return
                plannerAllowedEntry_;
        }

        /*
         * Quest-combat creature entries currently supported by
         * the combat/chase stack.
         *
         * 3098 = Mottled Boar
         * 3124 = Scorpid Worker
         * 3101 = Vile Familiar
         *
         * Keep this shared allowlist authoritative. Components
         * such as ChaseController call IsSelectable(), so adding
         * a new quest mob only in CombatController is insufficient.
         */
        static bool IsAllowedEntry(
            std::uint32_t entryId)
        {
            if (genericGrindEntriesEnabled_ && entryId != 0)
                return true;

            return
                entryId == 3098 ||
                entryId == 3124 ||
                entryId == 3101 ||
                /*
                 * Phase 14G.1 temporary Sen'jin grind profile.
                 * These entries are only actually acquired when
                 * CombatController's explicit grind policy is active;
                 * quest mode still requires an exact quest-policy match.
                 */
                entryId == 3103 || // Makrura Clacker
                entryId == 3106 || // Pygmy Surf Crawler
                entryId == 3121 || // Durotar Tiger
                entryId == 3125 || // Clattering Scorpid
                (
                    plannerAllowedEntry_ != 0 &&
                    entryId ==
                        plannerAllowedEntry_
                );
        }

        static bool IsSelectable(
            const Objects::UnitState& unit,
            float maximumDistance)
        {
            if (
                !unit.valid ||
                unit.guid == 0 ||
                unit.health == 0 ||
                unit.maxHealth == 0)
            {
                return false;
            }

            if (!IsAllowedEntry(
                    unit.entryId))
            {
                return false;
            }

            if (
                unit.distance < 0.0f ||
                unit.distance >
                    maximumDistance)
            {
                return false;
            }

            return true;
        }

        static const Objects::UnitState* FindByGuid(
            const Objects::WorldState& world,
            std::uint64_t guid)
        {
            if (guid == 0)
            {
                return nullptr;
            }

            for (
                const auto& unit :
                    world.units)
            {
                if (
                    unit.valid &&
                    unit.guid == guid)
                {
                    return &unit;
                }
            }

            return nullptr;
        }

        static TargetCandidate SelectNearestQuestMob(
            const Objects::WorldState& world,
            float maximumDistance)
        {
            TargetCandidate best{};

            float bestDistance =
                maximumDistance +
                1.0f;

            for (
                const auto& unit :
                    world.units)
            {
                if (!IsSelectable(
                        unit,
                        maximumDistance))
                {
                    continue;
                }

                if (
                    unit.distance >=
                        bestDistance)
                {
                    continue;
                }

                best.found =
                    true;

                best.unit =
                    unit;

                bestDistance =
                    unit.distance;
            }

            return best;
        }
    };
}
