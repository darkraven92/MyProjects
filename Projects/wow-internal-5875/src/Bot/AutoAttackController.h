#pragma once

#include "GameThreadDispatcher.h"
#include "CombatClientEvidence5875.h"
#include "CombatActionEvidenceScript.h"
#include "CombatFacingPolicy.h"
#include "TargetController.h"

#include "../Debug/Logger.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <array>
#include <cmath>
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
            std::string& result,
            const Objects::WorldState* combatWorld = nullptr,
            std::uint64_t expectedTarget = 0,
            float meleeEnvelope = 0.0f,
            bool requireDisengaged = false)
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

                        if (combatWorld)
                        {
                            std::uint64_t selected=0;
                            std::uint32_t flags=0, health=0;
                            if (!CombatClientEvidence5875::Selection(*combatWorld,selected) ||
                                selected!=expectedTarget || !expectedTarget ||
                                !Core::Memory::Read(combatWorld->player.descriptors+0x2f8,flags) || (flags&0x10u) ||
                                !Core::Memory::Read(combatWorld->player.descriptors+0x58,health) || !health)
                                return;
                            const Objects::UnitState* target=nullptr;
                            for (const auto& unit:combatWorld->units)
                                if (unit.guid==expectedTarget) { target=&unit; break; }
                            Objects::PlayerState freshPlayer{};
                            Objects::UnitState freshTarget{};
                            if (!target || !Objects::PlayerSnapshot::Read(combatWorld->player.address,freshPlayer) ||
                                !Objects::UnitSnapshot::Read(target->address,freshTarget) ||
                                freshTarget.guid!=expectedTarget || !freshTarget.health)
                                return;
                            if (requireDisengaged)
                            {
                                const auto e=CombatClientEvidence5875::Execution(*combatWorld,*target);
                                if (!e.known || !e.aggressorsKnown || e.PlayerCombat() ||
                                    e.aggressor || e.targetVictim==combatWorld->activePlayerGuid ||
                                    e.playerHp!=combatWorld->player.health || e.targetHp!=target->health ||
                                    (e.playerVictim && e.playerVictim!=expectedTarget)) return;
                            }
                            const float dx=freshTarget.x-freshPlayer.x, dy=freshTarget.y-freshPlayer.y,
                                dz=freshTarget.z-freshPlayer.z;
                            const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
                            const float angle=std::fabs(std::remainder(std::atan2(dy,dx)-freshPlayer.rotation,
                                6.2831853071795864769f));
                            // A guarded release is not a melee attack. It may
                            // retire a stalled chase while the same optional
                            // target is out of range; fresh disengagement and
                            // exact selected-GUID checks above still apply.
                            if (!requireDisengaged &&
                                (!std::isfinite(distance) || distance>meleeEnvelope ||
                                 !CombatFacingPolicy::IsAbilityFacingReady(angle))) return;
                        }

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

        struct CombatActionEvidence
        {
            bool known=false, inputSafe=false, waiting=false;
            AttackStatus attack{};
            std::string reason="readback_failed";
        };
        static CombatActionEvidence ProbeCombatAction()
        {
            CombatActionEvidence e{};
            std::string result;
            if (!ExecuteLuaReadback(CombatActionEvidenceScript,"wow-internal/CombatEvidence.lua",result)) return e;
            e.reason=result;
            if (result=="blocked") { e.known=true; return e; }
            if (result=="wait") { e.known=true; e.inputSafe=true; e.waiting=true; return e; }
            int slot=0, active=-1;
            if (std::sscanf(result.c_str(),"%d|%d",&slot,&active)!=2 || slot<0 || slot>120 || active < -1 || active>1) return e;
            e.known=true; e.inputSafe=true;
            e.attack={true,slot>0,active==1,slot};
            return e;
        }
        static CombatActionEvidence ProbeSelectionReleaseInput()
        {
            CombatActionEvidence e{};
            std::string result;
            const auto script=CombatActionScript("selection_probe");
            if (!ExecuteLuaReadback(script.c_str(),"wow-internal/SelectionReleaseProbe.lua",result))
                return e;
            e.reason=result;
            e.known=result=="ready" || result=="blocked" || result=="wait";
            e.inputSafe=result=="ready";
            e.waiting=result=="wait";
            return e;
        }
        // Build-5875 SetTarget(0) takes the native clear-selection path at
        // 4938F3 -> 493910, clears B4E2D8/DC, and sends CMSG_SET_SELECTION
        // with the zero GUID. Never call it for an unrelated UI selection.
        static bool ClearOwnSelection(const Objects::WorldState& world,
            const Objects::UnitState& target, std::uint64_t guid)
        {
            bool issued=false;
            GameThreadDispatcher::Invoke([&]
            {
                if (!GameThreadDispatcher::IsGameThread() || !guid ||
                    target.guid!=guid || !TargetController::Validate()) return;
                const auto base=Wow5875::Client::Base();
                constexpr std::array<unsigned char,10> clearBranch{
                    0xb9,0x01,0x00,0x00,0x00,0xe8,0x13,0x00,0x00,0x00};
                constexpr std::array<unsigned char,13> zeroPacketGuid{
                    0x8b,0x15,0xdc,0xe2,0xb4,0x00,0xa1,0xd8,0xe2,0xb4,0x00,0x52,0x50};
                std::array<unsigned char,10> actualBranch{};
                std::array<unsigned char,13> actualPacketGuid{};
                if (!Core::Memory::Read(base+0x938f3,actualBranch) ||
                    actualBranch!=clearBranch ||
                    !Core::Memory::Read(base+0x93a49,actualPacketGuid) ||
                    actualPacketGuid!=zeroPacketGuid) return;
                const auto input=ProbeSelectionReleaseInput();
                if (!input.known || !input.inputSafe) return;
                const auto e=CombatClientEvidence5875::Execution(world,target);
                if (!e.known || !e.aggressorsKnown || e.selected!=guid ||
                    (e.playerVictim && e.playerVictim!=guid) || !e.playerHp ||
                    !e.targetHp || e.PlayerCombat() || e.TargetCombat() ||
                    e.aggressor || e.targetVictim==world.activePlayerGuid)
                    return;
                using SetTargetFunction=void (__stdcall*)(std::uint64_t);
                reinterpret_cast<SetTargetFunction>(TargetController::FunctionAddress())(0);
                issued=true; // Dispatch only. Fresh selection/victim reads verify later.
            });
            return issued;
        }
        static bool RecoverCombatAction(const Objects::WorldState& world,
            std::uint64_t guid, bool refresh, float meleeEnvelope)
        {
            std::string result;
            const auto script=CombatActionScript(refresh ? "refresh" : "start");
            return ExecuteLuaReadback(script.c_str(),"wow-internal/CombatRecovery.lua",result,&world,guid,meleeEnvelope) && result=="issued";
        }
        // First melee start only: unlike a latch repair, there is no prior
        // offensive action to preserve. The caller must have a separate safe
        // UI/cast probe; the game-thread fresh GUID/range/facing guard below
        // still applies. This does not bypass the normal recovery script.
        static bool BootstrapCombatAction(const Objects::WorldState& world,
            std::uint64_t guid, float meleeEnvelope)
        {
            std::string result;
            const auto script=CombatActionScript("bootstrap");
            return ExecuteLuaReadback(script.c_str(),"wow-internal/CombatBootstrap.lua",
                result,&world,guid,meleeEnvelope) && result=="issued";
        }
        static bool AbandonOwnCombatTarget(const Objects::WorldState& world,
            std::uint64_t guid, float meleeEnvelope)
        {
            std::string result;
            const auto script=CombatActionScript("abandon");
            return ExecuteLuaReadback(script.c_str(),"wow-internal/CombatTargetRelease.lua",
                result,&world,guid,meleeEnvelope,true) && result=="issued";
        }
        static bool RestoreCombatTarget(const Objects::WorldState& world,
            const Objects::UnitState& target)
        {
            bool issued=false;
            GameThreadDispatcher::Invoke([&]
            {
                if (!GameThreadDispatcher::IsGameThread()) return;
                std::uint64_t selected=0;
                std::uint32_t hp=0, playerHp=0, period=0;
                const auto action=ProbeCombatAction();
                if (!action.known || !action.inputSafe || action.waiting ||
                    !CombatClientEvidence5875::Selection(world,selected) ||
                    !CombatClientEvidence5875::FreshHealth(world,target,hp,playerHp,period) || !hp)
                    return;
                issued=TargetController::SetTarget(target.guid);
            });
            return issued; // Native return only; actual UI selection verifies later.
        }

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
