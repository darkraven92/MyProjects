local source = io.read('*a')
local make = assert(loadstring(source))
local current
local tooltipPrimed = false

tt = {
    ClearLines = function() end,
    SetBagItem = function() tooltipPrimed = true end,
}
local saleReason = make()
function GetContainerItemInfo()
    return 'texture', 1, false, current.bagQuality
end
function GetItemInfo(key)
    if current.needsTooltip and not tooltipPrimed then return nil end
    local byId = type(key) == 'number'
    if (byId and not current.idReady) or
        (not byId and not current.linkReady) then return nil end
    return 'Item', 'item:12345:0', current.quality, nil, nil, current.itemType
end
function classifyConsumable()
    return current.food or false, current.drink or false,
        false, false, false, false, current.quest or false
end

local cases = {
    {name='link-ready poor junk', linkReady=true, quality=0,
     bagQuality=0, itemType='Miscellaneous', expected='sell'},
    {name='tooltip primes cache', linkReady=true, needsTooltip=true,
     quality=0, bagQuality=0, itemType='Miscellaneous', expected='sell'},
    {name='numeric ID fallback', idReady=true, quality=0,
     bagQuality=0, itemType='Miscellaneous', expected='sell'},
    {name='bag quality corroborates type', idReady=true,
     bagQuality=0, itemType='Miscellaneous', expected='sell'},
    {name='conflicting qualities fail closed', linkReady=true, quality=1,
     bagQuality=0, itemType='Armor', expected='classification_unknown'},
    {name='metadata absent fails closed', bagQuality=0,
     expected='classification_unknown'},
    {name='uncached quest stays protected', quest=true,
     expected='quest_item'},
    {name='uncached food stays protected', food=true,
     expected='food'},
    {name='uncommon equipment preserved', linkReady=true, quality=2,
     bagQuality=2, itemType='Armor', expected='quality_uncommon_or_higher'},
}
for _, case in ipairs(cases) do
    current = case
    tooltipPrimed = false
    local reason = saleReason(0, 1, '|Hitem:12345:0|h[Item]|h', 1, false)
    assert(reason == case.expected, case.name .. ': ' .. tostring(reason))
end
print('Vendor metadata Lua 5.1: 9/9 PASS')
