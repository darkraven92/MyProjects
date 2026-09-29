#pragma once

#include "GameThreadDispatcher.h"

#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace Bot
{
    class GrindTargetProbe
    {
    public:
        struct Result
        {
            bool valid = false;
            bool canAttack = false;
            bool elite = false;
            bool critter = false;
            std::string classification;
            std::string creatureType;
        };

    private:
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_GRIND_TARGET_RESULT";

        static bool IsExecutable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (
                address == 0 ||
                VirtualQuery(
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

    public:
        static bool Read(Result& result)
        {
            result = Result{};

            const auto doStringAddress =
                Wow5875::Client::Base() + LuaDoStringRva;
            const auto getTextAddress =
                Wow5875::Client::Base() + GetTextRva;

            if (!IsExecutable(doStringAddress) ||
                !IsExecutable(getTextAddress))
            {
                return false;
            }

            using DoStringFunction =
                bool (__fastcall*)(const char*, const char*);
            using GetTextFunction =
                const char* (__fastcall*)(char*, std::uint32_t, int);

            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);
            const auto getText =
                reinterpret_cast<GetTextFunction>(getTextAddress);

            const std::string script =
                "local a=UnitCanAttack('player','target') and 1 or 0; "
                "local c=UnitClassification('target') or ''; "
                "local t=UnitCreatureType('target') or ''; "
                "WOW_INTERNAL_GRIND_TARGET_RESULT=a..'|'..c..'|'..t;";

            char buffer[160]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (!onGameThread)
                        return;

                    luaExecuted = doString(
                        script.c_str(),
                        "wow-internal/GrindTargetProbe.lua");
                    if (!luaExecuted)
                        return;

                    const char* raw = getText(
                        const_cast<char*>(ResultVariable),
                        0xFFFFFFFFu,
                        0);

                    if (raw != nullptr && *raw != '\0')
                    {
                        std::strncpy(buffer, raw, sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';
                        gotText = true;
                    }
                });

            if (!dispatched || !onGameThread || !luaExecuted || !gotText)
                return false;

            char attack[8]{};
            char classification[64]{};
            char creatureType[64]{};
            if (std::sscanf(
                    buffer,
                    "%7[^|]|%63[^|]|%63[^\r\n]",
                    attack,
                    classification,
                    creatureType) < 2)
            {
                return false;
            }

            result.valid = true;
            result.canAttack = std::string(attack) == "1";
            result.classification = classification;
            result.creatureType = creatureType;
            result.elite =
                result.classification == "elite" ||
                result.classification == "rareelite" ||
                result.classification == "worldboss";
            result.critter = result.creatureType == "Critter";
            return true;
        }
    };
}
