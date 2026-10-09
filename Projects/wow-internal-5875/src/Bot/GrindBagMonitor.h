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
    class GrindBagMonitor
    {
    public:
        struct Snapshot
        {
            bool valid = false;
            int totalSlots = 0;
            int usedSlots = 0;
            int freeSlots = 0;
        };

    private:
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_GRIND_BAGS_RESULT";

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

        static std::uintptr_t LuaDoStringAddress()
        {
            return Wow5875::Client::Base() + LuaDoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return Wow5875::Client::Base() + GetTextRva;
        }

    public:
        // Shared game-thread bridge for bounded inventory transactions.
        template<std::size_t ReadbackCapacity=128>
        static bool ExecuteLuaReadback(
            const std::string& script,
            const char* scriptName,
            std::string& result,
            const char* resultVariable = ResultVariable)
        {
            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();

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

            static_assert(ReadbackCapacity>=2 && ReadbackCapacity<=65536);
            char buffer[ReadbackCapacity]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (!onGameThread)
                        return;

                    luaExecuted = doString(script.c_str(), scriptName);
                    if (!luaExecuted)
                        return;

                    const char* raw = getText(
                        const_cast<char*>(resultVariable),
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

            result = buffer;
            return true;
        }

    public:
        static bool Read(Snapshot& snapshot)
        {
            snapshot = Snapshot{};

            const std::string script =
                "local total=0; local used=0; local free=0; "
                "for b=0,4 do "
                "local n=GetContainerNumSlots(b) or 0; total=total+n; "
                "for s=1,n do "
                "if GetContainerItemLink(b,s) then used=used+1 else free=free+1 end; "
                "end; end; "
                "WOW_INTERNAL_GRIND_BAGS_RESULT=total..'|'..used..'|'..free;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/GrindBagMonitor.lua",
                    result))
            {
                return false;
            }

            int total = 0;
            int used = 0;
            int free = 0;
            if (std::sscanf(
                    result.c_str(),
                    "%d|%d|%d",
                    &total,
                    &used,
                    &free) != 3)
            {
                return false;
            }

            if (total < 0 || used < 0 || free < 0 || used + free != total)
                return false;

            snapshot.valid = true;
            snapshot.totalSlots = total;
            snapshot.usedSlots = used;
            snapshot.freeSlots = free;
            return true;
        }
    };
}
