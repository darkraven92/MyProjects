#pragma once

#include "GameThreadDispatcher.h"
#include "FirstAidController.h"
#include "MovementController.h"
#include "PlayerPostureController.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"
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
    enum class RecoveryState
    {
        Idle,
        Recovering,
        Ready,
        Dead
    };

    /*
     * Phase 13C.0
     *
     * Out-of-combat recovery now treats health and mana as resources.
     * Food/drink are discovered from the live bags through the Vanilla UI
     * tooltip text instead of a hardcoded item-ID list. This deliberately
     * classifies only long-duration seated food/drink and therefore avoids
     * consuming healing/mana potions as ordinary recovery supplies.
     *
     * Mana is gated by UnitPowerType("player") == 0. Warriors/rogues and
     * other non-mana power types therefore never enter drink recovery.
     */
    class RecoveryController
    {
    private:
        static constexpr float EnterHealthPercent = 70.0f;
        static constexpr float ExitHealthPercent = 95.0f;
        static constexpr float EnterManaPercent = 60.0f;
        static constexpr float ExitManaPercent = 90.0f;

        static constexpr std::uint64_t ProgressLogTicks = 20;       // ~5 s
        static constexpr std::uint64_t ResourceProbeTicks = 4;      // ~1 s
        static constexpr std::uint64_t SecondConsumableDelayTicks = 2; // ~0.5 s
        static constexpr std::uint64_t ConsumableNoProgressTicks = 24; // ~6 s
        static constexpr std::uint64_t BagRescanTicks = 80;         // ~20 s
        static constexpr std::uint64_t BandageChannelTicks = 36;     // ~9 s
        static constexpr std::uint64_t BandageCooldownTicks = 240;   // ~60 s
        static constexpr std::uint64_t BandageRetryGraceTicks = 12;  // ~3 s after combat
        static constexpr std::uint64_t BandageRetryIntervalTicks = 2; // ~0.5 s
        static constexpr std::uint64_t RecoveryNoProgressWakeTicks = 48; // ~12 s
        static constexpr std::uint64_t RecoveryWakeCooldownTicks = 20;   // ~5 s
        static constexpr std::uint64_t RecoveryNoSupplyStallTicks = 120; // ~30 s of true HP no-progress
        static constexpr std::uint64_t RecoveryNoSupplyPulseTicks = 240; // ~60 s between safe liveness pulses
        static constexpr std::uint64_t RecoveryHardTimeoutTicks = 360;   // ~90 s

        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr const char* ResultVariable = "WOW_INTERNAL_RECOVERY_RESULT";

        enum class BandageAttemptResult
        {
            Used,
            RetrySoon,
            Unavailable
        };

        struct ResourceSnapshot
        {
            bool valid = false;
            int powerType = -1;
            std::uint32_t mana = 0;
            std::uint32_t maxMana = 0;

            int foodBag = -1;
            int foodSlot = -1;
            int drinkBag = -1;
            int drinkSlot = -1;

            bool HasMana() const
            {
                return powerType == 0 && maxMana > 0;
            }
        };

        RecoveryState state_ = RecoveryState::Idle;

        std::uint64_t startTick_ = 0;
        std::uint64_t lastProgressLogTick_ = 0;
        std::uint64_t lastResourceProbeTick_ = 0;
        std::uint64_t nextBagRescanTick_ = 0;
        std::uint64_t drinkEligibleTick_ = 0;

        std::uint64_t lastFoodUseTick_ = 0;
        std::uint64_t lastDrinkUseTick_ = 0;
        std::uint64_t lastHealthProgressTick_ = 0;
        std::uint64_t lastManaProgressTick_ = 0;
        std::uint64_t bandageChannelUntilTick_ = 0;
        std::uint64_t bandageNextAllowedTick_ = 0;
        std::uint64_t bandageGraceUntilTick_ = 0;
        std::uint64_t bandageRetryAtTick_ = 0;
        std::uint64_t lastLivenessWakeTick_ = 0;
        std::uint64_t noSupplyStallSinceTick_ = 0;
        std::uint64_t lastNoSupplyPulseTick_ = 0;

        float startHealthPercent_ = 0.0f;
        float startManaPercent_ = 100.0f;

        std::uint32_t lastObservedHealth_ = 0;
        std::uint32_t lastObservedMana_ = 0;

        ResourceSnapshot resources_{};

        bool manaUser_ = false;
        bool foodUseIssued_ = false;
        bool drinkUseIssued_ = false;
        bool foodUnavailableLogged_ = false;
        bool drinkUnavailableLogged_ = false;
        bool bandageUseIssued_ = false;
        bool bandageUnavailableLogged_ = false;
        bool bandageTransientLogged_ = false;
        bool recoveryHardTimeoutLatched_ = false;
        bool noSupplyHealthStalled_ = false;

        int entries_ = 0;
        int completions_ = 0;
        int foodUses_ = 0;
        int drinkUses_ = 0;
        int bandageUses_ = 0;
        int livenessWakeAttempts_ = 0;
        int livenessWakeEvents_ = 0;
        int noSupplyStallEvents_ = 0;
        int noSupplyPulseEvents_ = 0;

        static std::string Float(float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1) << value;
            return stream.str();
        }

        static const char* StateNameInternal(RecoveryState state)
        {
            switch (state)
            {
                case RecoveryState::Idle: return "Idle";
                case RecoveryState::Recovering: return "Recovering";
                case RecoveryState::Ready: return "Ready";
                case RecoveryState::Dead: return "Dead";
                default: return "Unknown";
            }
        }

        void SetState(RecoveryState newState)
        {
            if (state_ == newState)
                return;

            Debug::Logger::Info(
                std::string("RecoveryController state: ") +
                StateNameInternal(state_) + " -> " +
                StateNameInternal(newState));

            state_ = newState;
        }

        static bool IsExecutable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};

            if (VirtualQuery(
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

        static float ManaPercent(const ResourceSnapshot& snapshot)
        {
            if (!snapshot.HasMana())
                return 100.0f;

            return 100.0f *
                static_cast<float>(snapshot.mana) /
                static_cast<float>(snapshot.maxMana);
        }

        bool ExecuteLuaReadback(
            const std::string& script,
            const char* scriptName,
            std::string& result) const
        {
            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();

            if (!IsExecutable(doStringAddress) ||
                !IsExecutable(getTextAddress))
            {
                return false;
            }

            using DoStringFunction = bool (__fastcall*)(const char*, const char*);
            using GetTextFunction = const char* (__fastcall*)(char*, std::uint32_t, int);

            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);
            const auto getText =
                reinterpret_cast<GetTextFunction>(getTextAddress);

            char buffer[256]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    luaExecuted = doString(script.c_str(), scriptName);

                    if (!luaExecuted)
                        return;

                    const char* raw = getText(
                        const_cast<char*>(ResultVariable),
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

        bool ProbeResources(
            ResourceSnapshot& snapshot,
            bool scanBags) const
        {
            snapshot = ResourceSnapshot{};

            std::string script;

            if (!scanBags)
            {
                script =
                    "local p=UnitPowerType('player'); if p==nil then p=-1 end; "
                    "local m=UnitMana('player') or 0; local mm=UnitManaMax('player') or 0; "
                    "WOW_INTERNAL_RECOVERY_RESULT=p..'|'..m..'|'..mm..'|-1|-1|-1|-1';";
            }
            else
            {
                /*
                 * Vanilla enUS tooltip classifier.
                 *
                 * Food:  long-duration recovery text containing health + eating.
                 * Drink: long-duration recovery text containing mana + drinking.
                 * Potions/bandages do not contain the seated eating/drinking text
                 * and are intentionally excluded.
                 *
                 * Choose the highest usable required-level/item-level item so a
                 * character does not waste low-level food when better supplies
                 * are already present.
                 */
                script =
                    "local p=UnitPowerType('player'); if p==nil then p=-1 end; "
                    "local m=UnitMana('player') or 0; local mm=UnitManaMax('player') or 0; "
                    "local lvl=UnitLevel('player') or 1; "
                    "local fb=-1; local fs=-1; local fscore=-1; "
                    "local db=-1; local ds=-1; local dscore=-1; "
                    "if not WOW_INTERNAL_RECOVERY_TOOLTIP then "
                    "WOW_INTERNAL_RECOVERY_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_RECOVERY_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                    "WOW_INTERNAL_RECOVERY_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                    "local tt=WOW_INTERNAL_RECOVERY_TOOLTIP; "
                    "for b=0,4 do for s=1,GetContainerNumSlots(b) do "
                    "local link=GetContainerItemLink(b,s); if link then "
                    "local _,_,_,ilvl,req=GetItemInfo(link); ilvl=ilvl or 0; req=req or 0; "
                    "if req<=lvl then "
                    "tt:ClearLines(); tt:SetBagItem(b,s); "
                    "local isf=0; local isd=0; "
                    "for i=1,tt:NumLines() do "
                    "local l=getglobal('WOW_INTERNAL_RECOVERY_TOOLTIPTextLeft'..i); "
                    "if l and l:GetText() then local t=string.lower(l:GetText()); "
                    "if string.find(t,'health over') and string.find(t,'eating') then isf=1 end; "
                    "if string.find(t,'mana over') and string.find(t,'drinking') then isd=1 end; end; "
                    "local r=getglobal('WOW_INTERNAL_RECOVERY_TOOLTIPTextRight'..i); "
                    "if r and r:GetText() then local t=string.lower(r:GetText()); "
                    "if string.find(t,'health over') and string.find(t,'eating') then isf=1 end; "
                    "if string.find(t,'mana over') and string.find(t,'drinking') then isd=1 end; end; end; "
                    "local score=req*10000+ilvl; "
                    "if isf==1 and score>fscore then fscore=score; fb=b; fs=s end; "
                    "if isd==1 and score>dscore then dscore=score; db=b; ds=s end; "
                    "end; end; end; end; "
                    "WOW_INTERNAL_RECOVERY_RESULT=p..'|'..m..'|'..mm..'|'..fb..'|'..fs..'|'..db..'|'..ds;";
            }

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    scanBags
                        ? "wow-internal/RecoveryConsumableScan.lua"
                        : "wow-internal/RecoveryResourceProbe.lua",
                    result))
            {
                return false;
            }

            int powerType = -1;
            unsigned mana = 0;
            unsigned maxMana = 0;
            int foodBag = -1;
            int foodSlot = -1;
            int drinkBag = -1;
            int drinkSlot = -1;

            if (std::sscanf(
                    result.c_str(),
                    "%d|%u|%u|%d|%d|%d|%d",
                    &powerType,
                    &mana,
                    &maxMana,
                    &foodBag,
                    &foodSlot,
                    &drinkBag,
                    &drinkSlot) != 7)
            {
                return false;
            }

            snapshot.valid = true;
            snapshot.powerType = powerType;
            snapshot.mana = static_cast<std::uint32_t>(mana);
            snapshot.maxMana = static_cast<std::uint32_t>(maxMana);
            snapshot.foodBag = foodBag;
            snapshot.foodSlot = foodSlot;
            snapshot.drinkBag = drinkBag;
            snapshot.drinkSlot = drinkSlot;
            return true;
        }

        bool UseBagSlot(
            int bag,
            int slot,
            const char* kind) const
        {
            if (bag < 0 || slot <= 0)
                return false;

            const std::string script =
                "WOW_INTERNAL_RECOVERY_RESULT='missing'; "
                "local b=" + std::to_string(bag) + "; local s=" + std::to_string(slot) + "; "
                "if GetContainerItemLink(b,s) then UseContainerItem(b,s); "
                "WOW_INTERNAL_RECOVERY_RESULT='used'; end;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/RecoveryUseConsumable.lua",
                    result))
            {
                return false;
            }

            if (result != "used")
                return false;

            Debug::Logger::Info(
                std::string("RECOVERY 13C.0: ") + kind +
                " use issued bag=" + std::to_string(bag) +
                " slot=" + std::to_string(slot));

            return true;
        }

        bool RefreshResourceSnapshot(
            std::uint64_t tick,
            bool scanBags)
        {
            ResourceSnapshot next{};
            if (!ProbeResources(next, scanBags))
                return false;

            resources_ = next;
            lastResourceProbeTick_ = tick;
            manaUser_ = resources_.HasMana();
            return true;
        }

        void TryUseFood(std::uint64_t tick)
        {
            if (foodUseIssued_ || resources_.foodBag < 0 || resources_.foodSlot <= 0)
                return;

            if (!UseBagSlot(resources_.foodBag, resources_.foodSlot, "FOOD"))
                return;

            foodUseIssued_ = true;
            lastFoodUseTick_ = tick;
            lastHealthProgressTick_ = tick;
            ++foodUses_;

            if (
                resources_.foodBag == resources_.drinkBag &&
                resources_.foodSlot == resources_.drinkSlot)
            {
                drinkUseIssued_ = true;
                lastDrinkUseTick_ = tick;
                lastManaProgressTick_ = tick;
            }
        }

        void TryUseDrink(std::uint64_t tick)
        {
            if (!manaUser_ || drinkUseIssued_ ||
                resources_.drinkBag < 0 || resources_.drinkSlot <= 0)
            {
                return;
            }

            if (!UseBagSlot(resources_.drinkBag, resources_.drinkSlot, "DRINK"))
                return;

            drinkUseIssued_ = true;
            lastDrinkUseTick_ = tick;
            lastManaProgressTick_ = tick;
            ++drinkUses_;

            if (
                resources_.foodBag == resources_.drinkBag &&
                resources_.foodSlot == resources_.drinkSlot)
            {
                foodUseIssued_ = true;
                lastFoodUseTick_ = tick;
                lastHealthProgressTick_ = tick;
            }
        }

        BandageAttemptResult TryUseBandage(std::uint64_t tick)
        {
            if (bandageUseIssued_ || tick < bandageNextAllowedTick_)
                return BandageAttemptResult::Unavailable;

            if (tick < bandageRetryAtTick_)
                return BandageAttemptResult::RetrySoon;

            const auto result = FirstAidController::TryUseBestBandage();
            if (!result.valid)
            {
                bandageRetryAtTick_ = tick + BandageRetryIntervalTicks;
                if (!bandageTransientLogged_)
                {
                    bandageTransientLogged_ = true;
                    Debug::Logger::Info(
                        "FIRST AID 14J.2: bandage probe was temporarily unavailable; delaying food fallback and retrying.");
                }
                return BandageAttemptResult::RetrySoon;
            }

            if (result.inCombat)
            {
                bandageRetryAtTick_ = tick + BandageRetryIntervalTicks;
                if (!bandageTransientLogged_)
                {
                    bandageTransientLogged_ = true;
                    Debug::Logger::Info(
                        "FIRST AID 14J.2: player is still flagged in combat; holding food fallback briefly so bandage gets first priority after combat clears.");
                }
                return BandageAttemptResult::RetrySoon;
            }

            bandageTransientLogged_ = false;

            if (result.recentlyBandaged)
            {
                if (!bandageUnavailableLogged_)
                {
                    bandageUnavailableLogged_ = true;
                    Debug::Logger::Info(
                        "FIRST AID 14J.2: Recently Bandaged is active; using food/natural regeneration until bandages become available again.");
                }

                bandageNextAllowedTick_ = tick + 20;
                return BandageAttemptResult::Unavailable;
            }

            if (!result.issued)
            {
                if (!bandageUnavailableLogged_)
                {
                    bandageUnavailableLogged_ = true;
                    Debug::Logger::Info(
                        "FIRST AID 14J.2: no usable bandage found in bags; recovery fallback remains food/natural regeneration.");
                }
                return BandageAttemptResult::Unavailable;
            }

            bandageUseIssued_ = true;
            bandageUnavailableLogged_ = false;
            bandageChannelUntilTick_ = tick + BandageChannelTicks;
            bandageNextAllowedTick_ = tick + BandageCooldownTicks;
            lastHealthProgressTick_ = tick;
            ++bandageUses_;

            Debug::Logger::Info(
                "FIRST AID 14J.2: BANDAGE USE issued name='" +
                result.name + "' bag=" + std::to_string(result.bag) +
                " slot=" + std::to_string(result.slot) +
                " stack=" + std::to_string(result.stackCount) +
                " selfTarget=" + std::string(result.selfTargeted ? "yes" : "auto") +
                " channelWindowTicks=" + std::to_string(BandageChannelTicks));

            return BandageAttemptResult::Used;
        }

        void LogUnavailableSupplies(
            bool needsFood,
            bool needsDrink)
        {
            if (needsFood && resources_.foodBag < 0 && !foodUnavailableLogged_)
            {
                foodUnavailableLogged_ = true;
                Debug::Logger::Info(
                    "RECOVERY 13C.0: no usable food found; natural HP regeneration remains enabled.");
            }

            if (needsDrink && resources_.drinkBag < 0 && !drinkUnavailableLogged_)
            {
                drinkUnavailableLogged_ = true;
                Debug::Logger::Info(
                    "RECOVERY 13C.0: no usable drink found; natural mana regeneration remains enabled.");
            }
        }

        bool TryRecoveryLivenessWake(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            bool needsHealth,
            bool needsMana)
        {
            if (bandageUseIssued_ && tick < bandageChannelUntilTick_)
                return false;

            const bool healthStalled =
                needsHealth &&
                tick >= lastHealthProgressTick_ + RecoveryNoProgressWakeTicks;
            const bool manaStalled =
                needsMana &&
                tick >= lastManaProgressTick_ + RecoveryNoProgressWakeTicks;
            const bool hardTimeout =
                !recoveryHardTimeoutLatched_ &&
                startTick_ != 0 &&
                tick >= startTick_ + RecoveryHardTimeoutTicks;

            if (!healthStalled && !manaStalled && !hardTimeout)
                return false;

            const bool noHealthSupplyKnown =
                needsHealth &&
                resources_.valid &&
                resources_.foodBag < 0 &&
                bandageUnavailableLogged_;

            if (noHealthSupplyKnown &&
                tick >= lastHealthProgressTick_ + RecoveryNoSupplyStallTicks)
            {
                if (!noSupplyHealthStalled_)
                {
                    noSupplyHealthStalled_ = true;
                    noSupplyStallSinceTick_ = tick;
                    ++noSupplyStallEvents_;

                    Debug::Logger::Info(
                        "ROBUSTNESS 14K.1.5: RECOVERY STALLED reason=no-food-no-bandage-no-health-progress hp=" +
                        std::to_string(player.health) + "/" +
                        std::to_string(player.maxHealth) +
                        " noProgressTicks=" +
                        std::to_string(tick - lastHealthProgressTick_) +
                        "; recovery ownership is retained so combat cannot restart at unsafe health.");
                }

                const bool pulseDue =
                    lastNoSupplyPulseTick_ == 0 ||
                    tick < lastNoSupplyPulseTick_ ||
                    tick - lastNoSupplyPulseTick_ >= RecoveryNoSupplyPulseTicks;

                if (pulseDue)
                {
                    lastNoSupplyPulseTick_ = tick;
                    const bool standIssued =
                        PlayerPostureController::EnsureStanding(
                            player,
                            "14K.1.5 no-supply recovery stall");

                    /*
                     * The verified CTM hold path sends bounded client movement
                     * ownership without displacing a critically injured player.
                     * It is a liveness pulse only; it does not pretend HP progress
                     * occurred and it never releases recovery ownership.
                     */
                    const bool holdIssued =
                        MovementController::HoldPosition(player);

                    ++noSupplyPulseEvents_;

                    Debug::Logger::Info(
                        "ROBUSTNESS 14K.1.5: RECOVERY SAFE LIVENESS PULSE hp=" +
                        std::to_string(player.health) + "/" +
                        std::to_string(player.maxHealth) +
                        " standIssued=" +
                        (standIssued ? std::string("yes") : std::string("no")) +
                        " holdIssued=" +
                        (holdIssued ? std::string("yes") : std::string("no")) +
                        " pulse=" + std::to_string(noSupplyPulseEvents_) +
                        "; no unsafe target acquisition was permitted.");
                }

                /*
                 * Keep probing inventory/bandages while stalled, but do not mint
                 * false HP progress and do not spam the ordinary 5 s wake path.
                 */
                if (pulseDue)
                {
                    foodUseIssued_ = false;
                    bandageUseIssued_ = false;
                    bandageChannelUntilTick_ = 0;
                    bandageRetryAtTick_ = tick;
                    bandageGraceUntilTick_ = tick + BandageRetryGraceTicks;
                    nextBagRescanTick_ = tick;
                    return true;
                }

                // Let the normal bag-rescan/bandage/food path below continue
                // between pulses so newly acquired supplies can break the stall.
                return false;
            }

            if (lastLivenessWakeTick_ != 0 &&
                tick >= lastLivenessWakeTick_ &&
                tick - lastLivenessWakeTick_ < RecoveryWakeCooldownTicks)
            {
                return false;
            }

            lastLivenessWakeTick_ = tick;
            ++livenessWakeAttempts_;
            ++livenessWakeEvents_;

            if (hardTimeout)
                recoveryHardTimeoutLatched_ = true;

            const bool standIssued =
                PlayerPostureController::EnsureStanding(
                    player,
                    hardTimeout
                        ? "recovery hard timeout"
                        : "recovery made no resource progress");

            /*
             * Drop stale local consumable latches and retry from a fresh bag
             * snapshot. Crucially, a wake is not resource progress: the true
             * health/mana progress timestamps are left untouched.
             */
            foodUseIssued_ = false;
            drinkUseIssued_ = false;
            bandageUseIssued_ = false;
            bandageChannelUntilTick_ = 0;
            bandageRetryAtTick_ = tick;
            bandageGraceUntilTick_ = tick + BandageRetryGraceTicks;
            nextBagRescanTick_ = tick;
            foodUnavailableLogged_ = false;
            drinkUnavailableLogged_ = false;
            bandageUnavailableLogged_ = false;
            bandageTransientLogged_ = false;

            Debug::Logger::Info(
                "ROBUSTNESS 14K.1: RECOVERY LIVENESS WAKE attempt=" +
                std::to_string(livenessWakeAttempts_) +
                " hp=" + std::to_string(player.health) + "/" +
                std::to_string(player.maxHealth) +
                " healthStalled=" + (healthStalled ? std::string("yes") : std::string("no")) +
                " manaStalled=" + (manaStalled ? std::string("yes") : std::string("no")) +
                " hardTimeout=" + (hardTimeout ? std::string("yes") : std::string("no")) +
                " standIssued=" + (standIssued ? std::string("yes") : std::string("no")) +
                "; recovery remains owner and will retry supplies without resetting true progress age.");

            return true;
        }

    public:
        static float HealthPercent(const Objects::PlayerState& player)
        {
            if (!player.valid || player.maxHealth == 0)
                return 0.0f;

            return 100.0f *
                static_cast<float>(player.health) /
                static_cast<float>(player.maxHealth);
        }

        bool ShouldStart(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!player.valid || player.maxHealth == 0 || player.health == 0)
                return false;

            const bool lowHealth =
                HealthPercent(player) < EnterHealthPercent;

            if (
                !resources_.valid ||
                tick >= lastResourceProbeTick_ + ResourceProbeTicks)
            {
                RefreshResourceSnapshot(tick, false);
            }

            const bool lowMana =
                resources_.valid &&
                resources_.HasMana() &&
                ManaPercent(resources_) < EnterManaPercent;

            return lowHealth || lowMana;
        }

        bool Start(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (state_ != RecoveryState::Idle && state_ != RecoveryState::Ready)
                return false;

            if (!ShouldStart(player, tick))
                return false;

            startTick_ = tick;
            lastProgressLogTick_ = tick;
            nextBagRescanTick_ = tick + BagRescanTicks;
            drinkEligibleTick_ = tick + SecondConsumableDelayTicks;

            startHealthPercent_ = HealthPercent(player);
            lastObservedHealth_ = player.health;
            lastHealthProgressTick_ = tick;

            /* Refresh mana + food/drink candidates in one game-thread pass. */
            if (!RefreshResourceSnapshot(tick, true))
            {
                Debug::Logger::Info(
                    "RECOVERY 13C.0: resource/bag probe unavailable; falling back to HP-only natural regeneration for this snapshot.");
            }

            manaUser_ = resources_.valid && resources_.HasMana();
            startManaPercent_ = manaUser_ ? ManaPercent(resources_) : 100.0f;
            lastObservedMana_ = resources_.mana;
            lastManaProgressTick_ = tick;

            foodUseIssued_ = false;
            drinkUseIssued_ = false;
            bandageUseIssued_ = false;
            bandageChannelUntilTick_ = 0;
            bandageGraceUntilTick_ = tick + BandageRetryGraceTicks;
            bandageRetryAtTick_ = tick;
            lastLivenessWakeTick_ = 0;
            noSupplyStallSinceTick_ = 0;
            lastNoSupplyPulseTick_ = 0;
            livenessWakeAttempts_ = 0;
            recoveryHardTimeoutLatched_ = false;
            noSupplyHealthStalled_ = false;
            foodUnavailableLogged_ = false;
            drinkUnavailableLogged_ = false;
            bandageUnavailableLogged_ = false;
            bandageTransientLogged_ = false;

            ++entries_;

            const bool needsFood = startHealthPercent_ < ExitHealthPercent;
            const bool needsDrink = manaUser_ && startManaPercent_ < ExitManaPercent;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("RECOVERY 13C.0: START");
            Debug::Logger::Info(
                "Player health: " + std::to_string(player.health) + "/" +
                std::to_string(player.maxHealth) + " (" +
                Float(startHealthPercent_) + "%)");

            if (manaUser_)
            {
                Debug::Logger::Info(
                    "Player mana: " + std::to_string(resources_.mana) + "/" +
                    std::to_string(resources_.maxMana) + " (" +
                    Float(startManaPercent_) + "%)");
            }
            else
            {
                Debug::Logger::Info(
                    "Mana gate: disabled for current non-mana power type.");
            }

            Debug::Logger::Info(
                "Acquire gate: HP <70% or mana-user MP <60%.");
            Debug::Logger::Info(
                "Resume target acquisition: HP >=95% and, for mana users, MP >=90%.");
            Debug::Logger::Info(
                "Recovery method: highest usable bandage first when available and off cooldown; food/drink and natural regeneration remain fallback.");
            Debug::Logger::Info("================================");

            SetState(RecoveryState::Recovering);

            LogUnavailableSupplies(needsFood, needsDrink);

            if (needsFood)
            {
                const BandageAttemptResult bandageAttempt = TryUseBandage(tick);
                if (bandageAttempt == BandageAttemptResult::Unavailable)
                {
                    TryUseFood(tick);
                }
                else if (bandageAttempt == BandageAttemptResult::RetrySoon)
                {
                    Debug::Logger::Info(
                        "FIRST AID 14J.2: recovery is reserving the first few post-combat seconds for a bandage before allowing seated food fallback.");
                }
            }
            else if (needsDrink)
            {
                TryUseDrink(tick);
            }

            return true;
        }

        void Update(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (state_ != RecoveryState::Recovering)
                return;

            if (!player.valid || player.maxHealth == 0)
                return;

            if (player.health == 0)
            {
                Debug::Logger::Info("================================");
                Debug::Logger::Info("RECOVERY: PLAYER DEAD");
                Debug::Logger::Info("================================");
                SetState(RecoveryState::Dead);
                return;
            }

            if (player.health > lastObservedHealth_)
            {
                lastHealthProgressTick_ = tick;
                noSupplyHealthStalled_ = false;
                noSupplyStallSinceTick_ = 0;
                lastNoSupplyPulseTick_ = 0;
            }
            lastObservedHealth_ = player.health;

            if (
                !resources_.valid ||
                tick >= lastResourceProbeTick_ + ResourceProbeTicks)
            {
                RefreshResourceSnapshot(tick, false);
            }

            if (resources_.valid && resources_.HasMana())
            {
                manaUser_ = true;
                if (resources_.mana > lastObservedMana_)
                    lastManaProgressTick_ = tick;
                lastObservedMana_ = resources_.mana;
            }

            const float healthPercent = HealthPercent(player);
            const float manaPercent = manaUser_ ? ManaPercent(resources_) : 100.0f;

            const bool healthReady = healthPercent >= ExitHealthPercent;
            const bool manaReady = !manaUser_ || manaPercent >= ExitManaPercent;

            if (healthReady && manaReady)
            {
                ++completions_;

                Debug::Logger::Info("================================");
                Debug::Logger::Info("RECOVERY 13C.0: PASS");
                Debug::Logger::Info(
                    "Start HP: " + Float(startHealthPercent_) + "%");
                Debug::Logger::Info(
                    "Final HP: " + Float(healthPercent) + "%");

                if (manaUser_)
                {
                    Debug::Logger::Info(
                        "Start MP: " + Float(startManaPercent_) + "%");
                    Debug::Logger::Info(
                        "Final MP: " + Float(manaPercent) + "%");
                }

                Debug::Logger::Info(
                    "Bandage uses: " + std::to_string(bandageUses_) +
                    " Food uses: " + std::to_string(foodUses_) +
                    " Drink uses: " + std::to_string(drinkUses_));
                Debug::Logger::Info(
                    "Recovery ticks: " + std::to_string(tick - startTick_));
                Debug::Logger::Info("================================");

                PlayerPostureController::EnsureStanding(
                    player,
                    "recovery complete");
                noSupplyHealthStalled_ = false;
                noSupplyStallSinceTick_ = 0;
                lastNoSupplyPulseTick_ = 0;
                SetState(RecoveryState::Ready);
                return;
            }

            const bool needsFood = !healthReady;
            const bool needsDrink = manaUser_ && !manaReady;

            // A bandage is an 8-second channel in Vanilla. Do not issue food,
            // drink or another recovery action while the channel window is
            // expected to be active; movement/combat preemption outside this
            // controller can still interrupt it naturally.
            if (bandageUseIssued_ && tick < bandageChannelUntilTick_)
            {
                if (tick >= lastProgressLogTick_ + ProgressLogTicks)
                {
                    lastProgressLogTick_ = tick;
                    Debug::Logger::Info(
                        "FIRST AID 14J.2: bandage channel active hp=" +
                        std::to_string(player.health) + "/" +
                        std::to_string(player.maxHealth) + " (" +
                        Float(healthPercent) + "%)");
                }
                return;
            }

            if (bandageUseIssued_ && tick >= bandageChannelUntilTick_)
            {
                bandageUseIssued_ = false;
                Debug::Logger::Info(
                    "FIRST AID 14J.2: bandage channel window complete; evaluating remaining recovery need.");
            }

            if (TryRecoveryLivenessWake(
                    player,
                    tick,
                    needsFood,
                    needsDrink))
            {
                return;
            }

            /*
             * If an eating/drinking effect was interrupted or a low-rank item
             * finished before the resource target was reached, allow another
             * consumable only after a bounded no-progress window. This avoids
             * repeatedly consuming items while the active food/drink tick is
             * still restoring resources.
             */
            if (
                needsFood &&
                foodUseIssued_ &&
                tick >= lastHealthProgressTick_ + ConsumableNoProgressTicks)
            {
                foodUseIssued_ = false;
            }

            if (
                needsDrink &&
                drinkUseIssued_ &&
                tick >= lastManaProgressTick_ + ConsumableNoProgressTicks)
            {
                drinkUseIssued_ = false;
            }

            const bool needsRescan =
                tick >= nextBagRescanTick_ &&
                ((needsFood && !foodUseIssued_) ||
                 (needsDrink && !drinkUseIssued_));

            if (needsRescan)
            {
                if (RefreshResourceSnapshot(tick, true))
                {
                    nextBagRescanTick_ = tick + BagRescanTicks;
                    foodUnavailableLogged_ = false;
                    drinkUnavailableLogged_ = false;
                }
                else
                {
                    nextBagRescanTick_ = tick + BagRescanTicks;
                }
            }

            LogUnavailableSupplies(needsFood, needsDrink);

            if (needsFood && !foodUseIssued_ && !bandageUseIssued_)
            {
                if (tick < bandageRetryAtTick_)
                {
                    if (tick < bandageGraceUntilTick_)
                        return;
                }
                else
                {
                    const BandageAttemptResult bandageAttempt = TryUseBandage(tick);
                    if (bandageAttempt == BandageAttemptResult::Used)
                        return;

                    if (
                        bandageAttempt == BandageAttemptResult::RetrySoon &&
                        tick < bandageGraceUntilTick_)
                    {
                        return;
                    }

                    if (
                        bandageAttempt == BandageAttemptResult::RetrySoon &&
                        tick >= bandageGraceUntilTick_)
                    {
                        Debug::Logger::Info(
                            "FIRST AID 14J.2: bounded bandage retry window expired; allowing food fallback so recovery cannot stall indefinitely.");
                    }
                }

                TryUseFood(tick);
            }

            if (
                needsDrink &&
                !drinkUseIssued_ &&
                tick >= drinkEligibleTick_)
            {
                TryUseDrink(tick);
            }

            if (tick >= lastProgressLogTick_ + ProgressLogTicks)
            {
                lastProgressLogTick_ = tick;

                std::string message =
                    "RECOVERY 13C.0: waiting hp=" +
                    std::to_string(player.health) + "/" +
                    std::to_string(player.maxHealth) + " (" +
                    Float(healthPercent) + "%)";

                if (manaUser_)
                {
                    message +=
                        " mana=" + std::to_string(resources_.mana) + "/" +
                        std::to_string(resources_.maxMana) + " (" +
                        Float(manaPercent) + "%)";
                }

                Debug::Logger::Info(message);
            }
        }

        void Reset()
        {
            state_ = RecoveryState::Idle;
            startTick_ = 0;
            lastProgressLogTick_ = 0;
            nextBagRescanTick_ = 0;
            drinkEligibleTick_ = 0;
            lastFoodUseTick_ = 0;
            lastDrinkUseTick_ = 0;
            lastHealthProgressTick_ = 0;
            lastManaProgressTick_ = 0;
            startHealthPercent_ = 0.0f;
            startManaPercent_ = 100.0f;
            lastObservedHealth_ = 0;
            lastObservedMana_ = 0;
            foodUseIssued_ = false;
            drinkUseIssued_ = false;
            bandageUseIssued_ = false;
            bandageChannelUntilTick_ = 0;
            bandageGraceUntilTick_ = 0;
            bandageRetryAtTick_ = 0;
            lastLivenessWakeTick_ = 0;
            noSupplyStallSinceTick_ = 0;
            lastNoSupplyPulseTick_ = 0;
            livenessWakeAttempts_ = 0;
            recoveryHardTimeoutLatched_ = false;
            noSupplyHealthStalled_ = false;
            foodUnavailableLogged_ = false;
            drinkUnavailableLogged_ = false;
            bandageUnavailableLogged_ = false;
            bandageTransientLogged_ = false;
            /* Keep the most recent power snapshot as a cheap next-pull probe. */
        }

        RecoveryState State() const { return state_; }
        const char* StateName() const { return StateNameInternal(state_); }
        bool IsActive() const { return state_ == RecoveryState::Recovering; }
        bool IsReady() const { return state_ == RecoveryState::Ready; }
        bool IsDead() const { return state_ == RecoveryState::Dead; }
        int Entries() const { return entries_; }
        int Completions() const { return completions_; }
        int FoodUses() const { return foodUses_; }
        int DrinkUses() const { return drinkUses_; }
        int BandageUses() const { return bandageUses_; }
        int LivenessWakeEvents() const { return livenessWakeEvents_; }
        bool IsNoSupplyHealthStalled() const { return noSupplyHealthStalled_; }
        int NoSupplyStallEvents() const { return noSupplyStallEvents_; }
        int NoSupplyPulseEvents() const { return noSupplyPulseEvents_; }

        static float EnterThresholdPercent() { return EnterHealthPercent; }
        static float ExitThresholdPercent() { return ExitHealthPercent; }
        static float EnterManaThresholdPercent() { return EnterManaPercent; }
        static float ExitManaThresholdPercent() { return ExitManaPercent; }
    };
}
