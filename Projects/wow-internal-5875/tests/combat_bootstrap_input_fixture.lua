local script=io.read('*a')
local function reset()
 UIParent={IsVisible=function() return true end}
 UnitExists=function() return true end
 UnitIsDeadOrGhost=function() return nil end
 CastingBarFrame={}
 SpellIsTargeting=function() return nil end
 getglobal=function(k) return _G[k] end
 EnumerateFrames=nil -- Bootstrap deliberately does not depend on this API.
 IsAttackAction=nil
 IsCurrentAction=nil
 GetActionCooldown=nil
 GetTime=nil
 for _,k in ipairs({'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame',
  'LootFrame','TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame',
  'GameMenuFrame','StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}) do
  _G[k]=nil
 end
end
local function check(expected)
 assert(loadstring(script))()
 assert(WOW_INTERNAL_AUTOATTACK_RESULT==expected,
  tostring(WOW_INTERNAL_AUTOATTACK_RESULT)..' ~= '..expected)
end
reset(); check('ready')
reset(); UIParent=nil; check('unknown_ui_parent')
reset(); UnitExists=nil; check('unknown_life_api')
reset(); UnitIsDeadOrGhost=function() return true end; check('blocked_player_dead')
reset(); CastingBarFrame=nil; check('unknown_casting_frame')
reset(); SpellIsTargeting=nil; check('unknown_spell_targeting')
reset(); getglobal=nil; check('unknown_modal_lookup_api')
reset(); MerchantFrame={}; check('unknown_frame_visibility_api')
reset(); MerchantFrame={IsVisible=function() return true end}; check('blocked_modal_frame')
reset(); SpellIsTargeting=function() return true end; check('blocked_spell_targeting')
reset(); CastingBarFrame.casting=1; check('waiting_cast')
reset(); CastingBarFrame.channeling=1; check('waiting_cast')
reset(); UIParent.IsVisible=function() error('bad frame') end; check('unknown_script_error')
print('Combat bootstrap input Lua: passed')
