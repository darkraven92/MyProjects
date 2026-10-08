#pragma once
namespace Bot
{
    struct EquipmentDurabilityProbe
    {
        // 1.12.1 PaperDollFrame.lua uses SetInventoryItem("player", slot).
        // GlobalStrings.lua defines DURABILITY_TEMPLATE. No modern API is
        // required; unfamiliar localized/cold tooltip data stays unknown.
        static const char* Lua()
        {
            return R"LUA(
local mind=100; local dn=0; local durabilityKnown=1;
if not WOW_INTERNAL_DURABILITY_SCAN then
    WOW_INTERNAL_DURABILITY_SCAN=CreateFrame('GameTooltip','WOW_INTERNAL_DURABILITY_SCAN',UIParent,'GameTooltipTemplate');
end
local dt=WOW_INTERNAL_DURABILITY_SCAN;
dt:SetOwner(UIParent,'ANCHOR_NONE');
for slot=1,19 do
    local link=GetInventoryItemLink('player',slot);
    if link then
        local current,maximum;
        if GetInventoryItemDurability then current,maximum=GetInventoryItemDurability(slot) end;
        if not current or not maximum then
            if DURABILITY_TEMPLATE~='Durability %d / %d' or not dt.SetInventoryItem then
                durabilityKnown=0;
            else
                dt:ClearLines();
                local hasItem=dt:SetInventoryItem('player',slot);
                local name=GetItemInfo(link);
                if not hasItem or not name or dt:NumLines()<2 then durabilityKnown=0 else
                    for line=1,dt:NumLines() do
                        local obj=getglobal('WOW_INTERNAL_DURABILITY_SCANTextLeft'..line);
                        local text=obj and obj:GetText();
                        if text then
                            local _,_,c,m=string.find(text,'^Durability (%d+) / (%d+)$');
                            if c then current=tonumber(c); maximum=tonumber(m) end;
                        end
                    end
                end
            end
        end
        if current and maximum and maximum>0 and current>=0 and current<=maximum then
            dn=dn+1; mind=math.min(mind,current*100/maximum);
        elseif current or maximum then durabilityKnown=0 end;
    elseif GetInventoryItemTexture and GetInventoryItemTexture('player',slot) then
        durabilityKnown=0;
    end
end
dt:Hide();
)LUA";
        }
    };
}
