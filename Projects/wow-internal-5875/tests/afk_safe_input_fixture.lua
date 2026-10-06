local script=io.read('*a')
local count=0
local function reset()
 UIParent={IsVisible=function() return true end}
 UnitAffectingCombat=function() return nil end
 UnitIsDeadOrGhost=function() return nil end
 GetBindingAction=function(k) assert(k=='F12'); return '' end
 EnumerateFrames=function() return nil end
 getglobal=function(k) return _G[k] end
 GossipFrame=nil; QuestFrame=nil; MerchantFrame=nil; ClassTrainerFrame=nil
 LootFrame=nil; TalentFrame=nil; TradeFrame=nil; MailFrame=nil
 ItemTextFrame=nil; TaxiFrame=nil; GameMenuFrame=nil; StaticPopup1=nil
end
local function check(expected)
 assert(loadstring(script))()
 assert(WOW_INTERNAL_AFK_INPUT==expected, tostring(WOW_INTERNAL_AFK_INPUT)..' ~= '..expected)
 count=count+1
end
reset(); check('ready')
reset(); UnitAffectingCombat=function() return 1 end; check('world_dead_or_combat')
reset(); UnitIsDeadOrGhost=function() return 1 end; check('world_dead_or_combat')
reset(); UIParent.IsVisible=function() return nil end; check('world_dead_or_combat')
reset(); GetBindingAction=function() return 'ACTIONBUTTON1' end; check('key_bound')
reset(); GetBindingAction=function() return nil end; check('binding_unknown')
reset(); GetBindingAction=nil; check('api_missing')
for _,name in ipairs({'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
    'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame','StaticPopup1'}) do
 reset(); _G[name]={IsVisible=function() return true end}; check('dialog_visible')
end
reset()
local frame={IsVisible=function() return true end,GetObjectType=function() return 'EditBox' end}
EnumerateFrames=function(last) if not last then return frame end end
check('edit_box_visible')
frame.GetObjectType=function() return 'Frame' end
frame.IsKeyboardEnabled=function() return true end
frame.GetScript=function() return function() end end
check('keyboard_handler_active')
frame.GetScript=function() return nil end
check('ready')
frame.IsKeyboardEnabled=nil; check('keyboard_state_unknown')
reset(); EnumerateFrames=function() error('api') end; check('guard_error')
print('AFK safe input Lua: '..count..' passed')
