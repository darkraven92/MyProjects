#pragma once

#include "GameThreadDispatcher.h"

#include "../Core/Memory.h"
#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <string>

namespace Bot
{
    /*
     * Phase 14K.1
     *
     * Posture guard for recovery/liveness transitions. It reads the live
     * Vanilla stand-state byte before acting and never toggles posture blindly.
     */
    class PlayerPostureController
    {
    private:
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t StandStateOffset = 0x210;
        static constexpr std::uint8_t Standing = 0;
        static constexpr std::uint8_t Dead = 7;

        static bool IsExecutable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(
                    reinterpret_cast<LPCVOID>(address),
                    &info,
                    sizeof(info)) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            switch (info.Protect & 0xFF)
            {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;
                default:
                    return false;
            }
        }

        static bool ExecuteStandCommand()
        {
            const std::uintptr_t address =
                Wow5875::Client::Base() + LuaDoStringRva;
            if (!IsExecutable(address))
                return false;

            using DoStringFunction = bool (__fastcall*)(const char*, const char*);
            const auto doString =
                reinterpret_cast<DoStringFunction>(address);

            bool onGameThread = false;
            bool executed = false;
            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    executed = doString(
                        "if SitOrStand then SitOrStand(); end;",
                        "wow-internal/PostureStand.lua");
                });

            return dispatched && onGameThread && executed;
        }

    public:
        static bool ReadStandState(
            const Objects::PlayerState& player,
            std::uint8_t& standState)
        {
            standState = Standing;
            if (!player.valid || player.descriptors == 0)
                return false;

            std::uint32_t bytes1 = 0;
            if (!Core::Memory::Read(
                    static_cast<std::uintptr_t>(player.descriptors) +
                        StandStateOffset,
                    bytes1))
            {
                return false;
            }

            standState = static_cast<std::uint8_t>(bytes1 & 0xFFu);
            return true;
        }

        static bool EnsureStanding(
            const Objects::PlayerState& player,
            const char* reason)
        {
            std::uint8_t standState = Standing;
            if (!ReadStandState(player, standState))
            {
                Debug::Logger::Info(
                    std::string("ROBUSTNESS 14K.1: posture probe unavailable; no blind SitOrStand toggle. reason=") +
                    (reason != nullptr ? reason : "unspecified"));
                return false;
            }

            if (standState == Standing)
                return true;

            if (standState == Dead || player.health == 0)
                return false;

            if (!ExecuteStandCommand())
            {
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.1: failed to issue verified stand command; standState=" +
                    std::to_string(standState));
                return false;
            }

            Debug::Logger::Info(
                "ROBUSTNESS 14K.1: STAND command issued standState=" +
                std::to_string(standState) +
                " reason=" +
                std::string(reason != nullptr ? reason : "unspecified"));
            return true;
        }
    };
}
