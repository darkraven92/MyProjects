local source=io.read('*a')
local cases={
    {'sell',0,'Miscellaneous','sell'},
    {'sell',1,'Miscellaneous','questing_conservative_preservation'},
    {'sell',0,'Armor','questing_conservative_preservation'},
    {'sell',0,'Weapon','questing_conservative_preservation'},
    {'sell',-1,'Miscellaneous','questing_conservative_preservation'},
    {'quest_item',0,'Miscellaneous','quest_item'},
    {'food',0,'Miscellaneous','food'},
    {'drink',0,'Miscellaneous','drink'},
    {'locked',0,'Miscellaneous','locked'},
}
for _,case in ipairs(cases) do
    local environment={case=case}
    local chunk=assert(loadstring('local saleReason=function() return case[1],123,case[2],case[3] end; '..source..' return saleReason;'))
    setfenv(chunk,environment)
    local reason,id,quality,itemtype=chunk()()
    assert(reason==case[4] and id==123 and quality==case[2] and itemtype==case[3])
end
print('Quest maintenance Lua 5.1 sale restrictions: 9/9 PASS')
