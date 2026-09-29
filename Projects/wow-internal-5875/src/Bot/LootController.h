#pragma once

#include "ClickToMoveController.h"
#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"
#include "../Objects/UnitSnapshot.h"
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
    enum class LootState
    {
        Idle,
        Approaching,
        Opening,
        WaitingForOpen,
        Looting,
        WaitingForClose,
        Done,
        NoLoot,
        Failed
    };

    class LootController
    {
    private:
        /*
         * WoW 1.12.1 build 5875.
         *
         * These are the same client routines used by the
         * ClassicFramework / ZzukBot 1.12.1 implementation.
         *
         * Absolute addresses:
         *
         *   OnRightClickUnit = 0x0060BEA0
         *   AutoLoot         = 0x004C1FA0
         *   GetLootSlots     = 0x004C2260
         *   Lua DoString     = 0x00704CD0
         *
         * Image base:
         *
         *   0x00400000
         */
        static constexpr std::uintptr_t OnRightClickUnitRva =
            0x0020BEA0;

        static constexpr std::uintptr_t AutoLootRva =
            0x000C1FA0;

        static constexpr std::uintptr_t GetLootSlotsRva =
            0x000C2260;

        static constexpr std::uintptr_t LuaDoStringRva =
            0x00304CD0;

        /*
         * Verified ObjectManager layout for this project.
         */
        static constexpr std::uintptr_t ObjectManagerRootRva =
            0x00741414;

        static constexpr std::uintptr_t FirstObjectOffset =
            0x000000AC;

        static constexpr std::uintptr_t ObjectGuidOffset =
            0x00000030;

        static constexpr std::uintptr_t ObjectNextOffset =
            0x0000003C;

        /*
         * Current loot GUID in the local player object.
         */
        static constexpr std::uintptr_t CurrentLootGuidOffset =
            0x00001D28;

        static constexpr float InteractionDistance =
            4.0f;

        static constexpr float CorpseStandOffDistance =
            2.5f;

        static constexpr float DestinationTolerance =
            1.0f;

        /*
         * WorldMonitor runs at 250 ms.
         */
        static constexpr std::uint64_t MoveCooldownTicks =
            4;

        static constexpr std::uint64_t OpenRetryTicks =
            8;

        /*
         * Wait 1 second after a native AutoLoot attempt
         * before deciding that remaining slots require
         * another attempt.
         */
        static constexpr std::uint64_t NativeLootRetryTicks =
            4;

        /*
         * Once GetLootSlots reaches zero, wait two ticks
         * before closing the frame. This gives the client
         * time to process the server responses.
         */
        static constexpr std::uint64_t EmptySettleTicks =
            2;

        static constexpr std::uint64_t CloseWaitTicks =
            4;

        static constexpr std::uint64_t OverallTimeoutTicks =
            120;

        static constexpr int MaximumMoveCommands =
            8;

        static constexpr int MaximumOpenAttempts =
            3;

        static constexpr int MaximumLootAttempts =
            5;

        static constexpr int MaximumCloseAttempts =
            3;

        LootState state_ =
            LootState::Idle;

        std::uint64_t corpseGuid_ =
            0;

        float corpseX_ =
            0.0f;

        float corpseY_ =
            0.0f;

        float corpseZ_ =
            0.0f;

        float destinationX_ =
            0.0f;

        float destinationY_ =
            0.0f;

        float destinationZ_ =
            0.0f;

        std::uint64_t startTick_ =
            0;

        std::uint64_t lastMoveTick_ =
            0;

        std::uint64_t lastOpenTick_ =
            0;

        std::uint64_t lastLootTick_ =
            0;

        std::uint64_t firstEmptyTick_ =
            0;

        std::uint64_t closeRequestTick_ =
            0;

        int moveCommands_ =
            0;

        int openAttempts_ =
            0;

        int lootAttempts_ =
            0;

        int closeAttempts_ =
            0;

        int initialLootSlots_ =
            -1;

        int lastObservedLootSlots_ =
            -1;

        bool observedLootOpen_ =
            false;

        bool closeRequested_ =
            false;

        bool failurePending_ =
            false;

        std::string pendingFailureReason_{};

        std::uint64_t lastObservedLootGuid_ =
            0;

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
            LootState state)
        {
            switch (state)
            {
                case LootState::Idle:
                    return "Idle";

                case LootState::Approaching:
                    return "Approaching";

                case LootState::Opening:
                    return "Opening";

                case LootState::WaitingForOpen:
                    return "WaitingForOpen";

                case LootState::Looting:
                    return "Looting";

                case LootState::WaitingForClose:
                    return "WaitingForClose";

                case LootState::Done:
                    return "Done";

                case LootState::NoLoot:
                    return "NoLoot";

                case LootState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
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

        template <typename T>
        static bool ReadValue(
            std::uintptr_t address,
            T& value)
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

            const std::uintptr_t regionStart =
                reinterpret_cast<std::uintptr_t>(
                    info.BaseAddress
                );

            const std::uintptr_t regionEnd =
                regionStart +
                static_cast<std::uintptr_t>(
                    info.RegionSize
                );

            if (
                address < regionStart ||
                address + sizeof(T) > regionEnd)
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

        static std::uintptr_t
        OnRightClickUnitAddress()
        {
            return
                Wow5875::Client::Base() +
                OnRightClickUnitRva;
        }

        static std::uintptr_t
        AutoLootAddress()
        {
            return
                Wow5875::Client::Base() +
                AutoLootRva;
        }

        static std::uintptr_t
        GetLootSlotsAddress()
        {
            return
                Wow5875::Client::Base() +
                GetLootSlotsRva;
        }

        static std::uintptr_t
        LuaDoStringAddress()
        {
            return
                Wow5875::Client::Base() +
                LuaDoStringRva;
        }

        static std::uintptr_t
        ObjectManagerRootAddress()
        {
            return
                Wow5875::Client::Base() +
                ObjectManagerRootRva;
        }

        static std::uintptr_t
        FindObjectAddressByGuid(
            std::uint64_t guid)
        {
            if (guid == 0)
            {
                return 0;
            }

            std::uint32_t manager =
                0;

            if (!ReadValue(
                    ObjectManagerRootAddress(),
                    manager
                ))
            {
                return 0;
            }

            if (manager == 0)
            {
                return 0;
            }

            std::uint32_t object =
                0;

            if (!ReadValue(
                    static_cast<std::uintptr_t>(
                        manager
                    ) +
                    FirstObjectOffset,
                    object
                ))
            {
                return 0;
            }

            if (
                object == 0 ||
                (object & 1U) != 0)
            {
                return 0;
            }

            for (
                int i = 0;
                i < 4096;
                ++i)
            {
                if (
                    object == 0 ||
                    (object & 1U) != 0)
                {
                    break;
                }

                std::uint64_t objectGuid =
                    0;

                if (!ReadValue(
                        static_cast<std::uintptr_t>(
                            object
                        ) +
                        ObjectGuidOffset,
                        objectGuid
                    ))
                {
                    break;
                }

                if (objectGuid == guid)
                {
                    return
                        static_cast<std::uintptr_t>(
                            object
                        );
                }

                std::uint32_t next =
                    0;

                if (!ReadValue(
                        static_cast<std::uintptr_t>(
                            object
                        ) +
                        ObjectNextOffset,
                        next
                    ))
                {
                    break;
                }

                if (next == object)
                {
                    break;
                }

                object =
                    next;
            }

            return 0;
        }

        static bool ReadCurrentLootGuid(
            const Objects::PlayerState& player,
            std::uint64_t& guid)
        {
            guid =
                0;

            if (
                !player.valid ||
                player.address == 0)
            {
                return false;
            }

            return ReadValue(
                static_cast<std::uintptr_t>(
                    player.address
                ) +
                CurrentLootGuidOffset,
                guid
            );
        }

        static bool ExecuteLua(
            const char* script)
        {
            if (
                script == nullptr ||
                *script == '\0')
            {
                return false;
            }

            const auto address =
                LuaDoStringAddress();

            if (!IsExecutable(address))
            {
                return false;
            }

            using DoStringFunction =
                bool (__fastcall*)(
                    const char*,
                    const char*
                );

            const auto doString =
                reinterpret_cast<
                    DoStringFunction
                >(
                    address
                );

            bool onGameThread =
                false;

            bool luaExecuted =
                false;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            std::string(
                                "Loot Lua on game thread: "
                            ) +
                            (
                                onGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        luaExecuted =
                            doString(
                                script,
                                "wow-internal/LootController.lua"
                            );
                    }
                );

            return
                dispatched &&
                onGameThread &&
                luaExecuted;
        }

        static bool QueryLootSlots(
            int& slots)
        {
            slots =
                -1;

            const auto address =
                GetLootSlotsAddress();

            if (!IsExecutable(address))
            {
                return false;
            }

            using GetLootSlotsFunction =
                int (__stdcall*)();

            const auto getLootSlots =
                reinterpret_cast<
                    GetLootSlotsFunction
                >(
                    address
                );

            bool onGameThread =
                false;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        slots =
                            getLootSlots();
                    }
                );

            return
                dispatched &&
                onGameThread;
        }

        bool IssueApproach(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            const float dx =
                player.x -
                corpseX_;

            const float dy =
                player.y -
                corpseY_;

            const float horizontal =
                std::sqrt(
                    dx * dx +
                    dy * dy
                );

            if (horizontal <= 0.001f)
            {
                destinationX_ =
                    corpseX_;

                destinationY_ =
                    corpseY_;

                destinationZ_ =
                    corpseZ_;
            }
            else
            {
                const float nx =
                    dx /
                    horizontal;

                const float ny =
                    dy /
                    horizontal;

                destinationX_ =
                    corpseX_ +
                    nx *
                        CorpseStandOffDistance;

                destinationY_ =
                    corpseY_ +
                    ny *
                        CorpseStandOffDistance;

                destinationZ_ =
                    corpseZ_;
            }

            Debug::Logger::Info(
                "LOOT: moving to corpse."
            );

            Debug::Logger::Info(
                "Corpse GUID: " +
                Hex64(
                    corpseGuid_
                )
            );

            Debug::Logger::Info(
                "Corpse position: (" +
                Float(corpseX_) +
                "," +
                Float(corpseY_) +
                "," +
                Float(corpseZ_) +
                ")"
            );

            Debug::Logger::Info(
                "Loot destination: (" +
                Float(destinationX_) +
                "," +
                Float(destinationY_) +
                "," +
                Float(destinationZ_) +
                ")"
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        destinationX_,
                        destinationY_,
                        destinationZ_,
                        0.75f
                    ))
            {
                return false;
            }

            lastMoveTick_ =
                tick;

            ++moveCommands_;

            return true;
        }

        bool IssueOpen(
            std::uint64_t tick)
        {
            const auto functionAddress =
                OnRightClickUnitAddress();

            if (!IsExecutable(
                    functionAddress))
            {
                return false;
            }

            const std::uintptr_t objectAddress =
                FindObjectAddressByGuid(
                    corpseGuid_
                );

            if (objectAddress == 0)
            {
                Debug::Logger::Info(
                    "LOOT: corpse object pointer "
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

            const std::uint32_t corpseThis =
                static_cast<std::uint32_t>(
                    objectAddress
                );

            bool onGameThread =
                false;

            Debug::Logger::Info(
                "LOOT: opening corpse."
            );

            Debug::Logger::Info(
                "Corpse object: " +
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
                                "Loot interaction on "
                                "game thread: "
                            ) +
                            (
                                onGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        /*
                         * Open the corpse normally.
                         * Native AutoLoot is called only
                         * after CurrentLootGuid confirms
                         * that the loot frame is active.
                         */
                        onRightClickUnit(
                            corpseThis,
                            0
                        );
                    }
                );

            if (
                !dispatched ||
                !onGameThread)
            {
                return false;
            }

            lastOpenTick_ =
                tick;

            ++openAttempts_;

            return true;
        }

        bool IssueNativeLootAll(
            std::uint64_t tick)
        {
            const auto autoLootAddress =
                AutoLootAddress();

            const auto getLootSlotsAddress =
                GetLootSlotsAddress();

            if (
                !IsExecutable(
                    autoLootAddress
                ) ||
                !IsExecutable(
                    getLootSlotsAddress
                ))
            {
                return false;
            }

            using AutoLootFunction =
                void (__stdcall*)();

            using GetLootSlotsFunction =
                int (__stdcall*)();

            const auto autoLoot =
                reinterpret_cast<
                    AutoLootFunction
                >(
                    autoLootAddress
                );

            const auto getLootSlots =
                reinterpret_cast<
                    GetLootSlotsFunction
                >(
                    getLootSlotsAddress
                );

            bool onGameThread =
                false;

            int slotsBefore =
                -1;

            int slotsImmediatelyAfter =
                -1;

            Debug::Logger::Info(
                "LOOT: native AutoLoot requested."
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
                                "Native AutoLoot on "
                                "game thread: "
                            ) +
                            (
                                onGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        slotsBefore =
                            getLootSlots();

                        autoLoot();

                        /*
                         * This value can still be stale
                         * until the server responses are
                         * processed. It is diagnostic only.
                         */
                        slotsImmediatelyAfter =
                            getLootSlots();
                    }
                );

            if (
                !dispatched ||
                !onGameThread)
            {
                return false;
            }

            Debug::Logger::Info(
                "Native loot slots before call: " +
                std::to_string(
                    slotsBefore
                )
            );

            Debug::Logger::Info(
                "Native loot slots immediately "
                "after call: " +
                std::to_string(
                    slotsImmediatelyAfter
                )
            );

            if (
                initialLootSlots_ < 0 &&
                slotsBefore >= 0)
            {
                initialLootSlots_ =
                    slotsBefore;
            }

            if (slotsImmediatelyAfter >= 0)
            {
                lastObservedLootSlots_ =
                    slotsImmediatelyAfter;
            }

            lastLootTick_ =
                tick;

            firstEmptyTick_ =
                0;

            ++lootAttempts_;

            return true;
        }

        bool IssueCloseLootWindow(
            std::uint64_t tick)
        {
            /*
             * CloseLoot() is the normal Vanilla Lua
             * API for closing an active loot frame.
             */
            static constexpr char CloseScript[] =
                "CloseLoot()";

            Debug::Logger::Info(
                "LOOT: requesting loot window close."
            );

            if (!ExecuteLua(
                    CloseScript))
            {
                return false;
            }

            closeRequested_ =
                true;

            closeRequestTick_ =
                tick;

            ++closeAttempts_;

            return true;
        }

        void SetState(
            LootState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "LootController state: "
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

        void FinishDone()
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "LOOT: PASS"
            );

            Debug::Logger::Info(
                "Corpse GUID: " +
                Hex64(
                    corpseGuid_
                )
            );

            Debug::Logger::Info(
                "Initial native loot slots: " +
                std::to_string(
                    initialLootSlots_
                )
            );

            Debug::Logger::Info(
                "Final native loot slots: " +
                std::to_string(
                    lastObservedLootSlots_
                )
            );

            Debug::Logger::Info(
                "Open attempts: " +
                std::to_string(
                    openAttempts_
                )
            );

            Debug::Logger::Info(
                "Native loot attempts: " +
                std::to_string(
                    lootAttempts_
                )
            );

            Debug::Logger::Info(
                "Close attempts: " +
                std::to_string(
                    closeAttempts_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                LootState::Done
            );
        }

        void FinishNoLoot(
            const std::string& reason)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "LOOT: SKIPPED"
            );

            Debug::Logger::Info(
                "Corpse GUID: " +
                Hex64(
                    corpseGuid_
                )
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                LootState::NoLoot
            );
        }

        void Fail(
            const std::string& reason)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "LOOT: FAILED"
            );

            Debug::Logger::Info(
                "Corpse GUID: " +
                Hex64(
                    corpseGuid_
                )
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "Initial native loot slots: " +
                std::to_string(
                    initialLootSlots_
                )
            );

            Debug::Logger::Info(
                "Last native loot slots: " +
                std::to_string(
                    lastObservedLootSlots_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                LootState::Failed
            );
        }

        void SetPendingFailure(
            const std::string& reason)
        {
            failurePending_ =
                true;

            pendingFailureReason_ =
                reason;
        }

    public:
        static bool Validate()
        {
            const auto rightClick =
                OnRightClickUnitAddress();

            const auto autoLoot =
                AutoLootAddress();

            const auto getLootSlots =
                GetLootSlotsAddress();

            const auto lua =
                LuaDoStringAddress();

            Debug::Logger::Info(
                "Loot OnRightClickUnit address: " +
                Hex32(
                    rightClick
                )
            );

            Debug::Logger::Info(
                "Loot AutoLoot address: " +
                Hex32(
                    autoLoot
                )
            );

            Debug::Logger::Info(
                "Loot GetLootSlots address: " +
                Hex32(
                    getLootSlots
                )
            );

            Debug::Logger::Info(
                "Loot Lua DoString address: " +
                Hex32(
                    lua
                )
            );

            if (!IsExecutable(
                    rightClick))
            {
                Debug::Logger::Info(
                    "Loot validation FAILED: "
                    "OnRightClickUnit not executable."
                );

                return false;
            }

            if (!IsExecutable(
                    autoLoot))
            {
                Debug::Logger::Info(
                    "Loot validation FAILED: "
                    "AutoLoot not executable."
                );

                return false;
            }

            if (!IsExecutable(
                    getLootSlots))
            {
                Debug::Logger::Info(
                    "Loot validation FAILED: "
                    "GetLootSlots not executable."
                );

                return false;
            }

            if (!IsExecutable(
                    lua))
            {
                Debug::Logger::Info(
                    "Loot validation FAILED: "
                    "Lua DoString not executable."
                );

                return false;
            }

            Debug::Logger::Info(
                "Loot validation PASS."
            );

            return true;
        }

        bool Start(
            const Objects::PlayerState& player,
            const Objects::UnitState& corpse,
            std::uint64_t tick)
        {
            if (
                state_ !=
                    LootState::Idle)
            {
                return false;
            }

            if (
                !player.valid ||
                player.address == 0)
            {
                return false;
            }

            if (
                !corpse.valid ||
                corpse.guid == 0 ||
                corpse.health != 0)
            {
                return false;
            }

            if (!Validate())
            {
                return false;
            }

            corpseGuid_ =
                corpse.guid;

            corpseX_ =
                corpse.x;

            corpseY_ =
                corpse.y;

            corpseZ_ =
                corpse.z;

            startTick_ =
                tick;

            lastMoveTick_ =
                tick;

            lastOpenTick_ =
                tick;

            lastLootTick_ =
                tick;

            firstEmptyTick_ =
                0;

            closeRequestTick_ =
                tick;

            moveCommands_ =
                0;

            openAttempts_ =
                0;

            lootAttempts_ =
                0;

            closeAttempts_ =
                0;

            initialLootSlots_ =
                -1;

            lastObservedLootSlots_ =
                -1;

            observedLootOpen_ =
                false;

            closeRequested_ =
                false;

            failurePending_ =
                false;

            pendingFailureReason_.clear();

            lastObservedLootGuid_ =
                0;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "LOOT: START"
            );

            Debug::Logger::Info(
                "Corpse GUID: " +
                Hex64(
                    corpseGuid_
                )
            );

            Debug::Logger::Info(
                "Initial corpse distance: " +
                Float(
                    Distance2D(
                        player.x,
                        player.y,
                        corpseX_,
                        corpseY_
                    )
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            const float distance =
                Distance2D(
                    player.x,
                    player.y,
                    corpseX_,
                    corpseY_
                );

            if (
                distance <=
                    InteractionDistance)
            {
                SetState(
                    LootState::Opening
                );

                return true;
            }

            SetState(
                LootState::Approaching
            );

            if (!IssueApproach(
                    player,
                    tick))
            {
                Fail(
                    "initial corpse movement "
                    "command failed."
                );

                return false;
            }

            return true;
        }

        void Update(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!IsActive())
            {
                return;
            }

            if (
                tick >=
                    startTick_ +
                    OverallTimeoutTicks)
            {
                if (closeRequested_)
                {
                    Fail(
                        "overall loot timeout "
                        "while closing loot frame."
                    );
                }
                else
                {
                    SetPendingFailure(
                        "overall loot timeout."
                    );

                    IssueCloseLootWindow(
                        tick
                    );
                }

                return;
            }

            std::uint64_t currentLootGuid =
                0;

            const bool lootGuidReadable =
                ReadCurrentLootGuid(
                    player,
                    currentLootGuid
                );

            if (
                lootGuidReadable &&
                currentLootGuid !=
                    lastObservedLootGuid_)
            {
                Debug::Logger::Info(
                    "CurrentLootGuid: " +
                    Hex64(
                        lastObservedLootGuid_
                    ) +
                    " -> " +
                    Hex64(
                        currentLootGuid
                    )
                );

                lastObservedLootGuid_ =
                    currentLootGuid;
            }

            if (
                lootGuidReadable &&
                currentLootGuid ==
                    corpseGuid_)
            {
                observedLootOpen_ =
                    true;
            }

            switch (state_)
            {
                case LootState::Approaching:
                {
                    const float distance =
                        Distance2D(
                            player.x,
                            player.y,
                            corpseX_,
                            corpseY_
                        );

                    const float destinationDistance =
                        Distance2D(
                            player.x,
                            player.y,
                            destinationX_,
                            destinationY_
                        );

                    if (
                        distance <=
                            InteractionDistance)
                    {
                        Debug::Logger::Info(
                            "LOOT: corpse in "
                            "interaction range."
                        );

                        SetState(
                            LootState::Opening
                        );

                        return;
                    }

                    const bool cooldownReady =
                        tick >=
                            lastMoveTick_ +
                            MoveCooldownTicks;

                    const bool destinationReached =
                        destinationDistance <=
                            DestinationTolerance;

                    if (
                        cooldownReady &&
                        (
                            destinationReached ||
                            moveCommands_ == 0
                        ))
                    {
                        if (
                            moveCommands_ >=
                                MaximumMoveCommands)
                        {
                            Fail(
                                "corpse approach "
                                "command limit reached."
                            );

                            return;
                        }

                        if (!IssueApproach(
                                player,
                                tick))
                        {
                            Fail(
                                "corpse movement "
                                "command failed."
                            );

                            return;
                        }
                    }

                    return;
                }

                case LootState::Opening:
                {
                    if (
                        openAttempts_ >=
                            MaximumOpenAttempts)
                    {
                        FinishNoLoot(
                            "loot window did not open."
                        );

                        return;
                    }

                    if (!IssueOpen(
                            tick))
                    {
                        ++openAttempts_;

                        lastOpenTick_ =
                            tick;
                    }

                    SetState(
                        LootState::WaitingForOpen
                    );

                    return;
                }

                case LootState::WaitingForOpen:
                {
                    if (
                        observedLootOpen_ ||
                        (
                            lootGuidReadable &&
                            currentLootGuid ==
                                corpseGuid_
                        ))
                    {
                        Debug::Logger::Info(
                            "LOOT: loot window observed."
                        );

                        int slots =
                            -1;

                        if (QueryLootSlots(
                                slots))
                        {
                            initialLootSlots_ =
                                slots;

                            lastObservedLootSlots_ =
                                slots;

                            Debug::Logger::Info(
                                "LOOT: native loot slots "
                                "on open: " +
                                std::to_string(
                                    slots
                                )
                            );
                        }
                        else
                        {
                            Debug::Logger::Info(
                                "LOOT: GetLootSlots query "
                                "failed on open."
                            );
                        }

                        SetState(
                            LootState::Looting
                        );

                        return;
                    }

                    if (
                        tick >=
                            lastOpenTick_ +
                            OpenRetryTicks)
                    {
                        if (
                            openAttempts_ >=
                                MaximumOpenAttempts)
                        {
                            FinishNoLoot(
                                "no loot window after "
                                "interaction retries."
                            );

                            return;
                        }

                        SetState(
                            LootState::Opening
                        );
                    }

                    return;
                }

                case LootState::Looting:
                {
                    if (
                        lootAttempts_ >=
                            MaximumLootAttempts)
                    {
                        SetPendingFailure(
                            "native AutoLoot command "
                            "limit reached while loot "
                            "slots remained."
                        );

                        if (!IssueCloseLootWindow(
                                tick))
                        {
                            Fail(
                                pendingFailureReason_
                            );
                        }

                        SetState(
                            LootState::WaitingForClose
                        );

                        return;
                    }

                    if (!IssueNativeLootAll(
                            tick))
                    {
                        ++lootAttempts_;

                        lastLootTick_ =
                            tick;

                        Debug::Logger::Info(
                            "LOOT: native AutoLoot "
                            "dispatch failed."
                        );
                    }

                    SetState(
                        LootState::WaitingForClose
                    );

                    return;
                }

                case LootState::WaitingForClose:
                {
                    int slots =
                        -1;

                    const bool slotsReadable =
                        QueryLootSlots(
                            slots
                        );

                    if (
                        slotsReadable &&
                        slots !=
                            lastObservedLootSlots_)
                    {
                        Debug::Logger::Info(
                            "Native loot slots: " +
                            std::to_string(
                                lastObservedLootSlots_
                            ) +
                            " -> " +
                            std::to_string(
                                slots
                            )
                        );

                        lastObservedLootSlots_ =
                            slots;
                    }

                    /*
                     * If the client closed the loot frame
                     * by itself, only accept success when
                     * the most recent native slot count is
                     * zero (or the frame had zero item
                     * slots from the beginning).
                     */
                    if (
                        lootGuidReadable &&
                        observedLootOpen_ &&
                        currentLootGuid == 0)
                    {
                        const bool emptied =
                            lastObservedLootSlots_ == 0 ||
                            (
                                initialLootSlots_ == 0 &&
                                lastObservedLootSlots_ <= 0
                            );

                        if (
                            failurePending_ ||
                            !emptied)
                        {
                            Fail(
                                failurePending_
                                    ? pendingFailureReason_
                                    : "loot frame closed "
                                      "before native loot "
                                      "slots were verified "
                                      "empty."
                            );
                        }
                        else
                        {
                            FinishDone();
                        }

                        return;
                    }

                    if (
                        closeRequested_)
                    {
                        if (
                            tick <
                                closeRequestTick_ +
                                CloseWaitTicks)
                        {
                            return;
                        }

                        if (
                            lootGuidReadable &&
                            currentLootGuid != 0)
                        {
                            if (
                                closeAttempts_ <
                                    MaximumCloseAttempts)
                            {
                                IssueCloseLootWindow(
                                    tick
                                );

                                return;
                            }

                            Fail(
                                failurePending_
                                    ? pendingFailureReason_
                                    : "loot frame would "
                                      "not close."
                            );

                            return;
                        }

                        if (failurePending_)
                        {
                            Fail(
                                pendingFailureReason_
                            );

                            return;
                        }

                        const bool emptied =
                            lastObservedLootSlots_ == 0 ||
                            (
                                initialLootSlots_ == 0 &&
                                lastObservedLootSlots_ <= 0
                            );

                        if (emptied)
                        {
                            FinishDone();
                        }
                        else
                        {
                            Fail(
                                "loot frame closed "
                                "with unverified remaining "
                                "loot slots."
                            );
                        }

                        return;
                    }

                    /*
                     * Native loot has succeeded when the
                     * slot count reaches zero.
                     */
                    if (
                        slotsReadable &&
                        slots == 0)
                    {
                        if (firstEmptyTick_ == 0)
                        {
                            firstEmptyTick_ =
                                tick;

                            Debug::Logger::Info(
                                "LOOT: native loot slots "
                                "are empty."
                            );
                        }

                        if (
                            tick >=
                                firstEmptyTick_ +
                                EmptySettleTicks)
                        {
                            if (!IssueCloseLootWindow(
                                    tick))
                            {
                                Fail(
                                    "failed to close "
                                    "empty loot frame."
                                );

                                return;
                            }
                        }

                        return;
                    }

                    /*
                     * Slots remain. Retry native AutoLoot
                     * only after allowing time for server
                     * responses.
                     */
                    if (
                        tick >=
                            lastLootTick_ +
                            NativeLootRetryTicks)
                    {
                        if (
                            lootAttempts_ <
                                MaximumLootAttempts)
                        {
                            SetState(
                                LootState::Looting
                            );

                            return;
                        }

                        SetPendingFailure(
                            "native loot slots still "
                            "remaining after maximum "
                            "AutoLoot attempts."
                        );

                        if (!IssueCloseLootWindow(
                                tick))
                        {
                            Fail(
                                pendingFailureReason_
                            );

                            return;
                        }
                    }

                    return;
                }

                default:
                    return;
            }
        }

        void Reset()
        {
            state_ =
                LootState::Idle;

            corpseGuid_ =
                0;

            corpseX_ =
                0.0f;

            corpseY_ =
                0.0f;

            corpseZ_ =
                0.0f;

            destinationX_ =
                0.0f;

            destinationY_ =
                0.0f;

            destinationZ_ =
                0.0f;

            startTick_ =
                0;

            lastMoveTick_ =
                0;

            lastOpenTick_ =
                0;

            lastLootTick_ =
                0;

            firstEmptyTick_ =
                0;

            closeRequestTick_ =
                0;

            moveCommands_ =
                0;

            openAttempts_ =
                0;

            lootAttempts_ =
                0;

            closeAttempts_ =
                0;

            initialLootSlots_ =
                -1;

            lastObservedLootSlots_ =
                -1;

            observedLootOpen_ =
                false;

            closeRequested_ =
                false;

            failurePending_ =
                false;

            pendingFailureReason_.clear();

            lastObservedLootGuid_ =
                0;
        }

        LootState State() const
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
                state_ ==
                    LootState::Approaching ||
                state_ ==
                    LootState::Opening ||
                state_ ==
                    LootState::WaitingForOpen ||
                state_ ==
                    LootState::Looting ||
                state_ ==
                    LootState::WaitingForClose;
        }

        bool IsFinished() const
        {
            return
                state_ ==
                    LootState::Done ||
                state_ ==
                    LootState::NoLoot ||
                state_ ==
                    LootState::Failed;
        }

        bool Succeeded() const
        {
            return
                state_ ==
                    LootState::Done;
        }

        bool NoLoot() const
        {
            return
                state_ ==
                    LootState::NoLoot;
        }

        bool Failed() const
        {
            return
                state_ ==
                    LootState::Failed;
        }

        std::uint64_t CorpseGuid() const
        {
            return corpseGuid_;
        }

        int MoveCommands() const
        {
            return moveCommands_;
        }

        int OpenAttempts() const
        {
            return openAttempts_;
        }

        int LootAttempts() const
        {
            return lootAttempts_;
        }

        int InitialLootSlots() const
        {
            return initialLootSlots_;
        }

        int RemainingLootSlots() const
        {
            return lastObservedLootSlots_;
        }
    };
}
