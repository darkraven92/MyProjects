#pragma once

#include "GameThreadDispatcher.h"
#include "MovementController.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace Bot
{
    /*
     * Phase 14J.2.1
     *
     * Shared First Aid support.
     *
     * Crafting policy:
     *   - open First Aid through the live Vanilla spell book
     *   - never craft a recipe whose TradeSkill difficulty is "trivial"
     *     (gray)
     *   - among craftable non-gray Bandage recipes, prefer the highest recipe
     *     currently shown by the First Aid trade-skill list
     *   - craft one bandage at a time until a small reserve is available
     *   - never consume all cloth in a single unattended burst
     *
     * Recovery policy:
     *   - bandages are considered only out of combat
     *   - the highest usable bag bandage is selected dynamically from item
     *     data; there is no hardcoded bandage-rank/item-id table
     *   - Recently Bandaged is checked through a tooltip probe when possible
     *     and RecoveryController also maintains a conservative 60 second
     *     local cooldown after a use command
     *
     * This controller intentionally uses live TradeSkill difficulty instead
     * of hardcoded First Aid skill thresholds. That is what guarantees the
     * bot does not keep producing gray bandages as First Aid skill increases.
     */
    class FirstAidController
    {
    public:
        enum class CraftState
        {
            Idle,
            Crafting
        };

        struct BandageUseResult
        {
            bool valid = false;
            bool issued = false;
            bool inCombat = false;
            bool recentlyBandaged = false;
            bool selfTargeted = false;
            int bag = -1;
            int slot = -1;
            int stackCount = 0;
            std::string name{};
        };

    private:
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr const char* ResultVariable = "WOW_INTERNAL_FIRST_AID_RESULT";

        static constexpr int TargetBandageStock = 8;
        static constexpr int MaximumCraftBatch = 4;
        static constexpr std::uint64_t ProbeIntervalTicks = 80; // ~20 s
        static constexpr std::uint64_t RetryTicks = 8;         // ~2 s
        static constexpr std::uint64_t CraftWindowTicksPerItem = 16; // ~4 s each

        CraftState state_ = CraftState::Idle;
        std::uint64_t nextProbeTick_ = 0;
        std::uint64_t craftCompleteTick_ = 0;
        bool unavailableLogged_ = false;
        bool allRecipesGrayLogged_ = false;
        int craftsIssued_ = 0;
        int activeCraftCount_ = 0;
        std::string activeRecipe_{};

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

        static bool ExecuteLuaReadback(
            const std::string& script,
            const char* scriptName,
            std::string& result)
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

            char buffer[512]{};
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

        static void CloseTradeSkillWindow()
        {
            const std::string script =
                "if CloseTradeSkill then CloseTradeSkill(); end; "
                "WOW_INTERNAL_FIRST_AID_RESULT='closed';";

            std::string ignored;
            ExecuteLuaReadback(
                script,
                "wow-internal/FirstAidClose.lua",
                ignored);
        }

        static bool HasDirectAggressor(const Objects::WorldState& world)
        {
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.health == 0)
                    continue;

                if (unit.targetGuid == world.activePlayerGuid)
                    return true;
            }

            return false;
        }

        bool TryCraftOne(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const std::string script =
                "WOW_INTERNAL_FIRST_AID_RESULT='probe-failed'; "
                "local known=0; local book=BOOKTYPE_SPELL or 'spell'; "
                "local tabs=GetNumSpellTabs and GetNumSpellTabs() or 0; "
                "for t=1,tabs do local _,_,off,num=GetSpellTabInfo(t); off=off or 0; num=num or 0; "
                "for j=1,num do local n=GetSpellName(off+j,book); if n=='First Aid' then known=1; break; end; end; "
                "if known==1 then break; end; end; "
                "if known==0 then WOW_INTERNAL_FIRST_AID_RESULT='unknown'; return; end; "
                "CastSpellByName('First Aid'); "
                "local line=GetTradeSkillLine and GetTradeSkillLine() or ''; "
                "if line~='First Aid' then WOW_INTERNAL_FIRST_AID_RESULT='opening'; return; end; "
                "local stock=0; "
                "for b=0,4 do for s=1,GetContainerNumSlots(b) do local link=GetContainerItemLink(b,s); "
                "if link then local name=GetItemInfo(link); if name and string.find(string.lower(name),'bandage') then "
                "local _,count=GetContainerItemInfo(b,s); stock=stock+(count or 1); end; end; end; end; "
                "if stock>=" + std::to_string(TargetBandageStock) + " then "
                "WOW_INTERNAL_FIRST_AID_RESULT='stock|'..stock; return; end; "
                "local best=0; local bestName=''; local bestType=''; local bestAvail=0; local sawBandage=0; local sawNonGray=0; "
                "local nskills=GetNumTradeSkills and GetNumTradeSkills() or 0; "
                "for i=1,nskills do local n,typ,avail=GetTradeSkillInfo(i); "
                "if n and string.find(string.lower(n),'bandage') then sawBandage=1; "
                "if typ and typ~='header' and typ~='trivial' then sawNonGray=1; "
                "if (avail or 0)>0 then best=i; bestName=n; bestType=typ; bestAvail=avail or 0; end; end; end; end; "
                "if best==0 then if sawBandage==1 and sawNonGray==0 then WOW_INTERNAL_FIRST_AID_RESULT='all-trivial|'..stock; "
                "else WOW_INTERNAL_FIRST_AID_RESULT='no-materials|'..stock; end; return; end; "
                "local need=" + std::to_string(TargetBandageStock) + "-stock; "
                "local count=math.min(need,bestAvail," + std::to_string(MaximumCraftBatch) + "); "
                "if count<1 then WOW_INTERNAL_FIRST_AID_RESULT='stock|'..stock; return; end; "
                "DoTradeSkill(best,count); "
                "WOW_INTERNAL_FIRST_AID_RESULT='craft|'..stock..'|'..bestAvail..'|'..bestType..'|'..count..'|'..bestName;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/FirstAidCraft.lua",
                    result))
            {
                nextProbeTick_ = tick + RetryTicks;
                return false;
            }

            if (result == "unknown")
            {
                if (!unavailableLogged_)
                {
                    unavailableLogged_ = true;
                    Debug::Logger::Info(
                        "FIRST AID 14J.0: profession not available; automatic bandage crafting is idle.");
                }
                nextProbeTick_ = tick + ProbeIntervalTicks;
                return false;
            }

            if (result == "opening")
            {
                nextProbeTick_ = tick + RetryTicks;
                return false;
            }

            if (result.rfind("stock|", 0) == 0)
            {
                unavailableLogged_ = false;
                allRecipesGrayLogged_ = false;
                CloseTradeSkillWindow();
                nextProbeTick_ = tick + ProbeIntervalTicks;
                return false;
            }

            if (result.rfind("all-trivial|", 0) == 0)
            {
                if (!allRecipesGrayLogged_)
                {
                    allRecipesGrayLogged_ = true;
                    Debug::Logger::Info(
                        "FIRST AID 14J.0: every currently craftable Bandage recipe is gray/trivial; crafting skipped until a non-gray recipe is available.");
                }
                CloseTradeSkillWindow();
                nextProbeTick_ = tick + ProbeIntervalTicks;
                return false;
            }

            if (result.rfind("no-materials|", 0) == 0)
            {
                allRecipesGrayLogged_ = false;
                CloseTradeSkillWindow();
                nextProbeTick_ = tick + ProbeIntervalTicks;
                return false;
            }

            if (result.rfind("craft|", 0) != 0)
            {
                CloseTradeSkillWindow();
                nextProbeTick_ = tick + RetryTicks;
                return false;
            }

            int stock = 0;
            int available = 0;
            int craftCount = 0;
            char difficulty[64]{};
            char recipe[256]{};

            if (std::sscanf(
                    result.c_str(),
                    "craft|%d|%d|%63[^|]|%d|%255[^\n]",
                    &stock,
                    &available,
                    difficulty,
                    &craftCount,
                    recipe) != 5 || craftCount < 1)
            {
                CloseTradeSkillWindow();
                nextProbeTick_ = tick + RetryTicks;
                return false;
            }

            MovementController::HoldPosition(world.player);
            state_ = CraftState::Crafting;
            activeCraftCount_ = craftCount;
            craftCompleteTick_ = tick +
                (CraftWindowTicksPerItem * static_cast<std::uint64_t>(craftCount));
            activeRecipe_ = recipe;
            craftsIssued_ += craftCount;
            unavailableLogged_ = false;
            allRecipesGrayLogged_ = false;

            Debug::Logger::Info(
                "FIRST AID 14J.1: crafting stationary batch recipe='" +
                activeRecipe_ + "' difficulty=" + difficulty +
                " count=" + std::to_string(activeCraftCount_) +
                " stockBefore=" + std::to_string(stock) +
                " craftable=" + std::to_string(available) +
                " targetStock=" + std::to_string(TargetBandageStock));

            return true;
        }

    public:
        bool Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (state_ == CraftState::Crafting)
            {
                if (HasDirectAggressor(world) || world.player.health == 0)
                {
                    Abort("combat/death preempted First Aid crafting");
                    return false;
                }

                if (tick < craftCompleteTick_)
                    return true;

                CloseTradeSkillWindow();
                Debug::Logger::Info(
                    "FIRST AID 14J.1: stationary craft batch complete recipe='" +
                    activeRecipe_ + "' count=" + std::to_string(activeCraftCount_) +
                    "; next craft requires a new idle opportunity.");
                state_ = CraftState::Idle;
                activeCraftCount_ = 0;
                activeRecipe_.clear();
                nextProbeTick_ = tick + ProbeIntervalTicks;
                return false;
            }

            if (tick < nextProbeTick_)
                return false;

            nextProbeTick_ = tick + ProbeIntervalTicks;

            if (!world.player.valid || world.player.health == 0)
                return false;

            if (HasDirectAggressor(world))
                return false;

            return TryCraftOne(world, tick);
        }

        void Abort(const char* reason = "external preemption")
        {
            if (state_ == CraftState::Crafting)
            {
                CloseTradeSkillWindow();
                Debug::Logger::Info(
                    std::string("FIRST AID 14J.1: crafting aborted; reason=") + reason);
            }

            state_ = CraftState::Idle;
            craftCompleteTick_ = 0;
            activeCraftCount_ = 0;
            activeRecipe_.clear();
            nextProbeTick_ = 0;
        }

        void Reset()
        {
            Abort("reset");
            unavailableLogged_ = false;
            allRecipesGrayLogged_ = false;
            craftsIssued_ = 0;
        }

        bool IsActive() const
        {
            return state_ == CraftState::Crafting;
        }

        int CraftsIssued() const
        {
            return craftsIssued_;
        }

        static int DesiredStock()
        {
            return TargetBandageStock;
        }

        static BandageUseResult TryUseBestBandage()
        {
            BandageUseResult output{};

            const std::string script =
                "WOW_INTERNAL_FIRST_AID_RESULT='probe-failed'; "
                "if UnitAffectingCombat and UnitAffectingCombat('player') then WOW_INTERNAL_FIRST_AID_RESULT='combat'; return; end; "
                "if not WOW_INTERNAL_FIRST_AID_DEBUFF_TT then "
                "WOW_INTERNAL_FIRST_AID_DEBUFF_TT=CreateFrame('GameTooltip','WOW_INTERNAL_FIRST_AID_DEBUFF_TT',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_FIRST_AID_DEBUFF_TT:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local dtt=WOW_INTERNAL_FIRST_AID_DEBUFF_TT; local recent=0; "
                "if dtt.SetUnitDebuff then for i=1,16 do dtt:ClearLines(); dtt:SetUnitDebuff('player',i); "
                "local l=getglobal('WOW_INTERNAL_FIRST_AID_DEBUFF_TTTextLeft1'); local n=l and l:GetText() or nil; "
                "if n and string.find(string.lower(n),'recently bandaged') then recent=1; break; end; end; end; "
                "if recent==1 then WOW_INTERNAL_FIRST_AID_RESULT='recent'; return; end; "
                "local lvl=UnitLevel('player') or 1; local bestBag=-1; local bestSlot=-1; local bestScore=-1; local bestCount=0; local bestName=''; "
                "for b=0,4 do for s=1,GetContainerNumSlots(b) do local link=GetContainerItemLink(b,s); if link then "
                "local name,_,_,ilvl,req=GetItemInfo(link); ilvl=ilvl or 0; req=req or 0; "
                "if not name then local _,_,captured=string.find(link,'%[(.-)%]'); name=captured; end; "
                "if name and req<=lvl and string.find(string.lower(name),'bandage') then "
                "local start,dur=GetContainerItemCooldown(b,s); start=start or 0; dur=dur or 0; "
                "if dur==0 then local _,count=GetContainerItemInfo(b,s); local score=req*10000+ilvl; "
                "if score>bestScore then bestScore=score; bestBag=b; bestSlot=s; bestCount=count or 1; bestName=name; end; end; end; end; end; end; "
                "if bestBag<0 then WOW_INTERNAL_FIRST_AID_RESULT='none'; return; end; "
                "UseContainerItem(bestBag,bestSlot); "
                "local selfTargeted=0; "
                "if SpellIsTargeting and SpellIsTargeting() and SpellTargetUnit then SpellTargetUnit('player'); selfTargeted=1; end; "
                "WOW_INTERNAL_FIRST_AID_RESULT='used|'..bestBag..'|'..bestSlot..'|'..bestCount..'|'..selfTargeted..'|'..bestName;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/FirstAidUse.lua",
                    result))
            {
                return output;
            }

            output.valid = true;

            if (result == "combat")
            {
                output.inCombat = true;
                return output;
            }

            if (result == "recent")
            {
                output.recentlyBandaged = true;
                return output;
            }

            if (result == "none")
                return output;

            int bag = -1;
            int slot = -1;
            int count = 0;
            int selfTargeted = 0;
            char name[256]{};

            if (std::sscanf(
                    result.c_str(),
                    "used|%d|%d|%d|%d|%255[^\n]",
                    &bag,
                    &slot,
                    &count,
                    &selfTargeted,
                    name) != 5)
            {
                output.valid = false;
                return output;
            }

            output.issued = true;
            output.bag = bag;
            output.slot = slot;
            output.stackCount = count;
            output.selfTargeted = selfTargeted != 0;
            output.name = name;
            return output;
        }
    };
}
