#pragma once

#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"
#include "../Objects/UnitSnapshot.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    class WarriorRotationController
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

        /*
         * PlayerState::powerRaw stores rage in tenths.
         *
         * 100 = 10.0 rage
         * 150 = 15.0 rage
         */
        static constexpr std::uint32_t BattleShoutMinimumRageRaw =
            100;

        /*
         * Phase 14M.0 keeps a 20-rage reserve above the current
         * Heroic Strike cost so reactive/control abilities are not
         * starved by queued swings. PlayerState::powerRaw is tenths.
         */
        static constexpr std::uint32_t HeroicStrikeMinimumRageRaw =
            350;

        /*
         * Cast the highest trained Rend rank by name. The current
         * low-rank rage requirement is 10 rage; cast at most once
         * per target and skip targets already near death.
         */
        static constexpr std::uint32_t RendMinimumRageRaw =
            100;

        static constexpr std::uint64_t RendMinimumTargetHealthPercent =
            35;

        /*
         * Phase 14M.0.1 reactive/control thresholds.
         * Build 5875 exposes IsUsableAction but not IsUsableSpell.
         * Overpower therefore resolves its spellbook texture to an action
         * slot and lets IsUsableAction/GetActionCooldown remain authoritative
         * for dodge-reactive availability. If Overpower is not present on
         * any action bar slot, the guarded probe simply skips the cast.
         */
        static constexpr std::uint32_t OverpowerMinimumRageRaw =
            50;

        static constexpr std::uint64_t OverpowerProbeIntervalTicks =
            4;

        static constexpr std::uint64_t OverpowerObservationWindowTicks =
            2;

        static constexpr std::uint64_t OverpowerMinimumTargetHealthPercent =
            10;

        /*
         * Bloodrage is a guarded low-rage recovery probe. Do not use
         * it on a damaged player; recovery owns low-health downtime.
         */
        static constexpr std::uint32_t BloodrageMaximumRageRaw =
            150;

        static constexpr std::uint64_t BloodrageMinimumHealthPercent =
            75;

        static constexpr std::uint64_t BloodrageProbeIntervalTicks =
            20;

        /*
         * No reliable fleeing flag exists in the current UnitState.
         * Hamstring is therefore bounded runner prevention: one guarded
         * command when a living target enters the low-health flee band.
         */
        static constexpr std::uint32_t HamstringMinimumRageRaw =
            100;

        static constexpr std::uint64_t HamstringMaximumTargetHealthPercent =
            30;

        static constexpr std::uint64_t HamstringMinimumTargetHealthPercent =
            5;

        /*
         * Charge is used only as an opener when the target
         * is already selected and starts inside the normal
         * Vanilla Charge distance band.
         *
         * We deliberately do not try to "micro-chase" into
         * Charge range in this phase. If the target begins
         * outside this band the existing verified CTM chase
         * remains responsible for approach.
         */
        static constexpr float ChargeMinimumDistance =
            8.0f;

        static constexpr float ChargeMaximumDistance =
            25.0f;

        /*
         * Thunder Clap costs 20 rage at the currently trained rank.
         * Phase 14M.0 reserves it for real multi-aggro: two or more
         * remembered/direct aggressors within the 8 yd combat radius.
         */
        static constexpr std::uint32_t ThunderClapMinimumRageRaw =
            200;

        /*
         * Battle Shout lasts long enough that repeatedly
         * spending rage on every target would be wasteful.
         *
         * 400 ticks * 250 ms ~= 100 seconds.
         *
         * This refresh window is intentionally shorter than
         * the normal buff duration so it can refresh before
         * expiry when combat is active.
         */
        static constexpr std::uint64_t BattleShoutRefreshTicks =
            400;

        /*
         * The Phase 6 live test showed repeated Charge
         * commands with zero movement observations.
         *
         * Do not spend one second on every future target
         * if Charge is currently untrained/unusable.
         * After two opener windows without any observed
         * Charge movement, disable Charge until the next
         * WoW process starts.
         */
        static constexpr int MaximumChargeNoMoveWindows =
            2;

        /*
         * Prevent duplicate Heroic Strike commands caused
         * by rapid external target-health changes.
         *
         * 6 * 250 ms = 1.5 seconds.
         */
        static constexpr std::uint64_t HeroicStrikeMinimumCommandIntervalTicks =
            6;

        std::uint64_t targetGuid_ =
            0;

        std::uint32_t previousTargetHealth_ =
            0;

        // =============================================
        // Charge state
        // =============================================

        bool chargeAttemptedForTarget_ =
            false;

        bool chargeObservationPending_ =
            false;

        bool chargeMovementObservedForTarget_ =
            false;

        float chargeStartDistance_ =
            0.0f;

        int chargeCommands_ =
            0;

        int chargeMovementObservations_ =
            0;

        int chargeNoMoveWindows_ =
            0;

        bool chargeDisabled_ =
            false;

        // =============================================
        // Thunder Clap state
        // =============================================

        bool thunderClapAttemptedForTarget_ =
            false;

        bool waitingForThunderClapRageSpend_ =
            false;

        std::uint32_t thunderClapPowerRaw_ =
            0;

        int thunderClapCommands_ =
            0;

        int thunderClapRageSpendObservations_ =
            0;

        // =============================================
        // Overpower state
        // =============================================

        std::uint64_t lastOverpowerProbeTick_ =
            0;

        bool waitingForOverpowerRageSpend_ =
            false;

        std::uint32_t overpowerProbePowerRaw_ =
            0;

        std::uint64_t overpowerObservationUntilTick_ =
            0;

        int overpowerProbes_ =
            0;

        int overpowerRageSpendObservations_ =
            0;

        // =============================================
        // Bloodrage state
        // =============================================

        std::uint64_t lastBloodrageProbeTick_ =
            0;

        int bloodrageProbes_ =
            0;

        // =============================================
        // Battle Shout state
        // =============================================

        bool battleShoutHasCommand_ =
            false;

        std::uint64_t lastBattleShoutCommandTick_ =
            0;

        bool waitingForBattleShoutRageSpend_ =
            false;

        std::uint32_t battleShoutPowerRaw_ =
            0;

        int battleShoutCommands_ =
            0;

        int battleShoutRageSpendObservations_ =
            0;

        // =============================================
        // Rend state
        // =============================================

        bool rendAttemptedForTarget_ =
            false;

        int rendCommands_ =
            0;

        // =============================================
        // Hamstring state
        // =============================================

        bool hamstringAttemptedForTarget_ =
            false;

        int hamstringCommands_ =
            0;

        // =============================================
        // Heroic Strike state
        // =============================================

        std::uint64_t lastHeroicStrikeCommandTick_ =
            0;

        bool hasIssuedHeroicStrike_ =
            false;

        bool waitingForPostQueueDamage_ =
            false;

        bool waitingForHeroicStrikeRageSpend_ =
            false;

        std::uint32_t heroicStrikeQueuePowerRaw_ =
            0;

        int heroicStrikeCommands_ =
            0;

        int postQueueDamageEvents_ =
            0;

        int heroicStrikeRageSpendObservations_ =
            0;

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
                << std::setprecision(1)
                << value;

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

        static std::uintptr_t DoStringAddress()
        {
            return
                Wow5875::Client::Base() +
                DoStringRva;
        }

        static bool ExecuteLua(
            const char* script,
            const char* operationName)
        {
            if (
                script == nullptr ||
                *script == '\0')
            {
                return false;
            }

            const auto address =
                DoStringAddress();

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

            DWORD executionThread =
                0;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        executionThread =
                            GetCurrentThreadId();

                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            std::string(
                                operationName
                            ) +
                            " executing thread: " +
                            std::to_string(
                                executionThread
                            )
                        );

                        Debug::Logger::Info(
                            std::string(
                                operationName
                            ) +
                            " on game thread: " +
                            (
                                onGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        luaExecuted =
                            doString(
                                script,
                                "wow-internal/WarriorRotationController.lua"
                            );
                    }
                );

            if (
                dispatched &&
                onGameThread &&
                !luaExecuted)
            {
                Debug::Logger::Info(
                    std::string(
                        operationName
                    ) +
                    " FrameScript_Execute returned false."
                );
            }

            return
                dispatched &&
                onGameThread &&
                luaExecuted;
        }

        bool ProbeOverpower(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            static constexpr char OverpowerScript[] =
                "if type(UnitExists)=='function' "
                "and type(UnitIsDead)=='function' "
                "and type(UnitCanAttack)=='function' "
                "and type(UnitClass)=='function' "
                "and type(GetSpellName)=='function' "
                "and type(GetSpellTexture)=='function' "
                "and type(GetActionTexture)=='function' "
                "and type(IsUsableAction)=='function' "
                "and type(GetActionCooldown)=='function' "
                "and type(CastSpellByName)=='function' "
                "and UnitExists('target') "
                "and not UnitIsDead('target') "
                "and UnitCanAttack('player','target') then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "local bt=(type(BOOKTYPE_SPELL)=='string' and BOOKTYPE_SPELL or 'spell'); "
                "local tex=nil; "
                "for i=1,200 do "
                "local n=GetSpellName(i,bt); "
                "if not n then break end; "
                "if n=='Overpower' then tex=GetSpellTexture(i,bt); break end; "
                "end; "
                "if tex then "
                "for a=1,120 do "
                "if GetActionTexture(a)==tex then "
                "local u=IsUsableAction(a); "
                "local _,d=GetActionCooldown(a); "
                "if u and (not d or d==0) then "
                "CastSpellByName('Overpower'); "
                "break; "
                "end; "
                "end; "
                "end; "
                "end; "
                "end; "
                "end";

            const bool issued =
                ExecuteLua(
                    OverpowerScript,
                    "OverpowerProbe"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "OVERPOWER 14M.0.1: guarded action-slot probe FAILED."
                );

                return false;
            }

            lastOverpowerProbeTick_ =
                tick;

            waitingForOverpowerRageSpend_ =
                true;

            overpowerProbePowerRaw_ =
                player.powerRaw;

            overpowerObservationUntilTick_ =
                tick + OverpowerObservationWindowTicks;

            ++overpowerProbes_;

            Debug::Logger::Info(
                "OVERPOWER 14M.0.1: guarded 5875 action-slot probe dispatched target=" +
                Hex64(target.guid) +
                " rage=" +
                Float(player.power) +
                ". IsUsableAction/GetActionCooldown are client-authoritative; missing action slot is a safe skip."
            );

            return true;
        }

        bool ProbeBloodrage(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            static constexpr char BloodrageScript[] =
                "if type(UnitClass)=='function' "
                "and type(GetSpellName)=='function' "
                "and type(GetSpellCooldown)=='function' "
                "and type(CastSpellByName)=='function' then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "local bt=(type(BOOKTYPE_SPELL)=='string' and BOOKTYPE_SPELL or 'spell'); "
                "local si=nil; "
                "for i=1,200 do "
                "local n=GetSpellName(i,bt); "
                "if not n then break end; "
                "if n=='Bloodrage' then si=i; break end; "
                "end; "
                "if si then "
                "local _,d=GetSpellCooldown(si,bt); "
                "if not d or d==0 then CastSpellByName('Bloodrage'); end; "
                "end; "
                "end; "
                "end";

            const bool issued =
                ExecuteLua(
                    BloodrageScript,
                    "BloodrageProbe"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "BLOODRAGE 14M.0.1: guarded spellbook cooldown probe FAILED."
                );

                return false;
            }

            lastBloodrageProbeTick_ =
                tick;

            ++bloodrageProbes_;

            Debug::Logger::Info(
                "BLOODRAGE 14M.0.1: guarded 5875 spellbook cooldown probe dispatched rage=" +
                Float(player.power) +
                "."
            );

            return true;
        }

        bool CastHamstring(
            const Objects::PlayerState& player,
            const Objects::UnitState& target)
        {
            static constexpr char HamstringScript[] =
                "if type(UnitExists)=='function' "
                "and type(UnitIsDead)=='function' "
                "and type(UnitCanAttack)=='function' "
                "and type(UnitClass)=='function' "
                "and type(GetSpellName)=='function' "
                "and type(GetSpellCooldown)=='function' "
                "and type(CastSpellByName)=='function' "
                "and UnitExists('target') "
                "and not UnitIsDead('target') "
                "and UnitCanAttack('player','target') then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "local bt=(type(BOOKTYPE_SPELL)=='string' and BOOKTYPE_SPELL or 'spell'); "
                "local si=nil; "
                "for i=1,200 do "
                "local n=GetSpellName(i,bt); "
                "if not n then break end; "
                "if n=='Hamstring' then si=i; break end; "
                "end; "
                "if si then "
                "local _,d=GetSpellCooldown(si,bt); "
                "if not d or d==0 then CastSpellByName('Hamstring'); end; "
                "end; "
                "end; "
                "end";

            Debug::Logger::Info(
                "HAMSTRING 14M.0.1: low-health runner-control command requested."
            );

            const bool issued =
                ExecuteLua(
                    HamstringScript,
                    "Hamstring"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "HAMSTRING 14M.0.1: command FAILED."
                );

                return false;
            }

            hamstringAttemptedForTarget_ =
                true;

            ++hamstringCommands_;

            Debug::Logger::Info(
                "HAMSTRING 14M.0.1: guarded 5875 spellbook cooldown command dispatched target=" +
                Hex64(target.guid) +
                " rage=" +
                Float(player.power) +
                "."
            );

            return true;
        }

        bool QueueHeroicStrike(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            static constexpr char HeroicStrikeScript[] =
                "if UnitExists('target') "
                "and not UnitIsDead('target') "
                "and UnitCanAttack('player','target') then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "CastSpellByName('Heroic Strike') "
                "end end";

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "HEROIC STRIKE: queue requested."
            );

            Debug::Logger::Info(
                "Target GUID: " +
                Hex64(
                    target.guid
                )
            );

            Debug::Logger::Info(
                "Target health: " +
                std::to_string(
                    target.health
                ) +
                "/" +
                std::to_string(
                    target.maxHealth
                )
            );

            Debug::Logger::Info(
                "Rage: " +
                Float(
                    player.power
                )
            );

            const bool issued =
                ExecuteLua(
                    HeroicStrikeScript,
                    "HeroicStrike"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "HEROIC STRIKE: command FAILED."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            hasIssuedHeroicStrike_ =
                true;

            lastHeroicStrikeCommandTick_ =
                tick;

            waitingForPostQueueDamage_ =
                true;

            waitingForHeroicStrikeRageSpend_ =
                true;

            heroicStrikeQueuePowerRaw_ =
                player.powerRaw;

            ++heroicStrikeCommands_;

            Debug::Logger::Info(
                "HEROIC STRIKE: command issued."
            );

            Debug::Logger::Info(
                "Commands: " +
                std::to_string(
                    heroicStrikeCommands_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        bool CastRend(
            const Objects::PlayerState& player,
            const Objects::UnitState& target)
        {
            static constexpr char RendScript[] =
                "if UnitExists('target') "
                "and not UnitIsDead('target') "
                "and UnitCanAttack('player','target') then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "CastSpellByName('Rend') "
                "end end";

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "REND: cast requested."
            );

            Debug::Logger::Info(
                "Target GUID: " +
                Hex64(
                    target.guid
                )
            );

            Debug::Logger::Info(
                "Target health: " +
                std::to_string(
                    target.health
                ) +
                "/" +
                std::to_string(
                    target.maxHealth
                )
            );

            Debug::Logger::Info(
                "Rage: " +
                Float(
                    player.power
                )
            );

            const bool issued =
                ExecuteLua(
                    RendScript,
                    "Rend"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "REND: command FAILED."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            rendAttemptedForTarget_ =
                true;

            ++rendCommands_;

            Debug::Logger::Info(
                "REND: command issued."
            );

            Debug::Logger::Info(
                "Commands: " +
                std::to_string(
                    rendCommands_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        bool CastThunderClap(
            const Objects::PlayerState& player,
            const Objects::UnitState& target)
        {
            static constexpr char ThunderClapScript[] =
                "if UnitExists('target') "
                "and not UnitIsDead('target') "
                "and UnitCanAttack('player','target') then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "CastSpellByName('Thunder Clap') "
                "end end";

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "THUNDER CLAP 14G.1.1: cast requested."
            );

            Debug::Logger::Info(
                "Target GUID: " +
                Hex64(
                    target.guid
                )
            );

            Debug::Logger::Info(
                "Target distance: " +
                Float(
                    target.distance
                )
            );

            Debug::Logger::Info(
                "Rage: " +
                Float(
                    player.power
                )
            );

            const bool issued =
                ExecuteLua(
                    ThunderClapScript,
                    "ThunderClap"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "THUNDER CLAP 14G.1.1: command FAILED."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            thunderClapAttemptedForTarget_ =
                true;

            waitingForThunderClapRageSpend_ =
                true;

            thunderClapPowerRaw_ =
                player.powerRaw;

            ++thunderClapCommands_;

            Debug::Logger::Info(
                "THUNDER CLAP 14G.1.1: command issued."
            );

            Debug::Logger::Info(
                "Commands: " +
                std::to_string(
                    thunderClapCommands_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        bool CastBattleShout(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            static constexpr char BattleShoutScript[] =
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "CastSpellByName('Battle Shout') "
                "end";

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "BATTLE SHOUT: cast requested."
            );

            Debug::Logger::Info(
                "Rage: " +
                Float(
                    player.power
                )
            );

            const bool issued =
                ExecuteLua(
                    BattleShoutScript,
                    "BattleShout"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "BATTLE SHOUT: command FAILED."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            battleShoutHasCommand_ =
                true;

            lastBattleShoutCommandTick_ =
                tick;

            waitingForBattleShoutRageSpend_ =
                true;

            battleShoutPowerRaw_ =
                player.powerRaw;

            ++battleShoutCommands_;

            Debug::Logger::Info(
                "BATTLE SHOUT: command issued."
            );

            Debug::Logger::Info(
                "Commands: " +
                std::to_string(
                    battleShoutCommands_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

    public:
        static bool Validate()
        {
            const auto address =
                DoStringAddress();

            std::ostringstream stream;

            stream
                << "Warrior Lua DoString address: 0x"
                << std::hex
                << std::uppercase
                << std::setw(8)
                << std::setfill('0')
                << static_cast<std::uint32_t>(
                    address
                );

            Debug::Logger::Info(
                stream.str()
            );

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "WarriorRotation validation FAILED."
                );

                return false;
            }

            Debug::Logger::Info(
                "WarriorRotation validation PASS."
            );

            return true;
        }

        void BeginTarget(
            const Objects::UnitState& target)
        {
            targetGuid_ =
                target.guid;

            previousTargetHealth_ =
                target.health;

            chargeAttemptedForTarget_ =
                false;

            chargeObservationPending_ =
                false;

            chargeMovementObservedForTarget_ =
                false;

            chargeStartDistance_ =
                0.0f;

            thunderClapAttemptedForTarget_ =
                false;

            waitingForThunderClapRageSpend_ =
                false;

            thunderClapPowerRaw_ =
                0;

            lastOverpowerProbeTick_ =
                0;

            waitingForOverpowerRageSpend_ =
                false;

            overpowerProbePowerRaw_ =
                0;

            overpowerObservationUntilTick_ =
                0;

            rendAttemptedForTarget_ =
                false;

            hamstringAttemptedForTarget_ =
                false;

            hasIssuedHeroicStrike_ =
                false;

            lastHeroicStrikeCommandTick_ =
                0;

            waitingForPostQueueDamage_ =
                false;

            waitingForHeroicStrikeRageSpend_ =
                false;

            heroicStrikeQueuePowerRaw_ =
                0;

            Debug::Logger::Info(
                "WARRIOR ROTATION: target started."
            );

            Debug::Logger::Info(
                "Rotation target GUID: " +
                Hex64(
                    targetGuid_
                )
            );

            Debug::Logger::Info(
                "Charge policy: enabled at 8-25 yards; "
                "Phase 14G.1.1 grind acquisition now hands targets over inside this band."
            );

            Debug::Logger::Info(
                "CROSSROADS WARRIOR 14M.0: Thunder Clap requires >=2 direct aggressors within 8 yd; single-target rage is preserved."
            );

            Debug::Logger::Info(
                "CROSSROADS WARRIOR 14M.0.1: build 5875 compatibility uses IsUsableAction/GetActionCooldown for Overpower and spellbook-slot GetSpellCooldown for Bloodrage/Hamstring; every new Lua API is guarded."
            );

            Debug::Logger::Info(
                "Rend policy: cast highest trained rank once per target when rage >=10 and target health >35%."
            );

            Debug::Logger::Info(
                "CROSSROADS WARRIOR 14M.0: Hamstring is one-shot runner prevention at 5-30% target HP; no synthetic flee flag is invented."
            );

            Debug::Logger::Info(
                "Battle Shout policy: cast when due and rage >=10."
            );

            Debug::Logger::Info(
                "Heroic Strike policy: queue after observed melee damage only with rage >=35, preserving rage for reactive/control abilities."
            );
        }

        void EndTarget()
        {
            if (targetGuid_ != 0)
            {
                Debug::Logger::Info(
                    "WARRIOR ROTATION: target ended."
                );
            }

            targetGuid_ =
                0;

            previousTargetHealth_ =
                0;

            chargeAttemptedForTarget_ =
                false;

            chargeObservationPending_ =
                false;

            chargeMovementObservedForTarget_ =
                false;

            chargeStartDistance_ =
                0.0f;

            thunderClapAttemptedForTarget_ =
                false;

            waitingForThunderClapRageSpend_ =
                false;

            thunderClapPowerRaw_ =
                0;

            lastOverpowerProbeTick_ =
                0;

            waitingForOverpowerRageSpend_ =
                false;

            overpowerProbePowerRaw_ =
                0;

            overpowerObservationUntilTick_ =
                0;

            waitingForBattleShoutRageSpend_ =
                false;

            battleShoutPowerRaw_ =
                0;

            rendAttemptedForTarget_ =
                false;

            hamstringAttemptedForTarget_ =
                false;

            hasIssuedHeroicStrike_ =
                false;

            lastHeroicStrikeCommandTick_ =
                0;

            waitingForPostQueueDamage_ =
                false;

            waitingForHeroicStrikeRageSpend_ =
                false;

            heroicStrikeQueuePowerRaw_ =
                0;
        }

        bool CanPrepareCharge(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            bool selectedTargetMatches) const
        {
            return
                !chargeAttemptedForTarget_ &&
                !chargeDisabled_ &&
                player.valid &&
                target.valid &&
                target.health > 0 &&
                target.maxHealth > 0 &&
                selectedTargetMatches &&
                target.distance >=
                    ChargeMinimumDistance &&
                target.distance <=
                    ChargeMaximumDistance;
        }

        bool TryCharge(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick,
            bool selectedTargetMatches,
            bool facingReady)
        {
            if (
                targetGuid_ == 0 ||
                targetGuid_ != target.guid)
            {
                BeginTarget(
                    target
                );
            }

            if (chargeAttemptedForTarget_)
            {
                return false;
            }

            if (chargeDisabled_)
            {
                Debug::Logger::Info(
                    "CHARGE: disabled for this session "
                    "after repeated no-movement windows."
                );

                return false;
            }

            if (
                !player.valid ||
                !target.valid ||
                target.health == 0 ||
                target.maxHealth == 0 ||
                !selectedTargetMatches)
            {
                Debug::Logger::Info(
                    "CHARGE: opener rejected by "
                    "target/player validation."
                );

                return false;
            }

            if (
                target.distance <
                    ChargeMinimumDistance ||
                target.distance >
                    ChargeMaximumDistance)
            {
                Debug::Logger::Info(
                    "CHARGE: target outside opener "
                    "range for now; attempt remains "
                    "available (distance=" +
                    Float(
                        target.distance
                    ) +
                    ")."
                );

                return false;
            }

            if (!facingReady)
            {
                Debug::Logger::Info(
                    "CHARGE: pre-cast facing is not verified; caller should use bounded facing acquisition or chase fallback."
                );

                return false;
            }

            chargeAttemptedForTarget_ =
                true;

            static constexpr char ChargeScript[] =
                "if UnitExists('target') "
                "and not UnitIsDead('target') "
                "and UnitCanAttack('player','target') then "
                "local _,c=UnitClass('player'); "
                "if c=='WARRIOR' then "
                "CastSpellByName('Charge') "
                "end end";

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "CHARGE: opener requested."
            );

            Debug::Logger::Info(
                "Target GUID: " +
                Hex64(
                    target.guid
                )
            );

            Debug::Logger::Info(
                "Target distance: " +
                Float(
                    target.distance
                )
            );

            const bool issued =
                ExecuteLua(
                    ChargeScript,
                    "Charge"
                );

            if (!issued)
            {
                Debug::Logger::Info(
                    "CHARGE: command FAILED."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            chargeObservationPending_ =
                true;

            chargeMovementObservedForTarget_ =
                false;

            chargeStartDistance_ =
                target.distance;

            ++chargeCommands_;

            Debug::Logger::Info(
                "CHARGE: command issued."
            );

            Debug::Logger::Info(
                "Commands: " +
                std::to_string(
                    chargeCommands_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        void ObserveCharge(
            const Objects::UnitState& target)
        {
            if (
                !chargeObservationPending_ ||
                target.guid !=
                    targetGuid_)
            {
                return;
            }

            /*
             * A large distance collapse immediately after
             * the opener is useful evidence that Charge
             * actually fired. This is diagnostic only
             * because the target itself can move.
             */
            if (
                !chargeMovementObservedForTarget_ &&
                chargeStartDistance_ >=
                    ChargeMinimumDistance &&
                target.distance <= 5.5f)
            {
                chargeMovementObservedForTarget_ =
                    true;

                ++chargeMovementObservations_;

                Debug::Logger::Info(
                    "CHARGE: close-range movement "
                    "observed (" +
                    Float(
                        chargeStartDistance_
                    ) +
                    " -> " +
                    Float(
                        target.distance
                    ) +
                    ")."
                );
            }
        }

        void FinishChargeWindow(
            const Objects::UnitState& target)
        {
            if (!chargeObservationPending_)
            {
                return;
            }

            ObserveCharge(
                target
            );

            if (
                chargeMovementObservedForTarget_)
            {
                Debug::Logger::Info(
                    "CHARGE: opener window complete; "
                    "movement evidence observed."
                );
            }
            else
            {
                ++chargeNoMoveWindows_;

                Debug::Logger::Info(
                    "CHARGE: opener window complete; "
                    "no close-range movement evidence, "
                    "falling back to CTM chase."
                );

                Debug::Logger::Info(
                    "CHARGE: no-movement windows: " +
                    std::to_string(
                        chargeNoMoveWindows_
                    ) +
                    "/" +
                    std::to_string(
                        MaximumChargeNoMoveWindows
                    )
                );

                if (
                    chargeNoMoveWindows_ >=
                        MaximumChargeNoMoveWindows)
                {
                    chargeDisabled_ =
                        true;

                    Debug::Logger::Info(
                        "CHARGE: disabled for the "
                        "remainder of this session."
                    );
                }
            }

            chargeObservationPending_ =
                false;
        }

        void Update(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick,
            bool autoAttackStarted,
            bool inRange,
            bool selectedTargetMatches,
            bool facingReady,
            std::size_t nearbyAggressorCount)
        {
            if (
                targetGuid_ == 0 ||
                targetGuid_ != target.guid)
            {
                BeginTarget(
                    target
                );
            }

            if (
                !player.valid ||
                !target.valid ||
                target.health == 0 ||
                target.maxHealth == 0)
            {
                return;
            }

            /*
             * Thunder Clap rage-spend observation.
             * This gives the runtime test a stronger signal than
             * FrameScript_Execute alone: if rage drops after the
             * command, the client accepted a rage-spending ability.
             */
            if (
                waitingForThunderClapRageSpend_ &&
                player.powerRaw <
                    thunderClapPowerRaw_)
            {
                ++thunderClapRageSpendObservations_;

                waitingForThunderClapRageSpend_ =
                    false;

                Debug::Logger::Info(
                    "THUNDER CLAP 14G.1.1: rage spend observed (" +
                    std::to_string(
                        thunderClapPowerRaw_
                    ) +
                    " -> " +
                    std::to_string(
                        player.powerRaw
                    ) +
                    ")."
                );
            }

            /*
             * A probe only calls CastSpellByName when Vanilla reports
             * Overpower usable and off cooldown. Because no other rage
             * ability is issued in the probe tick, an immediate rage drop
             * is useful runtime evidence that the reactive cast landed.
             */
            if (waitingForOverpowerRageSpend_)
            {
                if (player.powerRaw < overpowerProbePowerRaw_)
                {
                    ++overpowerRageSpendObservations_;

                    waitingForOverpowerRageSpend_ =
                        false;

                    Debug::Logger::Info(
                        "OVERPOWER 14M.0.1: probable cast confirmed by immediate rage spend (" +
                        std::to_string(overpowerProbePowerRaw_) +
                        " -> " +
                        std::to_string(player.powerRaw) +
                        ")."
                    );
                }
                else if (tick > overpowerObservationUntilTick_)
                {
                    waitingForOverpowerRageSpend_ =
                        false;
                }
            }

            /*
             * Battle Shout rage-spend observation.
             * This is diagnostic only because incoming
             * damage can add rage at the same time.
             */
            if (
                waitingForBattleShoutRageSpend_ &&
                player.powerRaw <
                    battleShoutPowerRaw_)
            {
                ++battleShoutRageSpendObservations_;

                waitingForBattleShoutRageSpend_ =
                    false;

                Debug::Logger::Info(
                    "BATTLE SHOUT: rage spend "
                    "observed (" +
                    std::to_string(
                        battleShoutPowerRaw_
                    ) +
                    " -> " +
                    std::to_string(
                        player.powerRaw
                    ) +
                    ")."
                );
            }

            /*
             * Heroic Strike rage-spend observation.
             */
            if (
                waitingForHeroicStrikeRageSpend_ &&
                player.powerRaw <
                    heroicStrikeQueuePowerRaw_)
            {
                ++heroicStrikeRageSpendObservations_;

                waitingForHeroicStrikeRageSpend_ =
                    false;

                Debug::Logger::Info(
                    "HEROIC STRIKE: rage spend observed "
                    "(" +
                    std::to_string(
                        heroicStrikeQueuePowerRaw_
                    ) +
                    " -> " +
                    std::to_string(
                        player.powerRaw
                    ) +
                    ")."
                );
            }

            const bool targetTookDamage =
                previousTargetHealth_ > 0 &&
                target.health <
                    previousTargetHealth_;

            if (targetTookDamage)
            {
                Debug::Logger::Info(
                    "WARRIOR: target damage observed "
                    "(" +
                    std::to_string(
                        previousTargetHealth_
                    ) +
                    " -> " +
                    std::to_string(
                        target.health
                    ) +
                    ")."
                );

                if (waitingForPostQueueDamage_)
                {
                    ++postQueueDamageEvents_;

                    waitingForPostQueueDamage_ =
                        false;

                    Debug::Logger::Info(
                        "HEROIC STRIKE: post-queue "
                        "damage event observed."
                    );
                }
            }

            previousTargetHealth_ =
                target.health;

            /*
             * Do not gate the whole Warrior rotation behind
             * autoAttackStarted. Phase 9B.3 exposed a
             * deadlock after Charge where facing correction
             * could keep autoattack from starting, which in
             * turn prevented every Warrior ability.
             *
             * Target/range are the global requirements.
             * Individual abilities apply their own
             * autoattack/facing requirements below.
             */
            if (
                !inRange ||
                !selectedTargetMatches)
            {
                return;
            }

            const std::uint64_t healthPercent =
                (
                    static_cast<std::uint64_t>(
                        target.health
                    ) *
                    100ULL
                ) /
                static_cast<std::uint64_t>(
                    target.maxHealth
                );

            const std::uint64_t playerHealthPercent =
                player.maxHealth > 0
                    ? (
                        static_cast<std::uint64_t>(
                            player.health
                        ) *
                        100ULL
                    ) /
                    static_cast<std::uint64_t>(
                        player.maxHealth
                    )
                    : 0ULL;

            /*
             * Phase 14M.0: Thunder Clap is no longer a single-target
             * opener. Preserve rage unless there are at least two direct/
             * remembered aggressors physically inside its local 8 yd band.
             */
            if (
                facingReady &&
                nearbyAggressorCount >= 2 &&
                !thunderClapAttemptedForTarget_ &&
                player.powerRaw >=
                    ThunderClapMinimumRageRaw)
            {
                Debug::Logger::Info(
                    "THUNDER CLAP 14M.0: multi-aggro gate open aggressors=" +
                    std::to_string(nearbyAggressorCount) +
                    "."
                );

                if (
                    CastThunderClap(
                        player,
                        target
                    ))
                {
                    return;
                }
            }

            if (
                !facingReady &&
                nearbyAggressorCount >= 2 &&
                !thunderClapAttemptedForTarget_ &&
                player.powerRaw >=
                    ThunderClapMinimumRageRaw)
            {
                Debug::Logger::Info(
                    "THUNDER CLAP 14M.0: multi-aggro ready but waiting for facing alignment."
                );
            }

            /*
             * Overpower is reactive and time-limited. Build 5875 has no
             * IsUsableSpell global, so the guarded Lua probe resolves the
             * spell texture to an action slot and asks IsUsableAction plus
             * GetActionCooldown. The probe is rate-limited to once per
             * second; if Overpower is not on an action bar it safely skips.
             */
            const bool overpowerProbeReady =
                lastOverpowerProbeTick_ == 0 ||
                tick >=
                    lastOverpowerProbeTick_ +
                    OverpowerProbeIntervalTicks;

            if (
                facingReady &&
                overpowerProbeReady &&
                healthPercent >
                    OverpowerMinimumTargetHealthPercent &&
                player.powerRaw >=
                    OverpowerMinimumRageRaw)
            {
                if (
                    ProbeOverpower(
                        player,
                        target,
                        tick
                    ))
                {
                    return;
                }
            }

            /*
             * Bloodrage uses the verified build-5875 spellbook cooldown
             * API instead of the absent IsUsableSpell global. It remains
             * conservative because the ability trades health for rage;
             * recovery remains authoritative at low HP.
             */
            const bool bloodrageProbeReady =
                lastBloodrageProbeTick_ == 0 ||
                tick >=
                    lastBloodrageProbeTick_ +
                    BloodrageProbeIntervalTicks;

            if (
                bloodrageProbeReady &&
                player.powerRaw <
                    BloodrageMaximumRageRaw &&
                playerHealthPercent >=
                    BloodrageMinimumHealthPercent)
            {
                if (
                    ProbeBloodrage(
                        player,
                        tick
                    ))
                {
                    return;
                }
            }

            /*
             * Rend is a once-per-target opener after we have reached melee
             * range and generated enough rage. CastSpellByName selects the
             * highest trained rank, so Rank 2 requires no hard-coded rank.
             */
            if (
                facingReady &&
                !rendAttemptedForTarget_ &&
                healthPercent >
                    RendMinimumTargetHealthPercent &&
                player.powerRaw >=
                    RendMinimumRageRaw)
            {
                if (
                    CastRend(
                        player,
                        target
                    ))
                {
                    return;
                }
            }

            if (
                !facingReady &&
                !rendAttemptedForTarget_ &&
                healthPercent >
                    RendMinimumTargetHealthPercent &&
                player.powerRaw >=
                    RendMinimumRageRaw)
            {
                Debug::Logger::Info(
                    "REND: waiting for facing alignment."
                );
            }

            if (
                !rendAttemptedForTarget_ &&
                healthPercent <=
                    RendMinimumTargetHealthPercent)
            {
                rendAttemptedForTarget_ =
                    true;

                Debug::Logger::Info(
                    "REND: skipped because target is already <=35% health."
                );
            }

            /*
             * UnitState has no verified fleeing bit in this snapshot. Use a
             * bounded low-health runner-prevention cast instead of inventing
             * motion/flee state. It is attempted at most once per target.
             */
            if (
                facingReady &&
                !hamstringAttemptedForTarget_ &&
                healthPercent <=
                    HamstringMaximumTargetHealthPercent &&
                healthPercent >
                    HamstringMinimumTargetHealthPercent &&
                player.powerRaw >=
                    HamstringMinimumRageRaw)
            {
                if (
                    CastHamstring(
                        player,
                        target
                    ))
                {
                    return;
                }
            }

            /*
             * Battle Shout runs after reactive/control actions and Rend,
             * but still before the Heroic Strike rage dump.
             *
             * We issue at most one Warrior ability command
             * during one Update() call.
             */
            const bool battleShoutDue =
                !battleShoutHasCommand_ ||
                tick >=
                    lastBattleShoutCommandTick_ +
                    BattleShoutRefreshTicks;

            if (
                battleShoutDue &&
                player.powerRaw >=
                    BattleShoutMinimumRageRaw)
            {
                if (
                    CastBattleShout(
                        player,
                        tick
                    ))
                {
                    return;
                }
            }

            /*
             * Heroic Strike modifies the next melee swing,
             * so it is meaningful only after autoattack has
             * successfully been enabled.
             */
            if (!autoAttackStarted)
            {
                return;
            }

            /*
             * Phase 14G.2.1:
             * Heroic Strike is an offensive melee action and must
             * obey the same verified-facing gate as Rend/Thunder Clap.
             * This closes the remaining path that could queue an attack
             * while the target had moved outside the safe facing cone.
             */
            if (!facingReady)
            {
                Debug::Logger::Info(
                    "HEROIC STRIKE: waiting for facing alignment."
                );

                return;
            }

            if (!targetTookDamage)
            {
                return;
            }

            if (
                player.powerRaw <
                    HeroicStrikeMinimumRageRaw)
            {
                Debug::Logger::Info(
                    "HEROIC STRIKE 14M.0: preserving rage reserve; "
                    "requires >=35 rage after damage event."
                );

                return;
            }

            /*
             * Do not dump rage into a target that is
             * already at <=20% health.
             */
            if (healthPercent <= 20)
            {
                Debug::Logger::Info(
                    "HEROIC STRIKE: target <=20%; "
                    "saving rage."
                );

                return;
            }

            const bool cooldownReady =
                !hasIssuedHeroicStrike_ ||
                tick >=
                    lastHeroicStrikeCommandTick_ +
                    HeroicStrikeMinimumCommandIntervalTicks;

            if (!cooldownReady)
            {
                return;
            }

            QueueHeroicStrike(
                player,
                target,
                tick
            );
        }

        std::uint64_t TargetGuid() const
        {
            return targetGuid_;
        }

        bool ChargeAttemptedForTarget() const
        {
            return chargeAttemptedForTarget_;
        }

        bool ChargeObservationPending() const
        {
            return chargeObservationPending_;
        }

        int ChargeCommands() const
        {
            return chargeCommands_;
        }

        int ChargeMovementObservations() const
        {
            return chargeMovementObservations_;
        }

        int ThunderClapCommands() const
        {
            return thunderClapCommands_;
        }

        int ThunderClapRageSpendObservations() const
        {
            return thunderClapRageSpendObservations_;
        }

        int BattleShoutCommands() const
        {
            return battleShoutCommands_;
        }

        int BattleShoutRageSpendObservations() const
        {
            return battleShoutRageSpendObservations_;
        }

        int RendCommands() const
        {
            return rendCommands_;
        }

        int OverpowerProbes() const
        {
            return overpowerProbes_;
        }

        int OverpowerRageSpendObservations() const
        {
            return overpowerRageSpendObservations_;
        }

        int BloodrageProbes() const
        {
            return bloodrageProbes_;
        }

        int HamstringCommands() const
        {
            return hamstringCommands_;
        }

        int HeroicStrikeCommands() const
        {
            return heroicStrikeCommands_;
        }

        int PostQueueDamageEvents() const
        {
            return postQueueDamageEvents_;
        }

        int RageSpendObservations() const
        {
            return heroicStrikeRageSpendObservations_;
        }
    };
}
