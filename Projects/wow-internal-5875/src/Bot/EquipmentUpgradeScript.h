#pragma once
#include "EquipmentUpgradePolicy.h"
#include <sstream>
#include <string>

namespace Bot
{
    inline std::string EquipmentUpgradeScript()
    {
        std::ostringstream script;
        script<<"local weights={";
        for(const auto& entry:EquipmentUpgradePolicy::Classes)
        {
            script<<entry.token<<"={";
            for(const auto value:EquipmentUpgradePolicy::Values(entry.weights)) script<<value<<",";
            script<<"},";
        }
        script<<"}; local minimumGain="<<EquipmentUpgradePolicy::MinimumGain
              <<"; local relativeGain="<<EquipmentUpgradePolicy::RelativeGain<<";\n";
        script<<R"LUA(
WOW_INTERNAL_EQUIPMENT_RESULT='no_change';
WOW_INTERNAL_EQUIPMENT_DIAGNOSTICS='';
if not (GetItemInfo and GetInventoryItemLink and UnitAffectingCombat and
        PickupContainerItem and PickupInventoryItem and CursorHasItem and
        CursorCanGoInSlot and IsInventoryItemLocked and ClearCursor) then
    WOW_INTERNAL_EQUIPMENT_RESULT='unsupported_client'; return;
end
local function id(link)
    local _,_,n=string.find(link or '', 'item:(%d+)'); return tonumber(n) or 0;
end
if not WOW_INTERNAL_EQUIPMENT_SESSION then
    WOW_INTERNAL_EQUIPMENT_SESSION={attempted={},pending=nil,signature=''};
end
local session=WOW_INTERNAL_EQUIPMENT_SESSION;
local tick=WOW_INTERNAL_EQUIPMENT_TICK or 0;
if session.pending then
    local p=session.pending;
    if id(GetInventoryItemLink('player',p.slot))==p.id then
        WOW_INTERNAL_EQUIPMENT_RESULT='verified|'..p.id..'|'..p.slot;
        session.pending=nil; session.signature=''; return;
    end
    p.polls=p.polls+1;
    if p.polls>=20 or (tick>=p.started and tick-p.started>=40) then
        WOW_INTERNAL_EQUIPMENT_RESULT='failed_verification|'..p.id;
        session.pending=nil; return;
    end
    WOW_INTERNAL_EQUIPMENT_RESULT='pending'; return;
end
if UnitAffectingCombat('player') then return end
if CursorHasItem() or (SpellIsTargeting and SpellIsTargeting()) or
   (CursorHasSpell and CursorHasSpell()) then return end;
if (MerchantFrame and MerchantFrame:IsVisible()) or (QuestFrame and QuestFrame:IsVisible()) or
   (GossipFrame and GossipFrame:IsVisible()) or (TrainerFrame and TrainerFrame:IsVisible()) or
   (LootFrame and LootFrame:IsVisible()) then return end;
local _,classToken=UnitClass('player');
local w=weights[classToken]; if not w then return end;
local level=UnitLevel('player'); if not level or level<1 then return end;
local signature=classToken..':'..level;
local copies={};
for slot=1,19 do signature=signature..'|'..(GetInventoryItemLink('player',slot) or '') end
for bag=0,4 do
    for slot=1,(GetContainerNumSlots(bag) or 0) do
        local _,count,locked=GetContainerItemInfo(bag,slot);
        local item=id(GetContainerItemLink(bag,slot));
        if item>0 then copies[item]=(copies[item] or 0)+1 end;
        signature=signature..'|'..(GetContainerItemLink(bag,slot) or '')..':'..(count or 0)..':'..tostring(locked);
    end
end
if signature==session.signature then return end;
session.signature=signature;
if not WOW_INTERNAL_EQUIPMENT_SCAN then
    WOW_INTERNAL_EQUIPMENT_SCAN=CreateFrame('GameTooltip','WOW_INTERNAL_EQUIPMENT_SCAN',UIParent,'GameTooltipTemplate');
end
local tooltip=WOW_INTERNAL_EQUIPMENT_SCAN;
local slots={INVTYPE_HEAD=1,INVTYPE_SHOULDER=3,INVTYPE_CHEST=5,INVTYPE_ROBE=5,
    INVTYPE_WAIST=6,INVTYPE_LEGS=7,INVTYPE_FEET=8,INVTYPE_WRIST=9,INVTYPE_HAND=10,
    INVTYPE_CLOAK=15,INVTYPE_WEAPON=16,INVTYPE_WEAPONMAINHAND=16,INVTYPE_2HWEAPON=16};
local function scan(link,bag,index,inventorySlot)
    local name,_,quality,_,minimumLevel,kind,subtype,_,loc=GetItemInfo(link);
    if not name or not quality or not minimumLevel or not subtype then return nil,'unknown_item_metadata' end;
    if minimumLevel>level then return nil,'required_level' end;
    if kind~='Armor' and kind~='Weapon' then return nil,'not_equipment' end;
    local slot=slots[loc]; if not slot then return nil,'unsupported_slot' end;
    local stats={0,0,0,0,0,0,0,0}; local speed=0; local valid=true; local reason='';
    local function reject(why) valid=false; if reason=='' then reason=why end end;
    tooltip:ClearLines(); tooltip:SetOwner(WorldFrame,'ANCHOR_NONE');
    if bag~=nil then tooltip:SetBagItem(bag,index) else
        if not tooltip.SetInventoryItem then return nil,'unsupported_inventory_tooltip' end;
        tooltip:SetInventoryItem('player',inventorySlot);
    end;
    local function line(text)
        if not text or text=='' or text==name then return end;
        text=string.gsub(string.gsub(text,'|c%x%x%x%x%x%x%x%x',''),'|r','');
        text=string.gsub(string.gsub(text,'^%s+',''),'%s+$','');
        if text=='Soulbound' or
           text==subtype or text=='One-Hand' or text=='Two-Hand' or text=='Main Hand' or
           text=='Head' or text=='Shoulder' or text=='Chest' or text=='Waist' or text=='Legs' or
           text=='Feet' or text=='Wrist' or text=='Hands' or text=='Back' then return end;
        local names={'Strength','Agility','Stamina','Intellect','Spirit','Armor','Attack Power'};
        for i=1,7 do
            local _,_,v=string.find(text,'^%+?(%d+) '..names[i]..'$');
            if v then stats[i]=stats[i]+tonumber(v); return end;
        end
        local _,_,a,b=string.find(text,'^Durability (%d+) / (%d+)$');
        if a then if tonumber(a)==0 then reject('broken') end; return end;
        local _,_,required=string.find(text,'^Requires Level (%d+)$');
        if required then if tonumber(required)>level then reject('required_level') end; return end;
        local _,_,dps=string.find(text,'^%((%d+%.?%d*) damage per second%)$');
        if dps then stats[8]=tonumber(dps); return end;
        local _,_,s=string.find(text,'^Speed (%d+%.?%d*)$');
        if s then speed=tonumber(s); return end;
        if string.find(text,'^%d+ %- %d+ Damage$') then return end;
        -- Binding confirmation, Unique, Classes, skills, set bonuses, enchantments, effects,
        -- localized/unrecognized lines: never guess compatibility or value.
        reject('unknown_tooltip:'..text);
    end
    local n=tooltip:NumLines(); if not n or n<2 then reject('missing_tooltip') else
        for i=1,n do
            local left=getglobal('WOW_INTERNAL_EQUIPMENT_SCANTextLeft'..i);
            local right=getglobal('WOW_INTERNAL_EQUIPMENT_SCANTextRight'..i);
            if left then line(left:GetText()) end;
            if right then line(right:GetText()) end;
        end
    end
    tooltip:Hide();
    if kind=='Weapon' and (speed<=0 or stats[8]<=0) then reject('unknown_weapon_damage_or_speed') end;
    if not valid then return nil,reason end;
    return {stats=stats,speed=speed,slot=slot,loc=loc,kind=kind,subtype=subtype};
end
local function score(stats)
    local value=0; for i=1,8 do value=value+stats[i]*w[i] end; return value;
end
local best=nil;
local considered=0;
local diagnostics={};
local function text(value)
    return string.gsub(tostring(value or 'unknown'),'[%c|]',' ');
end
local function armorProficiency(candidate,current)
    if current and current.kind=='Armor' and current.subtype==candidate.subtype then return true end;
    -- An actual equipped supported armor item proves this subtype is usable.
    -- Final acceptance still requires the client's CursorCanGoInSlot check.
    for slot=1,15 do
        local link=GetInventoryItemLink('player',slot);
        local item=link and scan(link,nil,nil,slot);
        if item and item.kind=='Armor' and item.subtype==candidate.subtype then return true end;
    end
    return false;
end
for bag=0,4 do
    for index=1,(GetContainerNumSlots(bag) or 0) do
        local link=GetContainerItemLink(bag,index); local item=id(link);
        local _,count,locked=GetContainerItemInfo(bag,index);
        if item>0 then
            considered=considered+1;
            local candidate,reason=scan(link,bag,index);
            local equipped=nil; local current=nil; local before=nil; local after=nil;
            local approved=false;
            if copies[item]~=1 or count~=1 then reason='duplicate_or_stacked_item'; candidate=nil end;
            if locked then reason='locked_item'; candidate=nil end;
            if session.attempted[item] then reason='already_attempted'; candidate=nil end;
            if candidate then
                equipped=GetInventoryItemLink('player',candidate.slot);
                local currentReason;
                if equipped then current,currentReason=scan(equipped,nil,nil,candidate.slot) end;
                after=score(candidate.stats); before=current and score(current.stats) or (not equipped and 0 or nil);
                reason=nil;
                if equipped and not current then reason='current_'..(currentReason or 'unknown')
                elseif id(equipped)==item then reason='already_equipped'
                elseif candidate.kind=='Armor' and not armorProficiency(candidate,current) then reason='unknown_armor_proficiency'
                elseif current and (current.kind~=candidate.kind or current.slot~=candidate.slot) then reason='incompatible_slot'
                elseif candidate.kind=='Weapon' and (not current or current.loc~=candidate.loc or current.subtype~=candidate.subtype) then reason='weapon_loadout_or_proficiency'
                elseif candidate.loc=='INVTYPE_2HWEAPON' and GetInventoryItemLink('player',17) then reason='occupied_offhand'
                elseif current and candidate.kind=='Weapon' and math.abs(candidate.speed-current.speed)>current.speed*.1 then reason='weapon_speed_tradeoff'
                end;
                if not reason and current then
                    for i=1,8 do if candidate.stats[i]<current.stats[i] then reason='stat_tradeoff' end end;
                end;
                -- Empty, proven-compatible armor is not a sidegrade. Require positive
                -- modeled value; occupied slots retain the existing clear-gain margin.
                if not reason and after<=before+(current and math.max(minimumGain,before*relativeGain) or 0) then
                    reason='insufficient_clear_gain';
                end;
                if not reason then
                    approved=true; reason=current and 'clear_compatible_upgrade' or 'empty_proven_armor_slot';
                    if not best or after-before>best.gain then
                        best={id=item,slot=candidate.slot,bag=bag,index=index,link=link,equipped=equipped,
                              gain=after-before,before=before,after=after,polls=0,started=tick};
                    end
                end
            end
            local name,_,quality,itemLevel,minimum,kind,subtype,_,loc=GetItemInfo(link);
            local slot=loc and slots[loc];
            local function stats(value) return value and table.concat(value.stats,',') or 'unknown' end;
            table.insert(diagnostics,'EQUIPMENT CANDIDATE item='..item..' bag='..bag..' index='..index..
                ' name={'..text(name)..'} quality='..text(quality)..' itemLevel='..text(itemLevel)..
                ' requiredLevel='..text(minimum)..' playerClass='..text(classToken)..' type={'..text(kind)..
                '/'..text(subtype)..'} location='..text(loc)..' slot='..text(slot)..
                ' current='..id(slot and GetInventoryItemLink('player',slot))..
                ' stats='..stats(candidate)..' currentStats='..stats(current)..
                ' candidateScore='..text(after)..' currentScore='..text(before)..
                ' delta='..text(after and before and after-before)..
                ' decision='..(approved and 'approve' or 'reject')..' reason={'..text(reason)..'}');
        end
    end
end
WOW_INTERNAL_EQUIPMENT_DIAGNOSTICS=table.concat(diagnostics,'\n');
if best and not UnitAffectingCombat('player') then
    local _,count,locked=GetContainerItemInfo(best.bag,best.index);
    if locked or IsInventoryItemLocked(best.slot) or CursorHasItem() or count~=1 or GetContainerItemLink(best.bag,best.index)~=best.link or
       GetInventoryItemLink('player',best.slot)~=best.equipped then return end;
    -- One command per item per DLL session, regardless of verification failure.
    session.attempted[best.id]=true;
    -- Vanilla paper-doll/bag click protocol; no modern EquipItemByName dependency.
    PickupContainerItem(best.bag,best.index);
    if not CursorHasItem() then
        WOW_INTERNAL_EQUIPMENT_RESULT='pickup_failed|'..best.id; return;
    end
    if not CursorCanGoInSlot(best.slot) then
        ClearCursor(); WOW_INTERNAL_EQUIPMENT_RESULT='client_rejected_slot|'..best.id; return;
    end
    PickupInventoryItem(best.slot);
    -- Return displaced gear (or the rejected candidate) to its reserved bag slot.
    if CursorHasItem() then PickupContainerItem(best.bag,best.index) end;
    if CursorHasItem() then ClearCursor() end;
    session.pending=best;
    WOW_INTERNAL_EQUIPMENT_RESULT='issued|'..best.id..'|'..best.slot..'|'..best.before..'|'..best.after;
else
    WOW_INTERNAL_EQUIPMENT_RESULT='scan_complete|considered='..considered..'|approved=0|reason=no_proven_clear_upgrade';
end
)LUA";
        return script.str();
    }
}
