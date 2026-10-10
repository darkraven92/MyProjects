local script=io.read('*a')
local cases=0
local function run(expected,setup)
 WOW_INTERNAL_AUTOATTACK_RESULT='active' -- old result must never leak
 IsAttackAction=function(i) return i==7 end
 IsCurrentAction=function() return false end
 UseAction=function() error('observation issued input') end
 AttackTarget=UseAction; TargetUnit=UseAction; ClearTarget=UseAction
 setup()
 assert(loadstring(script))()
 assert(WOW_INTERNAL_AUTOATTACK_RESULT==expected, tostring(WOW_INTERNAL_AUTOATTACK_RESULT))
 cases=cases+1
end
run('active',function() IsCurrentAction=function() return 1 end end)
run('inactive',function() end)
run('inactive',function() IsCurrentAction=function() return nil end end)
run('unknown',function() IsAttackAction=nil end)
run('unknown',function() IsCurrentAction=nil end)
run('unknown',function() IsAttackAction=function() return false end end)
run('unknown',function() IsAttackAction=function() error('unavailable') end end)
run('unknown',function() IsCurrentAction=function() error('unavailable') end end)
run('active',function() IsAttackAction=function(i) assert(i<=120); return i==120 end; IsCurrentAction=function() return true end end)
run('unknown',function() IsAttackAction=function(i) assert(i<=120); return i==121 end end)
print('Living attack read-only Lua 5.1: '..cases..'/'..cases..' PASS')
