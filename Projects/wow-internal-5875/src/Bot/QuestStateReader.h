#pragma once

#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    class QuestStateReader
    {
    private:
        /*
         * WoW 1.12.1 build 5875.
         *
         * Lua DoString absolute:
         *     0x00704CD0
         *
         * GetText absolute:
         *     0x00703BF0
         *
         * Image base:
         *     0x00400000
         */
        static constexpr std::uintptr_t DoStringRva =
            0x00304CD0;

        static constexpr std::uintptr_t GetTextRva =
            0x00303BF0;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_QUEST_STATE";

        static std::string Hex32(
            std::uintptr_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(8)
                << std::setfill('0')
                << static_cast<std::uint32_t>(
                    value
                );

            return stream.str();
        }

        static bool IsExecutable(
            std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};

            if (VirtualQuery(
                    reinterpret_cast<LPCVOID>(
                        address
                    ),
                    &info,
                    sizeof(info)
                ) == 0)
            {
                return false;
            }

            if (info.State != MEM_COMMIT)
            {
                return false;
            }

            if (
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD protect =
                info.Protect & 0xFF;

            switch (protect)
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

        static std::uintptr_t
        DoStringAddress()
        {
            return
                Wow5875::Client::Base() +
                DoStringRva;
        }

        static std::uintptr_t
        GetTextAddress()
        {
            return
                Wow5875::Client::Base() +
                GetTextRva;
        }

    public:
        struct Snapshot
        {
            bool valid =
                false;

            int questEntries =
                0;

            bool cuttingTeethActive =
                false;

            bool cuttingTeethComplete =
                false;

            bool stingOfTheScorpidActive =
                false;

            bool stingOfTheScorpidComplete =
                false;

            bool vileFamiliarsActive =
                false;

            bool vileFamiliarsComplete =
                false;
        };

        static bool Validate()
        {
            const auto doString =
                DoStringAddress();

            const auto getText =
                GetTextAddress();

            Debug::Logger::Info(
                "Quest DoString address: " +
                Hex32(
                    doString
                )
            );

            Debug::Logger::Info(
                "Quest GetText address: " +
                Hex32(
                    getText
                )
            );

            if (!IsExecutable(
                    doString))
            {
                Debug::Logger::Info(
                    "QuestStateReader validation FAILED: "
                    "DoString not executable."
                );

                return false;
            }

            if (!IsExecutable(
                    getText))
            {
                Debug::Logger::Info(
                    "QuestStateReader validation FAILED: "
                    "GetText not executable."
                );

                return false;
            }

            Debug::Logger::Info(
                "QuestStateReader validation PASS."
            );

            return true;
        }

        static bool Read(
            Snapshot& snapshot)
        {
            snapshot =
                Snapshot{};

            if (!Validate())
            {
                return false;
            }

            /*
             * Phase 7A is deliberately read-only.
             *
             * We ask the normal Vanilla quest-log API for
             * the supported Valley of Trials objectives we care
             * about and store a compact result in a Lua
             * global. GetText then reads that value back
             * inside the client.
             *
             * No quest state is modified.
             */
            static constexpr char ProbeScript[] =
                "local a788=0; "
                "local c788=0; "
                "local a789=0; "
                "local c789=0; "
                "local a792=0; "
                "local c792=0; "
                "local n=GetNumQuestLogEntries(); "
                "if n==nil then n=0 end; "
                "for i=1,n do "
                "local t,_,_,h,_,done=GetQuestLogTitle(i); "
                "if not h and t then "
                "if t=='Cutting Teeth' then "
                "a788=1; "
                "if done then c788=1 end; "
                "end; "
                "if t=='Sting of the Scorpid' then "
                "a789=1; "
                "if done then c789=1 end; "
                "end; "
                "if t=='Vile Familiars' then "
                "a792=1; "
                "if done then c792=1 end; "
                "end; "
                "end; "
                "end; "
                "WOW_INTERNAL_QUEST_STATE="
                "a788..','..c788..','.."
                "a789..','..c789..','.."
                "a792..','..c792..','..n";

            using DoStringFunction =
                bool (__fastcall*)(
                    const char*,
                    const char*
                );

            using GetTextFunction =
                const char* (__fastcall*)(
                    char*,
                    std::uint32_t,
                    int
                );

            const auto doString =
                reinterpret_cast<
                    DoStringFunction
                >(
                    DoStringAddress()
                );

            const auto getText =
                reinterpret_cast<
                    GetTextFunction
                >(
                    GetTextAddress()
                );

            char result[128]{};

            bool onGameThread =
                false;

            bool luaExecuted =
                false;

            bool gotText =
                false;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        luaExecuted =
                            doString(
                                ProbeScript,
                                "wow-internal/QuestStateReader.lua"
                            );

                        if (!luaExecuted)
                        {
                            return;
                        }

                        const char* raw =
                            getText(
                                const_cast<char*>(
                                    ResultVariable
                                ),
                                0xFFFFFFFFu,
                                0
                            );

                        if (
                            raw != nullptr &&
                            *raw != '\0')
                        {
                            std::strncpy(
                                result,
                                raw,
                                sizeof(result) - 1
                            );

                            result[
                                sizeof(result) - 1
                            ] = '\0';

                            gotText =
                                true;
                        }
                    }
                );

            if (
                !dispatched ||
                !onGameThread ||
                !luaExecuted ||
                !gotText)
            {
                Debug::Logger::Info(
                    "QUEST PROBE: failed to read "
                    "Lua result."
                );

                return false;
            }

            int a788 =
                0;

            int c788 =
                0;

            int a789 =
                0;

            int c789 =
                0;

            int a792 =
                0;

            int c792 =
                0;

            int entries =
                0;

            const int parsed =
                std::sscanf(
                    result,
                    "%d,%d,%d,%d,%d,%d,%d",
                    &a788,
                    &c788,
                    &a789,
                    &c789,
                    &a792,
                    &c792,
                    &entries
                );

            if (parsed != 7)
            {
                Debug::Logger::Info(
                    std::string(
                        "QUEST PROBE: unexpected "
                        "result: "
                    ) +
                    result
                );

                return false;
            }

            snapshot.valid =
                true;

            snapshot.questEntries =
                entries;

            snapshot.cuttingTeethActive =
                a788 != 0;

            snapshot.cuttingTeethComplete =
                c788 != 0;

            snapshot.stingOfTheScorpidActive =
                a789 != 0;

            snapshot.stingOfTheScorpidComplete =
                c789 != 0;

            snapshot.vileFamiliarsActive =
                a792 != 0;

            snapshot.vileFamiliarsComplete =
                c792 != 0;

            return true;
        }

        static bool Same(
            const Snapshot& a,
            const Snapshot& b)
        {
            return
                a.valid ==
                    b.valid &&
                a.questEntries ==
                    b.questEntries &&
                a.cuttingTeethActive ==
                    b.cuttingTeethActive &&
                a.cuttingTeethComplete ==
                    b.cuttingTeethComplete &&
                a.stingOfTheScorpidActive ==
                    b.stingOfTheScorpidActive &&
                a.stingOfTheScorpidComplete ==
                    b.stingOfTheScorpidComplete &&
                a.vileFamiliarsActive ==
                    b.vileFamiliarsActive &&
                a.vileFamiliarsComplete ==
                    b.vileFamiliarsComplete;
        }

        static const char* YesNo(
            bool value)
        {
            return
                value
                    ? "yes"
                    : "no";
        }

        static void Log(
            const Snapshot& snapshot)
        {
            if (!snapshot.valid)
            {
                Debug::Logger::Info(
                    "QUEST PROBE: state unavailable."
                );

                return;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PROBE: STATE"
            );

            Debug::Logger::Info(
                "Quest log entries: " +
                std::to_string(
                    snapshot.questEntries
                )
            );

            Debug::Logger::Info(
                std::string(
                    "788 Cutting Teeth: active="
                ) +
                YesNo(
                    snapshot.cuttingTeethActive
                ) +
                " complete=" +
                YesNo(
                    snapshot.cuttingTeethComplete
                )
            );

            Debug::Logger::Info(
                std::string(
                    "789 Sting of the Scorpid: active="
                ) +
                YesNo(
                    snapshot.stingOfTheScorpidActive
                ) +
                " complete=" +
                YesNo(
                    snapshot.stingOfTheScorpidComplete
                )
            );

            Debug::Logger::Info(
                std::string(
                    "792 Vile Familiars: active="
                ) +
                YesNo(
                    snapshot.vileFamiliarsActive
                ) +
                " complete=" +
                YesNo(
                    snapshot.vileFamiliarsComplete
                )
            );

            Debug::Logger::Info(
                "================================"
            );
        }
    };
}
