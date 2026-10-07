local script=io.read('*a')
local count=0
local function reset()
 UIParent={IsVisible=function() return true end}
 UnitAffectingCombat=function() return nil end
 UnitIsDeadOrGhost=function() return 1 end
 GetBindingAction=function(k) assert(k=='F12'); return '' end
 EnumerateFrames=function() return nil end
 getglobal=function(k) return _G[k] end
 for _,n in ipairs({'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
 'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame',
 'StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}) do _G[n]=nil end
end
local function check(expected)
 assert(loadstring(script))()
 assert(WOW_INTERNAL_AFK_INPUT==expected,tostring(WOW_INTERNAL_AFK_INPUT)..' ~= '..expected)
 count=count+1
end
reset(); check('ready') -- dead or ghost, no UI/input conflict
reset(); UnitIsDeadOrGhost=function() return nil end; check('death_state_changed')
reset(); UnitIsDeadOrGhost=nil; check('api_missing')
reset(); UnitAffectingCombat=function() return 1 end; check('world_dead_or_combat')
reset(); UIParent.IsVisible=function() return nil end; check('world_dead_or_combat')
reset(); GetBindingAction=function() return 'ACTIONBUTTON1' end; check('key_bound')
reset(); GetBindingAction=function() return nil end; check('binding_unknown')
for _,name in ipairs({'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
 'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame',
 'StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}) do
 reset(); _G[name]={IsVisible=function() return true end}; check('dialog_visible')
end
reset()
local frame={IsVisible=function() return true end,GetObjectType=function() return 'Frame' end,
 IsKeyboardEnabled=function() return true end,GetScript=function() return function() end end}
EnumerateFrames=function(last) if not last then return frame end end
check('keyboard_handler_active')
frame.GetObjectType=function() return 'EditBox' end; check('edit_box_visible')
reset(); EnumerateFrames=function() error('unavailable') end; check('guard_error')
print('AFK dead/ghost Lua: '..count..' passed')
