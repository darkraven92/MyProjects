#pragma once

#include <cstdint>
#include <string_view>

namespace Bot
{
    struct AutoSellItemFacts
    {
        std::uint32_t itemId = 0;
        int quality = -1; // GetItemInfo: poor=0, common=1, uncommon=2.
        std::string_view itemType{};
        std::string_view itemSubType{};
        bool locked = false;
        bool equipped = false;
        bool quest = false;
        bool explicitProtection = false;
        bool food = false;
        bool drink = false;
    };

    struct AutoSellItemPolicy
    {
        static constexpr std::string_view Reason(const AutoSellItemFacts& item)
        {
            if (item.locked) return "locked";
            if (item.equipped) return "equipped";
            if (item.explicitProtection || item.itemId == 6948)
                return "explicit_protection";
            if (item.quest || item.itemType == "Quest" || item.itemType == "Key")
                return "quest_item";
            if (item.food) return "food";
            if (item.drink) return "drink";
            if (item.itemType == "Trade Goods" || item.itemType == "Tradeskill")
                return "gathering_material";
            if (item.quality >= 2) return "quality_uncommon_or_higher";
            if (item.itemId == 0 || item.quality < 0 || item.itemType.empty())
                return "classification_unknown";
            // Only positively identified low-quality equipment or poor junk.
            if ((item.itemType == "Armor" || item.itemType == "Weapon") &&
                item.quality <= 1)
                return "sell";
            if (item.itemType == "Miscellaneous" && item.quality == 0)
                return "sell";
            return "classification_unknown";
        }

        static constexpr bool CanSell(const AutoSellItemFacts& item)
        {
            return Reason(item) == "sell";
        }

        static constexpr bool PlanStillMatches(
            std::string_view plannedLink, std::uint32_t plannedId,
            int plannedCount, bool plannedLocked,
            std::string_view currentLink, std::uint32_t currentId,
            int currentCount, bool currentLocked)
        {
            return !plannedLink.empty() && plannedLink == currentLink &&
                plannedId != 0 && plannedId == currentId &&
                plannedCount > 0 && plannedCount == currentCount &&
                !plannedLocked && !currentLocked;
        }

        // Lua mirrors Reason(), using the shared consumable tooltip function.
        static constexpr const char* LuaDefinition()
        {
            return
                "local function saleReason(b,s,link,count,locked) "
                "local _,_,idText=string.find(link,'item:(%d+)'); "
                "local id=tonumber(idText or '0') or 0; "
                "tt:ClearLines(); tt:SetBagItem(b,s); "
                "local _,_,quality,_,_,itype=GetItemInfo(link); "
                "if id>0 and (quality==nil or not itype) then "
                "local _,_,idQuality,_,_,idType=GetItemInfo(id); "
                "if idType then quality=idQuality; itype=idType end; end; "
                "local _,_,_,bagQuality=GetContainerItemInfo(b,s); "
                "if bagQuality and bagQuality>=0 and quality and quality~=bagQuality then "
                "quality=nil; itype=nil; end; "
                "if itype and quality==nil and bagQuality and bagQuality>=0 then "
                "quality=bagQuality; end; "
                "local food,drink,_,_,_,_,quest=classifyConsumable(tt); "
                "local reason='classification_unknown'; "
                "if not count or count<1 then reason='classification_unknown' "
                "elseif locked then reason='locked' "
                "elseif id==6948 then reason='explicit_protection' "
                "elseif quest or itype=='Quest' or itype=='Key' then reason='quest_item' "
                "elseif food then reason='food' "
                "elseif drink then reason='drink' "
                "elseif itype=='Trade Goods' or itype=='Tradeskill' then reason='gathering_material' "
                "elseif quality and quality>=2 then reason='quality_uncommon_or_higher' "
                "elseif id>0 and quality and itype and "
                "((quality>=0 and quality<=1 and (itype=='Armor' or itype=='Weapon')) or "
                "(quality==0 and itype=='Miscellaneous')) then reason='sell' end; "
                "return reason,id,quality or -1,itype or 'unknown'; end; ";
        }
    };
}
