local script=io.read('*a')
local function check(expected,setup)
 WOW_INTERNAL_AUTOATTACK_RESULT='12|1'
 IsAttackAction=function(i) assert(i<=120); return i==12 end
 IsCurrentAction=function(i) assert(i==12); return nil end
 local forbidden=function() error('readback must not issue input') end
 UseAction=forbidden; AttackTarget=forbidden; StopAttack=forbidden; ClearTarget=forbidden
 -- Readback must remain available when the full UI scan is unavailable.
 UIParent=nil; EnumerateFrames=nil; CastingBarFrame=nil; GetActionCooldown=nil
 setup()
 assert(loadstring(script))()
 assert(WOW_INTERNAL_AUTOATTACK_RESULT==expected,tostring(WOW_INTERNAL_AUTOATTACK_RESULT))
end
check('12|0',function() end)
check('12|1',function() IsCurrentAction=function() return 1 end end)
check('120|0',function() IsAttackAction=function(i) return i==120 end; IsCurrentAction=function() return false end end)
check('0|-1',function() IsAttackAction=function() return nil end end)
check('unknown_attack_action_api',function() IsAttackAction=nil end)
check('unknown_attack_action_api',function() IsCurrentAction=nil end)
check('unknown_script_error',function() IsAttackAction=function() error('read') end end)
check('unknown_script_error',function() IsCurrentAction=function() error('read') end end)
print('Combat independent Attack readback Lua: passed')
