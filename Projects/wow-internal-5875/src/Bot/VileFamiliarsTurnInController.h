#pragma once

#include "ClickToMoveController.h"
#include "ClassAwareRewardController.h"
#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    enum class VileFamiliarsTurnInState
    {
        Idle,
        FindingNpc,
        ApproachingNpc,
        AdvancingDialog,
        EvaluatingReward,
        RewardChoiceRequired,
        WaitingForQuestRemoval,
        Done,
        Failed
    };

    class VileFamiliarsTurnInController
    {
    private:
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

        static constexpr float InteractionDistance =
            4.50f;

        static constexpr std::uint64_t MoveCooldownTicks =
            4;
        static constexpr std::uint64_t InteractionRetryTicks =
            8;
        static constexpr std::uint64_t DialogStepTicks =
            4;
        static constexpr std::uint64_t NpcSearchTimeoutTicks =
            80;
        static constexpr std::uint64_t OverallTimeoutTicks =
            200;

        static constexpr int MaximumMoveCommands =
            8;
        static constexpr int MaximumInteractionAttempts =
            5;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_792_TURNIN_STATE";

        VileFamiliarsTurnInState state_ =
            VileFamiliarsTurnInState::Idle;

        std::uint64_t startTick_ = 0;
        std::uint64_t lastMoveTick_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        std::uint64_t lastDialogTick_ = 0;

        int moveCommands_ = 0;
        int interactionAttempts_ = 0;
        int dialogActions_ = 0;
        int rewardChoices_ = 0;

        std::uint64_t zureethaGuid_ = 0;
        std::string lastDialogResult_{};

        ClassAwareRewardController rewardController_;

        bool questRemovalObserved_ =
            false;

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
                << static_cast<std::uint32_t>(value);
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
            VileFamiliarsTurnInState state)
        {
            switch (state)
            {
                case VileFamiliarsTurnInState::Idle:
                    return "Idle";
                case VileFamiliarsTurnInState::FindingNpc:
                    return "FindingNpc";
                case VileFamiliarsTurnInState::ApproachingNpc:
                    return "ApproachingNpc";
                case VileFamiliarsTurnInState::AdvancingDialog:
                    return "AdvancingDialog";
                case VileFamiliarsTurnInState::EvaluatingReward:
                    return "EvaluatingReward";
                case VileFamiliarsTurnInState::RewardChoiceRequired:
                    return "RewardChoiceRequired";
                case VileFamiliarsTurnInState::WaitingForQuestRemoval:
                    return "WaitingForQuestRemoval";
                case VileFamiliarsTurnInState::Done:
                    return "Done";
                case VileFamiliarsTurnInState::Failed:
                    return "Failed";
                default:
                    return "Unknown";
            }
        }

        void SetState(
            VileFamiliarsTurnInState next)
        {
            if (state_ == next)
                return;

            Debug::Logger::Info(
                std::string("VileFamiliarsTurnIn state: ") +
                StateNameInternal(state_) +
                " -> " +
                StateNameInternal(next)
            );

            state_ = next;
        }

        void Fail(
            const std::string& reason)
        {
            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "QUEST 792 TURN-IN: FAILED"
            );
            Debug::Logger::Info(
                "Reason: " + reason
            );
            Debug::Logger::Info(
                "Zureetha GUID: " +
                Hex64(zureethaGuid_)
            );
            Debug::Logger::Info(
                "================================"
            );

            SetState(
                VileFamiliarsTurnInState::Failed
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
                    reinterpret_cast<LPCVOID>(address),
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
                reinterpret_cast<const void*>(address),
                sizeof(T)
            );

            return true;
        }

        static bool IsExecutable(
            std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};

            if (
                VirtualQuery(
                    reinterpret_cast<LPCVOID>(address),
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
                return 0;

            std::uint32_t manager = 0;

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

            std::uint32_t current = 0;

            if (!ReadValue(
                    static_cast<std::uintptr_t>(manager) +
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

                std::uint64_t currentGuid = 0;

                if (!ReadValue(
                        static_cast<std::uintptr_t>(current) +
                            ObjectGuidOffset,
                        currentGuid))
                {
                    break;
                }

                if (currentGuid == guid)
                    return
                        static_cast<std::uintptr_t>(current);

                std::uint32_t next = 0;

                if (!ReadValue(
                        static_cast<std::uintptr_t>(current) +
                            ObjectNextOffset,
                        next))
                {
                    break;
                }

                if (next == current)
                    break;

                current = next;
            }

            return 0;
        }

        static const Objects::UnitState* FindZureetha(
            const Objects::WorldState& world)
        {
            const Objects::UnitState* best =
                nullptr;

            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.entryId != ZureethaEntry)
                {
                    continue;
                }

                if (
                    best == nullptr ||
                    unit.distance < best->distance)
                {
                    best = &unit;
                }
            }

            return best;
        }

        bool IssueApproach(
            const Objects::PlayerState& player,
            const Objects::UnitState& zureetha,
            std::uint64_t tick)
        {
            if (moveCommands_ >= MaximumMoveCommands)
                return false;

            Debug::Logger::Info(
                "QUEST 792 TURN-IN: approaching Zureetha."
            );
            Debug::Logger::Info(
                "Distance: " +
                Float(zureetha.distance)
            );

            if (!ClickToMoveController::MoveTo(
                    player,
                    zureetha.x,
                    zureetha.y,
                    zureetha.z,
                    1.25f))
            {
                return false;
            }

            ++moveCommands_;
            lastMoveTick_ = tick;

            SetState(
                VileFamiliarsTurnInState::ApproachingNpc
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

            if (!IsExecutable(functionAddress))
                return false;

            const std::uintptr_t objectAddress =
                FindObjectAddressByGuid(zureetha.guid);

            if (objectAddress == 0)
            {
                Debug::Logger::Info(
                    "QUEST 792 TURN-IN: Zureetha object "
                    "pointer not found."
                );
                return false;
            }

            using OnRightClickUnitFunction =
                void (__thiscall*)(std::uint32_t, int);

            const auto onRightClickUnit =
                reinterpret_cast<OnRightClickUnitFunction>(
                    functionAddress
                );

            bool onGameThread = false;

            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "QUEST 792 TURN-IN: interacting with "
                "Zureetha Fargaze."
            );
            Debug::Logger::Info(
                "Zureetha GUID: " +
                Hex64(zureetha.guid)
            );
            Debug::Logger::Info(
                "Zureetha object: " +
                Hex32(objectAddress)
            );

            const std::uint32_t zureethaThis =
                static_cast<std::uint32_t>(objectAddress);

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            std::string(
                                "Quest 792 interaction on "
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
            lastInteractionTick_ = tick;
            lastDialogTick_ = tick;

            SetState(
                VileFamiliarsTurnInState::
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
                !IsExecutable(doStringAddress) ||
                !IsExecutable(getTextAddress))
            {
                return false;
            }

            static constexpr char Script[] =
                "WOW_INTERNAL_792_TURNIN_STATE='waiting'; "
                "local t=GetTitleText(); "
                "if QuestFrameRewardPanel and "
                "QuestFrameRewardPanel:IsVisible() and "
                "t=='Vile Familiars' then "
                "local c=GetNumQuestChoices(); "
                "if c==nil then c=0 end; "
                "if c==0 then "
                "WOW_INTERNAL_792_TURNIN_STATE='reward_claim'; "
                "GetQuestReward(0); "
                "else "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'reward_evaluate:'..c; "
                "end; "
                "elseif QuestFrameProgressPanel and "
                "QuestFrameProgressPanel:IsVisible() and "
                "t=='Vile Familiars' then "
                "if IsQuestCompletable() then "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'progress_complete'; "
                "CompleteQuest(); "
                "else "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'progress_blocked'; "
                "end; "
                "elseif QuestFrameGreetingPanel and "
                "QuestFrameGreetingPanel:IsVisible() then "
                "local n=GetNumActiveQuests(); "
                "local f=0; "
                "for i=1,n do "
                "if GetActiveTitle(i)=='Vile Familiars' then "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'greeting_select'; "
                "SelectActiveQuest(i); "
                "f=1; break; "
                "end; "
                "end; "
                "if f==0 then "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'greeting_not_found'; "
                "end; "
                "elseif GossipFrame and GossipFrame:IsVisible() then "
                "local a={GetGossipActiveQuests()}; "
                "local q=1; local f=0; "
                "for i=1,table.getn(a),2 do "
                "if a[i]=='Vile Familiars' then "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'gossip_select'; "
                "SelectGossipActiveQuest(q); "
                "f=1; break; "
                "end; "
                "q=q+1; "
                "end; "
                "if f==0 then "
                "WOW_INTERNAL_792_TURNIN_STATE="
                "'gossip_not_found'; "
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
                reinterpret_cast<DoStringFunction>(
                    doStringAddress
                );
            const auto getText =
                reinterpret_cast<GetTextFunction>(
                    getTextAddress
                );

            char result[128]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

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
                                "wow-internal/VileFamiliarsTurnInController.lua"
                            );

                        if (!luaExecuted)
                            return;

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
                            gotText = true;
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
            lastDialogTick_ = tick;

            const std::string current =
                result;

            if (current != lastDialogResult_)
            {
                Debug::Logger::Info(
                    "QUEST 792 TURN-IN dialog: " +
                    current
                );
                lastDialogResult_ =
                    current;
            }

            if (
                current == "greeting_select" ||
                current == "gossip_select" ||
                current == "progress_complete")
            {
                return true;
            }

            if (current == "reward_claim")
            {
                Debug::Logger::Info(
                    "================================"
                );
                Debug::Logger::Info(
                    "QUEST 792 TURN-IN: reward claim issued."
                );
                Debug::Logger::Info(
                    "Waiting for QuestStateReader to verify "
                    "quest 792 removal."
                );
                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    VileFamiliarsTurnInState::
                        WaitingForQuestRemoval
                );

                return true;
            }

            static constexpr char RewardEvaluatePrefix[] =
                "reward_evaluate:";

            if (
                current.rfind(
                    RewardEvaluatePrefix,
                    0
                ) == 0)
            {
                rewardChoices_ =
                    std::atoi(
                        current.substr(
                            sizeof(RewardEvaluatePrefix) - 1
                        ).c_str()
                    );

                Debug::Logger::Info(
                    "================================"
                );
                Debug::Logger::Info(
                    "QUEST 792 TURN-IN: CLASS-AWARE REWARD EVALUATION"
                );
                Debug::Logger::Info(
                    "Choices: " +
                    std::to_string(rewardChoices_)
                );
                Debug::Logger::Info(
                    "================================"
                );

                if (!rewardController_.Start(tick))
                {
                    Fail(
                        "class-aware reward controller failed to start."
                    );
                    return true;
                }

                SetState(
                    VileFamiliarsTurnInState::
                        EvaluatingReward
                );

                return true;
            }

            if (current == "progress_blocked")
            {
                Fail(
                    "progress panel visible but "
                    "IsQuestCompletable() returned false."
                );
                return true;
            }

            return true;
        }

    public:
        static bool Validate()
        {
            const auto click =
                OnRightClickUnitAddress();
            const auto doString =
                LuaDoStringAddress();
            const auto getText =
                GetTextAddress();

            Debug::Logger::Info(
                "Quest 792 turn-in OnRightClickUnit: " +
                Hex32(click)
            );
            Debug::Logger::Info(
                "Quest 792 turn-in DoString: " +
                Hex32(doString)
            );
            Debug::Logger::Info(
                "Quest 792 turn-in GetText: " +
                Hex32(getText)
            );

            const bool valid =
                IsExecutable(click) &&
                IsExecutable(doString) &&
                IsExecutable(getText);

            Debug::Logger::Info(
                std::string(
                    "VileFamiliarsTurnIn validation "
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
                    VileFamiliarsTurnInState::Idle)
            {
                return false;
            }

            if (!Validate())
                return false;

            startTick_ = tick;
            lastMoveTick_ = 0;
            lastInteractionTick_ = 0;
            lastDialogTick_ = 0;
            moveCommands_ = 0;
            interactionAttempts_ = 0;
            dialogActions_ = 0;
            rewardChoices_ = 0;
            zureethaGuid_ = 0;
            lastDialogResult_.clear();
            questRemovalObserved_ = false;

            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "QUEST 792 TURN-IN: START"
            );
            Debug::Logger::Info(
                "Quest: 792 Vile Familiars"
            );
            Debug::Logger::Info(
                "NPC: Zureetha Fargaze (3145)"
            );
            Debug::Logger::Info(
                "Policy: exact title only; CompleteQuest(); "
                "auto-claim only when reward choices == 0."
            );
            Debug::Logger::Info(
                "================================"
            );

            SetState(
                VileFamiliarsTurnInState::
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
                    VileFamiliarsTurnInState::Idle ||
                state_ ==
                    VileFamiliarsTurnInState::Done ||
                state_ ==
                    VileFamiliarsTurnInState::Failed ||
                state_ ==
                    VileFamiliarsTurnInState::
                        RewardChoiceRequired)
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

            if (
                state_ ==
                    VileFamiliarsTurnInState::
                        EvaluatingReward)
            {
                rewardController_.Update(tick);

                if (rewardController_.UnsupportedClass())
                {
                    Debug::Logger::Info(
                        "QUEST 792 TURN-IN: class profile not "
                        "validated for automatic reward selection."
                    );

                    SetState(
                        VileFamiliarsTurnInState::
                            RewardChoiceRequired
                    );
                    return;
                }

                if (rewardController_.Failed())
                {
                    Fail(
                        "class-aware reward evaluation failed."
                    );
                    return;
                }

                if (rewardController_.Verified())
                {
                    SetState(
                        VileFamiliarsTurnInState::
                            WaitingForQuestRemoval
                    );

                    if (questRemovalObserved_)
                    {
                        MarkQuestRemoved();
                    }
                }

                return;
            }

            if (
                state_ ==
                    VileFamiliarsTurnInState::
                        WaitingForQuestRemoval)
            {
                if (
                    questRemovalObserved_ &&
                    rewardController_.Verified())
                {
                    MarkQuestRemoved();
                }
                return;
            }

            const auto* zureetha =
                FindZureetha(world);

            if (zureetha == nullptr)
            {
                if (
                    tick >=
                        startTick_ +
                        NpcSearchTimeoutTicks)
                {
                    Fail(
                        "Zureetha was not found in WorldState."
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
                    moveCommands_ >=
                        MaximumMoveCommands)
                {
                    Fail(
                        "too many approach commands to Zureetha."
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
                            *zureetha,
                            tick))
                    {
                        Fail(
                            "failed to approach Zureetha."
                        );
                    }
                }
                return;
            }

            if (
                state_ ==
                    VileFamiliarsTurnInState::
                        FindingNpc ||
                state_ ==
                    VileFamiliarsTurnInState::
                        ApproachingNpc)
            {
                if (
                    interactionAttempts_ == 0 ||
                    tick >=
                        lastInteractionTick_ +
                        InteractionRetryTicks)
                {
                    if (!IssueInteraction(
                            *zureetha,
                            tick))
                    {
                        Fail(
                            "failed to interact with Zureetha."
                        );
                    }
                }
                return;
            }

            if (
                state_ ==
                    VileFamiliarsTurnInState::
                        AdvancingDialog)
            {
                if (
                    lastDialogResult_ == "waiting" &&
                    tick >=
                        lastInteractionTick_ +
                        InteractionRetryTicks)
                {
                    if (!IssueInteraction(
                            *zureetha,
                            tick))
                    {
                        Fail(
                            "failed to retry interaction "
                            "with Zureetha."
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

                if (!AdvanceDialog(tick))
                {
                    Fail(
                        "failed to advance Vile Familiars "
                        "turn-in dialog."
                    );
                }
            }
        }

        void MarkQuestRemoved()
        {
            questRemovalObserved_ =
                true;

            if (
                state_ ==
                    VileFamiliarsTurnInState::Done ||
                state_ ==
                    VileFamiliarsTurnInState::Failed)
            {
                return;
            }

            /*
             * Phase 10C treated quest-log disappearance as
             * sufficient PASS even while a reward choice was
             * still pending. Vanilla can remove/hide the quest
             * log entry during the completion UI, so Phase 10D
             * requires both conditions:
             *
             *   1. reward delivery/equipment verified
             *   2. quest 792 no longer active
             */
            if (!rewardController_.Verified())
            {
                Debug::Logger::Info(
                    "QUEST 792 TURN-IN: quest removal observed; "
                    "waiting for reward verification before PASS."
                );
                return;
            }

            if (
                state_ !=
                    VileFamiliarsTurnInState::
                        WaitingForQuestRemoval &&
                state_ !=
                    VileFamiliarsTurnInState::
                        EvaluatingReward)
            {
                Debug::Logger::Info(
                    "QUEST 792 TURN-IN: reward is verified, "
                    "but turn-in state is not ready for PASS."
                );
                return;
            }

            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "QUEST 792 TURN-IN: PASS"
            );
            Debug::Logger::Info(
                "Vile Familiars reward and quest removal "
                "are both verified."
            );
            Debug::Logger::Info(
                "Selected reward: " +
                rewardController_.SelectedName() +
                " id=" +
                std::to_string(
                    rewardController_.SelectedItemId()
                )
            );
            Debug::Logger::Info(
                std::string("Auto-equipped: ") +
                (
                    rewardController_.ShouldEquip()
                        ? "yes"
                        : "no"
                )
            );
            Debug::Logger::Info(
                "Interactions: " +
                std::to_string(interactionAttempts_)
            );
            Debug::Logger::Info(
                "Dialog actions: " +
                std::to_string(dialogActions_)
            );
            Debug::Logger::Info(
                "================================"
            );

            SetState(
                VileFamiliarsTurnInState::Done
            );
        }

        const char* StateName() const
        {
            return
                StateNameInternal(state_);
        }

        bool IsActive() const
        {
            return
                state_ != VileFamiliarsTurnInState::Idle &&
                state_ != VileFamiliarsTurnInState::Done &&
                state_ != VileFamiliarsTurnInState::Failed;
        }

        bool IsDone() const
        {
            return
                state_ ==
                    VileFamiliarsTurnInState::Done;
        }

        bool Failed() const
        {
            return
                state_ ==
                    VileFamiliarsTurnInState::Failed;
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

        int RewardChoices() const
        {
            return rewardChoices_;
        }

        const char* RewardStateName() const
        {
            return rewardController_.StateName();
        }

        int SelectedRewardItemId() const
        {
            return rewardController_.SelectedItemId();
        }

        bool RewardShouldEquip() const
        {
            return rewardController_.ShouldEquip();
        }
    };
}
