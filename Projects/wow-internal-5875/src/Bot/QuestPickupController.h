#pragma once

#include "ClickToMoveController.h"
#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    enum class QuestPickupState
    {
        Idle,
        RoutingToNpc,
        FindingNpc,
        ApproachingNpc,
        AdvancingDialog,
        WaitingForQuestLog,
        Unavailable,
        Done,
        Failed
    };

    class QuestPickupController
    {
    private:
        struct Waypoint
        {
            float x;
            float y;
            float z;
            const char* name;
        };

        /*
         * WoW 1.12.1 build 5875.
         *
         * OnRightClickUnit absolute:
         *     0x0060BEA0
         *
         * Lua DoString absolute:
         *     0x00704CD0
         *
         * GetText absolute:
         *     0x00703BF0
         */
        static constexpr std::uintptr_t OnRightClickUnitRva =
            0x0020BEA0;

        static constexpr std::uintptr_t LuaDoStringRva =
            0x00304CD0;

        static constexpr std::uintptr_t GetTextRva =
            0x00303BF0;

        static constexpr std::uintptr_t ObjectManagerRootRva =
            0x00741414;

        static constexpr std::uintptr_t FirstObjectOffset =
            0x000000AC;

        static constexpr std::uintptr_t ObjectGuidOffset =
            0x00000030;

        static constexpr std::uintptr_t ObjectNextOffset =
            0x0000003C;

        static constexpr std::uint32_t ZureethaEntry =
            3145;

        static constexpr float GornekX =
            -600.132f;

        static constexpr float GornekY =
            -4186.190f;

        static constexpr float StartRadius =
            80.0f;

        static constexpr float WaypointTolerance =
            4.0f;

        static constexpr float InteractionDistance =
            5.0f;

        static constexpr std::uint64_t RouteStuckTicks =
            24;

        static constexpr std::uint64_t MoveCooldownTicks =
            6;

        static constexpr std::uint64_t DialogStepTicks =
            4;

        static constexpr std::uint64_t ReinteractTicks =
            24;

        static constexpr std::uint64_t OverallTimeoutTicks =
            240;

        static constexpr int MaximumRouteReissues =
            3;

        static constexpr int MaximumMoveCommands =
            10;

        static constexpr int MaximumInteractionAttempts =
            4;

        static constexpr int MaximumNotFoundResults =
            3;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_PICKUP_STATE";

        /*
         * Waypoint 1 is taken from the now-live-verified
         * Gornek -> south-exit corridor. It keeps CTM away
         * from the static object that blocked the earlier
         * direct route.
         *
         * Waypoint 2 is Zureetha Fargaze's Valley of Trials
         * position from the quest profile used for this
         * phase.
         */
        inline static constexpr Waypoint Route[] =
        {
            {
                -605.972f,
                -4219.559f,
                38.574f,
                "The Den south corridor"
            },
            {
                -629.052f,
                -4228.060f,
                38.2334f,
                "Zureetha Fargaze"
            }
        };

        QuestPickupState state_ =
            QuestPickupState::Idle;

        std::uint64_t startTick_ =
            0;

        std::uint64_t lastMoveTick_ =
            0;

        std::uint64_t lastProgressTick_ =
            0;

        std::uint64_t lastInteractionTick_ =
            0;

        std::uint64_t lastDialogTick_ =
            0;

        std::size_t waypointIndex_ =
            0;

        float bestWaypointDistance_ =
            0.0f;

        int routeReissues_ =
            0;

        int moveCommands_ =
            0;

        int interactionAttempts_ =
            0;

        int dialogActions_ =
            0;

        int notFoundResults_ =
            0;

        std::uint64_t zureethaGuid_ =
            0;

        std::string lastDialogResult_{};

        /*
         * Phase 11B.3 keeps the proven Zureetha route/interaction machinery
         * but lets the planner choose the exact quest title to accept.
         * The route is intentionally bounded to giver entry 3145 in this
         * phase; other givers remain disabled until separately profiled.
         */
        int configuredQuestId_ =
            792;

        std::string configuredQuestTitle_ =
            "Vile Familiars";

        std::uint32_t configuredGiverEntry_ =
            ZureethaEntry;

        std::string configuredGiverName_ =
            "Zureetha Fargaze";

        static std::string LuaSingleQuoted(
            const std::string& value)
        {
            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('\'');

            for (const char ch : value)
            {
                switch (ch)
                {
                    case '\\':
                        result += "\\\\";
                        break;
                    case '\'':
                        result += "\\'";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    default:
                        result.push_back(ch);
                        break;
                }
            }

            result.push_back('\'');
            return result;
        }

        static float Distance2D(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx =
                bx - ax;

            const float dy =
                by - ay;

            return std::sqrt(
                dx * dx +
                dy * dy
            );
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

        static std::string Hex64(
            std::uint64_t value)
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

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(3)
                << value;

            return stream.str();
        }

        static constexpr std::size_t RouteCount()
        {
            return
                sizeof(Route) /
                sizeof(Route[0]);
        }

        static const char* StateNameInternal(
            QuestPickupState state)
        {
            switch (state)
            {
                case QuestPickupState::Idle:
                    return "Idle";

                case QuestPickupState::RoutingToNpc:
                    return "RoutingToNpc";

                case QuestPickupState::FindingNpc:
                    return "FindingNpc";

                case QuestPickupState::ApproachingNpc:
                    return "ApproachingNpc";

                case QuestPickupState::AdvancingDialog:
                    return "AdvancingDialog";

                case QuestPickupState::WaitingForQuestLog:
                    return "WaitingForQuestLog";

                case QuestPickupState::Unavailable:
                    return "Unavailable";

                case QuestPickupState::Done:
                    return "Done";

                case QuestPickupState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            QuestPickupState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "QuestPickupController state: "
                ) +
                StateNameInternal(
                    state_
                ) +
                " -> " +
                StateNameInternal(
                    newState
                )
            );

            state_ =
                newState;
        }

        void Fail(
            const std::string& reason)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PICKUP: FAILED"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "Zureetha GUID: " +
                Hex64(
                    zureethaGuid_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestPickupState::Failed
            );
        }

        void MarkUnavailableInternal(
            const std::string& reason)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PICKUP: NOT OFFERED"
            );

            Debug::Logger::Info(
                "Quest: " +
                std::to_string(configuredQuestId_) +
                " " +
                configuredQuestTitle_
            );

            Debug::Logger::Info(
                "NPC: " +
                configuredGiverName_ +
                " (" +
                std::to_string(configuredGiverEntry_) +
                ")"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "Exact-title policy prevented accepting any "
                "other available quest."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestPickupState::Unavailable
            );
        }

        template <typename T>
        static bool ReadValue(
            std::uintptr_t address,
            T& value)
        {
            MEMORY_BASIC_INFORMATION info{};

            if (
                address == 0 ||
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

            std::memcpy(
                &value,
                reinterpret_cast<const void*>(
                    address
                ),
                sizeof(T)
            );

            return true;
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

        static std::uintptr_t OnRightClickUnitAddress()
        {
            return
                Wow5875::Client::Base() +
                OnRightClickUnitRva;
        }

        static std::uintptr_t LuaDoStringAddress()
        {
            return
                Wow5875::Client::Base() +
                LuaDoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return
                Wow5875::Client::Base() +
                GetTextRva;
        }

        static std::uintptr_t FindObjectAddressByGuid(
            std::uint64_t guid)
        {
            if (guid == 0)
            {
                return 0;
            }

            std::uint32_t manager =
                0;

            if (!ReadValue(
                    Wow5875::Client::Base() +
                        ObjectManagerRootRva,
                    manager))
            {
                return 0;
            }

            if (
                manager == 0 ||
                (manager & 1u) != 0)
            {
                return 0;
            }

            std::uint32_t current =
                0;

            if (!ReadValue(
                    static_cast<std::uintptr_t>(
                        manager
                    ) +
                        FirstObjectOffset,
                    current))
            {
                return 0;
            }

            for (int i = 0; i < 4096; ++i)
            {
                if (
                    current == 0 ||
                    (current & 1u) != 0)
                {
                    break;
                }

                std::uint64_t currentGuid =
                    0;

                if (!ReadValue(
                        static_cast<std::uintptr_t>(
                            current
                        ) +
                            ObjectGuidOffset,
                        currentGuid))
                {
                    break;
                }

                if (currentGuid == guid)
                {
                    return
                        static_cast<std::uintptr_t>(
                            current
                        );
                }

                std::uint32_t next =
                    0;

                if (!ReadValue(
                        static_cast<std::uintptr_t>(
                            current
                        ) +
                            ObjectNextOffset,
                        next))
                {
                    break;
                }

                if (next == current)
                {
                    break;
                }

                current =
                    next;
            }

            return 0;
        }

        const Objects::UnitState* FindConfiguredGiver(
            const Objects::WorldState& world) const
        {
            const Objects::UnitState* best =
                nullptr;

            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.entryId !=
                        configuredGiverEntry_)
                {
                    continue;
                }

                if (
                    best == nullptr ||
                    unit.distance <
                        best->distance)
                {
                    best =
                        &unit;
                }
            }

            return best;
        }

        bool IssueRouteWaypoint(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            bool reissue)
        {
            if (
                waypointIndex_ >=
                    RouteCount())
            {
                SetState(
                    QuestPickupState::FindingNpc
                );

                return true;
            }

            if (
                moveCommands_ >=
                    MaximumMoveCommands)
            {
                return false;
            }

            const auto& waypoint =
                Route[
                    waypointIndex_
                ];

            const float distance =
                Distance2D(
                    player.x,
                    player.y,
                    waypoint.x,
                    waypoint.y
                );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                reissue
                    ? "QUEST PICKUP: reissuing route waypoint."
                    : "QUEST PICKUP: moving to route waypoint."
            );

            Debug::Logger::Info(
                "Waypoint " +
                std::to_string(
                    waypointIndex_ + 1
                ) +
                "/" +
                std::to_string(
                    RouteCount()
                ) +
                ": " +
                waypoint.name
            );

            Debug::Logger::Info(
                "Player: (" +
                Float(player.x) +
                "," +
                Float(player.y) +
                "," +
                Float(player.z) +
                ")"
            );

            Debug::Logger::Info(
                "Destination: (" +
                Float(waypoint.x) +
                "," +
                Float(waypoint.y) +
                "," +
                Float(waypoint.z) +
                ")"
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    distance
                )
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        waypoint.x,
                        waypoint.y,
                        waypoint.z,
                        1.0f
                    ))
            {
                Debug::Logger::Info(
                    "QUEST PICKUP: CTM route command failed."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            ++moveCommands_;

            if (reissue)
            {
                ++routeReissues_;
            }

            lastMoveTick_ =
                tick;

            lastProgressTick_ =
                tick;

            bestWaypointDistance_ =
                distance;

            Debug::Logger::Info(
                "QUEST PICKUP: CTM route command issued."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestPickupState::RoutingToNpc
            );

            return true;
        }

        bool IssueApproach(
            const Objects::PlayerState& player,
            const Objects::UnitState& zureetha,
            std::uint64_t tick)
        {
            if (
                moveCommands_ >=
                    MaximumMoveCommands)
            {
                return false;
            }

            Debug::Logger::Info(
                "QUEST PICKUP: approaching " +
                configuredGiverName_ +
                "."
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    zureetha.distance
                )
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        zureetha.x,
                        zureetha.y,
                        zureetha.z,
                        1.5f
                    ))
            {
                return false;
            }

            ++moveCommands_;

            lastMoveTick_ =
                tick;

            SetState(
                QuestPickupState::
                    ApproachingNpc
            );

            return true;
        }

        bool IssueInteraction(
            const Objects::UnitState& zureetha,
            std::uint64_t tick)
        {
            if (
                interactionAttempts_ >=
                    MaximumInteractionAttempts)
            {
                return false;
            }

            const auto functionAddress =
                OnRightClickUnitAddress();

            if (!IsExecutable(
                    functionAddress))
            {
                return false;
            }

            const std::uintptr_t objectAddress =
                FindObjectAddressByGuid(
                    zureetha.guid
                );

            if (objectAddress == 0)
            {
                Debug::Logger::Info(
                    "QUEST PICKUP: configured giver object pointer "
                    "not found."
                );

                return false;
            }

            using OnRightClickUnitFunction =
                void (__thiscall*)(
                    std::uint32_t,
                    int
                );

            const auto onRightClickUnit =
                reinterpret_cast<
                    OnRightClickUnitFunction
                >(
                    functionAddress
                );

            const std::uint32_t zureethaThis =
                static_cast<std::uint32_t>(
                    objectAddress
                );

            bool onGameThread =
                false;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PICKUP: interacting with " +
                configuredGiverName_ +
                "."
            );

            Debug::Logger::Info(
                "Zureetha GUID: " +
                Hex64(
                    zureetha.guid
                )
            );

            Debug::Logger::Info(
                "Zureetha object: " +
                Hex32(
                    objectAddress
                )
            );

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            std::string(
                                "Quest pickup interaction on "
                                "game thread: "
                            ) +
                            (
                                onGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        onRightClickUnit(
                            zureethaThis,
                            0
                        );
                    }
                );

            Debug::Logger::Info(
                "================================"
            );

            if (
                !dispatched ||
                !onGameThread)
            {
                return false;
            }

            zureethaGuid_ =
                zureetha.guid;

            ++interactionAttempts_;

            lastInteractionTick_ =
                tick;

            lastDialogTick_ =
                tick;

            SetState(
                QuestPickupState::
                    AdvancingDialog
            );

            return true;
        }

        bool AdvanceDialog(
            std::uint64_t tick)
        {
            const auto doStringAddress =
                LuaDoStringAddress();

            const auto getTextAddress =
                GetTextAddress();

            if (
                !IsExecutable(
                    doStringAddress
                ) ||
                !IsExecutable(
                    getTextAddress
                ))
            {
                return false;
            }

            /*
             * Vanilla 1.12 can show available quests either
             * in GossipFrame or QuestFrameGreetingPanel.
             *
             * We select only the exact title "Vile Familiars".
             * No other available quest is touched.
             *
             * Once QuestFrameDetailPanel is showing that exact
             * title, AcceptQuest() is issued.
             */
            const std::string questTitle =
                LuaSingleQuoted(
                    configuredQuestTitle_
                );

            const std::string script =
                "WOW_INTERNAL_PICKUP_STATE='waiting'; "
                "local t=GetTitleText(); "
                "if QuestFrameDetailPanel and "
                "QuestFrameDetailPanel:IsVisible() and "
                "t==" + questTitle + " then "
                "WOW_INTERNAL_PICKUP_STATE='accept'; "
                "AcceptQuest(); "
                "elseif QuestFrameGreetingPanel and "
                "QuestFrameGreetingPanel:IsVisible() then "
                "local n=GetNumAvailableQuests(); "
                "local f=0; "
                "for i=1,n do "
                "if GetAvailableTitle(i)==" + questTitle + " then "
                "WOW_INTERNAL_PICKUP_STATE='greeting_select'; "
                "SelectAvailableQuest(i); "
                "f=1; break; "
                "end; "
                "end; "
                "if f==0 then "
                "WOW_INTERNAL_PICKUP_STATE='greeting_not_found'; "
                "end; "
                "elseif GossipFrame and GossipFrame:IsVisible() then "
                "local a={GetGossipAvailableQuests()}; "
                "local q=1; local f=0; "
                "for i=1,table.getn(a),2 do "
                "if a[i]==" + questTitle + " then "
                "WOW_INTERNAL_PICKUP_STATE='gossip_select'; "
                "SelectGossipAvailableQuest(q); "
                "f=1; break; "
                "end; "
                "q=q+1; "
                "end; "
                "if f==0 then "
                "WOW_INTERNAL_PICKUP_STATE='gossip_not_found'; "
                "end; "
                "end";

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
                    doStringAddress
                );

            const auto getText =
                reinterpret_cast<
                    GetTextFunction
                >(
                    getTextAddress
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
                                script.c_str(),
                                "wow-internal/QuestPickupController.lua"
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
                return false;
            }

            const std::string current =
                result;

            if (
                current !=
                    lastDialogResult_)
            {
                Debug::Logger::Info(
                    "QUEST PICKUP dialog: " +
                    current
                );

                lastDialogResult_ =
                    current;
            }

            if (
                current ==
                    "greeting_select" ||
                current ==
                    "gossip_select")
            {
                ++dialogActions_;

                notFoundResults_ =
                    0;

                lastDialogTick_ =
                    tick;

                return true;
            }

            if (current == "accept")
            {
                ++dialogActions_;

                notFoundResults_ =
                    0;

                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST PICKUP: AcceptQuest() issued."
                );

                Debug::Logger::Info(
                    "Quest: " +
                std::to_string(configuredQuestId_) +
                " " +
                configuredQuestTitle_
                );

                Debug::Logger::Info(
                    "Waiting for QuestStateReader to "
                    "verify quest activation."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestPickupState::
                        WaitingForQuestLog
                );

                return true;
            }

            if (
                current ==
                    "greeting_not_found" ||
                current ==
                    "gossip_not_found")
            {
                ++notFoundResults_;

                if (
                    notFoundResults_ >=
                        MaximumNotFoundResults)
                {
                    MarkUnavailableInternal(
                        configuredQuestTitle_ +
                        " was not present in " +
                        configuredGiverName_ +
                        "'s available quest list."
                    );

                    return true;
                }

                return true;
            }

            return true;
        }

    public:
        static bool ShouldStart(
            const Objects::PlayerState& player)
        {
            if (!player.valid)
            {
                return false;
            }

            return
                Distance2D(
                    player.x,
                    player.y,
                    GornekX,
                    GornekY
                ) <=
                StartRadius;
        }

        bool ConfigureExactQuest(
            int questId,
            const std::string& questTitle,
            std::uint32_t giverEntry,
            const std::string& giverName)
        {
            if (
                state_ != QuestPickupState::Idle ||
                questId <= 0 ||
                questTitle.empty() ||
                giverEntry == 0 ||
                giverName.empty())
            {
                return false;
            }

            /*
             * Phase 11B.3 intentionally reuses only the already verified
             * route to Zureetha. This prevents a supposedly generic pickup
             * request from silently navigating to an unprofiled NPC.
             */
            if (giverEntry != ZureethaEntry)
            {
                Debug::Logger::Info(
                    "QUEST PICKUP 11B.3: giver entry " +
                    std::to_string(giverEntry) +
                    " is not enabled in this bounded phase."
                );

                return false;
            }

            configuredQuestId_ =
                questId;

            configuredQuestTitle_ =
                questTitle;

            configuredGiverEntry_ =
                giverEntry;

            configuredGiverName_ =
                giverName;

            return true;
        }

        static bool Validate()
        {
            const auto rightClick =
                OnRightClickUnitAddress();

            const auto doString =
                LuaDoStringAddress();

            const auto getText =
                GetTextAddress();

            Debug::Logger::Info(
                "Quest pickup OnRightClickUnit: " +
                Hex32(
                    rightClick
                )
            );

            Debug::Logger::Info(
                "Quest pickup DoString: " +
                Hex32(
                    doString
                )
            );

            Debug::Logger::Info(
                "Quest pickup GetText: " +
                Hex32(
                    getText
                )
            );

            if (
                !IsExecutable(
                    rightClick
                ) ||
                !IsExecutable(
                    doString
                ) ||
                !IsExecutable(
                    getText
                ))
            {
                Debug::Logger::Info(
                    "QuestPickupController validation FAILED."
                );

                return false;
            }

            Debug::Logger::Info(
                "QuestPickupController validation PASS."
            );

            return true;
        }

        bool Start(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (
                state_ !=
                    QuestPickupState::Idle)
            {
                return false;
            }

            if (
                !player.valid ||
                !Validate())
            {
                return false;
            }

            startTick_ =
                tick;

            lastMoveTick_ =
                tick;

            lastProgressTick_ =
                tick;

            lastInteractionTick_ =
                0;

            lastDialogTick_ =
                tick;

            waypointIndex_ =
                0;

            bestWaypointDistance_ =
                0.0f;

            routeReissues_ =
                0;

            moveCommands_ =
                0;

            interactionAttempts_ =
                0;

            dialogActions_ =
                0;

            notFoundResults_ =
                0;

            zureethaGuid_ =
                0;

            lastDialogResult_.clear();

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PICKUP: START"
            );

            Debug::Logger::Info(
                "Quest: " +
                std::to_string(configuredQuestId_) +
                " " +
                configuredQuestTitle_
            );

            Debug::Logger::Info(
                "NPC: " +
                configuredGiverName_ +
                " (" +
                std::to_string(configuredGiverEntry_) +
                ")"
            );

            Debug::Logger::Info(
                "Policy: route to Zureetha, select only "
                "the exact quest title, call AcceptQuest(), "
                "then wait for quest-log verification."
            );

            Debug::Logger::Info(
                "================================"
            );

            if (!IssueRouteWaypoint(
                    player,
                    tick,
                    false))
            {
                Fail(
                    "failed to start route to Zureetha."
                );

                return false;
            }

            return true;
        }

        void Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (
                state_ ==
                    QuestPickupState::Idle ||
                state_ ==
                    QuestPickupState::Done ||
                state_ ==
                    QuestPickupState::Unavailable ||
                state_ ==
                    QuestPickupState::Failed)
            {
                return;
            }

            if (
                tick >
                    startTick_ +
                    OverallTimeoutTicks)
            {
                Fail(
                    "overall pickup timeout."
                );

                return;
            }

            if (
                state_ ==
                    QuestPickupState::RoutingToNpc)
            {
                if (
                    waypointIndex_ >=
                        RouteCount())
                {
                    SetState(
                        QuestPickupState::FindingNpc
                    );

                    return;
                }

                const auto& waypoint =
                    Route[
                        waypointIndex_
                    ];

                const float distance =
                    Distance2D(
                        world.player.x,
                        world.player.y,
                        waypoint.x,
                        waypoint.y
                    );

                if (
                    distance <=
                        WaypointTolerance)
                {
                    Debug::Logger::Info(
                        "QUEST PICKUP: reached waypoint " +
                        std::to_string(
                            waypointIndex_ + 1
                        ) +
                        "/" +
                        std::to_string(
                            RouteCount()
                        ) +
                        " (" +
                        waypoint.name +
                        ")."
                    );

                    ++waypointIndex_;

                    routeReissues_ =
                        0;

                    if (
                        waypointIndex_ >=
                            RouteCount())
                    {
                        Debug::Logger::Info(
                            "QUEST PICKUP: route to "
                            "Zureetha area complete."
                        );

                        SetState(
                            QuestPickupState::
                                FindingNpc
                        );

                        return;
                    }

                    if (!IssueRouteWaypoint(
                            world.player,
                            tick,
                            false))
                    {
                        Fail(
                            "failed to issue next pickup "
                            "route waypoint."
                        );
                    }

                    return;
                }

                if (
                    distance <
                        bestWaypointDistance_ -
                        0.75f)
                {
                    bestWaypointDistance_ =
                        distance;

                    lastProgressTick_ =
                        tick;
                }

                if (
                    tick <
                        lastProgressTick_ +
                        RouteStuckTicks)
                {
                    return;
                }

                if (
                    routeReissues_ >=
                        MaximumRouteReissues)
                {
                    Fail(
                        "no progress on route to Zureetha."
                    );

                    return;
                }

                if (!IssueRouteWaypoint(
                        world.player,
                        tick,
                        true))
                {
                    Fail(
                        "failed to reissue pickup "
                        "route waypoint."
                    );
                }

                return;
            }

            const auto* zureetha =
                FindConfiguredGiver(
                    world
                );

            if (
                state_ ==
                    QuestPickupState::FindingNpc ||
                state_ ==
                    QuestPickupState::ApproachingNpc)
            {
                if (zureetha == nullptr)
                {
                    /*
                     * Zureetha should be in range after the
                     * two-point route. A short retry window
                     * handles object-manager refresh.
                     */
                    if (
                        tick >=
                            lastMoveTick_ +
                            20)
                    {
                        MarkUnavailableInternal(
                            configuredGiverName_ +
                            " was not present in the local object list "
                            "after the verified approach route."
                        );
                    }

                    return;
                }

                zureethaGuid_ =
                    zureetha->guid;

                if (
                    zureetha->distance >
                        InteractionDistance)
                {
                    if (
                        tick <
                            lastMoveTick_ +
                            MoveCooldownTicks)
                    {
                        return;
                    }

                    if (!IssueApproach(
                            world.player,
                            *zureetha,
                            tick))
                    {
                        Fail(
                            "failed to approach configured quest giver."
                        );
                    }

                    return;
                }

                if (!IssueInteraction(
                        *zureetha,
                        tick))
                {
                    Fail(
                        "failed to interact with configured quest giver."
                    );
                }

                return;
            }

            if (
                state_ ==
                    QuestPickupState::AdvancingDialog)
            {
                if (
                    tick >=
                        lastDialogTick_ +
                        DialogStepTicks)
                {
                    if (!AdvanceDialog(
                            tick))
                    {
                        Fail(
                            "failed to advance quest "
                            "pickup dialog."
                        );

                        return;
                    }

                    lastDialogTick_ =
                        tick;
                }

                if (
                    state_ ==
                        QuestPickupState::
                            AdvancingDialog &&
                    tick >=
                        lastInteractionTick_ +
                        ReinteractTicks)
                {
                    if (
                        interactionAttempts_ >=
                            MaximumInteractionAttempts)
                    {
                        MarkUnavailableInternal(
                            "quest dialog did not reach " +
                            configuredQuestTitle_ +
                            " after repeated interactions."
                        );

                        return;
                    }

                    SetState(
                        QuestPickupState::FindingNpc
                    );
                }

                return;
            }

            if (
                state_ ==
                    QuestPickupState::
                        WaitingForQuestLog)
            {
                return;
            }
        }

        void MarkQuestActive()
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PICKUP: PASS"
            );

            Debug::Logger::Info(
                "Quest " +
                std::to_string(configuredQuestId_) +
                " " +
                configuredQuestTitle_ +
                " is now active in the quest log."
            );

            Debug::Logger::Info(
                "Quest-log verification matched the exact "
                "planner-selected quest."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestPickupState::Done
            );
        }

        void Reset()
        {
            state_ =
                QuestPickupState::Idle;

            startTick_ =
                0;

            lastMoveTick_ =
                0;

            lastProgressTick_ =
                0;

            lastInteractionTick_ =
                0;

            lastDialogTick_ =
                0;

            waypointIndex_ =
                0;

            bestWaypointDistance_ =
                0.0f;

            routeReissues_ =
                0;

            moveCommands_ =
                0;

            interactionAttempts_ =
                0;

            dialogActions_ =
                0;

            notFoundResults_ =
                0;

            zureethaGuid_ =
                0;

            lastDialogResult_.clear();

            configuredQuestId_ =
                792;

            configuredQuestTitle_ =
                "Vile Familiars";

            configuredGiverEntry_ =
                ZureethaEntry;

            configuredGiverName_ =
                "Zureetha Fargaze";
        }

        QuestPickupState State() const
        {
            return state_;
        }

        const char* StateName() const
        {
            return
                StateNameInternal(
                    state_
                );
        }

        bool IsActive() const
        {
            return
                state_ !=
                    QuestPickupState::Idle &&
                state_ !=
                    QuestPickupState::Done &&
                state_ !=
                    QuestPickupState::Unavailable &&
                state_ !=
                    QuestPickupState::Failed;
        }

        bool IsDone() const
        {
            return
                state_ ==
                    QuestPickupState::Done;
        }

        bool IsUnavailable() const
        {
            return
                state_ ==
                    QuestPickupState::Unavailable;
        }

        bool Failed() const
        {
            return
                state_ ==
                    QuestPickupState::Failed;
        }

        std::size_t WaypointIndex() const
        {
            return waypointIndex_;
        }

        int MoveCommands() const
        {
            return moveCommands_;
        }

        int InteractionAttempts() const
        {
            return interactionAttempts_;
        }

        int DialogActions() const
        {
            return dialogActions_;
        }

        int NotFoundResults() const
        {
            return notFoundResults_;
        }

        std::uint64_t NpcGuid() const
        {
            return zureethaGuid_;
        }
    };
}
