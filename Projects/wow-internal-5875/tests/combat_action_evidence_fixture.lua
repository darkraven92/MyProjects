local script=io.read('*a')
local active,uses,stops,starts,clears
local function reset()
 active=false; uses=0; stops=0; starts=0; clears=0
 UIParent={IsVisible=function() return true end}
 CastingBarFrame={}
 UnitIsDeadOrGhost=function() return nil end
 UnitExists=function() return true end
 UnitIsDead=function() return nil end
 IsAttackAction=function(i) return i==12 end
 IsCurrentAction=function(i) assert(i==12); return active and 1 or nil end
 GetActionCooldown=function() return 0,0 end
 GetTime=function() return 10 end
 SpellIsTargeting=function() return nil end
 EnumerateFrames=function() return nil end
 getglobal=function(k) return _G[k] end
 UseAction=function(i) assert(i==12); uses=uses+1; active=not active end
 AttackTarget=function() starts=starts+1 end
 StopAttack=function() stops=stops+1 end
 ClearTarget=function() clears=clears+1 end
 for _,k in ipairs({'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
  'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame',
  'StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}) do _G[k]=nil end
end
local function check(expected,mode)
 local s=script
 if mode then s=string.gsub(s,"local mode='probe'","local mode='"..mode.."'") end
 assert(loadstring(s))()
 assert(WOW_INTERNAL_AUTOATTACK_RESULT==expected,tostring(WOW_INTERNAL_AUTOATTACK_RESULT)..' ~= '..expected)
end
reset(); check('12|0'); assert(uses==0)
reset(); active=true; check('12|1')
reset(); check('issued','start'); assert(uses==1 and active)
reset(); active=true; check('issued','start'); assert(uses==0 and active) -- never toggle a healthy latch off
reset(); active=true; check('issued','refresh'); assert(uses==1 and not active)
check('issued','start'); assert(uses==2 and active)
reset(); check('issued','refresh'); assert(uses==0 and not active)
-- Model delayed client latch publication: never toggle twice on stale state.
reset(); active=true
local queued=0
UseAction=function(i) assert(i==12); uses=uses+1; queued=queued+1 end
check('issued','refresh'); assert(uses==1 and queued==1 and active)
active=false; queued=0
check('issued','start'); assert(uses==2 and queued==1)
reset(); active=true; check('issued','abandon'); assert(uses==1 and not active and clears==1)
reset(); ClearTarget=nil; check('unsupported_release','abandon'); assert(uses==0)
for _,mode in ipairs({'probe','start','refresh','abandon'}) do
 reset(); CastingBarFrame.casting=1; check('waiting_cast_or_gcd',mode); assert(uses==0 and clears==0)
 reset(); CastingBarFrame.channeling=1; check('waiting_cast_or_gcd',mode); assert(uses==0)
 reset(); GetActionCooldown=function() return 9,1.5 end; check('waiting_cast_or_gcd',mode); assert(uses==0)
 reset(); SpellIsTargeting=function() return 1 end; check('blocked_spell_targeting',mode); assert(uses==0)
 reset(); UnitIsDeadOrGhost=function() return 1 end; check('blocked_player_dead',mode); assert(uses==0)
 reset(); CastingBarFrame=nil; check('unknown_casting_frame',mode); assert(uses==0)
 for _,k in ipairs({'QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame','TalentFrame','StaticPopup1'}) do
  reset(); _G[k]={IsVisible=function() return true end}; check('blocked_modal_frame',mode); assert(uses==0)
 end
end
reset(); UnitExists=function() return nil end; check('12|0'); check('blocked_target_invalid','start'); assert(uses==0)
reset(); IsAttackAction=function() return nil end; check('0|-1') -- unknown, not inactive
check('issued','start'); assert(starts==1)
check('issued','refresh'); assert(stops==1 and starts==1)
StopAttack=nil; check('unsupported_refresh','refresh'); assert(starts==1)
reset()
local f={IsVisible=function() return true end,GetObjectType=function() return 'EditBox' end}
EnumerateFrames=function(prev) if not prev then return f end end
check('blocked_editbox','start'); assert(uses==0)
reset(); GetActionCooldown=function() error('unknown') end; check('unknown_script_error','start'); assert(uses==0)
reset(); IsAttackAction=nil; IsCurrentAction=nil; GetActionCooldown=nil; GetTime=nil
check('ready','selection_probe'); assert(uses==0 and clears==0)
reset(); CastingBarFrame.casting=1; check('waiting_cast','selection_probe'); assert(clears==0)
reset(); _G.MerchantFrame={IsVisible=function() return true end}
check('blocked_modal_frame','selection_probe'); assert(clears==0)
reset(); check('issued','start'); assert(uses==1 and clears==0)
reset(); GetActionCooldown=function() error('unavailable') end
check('unknown_script_error','start'); assert(uses==0)
check('issued','bootstrap'); assert(uses==1 and active)
reset(); active=true; GetActionCooldown=nil; GetTime=nil
check('issued','bootstrap'); assert(uses==0 and active) -- never toggle active Attack off
reset(); IsAttackAction=function() return nil end
check('issued','bootstrap'); assert(starts==1 and stops==0) -- no slot starts, never stops
reset(); CastingBarFrame.casting=1; check('waiting_cast_or_gcd','bootstrap'); assert(uses==0)
reset(); EnumerateFrames=nil; check('issued','bootstrap'); assert(uses==1)
reset(); UIParent=nil; check('unknown_ui_parent','probe')
reset(); UnitIsDeadOrGhost=nil; check('unknown_life_api','probe')
reset(); SpellIsTargeting=nil; check('unknown_spell_targeting','probe')
reset(); EnumerateFrames=nil; check('unknown_enumerate_frames','probe')
reset(); IsAttackAction=nil; check('unknown_attack_action_api','probe')
reset(); GetActionCooldown=nil; check('unknown_attack_cooldown_api','probe')
reset(); local bad={GetObjectType=function() return 'Frame' end}
EnumerateFrames=function(prev) if not prev then return bad end end
check('unknown_frame_visibility_api','probe')
bad.IsVisible=function() return true end
bad.GetObjectType=nil; check('unknown_frame_object_type','probe')
bad.GetObjectType=function() return 'Frame' end
bad.IsKeyboardEnabled=nil; check('unknown_frame_keyboard_api','probe')
bad.IsKeyboardEnabled=function() return true end
bad.GetScript=nil; check('unknown_frame_script_api','probe')
bad.GetScript=function() return function() end end
check('blocked_keyboard_handler','probe')
reset(); local loop={IsVisible=function() return false end,GetObjectType=function() return 'Frame' end}
EnumerateFrames=function() return loop end
check('unknown_frame_cycle','probe')
-- Full traversal remains bounded and checks every frame through the bound.
local frames={}
for i=1,16385 do frames[i]={IsVisible=function() return false end,GetObjectType=function() return 'Frame' end} end
local index={}
for i,f in ipairs(frames) do index[f]=i end
EnumerateFrames=function(prev) return frames[prev and index[prev]+1 or 1] end
for _,mode in ipairs({'probe','start','refresh','abandon','selection_probe'}) do
 check('unknown_frame_iteration_limit',mode); assert(uses==0 and clears==0)
end
frames[16385]=nil; check('12|0','probe')
frames[16384].IsVisible=function() return true end
frames[16384].GetObjectType=function() return 'EditBox' end
check('blocked_editbox','start'); assert(uses==0)
frames[16384].IsVisible=function() return false end
frames[5001]=nil; check('12|0','probe') -- old 4096-limit failure, complete now
frames[5000].IsVisible=function() return true end
frames[5000].IsKeyboardEnabled=function() return true end
frames[5000].GetScript=function() return function() end end
for _,mode in ipairs({'probe','start','refresh','abandon','selection_probe'}) do
 check('blocked_keyboard_handler',mode); assert(uses==0 and clears==0)
end
-- A cycle or limit never permits repair/release or a selection clear.
for _,mode in ipairs({'start','refresh','abandon','selection_probe'}) do
 EnumerateFrames=function() return loop end
 check('unknown_frame_cycle',mode); assert(uses==0 and clears==0)
end
print('Combat action evidence Lua: passed')
