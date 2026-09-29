#pragma once

#include "../Core/Memory.h"
#include "../Debug/Logger.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Objects
{
    struct PlayerState
    {
        std::uint32_t address = 0;
        std::uint32_t descriptors = 0;

        std::uint32_t level = 0;

        std::uint32_t currentXp = 0;
        std::uint32_t nextLevelXp = 0;
        bool xpValid = false;

        std::uint32_t health = 0;
        std::uint32_t maxHealth = 0;

        std::uint8_t powerType = 0;

        std::uint32_t powerRaw = 0;
        std::uint32_t maxPowerRaw = 0;

        float power = 0.0f;
        float maxPower = 0.0f;

        std::uint64_t targetGuid = 0;

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float rotation = 0.0f;

        std::uint32_t movement = 0;

        bool valid = false;
    };

    class PlayerSnapshot
    {
    private:
        static std::string Hex32(std::uint32_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(8)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static std::string Hex64(std::uint64_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(16)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static std::string Float(float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(3)
                << value;

            return stream.str();
        }

        static const char* PowerTypeName(
            std::uint8_t type)
        {
            switch (type)
            {
                case 0:
                    return "Mana";

                case 1:
                    return "Rage";

                case 2:
                    return "Focus";

                case 3:
                    return "Energy";

                case 4:
                    return "Happiness";

                default:
                    return "Unknown";
            }
        }

        static float PowerDivisor(
            std::uint8_t type)
        {
            switch (type)
            {
                case 1:
                    // Rage: 0..1000 internally,
                    // displayed as 0..100.
                    return 10.0f;

                case 4:
                    // Happiness uses 1000x scale.
                    return 1000.0f;

                default:
                    return 1.0f;
            }
        }

        static std::uintptr_t PowerOffset(
            std::uint8_t type)
        {
            /*
             * Descriptor layout:
             *
             * POWER1 = +0x5C
             * POWER2 = +0x60
             * POWER3 = +0x64
             * POWER4 = +0x68
             * POWER5 = +0x6C
             */
            return
                0x5C +
                static_cast<std::uintptr_t>(type) * 4;
        }

        static std::uintptr_t MaxPowerOffset(
            std::uint8_t type)
        {
            /*
             * MAXPOWER1 = +0x74
             * MAXPOWER2 = +0x78
             * MAXPOWER3 = +0x7C
             * MAXPOWER4 = +0x80
             * MAXPOWER5 = +0x84
             */
            return
                0x74 +
                static_cast<std::uintptr_t>(type) * 4;
        }

    public:
        static bool Read(
            std::uint32_t player,
            PlayerState& state)
        {
            state = {};
            state.address = player;

            if (player == 0)
                return false;

            // ---------------------------------------------
            // Descriptor pointer
            // ---------------------------------------------

            if (!Core::Memory::Read(
                    player + 0x08,
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

            // ---------------------------------------------
            // Basic Unit fields
            // ---------------------------------------------

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

            // Vanilla 1.12.1 build 5875 Player update fields.
            // OBJECT_END = 0x18 bytes, UNIT_END = OBJECT_END + 0x2D8
            // = 0x2F0, PLAYER_XP = UNIT_END + 0x840 = 0xB30 and
            // PLAYER_NEXT_LEVEL_XP = UNIT_END + 0x844 = 0xB34.
            // XP is intentionally optional so a transient descriptor read never
            // takes the whole combat/world snapshot offline.
            const bool currentXpRead = Core::Memory::Read(
                d + 0xB30,
                state.currentXp
            );

            const bool nextLevelXpRead = Core::Memory::Read(
                d + 0xB34,
                state.nextLevelXp
            );

            state.xpValid = currentXpRead && nextLevelXpRead;

            // UNIT_FIELD_TARGET
            Core::Memory::Read(
                d + 0x40,
                state.targetGuid
            );

            // ---------------------------------------------
            // Primary power type
            //
            // UNIT_FIELD_BYTES_0 + byte 3
            // ---------------------------------------------

            Core::Memory::Read(
                d + 0x93,
                state.powerType
            );

            if (state.powerType <= 4)
            {
                Core::Memory::Read(
                    d + PowerOffset(
                        state.powerType
                    ),
                    state.powerRaw
                );

                Core::Memory::Read(
                    d + MaxPowerOffset(
                        state.powerType
                    ),
                    state.maxPowerRaw
                );

                const float divisor =
                    PowerDivisor(
                        state.powerType
                    );

                state.power =
                    static_cast<float>(
                        state.powerRaw
                    ) / divisor;

                state.maxPower =
                    static_cast<float>(
                        state.maxPowerRaw
                    ) / divisor;
            }

            // ---------------------------------------------
            // Movement data
            //
            // player + 0x118 -> MovementInfo*
            //
            // movement + 0x10 = X
            // movement + 0x14 = Y
            // movement + 0x18 = Z
            // movement + 0x1C = rotation
            // ---------------------------------------------

            if (!Core::Memory::Read(
                    player + 0x118,
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

            if (!Core::Memory::Read(
                    state.movement + 0x1C,
                    state.rotation))
            {
                return false;
            }

            if (!std::isfinite(state.x) ||
                !std::isfinite(state.y) ||
                !std::isfinite(state.z) ||
                !std::isfinite(state.rotation))
            {
                return false;
            }

            state.valid = true;

            return true;
        }

        static void Run(std::uint32_t player)
        {
            Debug::Logger::Info(
                "=== LocalPlayer snapshot ==="
            );

            PlayerState state{};

            if (!Read(
                    player,
                    state))
            {
                Debug::Logger::Info(
                    "Failed reading LocalPlayer state."
                );

                return;
            }

            Debug::Logger::Info(
                "Player object: " +
                Hex32(state.address)
            );

            Debug::Logger::Info(
                "Descriptors: " +
                Hex32(state.descriptors)
            );

            Debug::Logger::Info(
                "Movement data: " +
                Hex32(state.movement)
            );

            Debug::Logger::Info(
                "Level: " +
                std::to_string(state.level)
            );

            Debug::Logger::Info(
                state.xpValid
                    ? ("XP: " +
                       std::to_string(state.currentXp) +
                       " / " +
                       std::to_string(state.nextLevelXp))
                    : std::string("XP: unavailable")
            );

            Debug::Logger::Info(
                "Health: " +
                std::to_string(state.health) +
                " / " +
                std::to_string(state.maxHealth)
            );

            Debug::Logger::Info(
                std::string("Power type: ") +
                PowerTypeName(state.powerType) +
                " (" +
                std::to_string(state.powerType) +
                ")"
            );

            Debug::Logger::Info(
                "Power raw: " +
                std::to_string(state.powerRaw) +
                " / " +
                std::to_string(state.maxPowerRaw)
            );

            Debug::Logger::Info(
                "Power displayed: " +
                Float(state.power) +
                " / " +
                Float(state.maxPower)
            );

            Debug::Logger::Info(
                "TargetGuid: " +
                Hex64(state.targetGuid)
            );

            Debug::Logger::Info(
                "Position: X=" +
                Float(state.x) +
                " Y=" +
                Float(state.y) +
                " Z=" +
                Float(state.z)
            );

            Debug::Logger::Info(
                "Rotation: " +
                Float(state.rotation)
            );

            Debug::Logger::Info(
                "LocalPlayer snapshot completed."
            );
        }
    };
}
