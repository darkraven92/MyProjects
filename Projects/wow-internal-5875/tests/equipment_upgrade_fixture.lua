-- Exercise the actual generated runtime Lua, including equip revalidation.
local script=io.read('*a'); local execute=assert(loadstring(script));
local items,bags,equipped,commands,classToken,level,combat,deliver,lines,cursor,clientAllows,inventoryLocked;
local function reset()
    items={
        [100]={kind='Armor',sub='Leather',loc='INVTYPE_CHEST',minimum=1,
               lines={'Old','Chest','Leather','50 Armor','Durability 20 / 20'},name='Old'},
        [200]={kind='Armor',sub='Leather',loc='INVTYPE_CHEST',minimum=1,
               lines={'New','Chest','Leather','100 Armor','Durability 20 / 20'},name='New'}};
    bags={200}; equipped={[5]=100}; commands={}; classToken='WARRIOR'; level=5;
    combat=false; deliver=false; lines={};
    cursor=nil; clientAllows=true; inventoryLocked=false;
    WOW_INTERNAL_EQUIPMENT_TICK=0;
    WOW_INTERNAL_EQUIPMENT_SESSION=nil; WOW_INTERNAL_EQUIPMENT_SCAN=nil;
    MerchantFrame=nil; QuestFrame=nil; GossipFrame=nil; TrainerFrame=nil; LootFrame=nil;
end
function UnitClass() return classToken,classToken end
function UnitLevel() return level end
function UnitAffectingCombat() return combat end
function GetContainerNumSlots(bag) if bag==0 then return table.getn(bags) end; return 0 end
function GetContainerItemLink(bag,slot) if bag==0 and bags[slot] then return 'item:'..bags[slot] end end
function GetContainerItemInfo(bag,slot)
    local item=bag==0 and items[bags[slot]];
    if item then return 'texture',1,item.locked end
end
function GetInventoryItemLink(_,slot) if equipped[slot] then return 'item:'..equipped[slot] end end
function GetItemInfo(link)
    local _,_,id=string.find(link,'item:(%d+)'); local item=items[tonumber(id)];
    if item then return item.name,link,1,5,item.minimum,item.kind,item.sub,1,item.loc,'texture' end
end
function CreateFrame()
    return {ClearLines=function() lines={} end, SetOwner=function() end,
        SetBagItem=function(_,bag,slot) assert(bag==0); lines=items[bags[slot]].lines end,
        SetInventoryItem=function(_,_,slot) lines=items[equipped[slot]].lines; return true end,
        NumLines=function() return table.getn(lines) end, Hide=function() end};
end
function getglobal(name)
    local _,_,i=string.find(name,'TextLeft(%d+)');
    if i then return {GetText=function() return lines[tonumber(i)] end} end
end
function CursorHasItem() return cursor~=nil end
function CursorCanGoInSlot() return clientAllows end
function IsInventoryItemLocked() return inventoryLocked end
function ClearCursor() if cursor then bags[1]=cursor; cursor=nil end end
function PickupContainerItem(bag,index)
    assert(bag==0); cursor,bags[index]=bags[index],cursor;
end
function PickupInventoryItem(slot)
    assert(cursor); table.insert(commands,{cursor,slot});
    if deliver then cursor,equipped[slot]=equipped[slot],cursor end;
end
local passed=0;
local function test(name,body)
    reset(); body(); passed=passed+1; print('PASS '..name);
end
local function rejected() execute(); assert(table.getn(commands)==0,WOW_INTERNAL_EQUIPMENT_RESULT) end
test('clear armor upgrade issued then verified',function()
    deliver=true; execute(); assert(string.find(WOW_INTERNAL_EQUIPMENT_RESULT,'^issued|200|5|'));
    execute(); assert(WOW_INTERNAL_EQUIPMENT_RESULT=='verified|200|5');
    assert(table.getn(commands)==1);
    assert(not cursor and bags[1]==100); -- displaced equipment preserved
end)
test('command is not verification',function()
    execute(); execute(); assert(WOW_INTERNAL_EQUIPMENT_RESULT=='pending');
    for i=1,20 do execute() end;
    assert(table.getn(commands)==1); assert(not WOW_INTERNAL_EQUIPMENT_SESSION.pending);
    for i=1,30 do execute() end; assert(table.getn(commands)==1);
end)
test('verification deadline expires across busy ownership',function()
    execute(); WOW_INTERNAL_EQUIPMENT_TICK=41; execute();
    assert(WOW_INTERNAL_EQUIPMENT_RESULT=='failed_verification|200');
    assert(table.getn(commands)==1);
end)
test('wrong class restriction',function() table.insert(items[200].lines,'Classes: Mage'); rejected() end)
test('unknown class',function() classToken='UNKNOWN'; rejected() end)
test('level requirement',function() items[200].minimum=10; rejected() end)
test('tooltip level requirement',function() table.insert(items[200].lines,'Requires Level 10'); rejected() end)
test('unknown proficiency',function() items[200].sub='Mail'; rejected() end)
test('empty slot is not proficiency evidence',function() equipped[5]=nil; rejected() end)
test('sidegrade',function() items[200].lines[4]='51 Armor'; rejected() end)
test('stat tradeoff rejected',function() table.insert(items[100].lines,'+1 Stamina'); rejected() end)
test('unique rejected',function() table.insert(items[200].lines,'Unique'); rejected() end)
test('unhandled binding confirmation rejected',function() table.insert(items[200].lines,'Binds when equipped'); rejected() end)
test('unknown effects rejected',function() table.insert(items[200].lines,'Equip: mysterious effect'); rejected() end)
test('broken rejected',function() items[200].lines[5]='Durability 0 / 20'; rejected() end)
test('combat blocks command',function() combat=true; rejected() end)
test('merchant ownership blocks command',function() MerchantFrame={IsVisible=function() return true end}; rejected() end)
test('locked rejected',function() items[200].locked=true; rejected() end)
test('locked equipment rejected',function() inventoryLocked=true; rejected() end)
test('cursor owner preserved',function() cursor=300; rejected(); assert(cursor==300) end)
test('client rejects slot safely',function()
    clientAllows=false; rejected(); assert(bags[1]==200 and not cursor);
    assert(WOW_INTERNAL_EQUIPMENT_RESULT=='client_rejected_slot|200');
end)
test('duplicate id ambiguous equip rejected',function() bags={200,200}; rejected() end)
test('cached inventory avoids re-equipping',function()
    items[200].lines[4]='51 Armor'; rejected();
    items[200].lines[4]='100 Armor'; rejected(); -- unchanged inventory fingerprint
end)
test('inventory change causes reevaluation',function()
    bags={}; rejected(); bags={200}; execute(); assert(table.getn(commands)==1);
end)
local function weapons()
    equipped={[16]=100};
    for _,id in ipairs({100,200}) do
        local it=items[id]; it.kind='Weapon'; it.sub='Axes'; it.loc='INVTYPE_2HWEAPON';
        it.lines={it.name,'Two-Hand','Axes','6 - 10 Damage','Speed 2.00','(4.0 damage per second)','Durability 20 / 20'};
    end
    items[200].lines[6]='(5.0 damage per second)';
end
test('weapon dps upgrade',function() weapons(); execute(); assert(table.getn(commands)==1) end)
test('invalid twohand offhand combination',function() weapons(); equipped[17]=300; rejected() end)
test('weapon loadout change rejected',function() weapons(); items[200].loc='INVTYPE_WEAPON'; rejected() end)
test('large weapon speed tradeoff rejected',function() weapons(); items[200].lines[5]='Speed 1.50'; rejected() end)
test('caster does not equip dps-only upgrade',function() weapons(); classToken='PRIEST'; rejected() end)
local function reason(expected)
    assert(string.find(WOW_INTERNAL_EQUIPMENT_DIAGNOSTICS,expected,1,true),WOW_INTERNAL_EQUIPMENT_DIAGNOSTICS);
end
test('empty compatible armor slot uses equipped subtype evidence',function()
    items[100].loc='INVTYPE_LEGS'; items[100].lines[2]='Legs'; equipped={[7]=100};
    items[200].lines[4]='7 Armor'; deliver=true;
    execute(); reason('reason={empty_proven_armor_slot}'); reason('current=0');
    assert(table.getn(commands)==1 and equipped[5]==200 and equipped[7]==100);
    execute(); assert(WOW_INTERNAL_EQUIPMENT_RESULT=='verified|200|5');
end)
test('empty armor without proficiency explains rejection',function()
    equipped={}; rejected(); reason('unknown_armor_proficiency');
end)
test('empty armor still requires client slot approval',function()
    items[100].loc='INVTYPE_LEGS'; items[100].lines[2]='Legs'; equipped={[7]=100};
    clientAllows=false; rejected(); assert(bags[1]==200 and not cursor);
    assert(WOW_INTERNAL_EQUIPMENT_RESULT=='client_rejected_slot|200');
end)
test('different armor subtype requires independently equipped proof',function()
    items[100].sub='Cloth'; items[100].lines[3]='Cloth';
    items[300]={kind='Armor',sub='Leather',loc='INVTYPE_LEGS',minimum=1,
        lines={'Proof','Legs','Leather','20 Armor','Durability 20 / 20'},name='Proof'};
    equipped[7]=300; execute(); assert(table.getn(commands)==1);
end)
test('diagnostics include scores and stat ordering',function()
    execute(); reason('candidateScore=2'); reason('currentScore=1'); reason('delta=1');
    reason('stats=0,0,0,0,0,100,0,0'); reason('decision=approve'); reason('bag=0 index=1');
end)
test('marginal and downgrade reasons',function()
    items[200].lines[4]='51 Armor'; rejected(); reason('insufficient_clear_gain');
    WOW_INTERNAL_EQUIPMENT_SESSION.signature=''; items[200].lines[4]='40 Armor';
    rejected(); reason('stat_tradeoff');
end)
test('unrecognized restriction is observable',function()
    table.insert(items[200].lines,'Classes: Mage'); rejected(); reason('unknown_tooltip:Classes: Mage');
end)
test('required level is observable',function()
    items[200].minimum=10; rejected(); reason('required_level'); reason('requiredLevel=10');
end)
test('non equipment counted without claiming gear compatibility',function()
    items[200].kind='Trade Goods'; rejected(); reason('not_equipment');
end)
test('trainer and loot frames block equipment',function()
    TrainerFrame={IsVisible=function() return true end}; rejected();
    TrainerFrame=nil; LootFrame={IsVisible=function() return true end}; rejected();
end)
test('unchanged inventory emits no candidate spam',function()
    items[200].lines[4]='51 Armor'; rejected(); reason('insufficient_clear_gain');
    rejected(); assert(WOW_INTERNAL_EQUIPMENT_DIAGNOSTICS=='');
end)
print('equipment Lua '..passed..'/'..passed..' PASS');
