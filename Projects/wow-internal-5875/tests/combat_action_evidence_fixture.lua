local script=io.read('*a')
local active,uses,stops,starts
local function reset()
 active=false; uses=0; stops=0; starts=0
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
reset(); active=true; check('issued','refresh'); assert(uses==2 and active)
reset(); check('issued','refresh'); assert(uses==1 and active)
for _,mode in ipairs({'probe','start','refresh'}) do
 reset(); CastingBarFrame.casting=1; check('wait',mode); assert(uses==0)
 reset(); CastingBarFrame.channeling=1; check('wait',mode); assert(uses==0)
 reset(); GetActionCooldown=function() return 9,1.5 end; check('wait',mode); assert(uses==0)
 reset(); SpellIsTargeting=function() return 1 end; check('blocked',mode); assert(uses==0)
 reset(); UnitIsDeadOrGhost=function() return 1 end; check('unknown',mode); assert(uses==0)
 reset(); CastingBarFrame=nil; check('unknown',mode); assert(uses==0)
 for _,k in ipairs({'QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame','TalentFrame','StaticPopup1'}) do
  reset(); _G[k]={IsVisible=function() return true end}; check('blocked',mode); assert(uses==0)
 end
end
reset(); UnitExists=function() return nil end; check('12|0'); check('blocked','start'); assert(uses==0)
reset(); IsAttackAction=function() return nil end; check('0|-1') -- unknown, not inactive
check('issued','start'); assert(starts==1)
check('issued','refresh'); assert(stops==1 and starts==2)
StopAttack=nil; check('unsupported_refresh','refresh'); assert(starts==2)
reset()
local f={IsVisible=function() return true end,GetObjectType=function() return 'EditBox' end}
EnumerateFrames=function(prev) if not prev then return f end end
check('blocked','start'); assert(uses==0)
reset(); GetActionCooldown=function() error('unknown') end; check('unknown','start'); assert(uses==0)
print('Combat action evidence Lua: passed')
