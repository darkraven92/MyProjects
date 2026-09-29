#pragma once

#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Bot
{
    enum class ClassAwareRewardState
    {
        Idle,
        Evaluating,
        WaitingForDelivery,
        EquipIssued,
        Verified,
        UnsupportedClass,
        Failed
    };

    class ClassAwareRewardController
    {
    private:
        static constexpr std::uintptr_t LuaDoStringRva =
            0x00304CD0;

        static constexpr std::uintptr_t GetTextRva =
            0x00303BF0;

        static constexpr std::uint64_t PollIntervalTicks =
            2;

        static constexpr std::uint64_t RewardTimeoutTicks =
            80;

        // Vanilla 1.12 may expose quest reward metadata before the full
        // item link/GetItemInfo cache is populated. Retry the normal
        // class-aware path first, then use a bounded metadata-only claim
        // so questing cannot deadlock on an item-cache miss.
        static constexpr std::uint32_t CacheFallbackAttemptLimit =
            12;

        static constexpr const char* ResultVariable =
            "WOW_INTERNAL_GEAR_RESULT";

        static constexpr const char* ChoiceVariable =
            "WOW_INTERNAL_GEAR_CHOICES";

        ClassAwareRewardState state_ =
            ClassAwareRewardState::Idle;

        std::uint64_t startTick_ =
            0;

        std::uint64_t lastPollTick_ =
            0;

        int choiceCount_ =
            0;

        int selectedChoice_ =
            0;

        int selectedItemId_ =
            0;

        int targetSlot_ =
            0;

        bool shouldEquip_ =
            false;

        float candidateScore_ =
            0.0f;

        float baselineScore_ =
            0.0f;

        float upgradeDelta_ =
            0.0f;

        std::string classToken_{};
        std::string selectedName_{};
        std::string lastResult_{};

        std::uint32_t cachePendingAttempts_ =
            0;

        bool metadataFallbackClaimed_ =
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

        static const char* StateNameInternal(
            ClassAwareRewardState state)
        {
            switch (state)
            {
                case ClassAwareRewardState::Idle:
                    return "Idle";

                case ClassAwareRewardState::Evaluating:
                    return "Evaluating";

                case ClassAwareRewardState::WaitingForDelivery:
                    return "WaitingForDelivery";

                case ClassAwareRewardState::EquipIssued:
                    return "EquipIssued";

                case ClassAwareRewardState::Verified:
                    return "Verified";

                case ClassAwareRewardState::UnsupportedClass:
                    return "UnsupportedClass";

                case ClassAwareRewardState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            ClassAwareRewardState next)
        {
            if (state_ == next)
            {
                return;
            }

            Debug::Logger::Info(
                std::string("ClassAwareReward state: ") +
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
                "CLASS-AWARE REWARD: FAILED"
            );

            Debug::Logger::Info(
                "Reason: " + reason
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                ClassAwareRewardState::Failed
            );
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

        static std::vector<std::string> Split(
            const std::string& value,
            char delimiter)
        {
            std::vector<std::string> parts;
            std::string current;

            for (char ch : value)
            {
                if (ch == delimiter)
                {
                    parts.push_back(current);
                    current.clear();
                }
                else
                {
                    current.push_back(ch);
                }
            }

            parts.push_back(current);
            return parts;
        }

        static int ToInt(
            const std::string& value)
        {
            return std::atoi(value.c_str());
        }

        static float ToFloat(
            const std::string& value)
        {
            return static_cast<float>(
                std::atof(value.c_str())
            );
        }

        bool ExecuteScriptAndRead(
            const char* script,
            const char* scriptName,
            std::string& result,
            std::string* choices = nullptr)
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

            char resultBuffer[2048]{};
            char choiceBuffer[4096]{};

            bool onGameThread =
                false;

            bool luaExecuted =
                false;

            bool gotResult =
                false;

            bool gotChoices =
                choices == nullptr;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        onGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        luaExecuted =
                            doString(
                                script,
                                scriptName
                            );

                        if (!luaExecuted)
                        {
                            return;
                        }

                        const char* rawResult =
                            getText(
                                const_cast<char*>(
                                    ResultVariable
                                ),
                                0xFFFFFFFFu,
                                0
                            );

                        if (
                            rawResult != nullptr &&
                            *rawResult != '\0')
                        {
                            std::strncpy(
                                resultBuffer,
                                rawResult,
                                sizeof(resultBuffer) - 1
                            );

                            resultBuffer[
                                sizeof(resultBuffer) - 1
                            ] = '\0';

                            gotResult =
                                true;
                        }

                        if (choices != nullptr)
                        {
                            const char* rawChoices =
                                getText(
                                    const_cast<char*>(
                                        ChoiceVariable
                                    ),
                                    0xFFFFFFFFu,
                                    0
                                );

                            if (rawChoices != nullptr)
                            {
                                std::strncpy(
                                    choiceBuffer,
                                    rawChoices,
                                    sizeof(choiceBuffer) - 1
                                );

                                choiceBuffer[
                                    sizeof(choiceBuffer) - 1
                                ] = '\0';

                                gotChoices =
                                    true;
                            }
                        }
                    }
                );

            if (
                !dispatched ||
                !onGameThread ||
                !luaExecuted ||
                !gotResult ||
                !gotChoices)
            {
                return false;
            }

            result =
                resultBuffer;

            if (choices != nullptr)
            {
                *choices =
                    choiceBuffer;
            }

            return true;
        }

        bool EvaluateAndClaim(
            std::uint64_t tick)
        {
            /*
             * Phase 10D deliberately supports automatic class-aware
             * decisions only for WARRIOR. The architecture is generic,
             * but other class weight profiles remain disabled until they
             * have their own runtime validation.
             *
             * Vanilla does not expose a direct "item stats" table through
             * GetItemInfo, so the script uses a hidden GameTooltip scanner.
             * It reads the reward item link, parses relevant tooltip stats,
             * compares the result with the currently equipped item in the
             * target slot, then claims the best usable reward.
             *
             * Warrior leveling weights used in this first profile:
             *
             *   weapon DPS  = 12.0
             *   strength    = 2.0
             *   stamina     = 1.2
             *   agility     = 0.8
             *   attackPower = 0.5
             *   armor       = 0.02
             *   item level  = 0.05 armor / 0.03 weapon
             *
             * A candidate must beat the current slot by >0.75 score to
             * be marked for automatic equipping. Reward choice itself is
             * still made even when no candidate is an equipment upgrade:
             * the highest scoring usable equippable Warrior item wins.
             */
            static constexpr char Script[] = R"LUA(
WOW_INTERNAL_GEAR_RESULT='waiting';
WOW_INTERNAL_GEAR_CHOICES='';

if not (QuestFrameRewardPanel and QuestFrameRewardPanel:IsVisible()) then
    WOW_INTERNAL_GEAR_RESULT='waiting_panel';
    return;
end

local _,classToken=UnitClass('player');
if not classToken then classToken='UNKNOWN' end;
local choiceCount=GetNumQuestChoices();
if not choiceCount then choiceCount=0 end;

if choiceCount<=0 then
    WOW_INTERNAL_GEAR_RESULT='no_choices|'..classToken..'|0';
    return;
end

if classToken~='WARRIOR' then
    WOW_INTERNAL_GEAR_RESULT='unsupported_class|'..classToken..'|'..choiceCount;
    return;
end

if not WOW_INTERNAL_GEAR_SCAN then
    WOW_INTERNAL_GEAR_SCAN=CreateFrame(
        'GameTooltip',
        'WOW_INTERNAL_GEAR_SCAN',
        UIParent,
        'GameTooltipTemplate'
    );
    WOW_INTERNAL_GEAR_SCAN:SetOwner(WorldFrame,'ANCHOR_NONE');
end

local function itemId(link)
    if not link then return 0 end;
    local _,_,id=string.find(link,'item:(%d+)');
    if id then return tonumber(id) or 0 end;
    return 0;
end

local function isWeaponLoc(loc)
    return
        loc=='INVTYPE_WEAPON' or
        loc=='INVTYPE_2HWEAPON' or
        loc=='INVTYPE_WEAPONMAINHAND' or
        loc=='INVTYPE_WEAPONOFFHAND' or
        loc=='INVTYPE_RANGED' or
        loc=='INVTYPE_RANGEDRIGHT' or
        loc=='INVTYPE_THROWN';
end

local function slotFor(loc)
    if loc=='INVTYPE_HEAD' then return 1 end;
    if loc=='INVTYPE_NECK' then return 2 end;
    if loc=='INVTYPE_SHOULDER' then return 3 end;
    if loc=='INVTYPE_BODY' then return 4 end;
    if loc=='INVTYPE_CHEST' or loc=='INVTYPE_ROBE' then return 5 end;
    if loc=='INVTYPE_WAIST' then return 6 end;
    if loc=='INVTYPE_LEGS' then return 7 end;
    if loc=='INVTYPE_FEET' then return 8 end;
    if loc=='INVTYPE_WRIST' then return 9 end;
    if loc=='INVTYPE_HAND' then return 10 end;
    if loc=='INVTYPE_FINGER' then return 11 end;
    if loc=='INVTYPE_TRINKET' then return 13 end;
    if loc=='INVTYPE_CLOAK' then return 15 end;
    if loc=='INVTYPE_WEAPON' or
       loc=='INVTYPE_2HWEAPON' or
       loc=='INVTYPE_WEAPONMAINHAND' then return 16 end;
    if loc=='INVTYPE_SHIELD' or
       loc=='INVTYPE_HOLDABLE' or
       loc=='INVTYPE_WEAPONOFFHAND' then return 17 end;
    if loc=='INVTYPE_RANGED' or
       loc=='INVTYPE_RANGEDRIGHT' or
       loc=='INVTYPE_THROWN' then return 18 end;
    if loc=='INVTYPE_TABARD' then return 19 end;
    return 0;
end

local function clean(text)
    if not text then return '' end;
    text=string.gsub(text,'|c%x%x%x%x%x%x%x%x','');
    text=string.gsub(text,'|r','');
    return text;
end

local function addCapture(text,pattern)
    local _,_,v=string.find(text,pattern);
    if v then return tonumber(v) or 0 end;
    return 0;
end

local function scan(link)
    local s={
        str=0,agi=0,sta=0,intel=0,spi=0,
        armor=0,ap=0,dps=0
    };

    if not link then return s end;

    WOW_INTERNAL_GEAR_SCAN:ClearLines();
    WOW_INTERNAL_GEAR_SCAN:SetOwner(WorldFrame,'ANCHOR_NONE');
    WOW_INTERNAL_GEAR_SCAN:SetHyperlink(link);

    local damageMin=0;
    local damageMax=0;
    local speed=0;
    local lines=WOW_INTERNAL_GEAR_SCAN:NumLines();

    for lineIndex=1,lines do
        local leftObj=getglobal(
            'WOW_INTERNAL_GEAR_SCANTextLeft'..lineIndex
        );
        local rightObj=getglobal(
            'WOW_INTERNAL_GEAR_SCANTextRight'..lineIndex
        );

        local left='';
        local right='';
        if leftObj and leftObj:GetText() then
            left=clean(leftObj:GetText());
        end
        if rightObj and rightObj:GetText() then
            right=clean(rightObj:GetText());
        end

        local text=left..' '..right;

        s.str=s.str+addCapture(text,'%+(%d+) Strength');
        s.agi=s.agi+addCapture(text,'%+(%d+) Agility');
        s.sta=s.sta+addCapture(text,'%+(%d+) Stamina');
        s.intel=s.intel+addCapture(text,'%+(%d+) Intellect');
        s.spi=s.spi+addCapture(text,'%+(%d+) Spirit');

        local armor=addCapture(text,'(%d+) Armor');
        if armor>s.armor then s.armor=armor end;

        s.ap=s.ap+addCapture(text,'%+(%d+) Attack Power');

        local dps=addCapture(text,'(%d+%.?%d*) damage per second');
        if dps>s.dps then s.dps=dps end;

        if damageMin==0 then
            local _,_,a,b=string.find(
                text,
                '(%d+)%s*%-%s*(%d+)%s+Damage'
            );
            if a and b then
                damageMin=tonumber(a) or 0;
                damageMax=tonumber(b) or 0;
            end
        end

        if speed==0 then
            local _,_,sp=string.find(
                text,
                'Speed%s+(%d+%.?%d*)'
            );
            if sp then speed=tonumber(sp) or 0 end;
        end
    end

    if s.dps==0 and speed>0 and damageMax>0 then
        s.dps=((damageMin+damageMax)/2)/speed;
    end

    WOW_INTERNAL_GEAR_SCAN:Hide();
    return s;
end

local function score(link)
    if not link then return 0,'',0 end;

    local name,_,_,ilevel,_,_,_,_,equipLoc=GetItemInfo(link);
    if not name then return nil,nil,nil end;
    if not equipLoc then equipLoc='' end;
    if not ilevel then ilevel=0 end;

    local s=scan(link);
    local weapon=isWeaponLoc(equipLoc);
    local value=0;

    if weapon then
        value=
            s.dps*12.0+
            s.str*2.0+
            s.sta*1.2+
            s.agi*0.8+
            s.ap*0.5+
            s.armor*0.02+
            ilevel*0.03;
    else
        value=
            s.str*2.0+
            s.sta*1.2+
            s.agi*0.8+
            s.ap*0.5+
            s.armor*0.02+
            ilevel*0.05;
    end

    return value,equipLoc,s.dps;
end

local function scoreSlot(slot)
    if not slot or slot<=0 then return 0 end;
    local link=GetInventoryItemLink('player',slot);
    if not link then return 0 end;
    local value=score(link);
    if not value then return 0 end;
    return value;
end

local function baselineFor(equipLoc)
    local slot=slotFor(equipLoc);
    if slot==0 then return 0,0 end;

    if equipLoc=='INVTYPE_FINGER' then
        local a=scoreSlot(11);
        local b=scoreSlot(12);
        if a<=b then return a,11 else return b,12 end;
    end

    if equipLoc=='INVTYPE_TRINKET' then
        local a=scoreSlot(13);
        local b=scoreSlot(14);
        if a<=b then return a,13 else return b,14 end;
    end

    if equipLoc=='INVTYPE_2HWEAPON' then
        return scoreSlot(16)+(scoreSlot(17)*0.50),16;
    end

    return scoreSlot(slot),slot;
end

local records='';
local bestUpgradeIndex=0;
local bestUpgradeDelta=-999999;
local bestRelevantIndex=0;
local bestRelevantScore=-999999;
local bestFallbackIndex=0;
local bestFallbackScore=-999999;
local bestId=0;
local bestSlot=0;
local bestCandidate=0;
local bestBaseline=0;
local bestDelta=0;
local bestName='';
local bestEquipLoc='';
local cachePending=0;

for i=1,choiceCount do
    local name,_,_,quality,isUsable=GetQuestItemInfo('choice',i);
    local link=GetQuestItemLink('choice',i);

    if not quality then quality=0 end;
    if not name then name='unknown' end;

    -- Vanilla can present the reward panel before the client item cache
    -- has produced an item link. Asking a hidden tooltip to inspect the
    -- quest item is a bounded cache-warm request; the normal scorer is
    -- still used as soon as the link becomes available.
    if not link and WOW_INTERNAL_GEAR_SCAN.SetQuestItem then
        WOW_INTERNAL_GEAR_SCAN:ClearLines();
        WOW_INTERNAL_GEAR_SCAN:SetOwner(WorldFrame,'ANCHOR_NONE');
        WOW_INTERNAL_GEAR_SCAN:SetQuestItem('choice',i);
        WOW_INTERNAL_GEAR_SCAN:Hide();
        link=GetQuestItemLink('choice',i);
    end

    if not link then
        cachePending=1;
    else
        local candidate,equipLoc,dps=score(link);
        if candidate==nil then
            cachePending=1;
        else
            local baseline,targetSlot=baselineFor(equipLoc);
            local delta=candidate-baseline;
            local id=itemId(link);
            local usable=0;
            if isUsable then usable=1 end;

            local record=
                i..'~'..id..'~'..name..'~'..equipLoc..'~'..
                candidate..'~'..baseline..'~'..delta..'~'..
                usable..'~'..targetSlot..'~'..dps;

            if records=='' then
                records=record;
            else
                records=records..';'..record;
            end

            if usable==1 and targetSlot>0 and delta>0.75 then
                if delta>bestUpgradeDelta then
                    bestUpgradeDelta=delta;
                    bestUpgradeIndex=i;
                end
            end

            if usable==1 and targetSlot>0 then
                if candidate>bestRelevantScore then
                    bestRelevantScore=candidate;
                    bestRelevantIndex=i;
                end
            end

            local fallback=quality*1000+candidate;
            if usable==1 and fallback>bestFallbackScore then
                bestFallbackScore=fallback;
                bestFallbackIndex=i;
            end
        end
    end
end

WOW_INTERNAL_GEAR_CHOICES=records;

if cachePending==1 then
    WOW_INTERNAL_GEAR_RESULT='cache_pending|'..classToken..'|'..choiceCount;
    return;
end

local pick=0;
if bestUpgradeIndex>0 then
    pick=bestUpgradeIndex;
elseif bestRelevantIndex>0 then
    pick=bestRelevantIndex;
else
    pick=bestFallbackIndex;
end

if pick<=0 then
    WOW_INTERNAL_GEAR_RESULT='no_usable_reward|'..classToken..'|'..choiceCount;
    return;
end

local selectedName,_,_,_,selectedUsable=GetQuestItemInfo('choice',pick);
local selectedLink=GetQuestItemLink('choice',pick);
local selectedScore,selectedLoc=score(selectedLink);
local selectedBaseline,selectedSlot=baselineFor(selectedLoc);
local selectedDelta=selectedScore-selectedBaseline;
local selectedId=itemId(selectedLink);
local shouldEquip=0;
if selectedSlot>0 and selectedDelta>0.75 then shouldEquip=1 end;

if not selectedName then selectedName='unknown' end;

WOW_INTERNAL_GEAR_SELECTED_ID=selectedId;
WOW_INTERNAL_GEAR_SELECTED_SLOT=selectedSlot;
WOW_INTERNAL_GEAR_SHOULD_EQUIP=shouldEquip;

WOW_INTERNAL_GEAR_RESULT=
    'claimed|'..classToken..'|'..choiceCount..'|'..pick..'|'..
    selectedId..'|'..selectedSlot..'|'..shouldEquip..'|'..
    selectedScore..'|'..selectedBaseline..'|'..selectedDelta..'|'..
    selectedName;

GetQuestReward(pick);
)LUA";

            std::string result;
            std::string choices;

            if (!ExecuteScriptAndRead(
                    Script,
                    "wow-internal/ClassAwareRewardController-evaluate.lua",
                    result,
                    &choices))
            {
                return false;
            }

            if (result != lastResult_)
            {
                Debug::Logger::Info(
                    "CLASS-AWARE REWARD result: " +
                    result
                );

                lastResult_ =
                    result;
            }

            if (!choices.empty())
            {
                const auto records =
                    Split(choices, ';');

                for (const auto& record : records)
                {
                    if (record.empty())
                    {
                        continue;
                    }

                    const auto fields =
                        Split(record, '~');

                    if (fields.size() < 10)
                    {
                        continue;
                    }

                    Debug::Logger::Info(
                        "GEAR CHOICE[" +
                        fields[0] +
                        "]: id=" +
                        fields[1] +
                        " name=" +
                        fields[2] +
                        " equip=" +
                        fields[3] +
                        " score=" +
                        fields[4] +
                        " baseline=" +
                        fields[5] +
                        " delta=" +
                        fields[6] +
                        " usable=" +
                        fields[7] +
                        " slot=" +
                        fields[8] +
                        " dps=" +
                        fields[9]
                    );
                }
            }

            const auto fields =
                Split(result, '|');

            if (fields.empty())
            {
                return false;
            }

            if (
                fields[0] == "waiting" ||
                fields[0] == "waiting_panel")
            {
                return true;
            }

            if (fields[0] == "cache_pending")
            {
                ++cachePendingAttempts_;

                if (
                    cachePendingAttempts_ == 1 ||
                    (cachePendingAttempts_ % 4) == 0 ||
                    cachePendingAttempts_ >= CacheFallbackAttemptLimit)
                {
                    Debug::Logger::Info(
                        "CLASS-AWARE REWARD: item cache pending attempt " +
                        std::to_string(cachePendingAttempts_) +
                        "/" +
                        std::to_string(CacheFallbackAttemptLimit)
                    );
                }

                if (cachePendingAttempts_ < CacheFallbackAttemptLimit)
                {
                    return true;
                }

                Debug::Logger::Info(
                    "CLASS-AWARE REWARD: bounded item-cache wait exhausted; "
                    "using usable-reward metadata fallback without auto-equip."
                );

                return ClaimMetadataFallback();
            }

            cachePendingAttempts_ = 0;

            if (fields[0] == "unsupported_class")
            {
                if (fields.size() >= 2)
                {
                    classToken_ =
                        fields[1];
                }

                Debug::Logger::Info(
                    "CLASS-AWARE REWARD: unsupported class " +
                    classToken_ +
                    "; no reward was selected."
                );

                SetState(
                    ClassAwareRewardState::UnsupportedClass
                );

                return true;
            }

            if (fields[0] == "no_choices")
            {
                Fail(
                    "reward evaluator was invoked without "
                    "reward choices."
                );

                return true;
            }

            if (fields[0] == "no_usable_reward")
            {
                Fail(
                    "no usable class-appropriate reward "
                    "could be selected."
                );

                return true;
            }

            if (
                fields[0] != "claimed" ||
                fields.size() < 11)
            {
                return false;
            }

            classToken_ = fields[1];
            choiceCount_ = ToInt(fields[2]);
            selectedChoice_ = ToInt(fields[3]);
            selectedItemId_ = ToInt(fields[4]);
            targetSlot_ = ToInt(fields[5]);
            shouldEquip_ = ToInt(fields[6]) != 0;
            candidateScore_ = ToFloat(fields[7]);
            baselineScore_ = ToFloat(fields[8]);
            upgradeDelta_ = ToFloat(fields[9]);
            selectedName_ = fields[10];

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "CLASS-AWARE REWARD: CLAIM ISSUED"
            );

            Debug::Logger::Info(
                "Class: " + classToken_
            );

            Debug::Logger::Info(
                "Selected choice: " +
                std::to_string(selectedChoice_) +
                "/" +
                std::to_string(choiceCount_)
            );

            Debug::Logger::Info(
                "Selected item: " +
                selectedName_ +
                " id=" +
                std::to_string(selectedItemId_)
            );

            Debug::Logger::Info(
                "Target slot: " +
                std::to_string(targetSlot_)
            );

            Debug::Logger::Info(
                "Candidate score: " +
                fields[7] +
                " baseline=" +
                fields[8] +
                " delta=" +
                fields[9]
            );

            Debug::Logger::Info(
                std::string("Auto-equip after delivery: ") +
                (shouldEquip_ ? "yes" : "no")
            );

            Debug::Logger::Info(
                "================================"
            );

            lastPollTick_ =
                tick;

            SetState(
                ClassAwareRewardState::WaitingForDelivery
            );

            return true;
        }

        bool ClaimMetadataFallback()
        {
            static constexpr char Script[] = R"LUA(
WOW_INTERNAL_GEAR_RESULT='fallback_waiting';

if not (QuestFrameRewardPanel and QuestFrameRewardPanel:IsVisible()) then
    WOW_INTERNAL_GEAR_RESULT='fallback_waiting_panel';
    return;
end

local _,classToken=UnitClass('player');
if not classToken then classToken='UNKNOWN' end;
local choiceCount=GetNumQuestChoices();
if not choiceCount then choiceCount=0 end;

if choiceCount<=0 then
    WOW_INTERNAL_GEAR_RESULT='fallback_no_choices|'..classToken..'|0';
    return;
end

local bestIndex=0;
local bestQuality=-1;
local bestName='unknown';

for i=1,choiceCount do
    local name,_,_,quality,isUsable=GetQuestItemInfo('choice',i);
    if not quality then quality=0 end;
    if not name then name='unknown' end;

    if isUsable and quality>bestQuality then
        bestIndex=i;
        bestQuality=quality;
        bestName=name;
    end
end

if bestIndex<=0 then
    WOW_INTERNAL_GEAR_RESULT='fallback_no_usable|'..classToken..'|'..choiceCount;
    return;
end

WOW_INTERNAL_GEAR_RESULT=
    'fallback_claimed|'..classToken..'|'..choiceCount..'|'..
    bestIndex..'|'..bestQuality..'|'..bestName;

GetQuestReward(bestIndex);
)LUA";

            std::string result;

            if (!ExecuteScriptAndRead(
                    Script,
                    "wow-internal/ClassAwareRewardController-fallback.lua",
                    result))
            {
                return false;
            }

            Debug::Logger::Info(
                "CLASS-AWARE REWARD fallback result: " +
                result
            );

            const auto fields =
                Split(result, '|');

            if (fields.empty())
            {
                return false;
            }

            if (
                fields[0] == "fallback_waiting" ||
                fields[0] == "fallback_waiting_panel")
            {
                return true;
            }

            if (
                fields[0] == "fallback_no_choices" ||
                fields[0] == "fallback_no_usable")
            {
                Fail(
                    "metadata fallback could not find a usable quest reward."
                );
                return true;
            }

            if (
                fields[0] != "fallback_claimed" ||
                fields.size() < 6)
            {
                return false;
            }

            classToken_ = fields[1];
            choiceCount_ = ToInt(fields[2]);
            selectedChoice_ = ToInt(fields[3]);
            selectedItemId_ = 0;
            targetSlot_ = 0;
            shouldEquip_ = false;
            candidateScore_ = 0.0f;
            baselineScore_ = 0.0f;
            upgradeDelta_ = 0.0f;
            selectedName_ = fields[5];
            metadataFallbackClaimed_ = true;

            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "CLASS-AWARE REWARD: METADATA FALLBACK CLAIM ISSUED"
            );
            Debug::Logger::Info(
                "Class: " + classToken_
            );
            Debug::Logger::Info(
                "Selected choice: " +
                std::to_string(selectedChoice_) +
                "/" +
                std::to_string(choiceCount_)
            );
            Debug::Logger::Info(
                "Selected item: " + selectedName_
            );
            Debug::Logger::Info(
                "Policy: highest-quality reward reported usable by "
                "GetQuestItemInfo; auto-equip disabled because the full "
                "item link/stat cache never became available."
            );
            Debug::Logger::Info(
                "Final turn-in success still requires quest-log removal."
            );
            Debug::Logger::Info(
                "================================"
            );

            // GetQuestReward() has been issued. The owning turn-in executor
            // independently requires quest-log removal before it can report
            // success, so Verified here means the reward-choice action was
            // issued successfully, not that equipment delivery was inferred.
            SetState(
                ClassAwareRewardState::Verified
            );

            return true;
        }

        bool PollDeliveryAndEquip(
            std::uint64_t tick)
        {
            if (selectedItemId_ <= 0)
            {
                return false;
            }

            std::ostringstream script;

            script << R"LUA(
WOW_INTERNAL_GEAR_RESULT='waiting_reward';

local targetId=)LUA"
                << selectedItemId_
                << R"LUA(;
local targetSlot=)LUA"
                << targetSlot_
                << R"LUA(;
local shouldEquip=)LUA"
                << (shouldEquip_ ? 1 : 0)
                << R"LUA(;

local function itemId(link)
    if not link then return 0 end;
    local _,_,id=string.find(link,'item:(%d+)');
    if id then return tonumber(id) or 0 end;
    return 0;
end

if targetSlot>0 then
    local equipped=itemId(GetInventoryItemLink('player',targetSlot));
    if equipped==targetId then
        WOW_INTERNAL_GEAR_RESULT='equipped|'..targetId..'|'..targetSlot;
        return;
    end
end

local found=0;
for bag=0,4 do
    local slots=GetContainerNumSlots(bag);
    if slots then
        for slot=1,slots do
            local link=GetContainerItemLink(bag,slot);
            if itemId(link)==targetId then
                found=1;
                break;
            end
        end
    end
    if found==1 then break end
end

if found==0 then
    WOW_INTERNAL_GEAR_RESULT='waiting_reward|'..targetId;
    return;
end

if shouldEquip==0 or targetSlot<=0 then
    WOW_INTERNAL_GEAR_RESULT='received_no_upgrade|'..targetId;
    return;
end

if UnitAffectingCombat('player') then
    WOW_INTERNAL_GEAR_RESULT='waiting_out_of_combat|'..targetId;
    return;
end

EquipItemByName(targetId,targetSlot);
WOW_INTERNAL_GEAR_RESULT='equip_issued|'..targetId..'|'..targetSlot;
)LUA";

            const std::string scriptText =
                script.str();

            std::string result;

            if (!ExecuteScriptAndRead(
                    scriptText.c_str(),
                    "wow-internal/ClassAwareRewardController-equip.lua",
                    result))
            {
                return false;
            }

            if (result != lastResult_)
            {
                Debug::Logger::Info(
                    "CLASS-AWARE GEAR result: " +
                    result
                );

                lastResult_ =
                    result;
            }

            const auto fields =
                Split(result, '|');

            if (fields.empty())
            {
                return false;
            }

            if (
                fields[0] == "waiting_reward" ||
                fields[0] == "waiting_out_of_combat")
            {
                return true;
            }

            if (fields[0] == "equip_issued")
            {
                SetState(
                    ClassAwareRewardState::EquipIssued
                );

                return true;
            }

            if (
                fields[0] == "equipped" ||
                fields[0] == "received_no_upgrade")
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "CLASS-AWARE REWARD: VERIFIED"
                );

                Debug::Logger::Info(
                    "Item: " +
                    selectedName_ +
                    " id=" +
                    std::to_string(selectedItemId_)
                );

                Debug::Logger::Info(
                    fields[0] == "equipped"
                        ? "Equipment result: equipped upgrade."
                        : "Equipment result: reward received; "
                          "existing gear kept."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    ClassAwareRewardState::Verified
                );

                return true;
            }

            return false;
        }

    public:
        static bool Validate()
        {
            const auto doString =
                LuaDoStringAddress();

            const auto getText =
                GetTextAddress();

            Debug::Logger::Info(
                "ClassAwareReward DoString: " +
                Hex32(doString)
            );

            Debug::Logger::Info(
                "ClassAwareReward GetText: " +
                Hex32(getText)
            );

            const bool valid =
                IsExecutable(doString) &&
                IsExecutable(getText);

            Debug::Logger::Info(
                std::string(
                    "ClassAwareReward validation "
                ) +
                (valid ? "PASS." : "FAILED.")
            );

            return valid;
        }

        static bool RunReadOnlyProbe()
        {
            if (!Validate())
            {
                return false;
            }

            static constexpr char Script[] = R"LUA(
local _,classToken=UnitClass('player');
if not classToken then classToken='UNKNOWN' end;
local equipped=0;
local mainId=0;
local offId=0;

local function itemId(link)
    if not link then return 0 end;
    local _,_,id=string.find(link,'item:(%d+)');
    if id then return tonumber(id) or 0 end;
    return 0;
end

for slot=1,19 do
    local link=GetInventoryItemLink('player',slot);
    if link then equipped=equipped+1 end;
end

mainId=itemId(GetInventoryItemLink('player',16));
offId=itemId(GetInventoryItemLink('player',17));

WOW_INTERNAL_GEAR_RESULT=
    'probe|'..classToken..'|'..equipped..'|'..mainId..'|'..offId;
)LUA";

            const auto doStringAddress =
                LuaDoStringAddress();
            const auto getTextAddress =
                GetTextAddress();

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

            char result[256]{};
            bool onGameThread = false;
            bool executed = false;
            bool gotText = false;

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
                                "wow-internal/ClassAwareRewardController-probe.lua"
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
                            gotText = true;
                        }
                    }
                );

            if (
                !dispatched ||
                !onGameThread ||
                !executed ||
                !gotText)
            {
                Debug::Logger::Info(
                    "CLASS-AWARE GEAR PROBE: FAILED"
                );
                return false;
            }

            Debug::Logger::Info(
                std::string(
                    "CLASS-AWARE GEAR PROBE: "
                ) +
                result
            );

            return true;
        }

        bool Start(
            std::uint64_t tick)
        {
            if (
                state_ !=
                    ClassAwareRewardState::Idle)
            {
                return false;
            }

            if (!Validate())
            {
                return false;
            }

            startTick_ = tick;
            lastPollTick_ = 0;
            choiceCount_ = 0;
            selectedChoice_ = 0;
            selectedItemId_ = 0;
            targetSlot_ = 0;
            shouldEquip_ = false;
            candidateScore_ = 0.0f;
            baselineScore_ = 0.0f;
            upgradeDelta_ = 0.0f;
            classToken_.clear();
            selectedName_.clear();
            lastResult_.clear();
            cachePendingAttempts_ = 0;
            metadataFallbackClaimed_ = false;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "CLASS-AWARE REWARD: START"
            );

            Debug::Logger::Info(
                "Validated profile: WARRIOR leveling."
            );

            Debug::Logger::Info(
                "Selection policy: usable class item -> "
                "upgrade delta -> class score; then verify "
                "delivery and auto-equip only real upgrades."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                ClassAwareRewardState::Evaluating
            );

            return true;
        }

        void Update(
            std::uint64_t tick)
        {
            if (
                state_ == ClassAwareRewardState::Idle ||
                state_ == ClassAwareRewardState::Verified ||
                state_ == ClassAwareRewardState::UnsupportedClass ||
                state_ == ClassAwareRewardState::Failed)
            {
                return;
            }

            if (
                tick >=
                    startTick_ +
                    RewardTimeoutTicks)
            {
                Fail(
                    "reward/equipment verification timed out."
                );
                return;
            }

            if (
                lastPollTick_ != 0 &&
                tick <
                    lastPollTick_ +
                    PollIntervalTicks)
            {
                return;
            }

            lastPollTick_ = tick;

            if (
                state_ ==
                    ClassAwareRewardState::Evaluating)
            {
                if (!EvaluateAndClaim(tick))
                {
                    Fail(
                        "reward evaluation script failed."
                    );
                }
                return;
            }

            if (
                state_ ==
                    ClassAwareRewardState::WaitingForDelivery ||
                state_ ==
                    ClassAwareRewardState::EquipIssued)
            {
                if (!PollDeliveryAndEquip(tick))
                {
                    Fail(
                        "reward delivery/equip verification failed."
                    );
                }
            }
        }

        bool Verified() const
        {
            return
                state_ ==
                    ClassAwareRewardState::Verified;
        }

        bool UnsupportedClass() const
        {
            return
                state_ ==
                    ClassAwareRewardState::UnsupportedClass;
        }

        bool Failed() const
        {
            return
                state_ ==
                    ClassAwareRewardState::Failed;
        }

        bool IsActive() const
        {
            return
                state_ == ClassAwareRewardState::Evaluating ||
                state_ == ClassAwareRewardState::WaitingForDelivery ||
                state_ == ClassAwareRewardState::EquipIssued;
        }

        const char* StateName() const
        {
            return
                StateNameInternal(state_);
        }

        int SelectedChoice() const
        {
            return selectedChoice_;
        }

        int SelectedItemId() const
        {
            return selectedItemId_;
        }

        int TargetSlot() const
        {
            return targetSlot_;
        }

        bool ShouldEquip() const
        {
            return shouldEquip_;
        }

        float UpgradeDelta() const
        {
            return upgradeDelta_;
        }

        const std::string& SelectedName() const
        {
            return selectedName_;
        }
    };
}
