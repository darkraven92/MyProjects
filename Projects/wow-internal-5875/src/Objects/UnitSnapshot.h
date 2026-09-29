#pragma once

#include "../Core/Memory.h"

#include <cmath>
#include <cstdint>

namespace Objects
{
    struct UnitState
    {
        std::uint32_t address = 0;
        std::uint32_t descriptors = 0;
        std::uint32_t movement = 0;

        std::uint64_t guid = 0;
        std::uint64_t targetGuid = 0;

        std::uint32_t entryId = 0;

        std::uint32_t level = 0;

        std::uint32_t health = 0;
        std::uint32_t maxHealth = 0;

        std::uint32_t factionTemplate = 0;
        std::uint32_t unitFlags = 0;
        std::uint32_t npcFlags = 0;

        bool isPet = false;

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float rotation = 0.0f;

        float distance = 0.0f;

        bool valid = false;
    };

    class UnitSnapshot
    {
    public:
        static std::uint32_t EntryId(
            std::uint64_t guid)
        {
            const std::uint16_t high =
                static_cast<std::uint16_t>(
                    (guid >> 48) & 0xFFFF
                );

            /*
             * Vanilla:
             *
             * F130 = Creature
             * F140 = Pet
             */
            if (high != 0xF130 &&
                high != 0xF140)
            {
                return 0;
            }

            return static_cast<std::uint32_t>(
                (guid >> 24) & 0x00FFFFFF
            );
        }

        static float Distance(
            float ax,
            float ay,
            float az,
            float bx,
            float by,
            float bz)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;

            return std::sqrt(
                dx * dx +
                dy * dy +
                dz * dz
            );
        }

        static bool Read(
            std::uint32_t address,
            UnitState& state)
        {
            state = {};
            state.address = address;

            if (address == 0)
                return false;

            /*
             * Object GUID.
             */
            if (!Core::Memory::Read(
                    address + 0x30,
                    state.guid))
            {
                return false;
            }

            state.entryId =
                EntryId(state.guid);

            state.isPet =
                static_cast<std::uint16_t>((state.guid >> 48) & 0xFFFF) ==
                0xF140;

            /*
             * Descriptor pointer.
             */
            if (!Core::Memory::Read(
                    address + 0x08,
                    state.descriptors))
            {
                return false;
            }

            if (state.descriptors == 0 ||
                !Core::Memory::IsReadable(
                    state.descriptors,
                    0x100))
            {
                return false;
            }

            const std::uint32_t d =
                state.descriptors;

            /*
             * Vanilla 1.12.1 Unit update fields.
             */

            Core::Memory::Read(
                d + 0x40,
                state.targetGuid
            );

            if (!Core::Memory::Read(
                    d + 0x58,
                    state.health))
            {
                return false;
            }

            if (!Core::Memory::Read(
                    d + 0x70,
                    state.maxHealth))
            {
                return false;
            }

            if (!Core::Memory::Read(
                    d + 0x88,
                    state.level))
            {
                return false;
            }

            Core::Memory::Read(
                d + 0x8C,
                state.factionTemplate
            );

            // Vanilla 1.12.1 update-field offsets in bytes:
            // UNIT_FIELD_FLAGS = OBJECT_END(0x18) + 0xA0 = 0xB8
            // UNIT_NPC_FLAGS   = OBJECT_END(0x18) + 0x234 = 0x24C
            if (!Core::Memory::Read(
                    d + 0xB8,
                    state.unitFlags))
            {
                return false;
            }

            if (!Core::Memory::Read(
                    d + 0x24C,
                    state.npcFlags))
            {
                return false;
            }

            /*
             * MovementInfo pointer.
             */
            if (!Core::Memory::Read(
                    address + 0x118,
                    state.movement))
            {
                return false;
            }

            if (state.movement == 0 ||
                !Core::Memory::IsReadable(
                    state.movement,
                    0x20))
            {
                return false;
            }

            if (!Core::Memory::Read(
                    state.movement + 0x10,
                    state.x))
            {
                return false;
            }

            if (!Core::Memory::Read(
                    state.movement + 0x14,
                    state.y))
            {
                return false;
            }

            if (!Core::Memory::Read(
                    state.movement + 0x18,
                    state.z))
            {
                return false;
            }

            Core::Memory::Read(
                state.movement + 0x1C,
                state.rotation
            );

            if (!std::isfinite(state.x) ||
                !std::isfinite(state.y) ||
                !std::isfinite(state.z))
            {
                return false;
            }

            state.valid = true;

            return true;
        }
    };
}
