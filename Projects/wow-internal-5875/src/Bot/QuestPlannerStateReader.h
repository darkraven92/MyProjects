#pragma once

#include "GameThreadDispatcher.h"
#include "QuestPlannerTypes.h"

#include "../Debug/Logger.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace Bot
{
    class QuestPlannerStateReader
    {
    private:
        static constexpr std::uintptr_t DoStringRva =
            0x00304CD0;

        static constexpr std::uintptr_t GetTextRva =
            0x00303BF0;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_QUEST_PLANNER_STATE";

        static std::uintptr_t DoStringAddress()
        {
            return
                Wow5875::Client::Base() +
                DoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return
                Wow5875::Client::Base() +
                GetTextRva;
        }

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

            if (
                VirtualQuery(
                    reinterpret_cast<LPCVOID>(
                        address
                    ),
                    &info,
                    sizeof(info)
                ) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD protect =
                info.Protect & 0xFF;

            return
                protect == PAGE_EXECUTE ||
                protect == PAGE_EXECUTE_READ ||
                protect == PAGE_EXECUTE_READWRITE ||
                protect == PAGE_EXECUTE_WRITECOPY;
        }

        static bool Parse(
            const std::string& text,
            QuestPlannerSnapshot& snapshot)
        {
            snapshot = QuestPlannerSnapshot{};

            const auto first = text.find('^');
            if (first == std::string::npos)
                return false;

            const auto second = text.find('^', first + 1);
            if (second == std::string::npos)
                return false;

            const auto third = text.find('^', second + 1);
            if (third == std::string::npos)
                return false;

            snapshot.classToken = text.substr(0, first);
            const auto raceSeparator=snapshot.classToken.find('~');
            if(raceSeparator!=std::string::npos)
            {
                snapshot.raceToken=snapshot.classToken.substr(raceSeparator+1);
                snapshot.classToken.resize(raceSeparator);
            }

            try
            {
                snapshot.playerLevel = std::stoi(
                    text.substr(first + 1, second - first - 1));
                snapshot.rawQuestLogEntries = std::stoi(
                    text.substr(second + 1, third - second - 1));
            }
            catch (...)
            {
                return false;
            }

            const std::string payload = text.substr(third + 1);
            std::size_t start = 0;

            while (start < payload.size())
            {
                const auto end = payload.find(';', start);
                const std::string row = payload.substr(
                    start,
                    end == std::string::npos ? std::string::npos : end - start);

                if (!row.empty())
                {
                    /*
                     * Phase 13A row format:
                     *   title|questDone|leaderboardCount|completionBits
                     *
                     * The title is sanitized in Lua, so parsing from the end is
                     * deterministic and remains compatible with the old
                     * title|questDone|leaderboardCount format.
                     */
                    const auto pipe3 = row.rfind('|');
                    if (pipe3 == std::string::npos)
                        return false;

                    const auto pipe2 = row.rfind('|', pipe3 - 1);
                    if (pipe2 == std::string::npos)
                        return false;

                    const auto pipe1 = row.rfind('|', pipe2 - 1);

                    PlannerQuestLogEntry entry{};
                    std::string bits;

                    try
                    {
                        if (pipe1 == std::string::npos)
                        {
                            // Phase 11A/12B compatibility.
                            entry.title = row.substr(0, pipe2);
                            entry.complete = std::stoi(
                                row.substr(pipe2 + 1, pipe3 - pipe2 - 1)) != 0;
                            entry.objectiveCount = std::stoi(row.substr(pipe3 + 1));
                        }
                        else
                        {
                            entry.title = row.substr(0, pipe1);
                            entry.complete = std::stoi(
                                row.substr(pipe1 + 1, pipe2 - pipe1 - 1)) != 0;
                            entry.objectiveCount = std::stoi(
                                row.substr(pipe2 + 1, pipe3 - pipe2 - 1));
                            bits = row.substr(pipe3 + 1);
                        }
                    }
                    catch (...)
                    {
                        return false;
                    }

                    entry.objectiveComplete.reserve(
                        static_cast<std::size_t>(std::max(0, entry.objectiveCount)));

                    if (!bits.empty())
                    {
                        for (char bit : bits)
                        {
                            if (bit == '0' || bit == '1')
                                entry.objectiveComplete.push_back(bit == '1');
                        }
                    }

                    // If the client returned fewer bits than its reported row
                    // count, keep the unknown rows false rather than inventing
                    // completion.
                    while (static_cast<int>(entry.objectiveComplete.size()) <
                           entry.objectiveCount)
                    {
                        entry.objectiveComplete.push_back(false);
                    }

                    snapshot.quests.push_back(std::move(entry));
                }

                if (end == std::string::npos)
                    break;
                start = end + 1;
            }

            snapshot.valid = true;
            return true;
        }

    public:
        static bool Validate()
        {
            const auto doString =
                DoStringAddress();

            const auto getText =
                GetTextAddress();

            Debug::Logger::Info(
                "QuestPlanner DoString: " +
                Hex32(
                    doString
                )
            );

            Debug::Logger::Info(
                "QuestPlanner GetText: " +
                Hex32(
                    getText
                )
            );

            const bool valid =
                IsExecutable(
                    doString
                ) &&
                IsExecutable(
                    getText
                );

            Debug::Logger::Info(
                std::string(
                    "QuestPlannerStateReader validation "
                ) +
                (
                    valid
                        ? "PASS."
                        : "FAILED."
                )
            );

            return valid;
        }

        static bool Read(
            QuestPlannerSnapshot& snapshot)
        {
            snapshot =
                QuestPlannerSnapshot{};

            if (
                !IsExecutable(
                    DoStringAddress()
                ) ||
                !IsExecutable(
                    GetTextAddress()
                ))
            {
                return false;
            }

            /*
             * Phase 13A still reads state through the unmodified Vanilla Lua
             * API. In addition to title/whole-quest completion/count, capture
             * one completion bit per GetQuestLogLeaderBoard row. No localized
             * objective text needs to be parsed.
             */
            static constexpr char Script[] =
                "local out={}; "
                "local n=GetNumQuestLogEntries(); "
                "if n==nil then n=0 end; "
                "local _,ct=UnitClass('player'); "
                "if ct==nil then ct='UNKNOWN' end; "
                "local rt='UNKNOWN'; if UnitRace then local _,r=UnitRace('player'); rt=r or rt; end; "
                "local lvl=UnitLevel('player'); "
                "if lvl==nil then lvl=0 end; "
                "for i=1,n do "
                "local t,_,_,h,_,done=GetQuestLogTitle(i); "
                "if not h and t then "
                "local c=GetNumQuestLeaderBoards(i); "
                "if c==nil then c=0 end; "
                "local bits={}; "
                "for j=1,c do "
                "local _,_,od=GetQuestLogLeaderBoard(j,i); "
                "table.insert(bits,od and '1' or '0'); "
                "end; "
                "t=string.gsub(t,'[|;^]',' '); "
                "table.insert(out,"
                "t..'|'..(done and '1' or '0')..'|'..c..'|'..table.concat(bits,'')); "
                "end; "
                "end; "
                "WOW_INTERNAL_QUEST_PLANNER_STATE="
                "ct..'~'..rt..'^'..lvl..'^'..n..'^'..table.concat(out,';')";

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
                reinterpret_cast<DoStringFunction>(
                    DoStringAddress()
                );

            const auto getText =
                reinterpret_cast<GetTextFunction>(
                    GetTextAddress()
                );

            char result[4096]{};

            bool onGameThread =
                false;

            bool executed =
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

                        executed =
                            doString(
                                Script,
                                "wow-internal/QuestPlannerStateReader.lua"
                            );

                        if (!executed)
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
                !executed ||
                !gotText)
            {
                return false;
            }

            return
                Parse(
                    result,
                    snapshot
                );
        }

        static bool Same(
            const QuestPlannerSnapshot& a,
            const QuestPlannerSnapshot& b)
        {
            if (
                a.valid != b.valid ||
                a.classToken != b.classToken ||
                a.playerLevel != b.playerLevel ||
                a.rawQuestLogEntries !=
                    b.rawQuestLogEntries ||
                a.quests.size() !=
                    b.quests.size())
            {
                return false;
            }

            for (
                std::size_t i = 0;
                i < a.quests.size();
                ++i)
            {
                if (
                    a.quests[i].title !=
                        b.quests[i].title ||
                    a.quests[i].complete !=
                        b.quests[i].complete ||
                    a.quests[i].objectiveCount !=
                        b.quests[i].objectiveCount ||
                    a.quests[i].objectiveComplete !=
                        b.quests[i].objectiveComplete)
                {
                    return false;
                }
            }

            return true;
        }
    };
}
