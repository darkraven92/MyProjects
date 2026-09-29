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
    class AutoAttackController
    {
    private:
        /*
         * WoW 1.12.1 build 5875:
         *
         * Lua DoString absolute address:
         *     0x00704CD0
         *
         * Image base:
         *     0x00400000
         */
        static constexpr std::uintptr_t DoStringRva =
            0x00304CD0;

        // FrameScript_GetText, already runtime-used elsewhere in this project.
        static constexpr std::uintptr_t GetTextRva =
            0x00303BF0;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_AUTOATTACK_RESULT";

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

            if ((info.Protect & PAGE_GUARD) != 0)
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
        GetTextAddress()
        {
            return
                Wow5875::Client::Base() +
                GetTextRva;
        }

        static bool ExecuteLuaReadback(
            const char* script,
            const char* operationName,
            std::string& result)
        {
            if (script == nullptr || *script == '\0')
                return false;

            const auto doStringAddress =
                DoStringAddress();
            const auto getTextAddress =
                GetTextAddress();

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

            char buffer[128]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::IsGameThread();

                        luaExecuted =
                            doString(
                                script,
                                operationName);

                        if (!luaExecuted)
                            return;

                        const char* raw =
                            getText(
                                const_cast<char*>(ResultVariable),
                                0xFFFFFFFFu,
                                0);

                        if (raw != nullptr && *raw != '\0')
                        {
                            std::strncpy(
                                buffer,
                                raw,
                                sizeof(buffer) - 1);
                            buffer[sizeof(buffer) - 1] = '\0';
                            gotText = true;
                        }
                    });

            if (!dispatched ||
                !onGameThread ||
                !luaExecuted ||
                !gotText)
            {
                return false;
            }

            result = buffer;
            return true;
        }

        static bool ExecuteLua(
            const char* script,
            const char* operationName)
        {
            if (
                script == nullptr ||
                *script == '\0')
            {
                Debug::Logger::Info(
                    "AutoAttackController: "
                    "empty Lua script."
                );

                return false;
            }

            const auto address =
                DoStringAddress();

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "AutoAttackController: "
                    "DoString address is not executable."
                );

                return false;
            }

            /*
             * ClassicFramework FastCallDll for 1.12.1:
             *
             * typedef void __fastcall func(
             *     char* code,
             *     int zero);
             *
             * f(code, 0);
             */
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

            const DWORD callerThread =
                GetCurrentThreadId();

            Debug::Logger::Info(
                std::string(
                    "AutoAttack "
                ) +
                operationName +
                " caller thread: " +
                std::to_string(
                    callerThread
                )
            );

            bool executedOnGameThread =
                false;

            bool luaExecuted =
                false;

            DWORD executionThread =
                0;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        executionThread =
                            GetCurrentThreadId();

                        executedOnGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            std::string(
                                "AutoAttack "
                            ) +
                            operationName +
                            " executing thread: " +
                            std::to_string(
                                executionThread
                            )
                        );

                        Debug::Logger::Info(
                            std::string(
                                "AutoAttack "
                            ) +
                            operationName +
                            " on game thread: " +
                            (
                                executedOnGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        /*
                         * WoW's function is typed as
                         * char* rather than const char*.
                         * The literal is not modified by
                         * the client function.
                         */
                        luaExecuted =
                            doString(
                                script,
                                "wow-internal/AutoAttackController.lua"
                            );
                    }
                );

            if (!dispatched)
            {
                Debug::Logger::Info(
                    std::string(
                        "AutoAttack "
                    ) +
                    operationName +
                    " FAILED: game-thread "
                    "dispatch failed."
                );

                return false;
            }

            if (!executedOnGameThread)
            {
                Debug::Logger::Info(
                    std::string(
                        "AutoAttack "
                    ) +
                    operationName +
                    " FAILED: not executed "
                    "on game thread."
                );

                return false;
            }

            if (!luaExecuted)
            {
                Debug::Logger::Info(
                    std::string(
                        "AutoAttack "
                    ) +
                    operationName +
                    " FAILED: FrameScript_Execute "
                    "returned false."
                );

                return false;
            }

            Debug::Logger::Info(
                std::string(
                    "AutoAttack "
                ) +
                operationName +
                " dispatched successfully."
            );

            return true;
        }

    public:
        static std::uintptr_t
        DoStringAddress()
        {
            return
                Wow5875::Client::Base() +
                DoStringRva;
        }

        static bool Validate()
        {
            const auto address =
                DoStringAddress();

            Debug::Logger::Info(
                "Lua DoString address: " +
                Hex32(address)
            );

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "AutoAttack validation FAILED: "
                    "DoString not executable."
                );

                return false;
            }

            Debug::Logger::Info(
                "AutoAttack validation PASS."
            );

            return true;
        }

        static bool Start()
        {
            /*
             * Phase 14G.1.4:
             *
             * Do NOT assume that action slot 24 contains the
             * Attack action. The old code tested slot 24 and
             * then toggled CastSpellByName("Attack"). If the
             * Warrior had already entered autoattack through
             * Charge while slot 24 was unrelated, that toggle
             * could turn melee OFF immediately after Charge.
             *
             * Search all 120 Vanilla action slots for the real
             * attack action. UseAction() is only issued when
             * that exact action is not current. If Attack is
             * not on any action bar, AttackTarget() is issued exactly
             * once because CombatController owns an explicit off/on latch.
             * PlayerFrame.inCombat is deliberately NOT used as an
             * autoattack-state proxy: multi-aggro keeps that flag true even
             * after melee itself has been stopped.
             */
            static constexpr char AttackScript[] =
                "local a=nil; "
                "for i=1,120 do "
                "if IsAttackAction(i) then a=i; break; end; "
                "end; "
                "if a then "
                "if not IsCurrentAction(a) then UseAction(a); end; "
                "elseif UnitExists('target') and not UnitIsDead('target') then "
                "AttackTarget(); end";

            Debug::Logger::Info(
                "AUTOATTACK 14G.5.2.1: START requested; scanning all 120 Vanilla action slots."
            );

            return ExecuteLua(
                AttackScript,
                "START"
            );
        }

        static bool Stop()
        {
            /*
             * Mirror Start(): only toggle the real Attack action when it is
             * currently active. If Attack is not on a bar, prefer the
             * idempotent StopAttack() API when available; otherwise use the
             * Vanilla clear/restore-target fallback, which stops melee
             * without turning it back on.
             */
            static constexpr char StopAttackScript[] =
                "local a=nil; "
                "for i=1,120 do "
                "if IsAttackAction(i) then a=i; break; end; "
                "end; "
                "if a then "
                "if IsCurrentAction(a) then UseAction(a); end; "
                "elseif type(StopAttack)=='function' then StopAttack(); "
                "elseif UnitExists('target') then "
                "ClearTarget(); TargetLastTarget(); end";

            Debug::Logger::Info(
                "AUTOATTACK 14G.5.2.1: STOP requested; scanning all 120 Vanilla action slots."
            );

            return ExecuteLua(
                StopAttackScript,
                "STOP"
            );
        }

        struct AttackStatus
        {
            bool valid = false;
            bool actionSlotFound = false;
            bool active = false;
            int actionSlot = 0;
        };

        static bool Probe(AttackStatus& status)
        {
            status = AttackStatus{};

            static constexpr char ProbeScript[] =
                "local a=0; "
                "for i=1,120 do "
                "if IsAttackAction(i) then a=i; break; end; "
                "end; "
                "local c=-1; "
                "if a>0 then if IsCurrentAction(a) then c=1 else c=0 end end; "
                "WOW_INTERNAL_AUTOATTACK_RESULT=a..'|'..c";

            std::string result;
            if (!ExecuteLuaReadback(
                    ProbeScript,
                    "wow-internal/AutoAttackProbe.lua",
                    result))
            {
                return false;
            }

            int slot = 0;
            int current = -1;
            if (std::sscanf(
                    result.c_str(),
                    "%d|%d",
                    &slot,
                    &current) != 2)
            {
                return false;
            }

            status.valid = true;
            status.actionSlot = slot;
            status.actionSlotFound = slot > 0;
            status.active = slot > 0 && current == 1;
            return true;
        }

        static bool Restart()
        {
            /*
             * Phase 14G.5.2: deterministic re-engagement after a chase or
             * combat-liveness desync.  When Attack is on an action bar,
             * IsCurrentAction makes this idempotent.  If it is not on a bar,
             * first force the melee latch off (StopAttack when exposed by the
             * client, otherwise the Vanilla ClearTarget/TargetLastTarget
             * fallback), then AttackTarget exactly once.  This avoids the old
             * PlayerFrame.inCombat mistake: being in combat with a second mob
             * does not mean autoattack is actually active.
             */
            static constexpr char RestartScript[] =
                "local a=nil; "
                "for i=1,120 do "
                "if IsAttackAction(i) then a=i; break; end; "
                "end; "
                "if a then "
                "if not IsCurrentAction(a) then UseAction(a); end; "
                "else "
                "if type(StopAttack)=='function' then StopAttack(); "
                "elseif UnitExists('target') then "
                "ClearTarget(); TargetLastTarget(); end; "
                "if UnitExists('target') and not UnitIsDead('target') then "
                "AttackTarget(); end; "
                "end";

            Debug::Logger::Info(
                "AUTOATTACK 14G.5.2.1: deterministic RE-ENGAGE requested with 120-slot Attack detection."
            );

            return ExecuteLua(
                RestartScript,
                "RE-ENGAGE"
            );
        }
    };
}
