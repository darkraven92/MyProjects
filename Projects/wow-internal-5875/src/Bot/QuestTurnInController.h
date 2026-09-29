#pragma once

#include "ClickToMoveController.h"
#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    enum class QuestTurnInState
    {
        Idle,
        FindingNpc,
        ApproachingNpc,
        Interacting,
        AdvancingDialog,
        RewardChoiceRequired,
        WaitingForQuestRemoval,
        Done,
        Failed
    };

    class QuestTurnInController
    {
    private:
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

        static constexpr std::uint32_t GornekEntry =
            3143;

        static constexpr float InteractionDistance =
            5.0f;

        static constexpr std::uint64_t MoveCooldownTicks =
            4;

        static constexpr std::uint64_t InteractionRetryTicks =
            8;

        static constexpr std::uint64_t DialogStepTicks =
            4;

        static constexpr std::uint64_t NpcSearchTimeoutTicks =
            80;

        static constexpr std::uint64_t OverallTimeoutTicks =
            160;

        static constexpr int MaximumMoveCommands =
            8;

        static constexpr int MaximumInteractionAttempts =
            4;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_TURNIN_STATE";

        QuestTurnInState state_ =
            QuestTurnInState::Idle;

        std::uint64_t startTick_ =
            0;

        std::uint64_t lastMoveTick_ =
            0;

        std::uint64_t lastInteractionTick_ =
            0;

        std::uint64_t lastDialogTick_ =
            0;

        int moveCommands_ =
            0;

        int interactionAttempts_ =
            0;

        int dialogActions_ =
            0;

        int rewardChoices_ =
            0;

        std::uint64_t gornekGuid_ =
            0;

        std::string lastDialogResult_{};

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

        static const char* StateNameInternal(
            QuestTurnInState state)
        {
            switch (state)
            {
                case QuestTurnInState::Idle:
                    return "Idle";

                case QuestTurnInState::FindingNpc:
                    return "FindingNpc";

                case QuestTurnInState::ApproachingNpc:
                    return "ApproachingNpc";

                case QuestTurnInState::Interacting:
                    return "Interacting";

                case QuestTurnInState::AdvancingDialog:
                    return "AdvancingDialog";

                case QuestTurnInState::RewardChoiceRequired:
                    return "RewardChoiceRequired";

                case QuestTurnInState::WaitingForQuestRemoval:
                    return "WaitingForQuestRemoval";

                case QuestTurnInState::Done:
                    return "Done";

                case QuestTurnInState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            QuestTurnInState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "QuestTurnInController state: "
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
                "QUEST TURN-IN: FAILED"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "Gornek GUID: " +
                Hex64(
                    gornekGuid_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestTurnInState::Failed
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

        static const Objects::UnitState* FindGornek(
            const Objects::WorldState& world)
        {
            const Objects::UnitState* best =
                nullptr;

            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.entryId !=
                        GornekEntry)
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

        bool IssueApproach(
            const Objects::PlayerState& player,
            const Objects::UnitState& gornek,
            std::uint64_t tick)
        {
            if (
                moveCommands_ >=
                    MaximumMoveCommands)
            {
                return false;
            }

            Debug::Logger::Info(
                "QUEST TURN-IN: approaching Gornek."
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    gornek.distance
                )
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        gornek.x,
                        gornek.y,
                        gornek.z,
                        1.5f
                    ))
            {
                return false;
            }

            ++moveCommands_;

            lastMoveTick_ =
                tick;

            SetState(
                QuestTurnInState::
                    ApproachingNpc
            );

            return true;
        }

        bool IssueInteraction(
            const Objects::UnitState& gornek,
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
                    gornek.guid
                );

            if (objectAddress == 0)
            {
                Debug::Logger::Info(
                    "QUEST TURN-IN: Gornek object pointer "
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

            const std::uint32_t gornekThis =
                static_cast<std::uint32_t>(
                    objectAddress
                );

            bool onGameThread =
                false;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST TURN-IN: interacting with Gornek."
            );

            Debug::Logger::Info(
                "Gornek GUID: " +
                Hex64(
                    gornek.guid
                )
            );

            Debug::Logger::Info(
                "Gornek object: " +
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
                                "Quest interaction on "
                                "game thread: "
                            ) +
                            (
                                onGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        onRightClickUnit(
                            gornekThis,
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

            gornekGuid_ =
                gornek.guid;

            ++interactionAttempts_;

            lastInteractionTick_ =
                tick;

            lastDialogTick_ =
                tick;

            SetState(
                QuestTurnInState::
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
             * Vanilla 1.12 has two relevant NPC quest list
             * presentations:
             *
             *   QuestFrameGreetingPanel
             *   GossipFrame
             *
             * We match only the exact quest title, then
             * advance one UI state per invocation.
             *
             * Phase 8B automates the verified reward panel
             * for quest 789, but only when the expected
             * warrior reward "Battleworn Cape" is present.
             *
             * If that exact reward cannot be found, the
             * controller stops at RewardChoiceRequired rather
             * than selecting an unknown item.
             */
            static constexpr char Script[] =
                "WOW_INTERNAL_TURNIN_STATE='waiting'; "
                "local t=GetTitleText(); "
                "if QuestFrameRewardPanel and "
                "QuestFrameRewardPanel:IsVisible() and "
                "t=='Sting of the Scorpid' then "
                "local c=GetNumQuestChoices(); "
                "if c and c>0 then "
                "local pick=0; "
                "local pickName=''; "
                "for i=1,c do "
                "local n=GetQuestItemInfo('choice',i); "
                "if n=='Battleworn Cape' then "
                "pick=i; pickName=n; break; "
                "end; "
                "end; "
                "if pick>0 then "
                "WOW_INTERNAL_TURNIN_STATE="
                "'reward_auto:'..pick..':'..pickName..':'..c; "
                "GetQuestReward(pick); "
                "else "
                "WOW_INTERNAL_TURNIN_STATE="
                "'reward_expected_not_found:'..c; "
                "end; "
                "else "
                "WOW_INTERNAL_TURNIN_STATE='reward_claim'; "
                "GetQuestReward(0); "
                "end; "
                "elseif QuestFrameProgressPanel and "
                "QuestFrameProgressPanel:IsVisible() and "
                "t=='Sting of the Scorpid' then "
                "if IsQuestCompletable() then "
                "WOW_INTERNAL_TURNIN_STATE='progress_complete'; "
                "CompleteQuest(); "
                "else "
                "WOW_INTERNAL_TURNIN_STATE='progress_blocked'; "
                "end; "
                "elseif QuestFrameGreetingPanel and "
                "QuestFrameGreetingPanel:IsVisible() then "
                "local n=GetNumActiveQuests(); "
                "local f=0; "
                "for i=1,n do "
                "if GetActiveTitle(i)=='Sting of the Scorpid' then "
                "WOW_INTERNAL_TURNIN_STATE='greeting_select'; "
                "SelectActiveQuest(i); "
                "f=1; break; "
                "end; "
                "end; "
                "if f==0 then "
                "WOW_INTERNAL_TURNIN_STATE='greeting_not_found'; "
                "end; "
                "elseif GossipFrame and GossipFrame:IsVisible() then "
                "local a={GetGossipActiveQuests()}; "
                "local q=1; local f=0; "
                "for i=1,table.getn(a),2 do "
                "if a[i]=='Sting of the Scorpid' then "
                "WOW_INTERNAL_TURNIN_STATE='gossip_select'; "
                "SelectGossipActiveQuest(q); "
                "f=1; break; "
                "end; "
                "q=q+1; "
                "end; "
                "if f==0 then "
                "WOW_INTERNAL_TURNIN_STATE='gossip_not_found'; "
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
                                Script,
                                "wow-internal/QuestTurnInController.lua"
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

            ++dialogActions_;

            lastDialogTick_ =
                tick;

            const std::string current =
                result;

            if (current != lastDialogResult_)
            {
                Debug::Logger::Info(
                    "QUEST TURN-IN dialog: " +
                    current
                );

                lastDialogResult_ =
                    current;
            }

            if (
                current.rfind(
                    "reward_auto:",
                    0
                ) == 0)
            {
                /*
                 * Expected shape:
                 *
                 *   reward_auto:<index>:Battleworn Cape:<count>
                 */
                const std::size_t firstColon =
                    current.find(':');

                const std::size_t secondColon =
                    current.find(
                        ':',
                        firstColon + 1
                    );

                const std::size_t thirdColon =
                    current.rfind(':');

                int selectedIndex =
                    0;

                int choiceCount =
                    0;

                std::string selectedName;

                if (
                    firstColon !=
                        std::string::npos &&
                    secondColon !=
                        std::string::npos &&
                    thirdColon !=
                        std::string::npos &&
                    secondColon <
                        thirdColon)
                {
                    selectedIndex =
                        std::atoi(
                            current.substr(
                                firstColon + 1,
                                secondColon -
                                    firstColon -
                                    1
                            ).c_str()
                        );

                    selectedName =
                        current.substr(
                            secondColon + 1,
                            thirdColon -
                                secondColon -
                                1
                        );

                    choiceCount =
                        std::atoi(
                            current.substr(
                                thirdColon + 1
                            ).c_str()
                        );
                }

                rewardChoices_ =
                    choiceCount;

                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST TURN-IN: AUTO REWARD SELECTED"
                );

                Debug::Logger::Info(
                    "Quest: 789 Sting of the Scorpid"
                );

                Debug::Logger::Info(
                    "Reward: " +
                    (
                        selectedName.empty()
                            ? std::string(
                                "Battleworn Cape"
                            )
                            : selectedName
                    )
                );

                Debug::Logger::Info(
                    "Reward index: " +
                    std::to_string(
                        selectedIndex
                    ) +
                    "/" +
                    std::to_string(
                        choiceCount
                    )
                );

                Debug::Logger::Info(
                    "Waiting for QuestStateReader to "
                    "verify quest removal."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestTurnInState::
                        WaitingForQuestRemoval
                );

                return true;
            }

            if (
                current.rfind(
                    "reward_expected_not_found:",
                    0
                ) == 0)
            {
                rewardChoices_ =
                    std::atoi(
                        current.c_str() +
                        std::strlen(
                            "reward_expected_not_found:"
                        )
                    );

                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST TURN-IN: EXPECTED REWARD NOT FOUND"
                );

                Debug::Logger::Info(
                    "Expected reward: Battleworn Cape"
                );

                Debug::Logger::Info(
                    "Choices reported by client: " +
                    std::to_string(
                        rewardChoices_
                    )
                );

                Debug::Logger::Info(
                    "No reward was selected. Falling back "
                    "to manual reward choice for safety."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestTurnInState::
                        RewardChoiceRequired
                );

                return true;
            }

            if (
                current.rfind(
                    "reward_choice:",
                    0
                ) == 0)
            {
                rewardChoices_ =
                    std::atoi(
                        current.c_str() +
                        std::strlen(
                            "reward_choice:"
                        )
                    );

                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST TURN-IN: MANUAL REWARD FALLBACK"
                );

                Debug::Logger::Info(
                    "Quest: 789 Sting of the Scorpid"
                );

                Debug::Logger::Info(
                    "Choices: " +
                    std::to_string(
                        rewardChoices_
                    )
                );

                Debug::Logger::Info(
                    "Phase 8B reached the legacy manual "
                    "reward fallback path."
                );

                Debug::Logger::Info(
                    "Select the reward manually; the bot "
                    "will still verify quest removal."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestTurnInState::
                        RewardChoiceRequired
                );

                return true;
            }

            if (current == "reward_claim")
            {
                SetState(
                    QuestTurnInState::
                        WaitingForQuestRemoval
                );

                return true;
            }

            if (
                current == "greeting_not_found" ||
                current == "gossip_not_found" ||
                current == "progress_blocked")
            {
                Debug::Logger::Info(
                    "QUEST TURN-IN: dialog did not offer "
                    "a completable Sting of the Scorpid."
                );
            }

            return true;
        }

    public:
        static bool Validate()
        {
            const auto rightClick =
                OnRightClickUnitAddress();

            const auto doString =
                LuaDoStringAddress();

            const auto getText =
                GetTextAddress();

            Debug::Logger::Info(
                "Quest TurnIn OnRightClickUnit address: " +
                Hex32(
                    rightClick
                )
            );

            Debug::Logger::Info(
                "Quest TurnIn DoString address: " +
                Hex32(
                    doString
                )
            );

            Debug::Logger::Info(
                "Quest TurnIn GetText address: " +
                Hex32(
                    getText
                )
            );

            const bool valid =
                IsExecutable(
                    rightClick
                ) &&
                IsExecutable(
                    doString
                ) &&
                IsExecutable(
                    getText
                );

            Debug::Logger::Info(
                std::string(
                    "QuestTurnInController validation "
                ) +
                (
                    valid
                        ? "PASS."
                        : "FAILED."
                )
            );

            return valid;
        }

        bool Start(
            std::uint64_t tick)
        {
            if (
                state_ !=
                    QuestTurnInState::Idle)
            {
                return false;
            }

            if (!Validate())
            {
                return false;
            }

            startTick_ =
                tick;

            lastMoveTick_ =
                0;

            lastInteractionTick_ =
                0;

            lastDialogTick_ =
                0;

            moveCommands_ =
                0;

            interactionAttempts_ =
                0;

            dialogActions_ =
                0;

            rewardChoices_ =
                0;

            gornekGuid_ =
                0;

            lastDialogResult_.clear();

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST TURN-IN: START"
            );

            Debug::Logger::Info(
                "Quest: 789 Sting of the Scorpid"
            );

            Debug::Logger::Info(
                "NPC: Gornek (entry 3143)"
            );

            Debug::Logger::Info(
                "Phase 8B automation: interaction, "
                "quest selection, CompleteQuest() and "
                "verified reward selection."
            );

            Debug::Logger::Info(
                "Reward policy: choose Battleworn Cape "
                "only when that exact choice is present; "
                "otherwise stop for manual selection."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestTurnInState::
                    FindingNpc
            );

            return true;
        }

        void Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (
                state_ ==
                    QuestTurnInState::Idle ||
                state_ ==
                    QuestTurnInState::Done ||
                state_ ==
                    QuestTurnInState::Failed ||
                state_ ==
                    QuestTurnInState::
                        RewardChoiceRequired ||
                state_ ==
                    QuestTurnInState::
                        WaitingForQuestRemoval)
            {
                return;
            }

            if (
                tick >=
                    startTick_ +
                    OverallTimeoutTicks)
            {
                Fail(
                    "turn-in flow timed out."
                );

                return;
            }

            const auto* gornek =
                FindGornek(
                    world
                );

            if (gornek == nullptr)
            {
                if (
                    tick >=
                        startTick_ +
                        NpcSearchTimeoutTicks)
                {
                    Fail(
                        "Gornek was not found in WorldState."
                    );
                }

                return;
            }

            gornekGuid_ =
                gornek->guid;

            if (
                gornek->distance >
                    InteractionDistance)
            {
                if (
                    moveCommands_ >=
                        MaximumMoveCommands)
                {
                    Fail(
                        "too many approach commands to Gornek."
                    );

                    return;
                }

                if (
                    lastMoveTick_ == 0 ||
                    tick >=
                        lastMoveTick_ +
                        MoveCooldownTicks)
                {
                    if (!IssueApproach(
                            world.player,
                            *gornek,
                            tick))
                    {
                        Fail(
                            "failed to approach Gornek."
                        );
                    }
                }

                return;
            }

            if (
                state_ ==
                    QuestTurnInState::
                        FindingNpc ||
                state_ ==
                    QuestTurnInState::
                        ApproachingNpc ||
                state_ ==
                    QuestTurnInState::
                        Interacting)
            {
                if (
                    interactionAttempts_ == 0 ||
                    tick >=
                        lastInteractionTick_ +
                        InteractionRetryTicks)
                {
                    if (!IssueInteraction(
                            *gornek,
                            tick))
                    {
                        Fail(
                            "failed to interact with Gornek."
                        );
                    }
                }

                return;
            }

            if (
                state_ ==
                    QuestTurnInState::
                        AdvancingDialog)
            {
                /*
                 * If no expected quest/gossip panel became
                 * visible after the previous right-click,
                 * retry the NPC interaction rather than
                 * polling "waiting" until timeout.
                 */
                if (
                    lastDialogResult_ ==
                        "waiting" &&
                    tick >=
                        lastInteractionTick_ +
                        InteractionRetryTicks)
                {
                    if (!IssueInteraction(
                            *gornek,
                            tick))
                    {
                        Fail(
                            "failed to retry interaction "
                            "with Gornek."
                        );
                    }

                    return;
                }

                if (
                    tick <
                        lastDialogTick_ +
                        DialogStepTicks)
                {
                    return;
                }

                if (!AdvanceDialog(
                        tick))
                {
                    Fail(
                        "failed to advance quest dialog."
                    );
                }

                return;
            }
        }

        void MarkQuestRemoved()
        {
            if (
                state_ ==
                    QuestTurnInState::Idle ||
                state_ ==
                    QuestTurnInState::Failed)
            {
                return;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST TURN-IN: PASS"
            );

            Debug::Logger::Info(
                "Quest 789 is no longer active."
            );

            Debug::Logger::Info(
                "Interactions: " +
                std::to_string(
                    interactionAttempts_
                )
            );

            Debug::Logger::Info(
                "Dialog actions: " +
                std::to_string(
                    dialogActions_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestTurnInState::Done
            );
        }

        void Reset()
        {
            state_ =
                QuestTurnInState::Idle;

            startTick_ =
                0;

            lastMoveTick_ =
                0;

            lastInteractionTick_ =
                0;

            lastDialogTick_ =
                0;

            moveCommands_ =
                0;

            interactionAttempts_ =
                0;

            dialogActions_ =
                0;

            rewardChoices_ =
                0;

            gornekGuid_ =
                0;

            lastDialogResult_.clear();
        }

        QuestTurnInState State() const
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
                    QuestTurnInState::Idle &&
                state_ !=
                    QuestTurnInState::Done &&
                state_ !=
                    QuestTurnInState::Failed;
        }

        bool IsDone() const
        {
            return
                state_ ==
                    QuestTurnInState::Done;
        }

        bool NeedsRewardChoice() const
        {
            return
                state_ ==
                    QuestTurnInState::
                        RewardChoiceRequired;
        }

        bool WaitingForQuestRemoval() const
        {
            return
                state_ ==
                    QuestTurnInState::
                        WaitingForQuestRemoval;
        }

        bool Failed() const
        {
            return
                state_ ==
                    QuestTurnInState::Failed;
        }

        int RewardChoices() const
        {
            return rewardChoices_;
        }

        int InteractionAttempts() const
        {
            return interactionAttempts_;
        }

        int DialogActions() const
        {
            return dialogActions_;
        }

        std::uint64_t GornekGuid() const
        {
            return gornekGuid_;
        }
    };
}
