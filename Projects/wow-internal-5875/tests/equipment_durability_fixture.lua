local script=io.read('*a'); local execute=assert(loadstring(script));
local text,hasItem,cached,link,texture;
function GetInventoryItemLink(_,slot) if slot==5 and link then return 'item:100' end end
function GetInventoryItemTexture(_,slot) if slot==5 and texture then return 'texture' end end
function GetItemInfo() if cached then return 'Item' end end
function CreateFrame()
    return {SetOwner=function() end,ClearLines=function() end,Hide=function() end,
        SetInventoryItem=function() return hasItem end,NumLines=function() return 3 end};
end
function getglobal(name)
    return {GetText=function() if string.find(name,'3$') then return text end; return 'Item' end};
end
local count=0;
local function test(name,body)
    text='Durability 5 / 20'; hasItem=true; cached=true; link=true; texture=true;
    GetInventoryItemDurability=nil; DURABILITY_TEMPLATE='Durability %d / %d';
    WOW_INTERNAL_DURABILITY_SCAN=nil;
    body(); count=count+1; print('PASS '..name);
end
test('native API',function()
    GetInventoryItemDurability=function() return 8,20 end;
    execute(); assert(DURABILITY_RESULT[1]==40 and DURABILITY_RESULT[2]==1 and DURABILITY_RESULT[3]==1);
end)
test('Vanilla tooltip fallback',function()
    execute(); assert(DURABILITY_RESULT[1]==25 and DURABILITY_RESULT[2]==1 and DURABILITY_RESULT[3]==1);
end)
test('repair verified by new live read',function()
    execute(); assert(DURABILITY_RESULT[1]==25);
    text='Durability 20 / 20'; execute(); assert(DURABILITY_RESULT[1]==100 and DURABILITY_RESULT[3]==1);
end)
test('broken equipment',function() text='Durability 0 / 20'; execute(); assert(DURABILITY_RESULT[1]==0) end)
test('cache miss unknown',function() cached=false; execute(); assert(DURABILITY_RESULT[3]==0) end)
test('tooltip unavailable unknown',function() hasItem=false; execute(); assert(DURABILITY_RESULT[3]==0) end)
test('localization unknown',function() DURABILITY_TEMPLATE='unknown'; execute(); assert(DURABILITY_RESULT[3]==0) end)
test('occupied uncached slot unknown',function() link=false; execute(); assert(DURABILITY_RESULT[3]==0) end)
print('durability Lua '..count..'/'..count..' PASS');
