-- Runs the exact C++-generated runtime script with synthetic client evidence.
local script=io.read('*a')
local known,offers,commands,frame,class,level,money,combat,dead,profession,actor,deliver,pet
local function reset()
 known={}; commands={}; frame=true; class='MAGE'; level=2; money=100; combat=false; dead=false
 profession=false; actor='Test Trainer'; deliver=false; pet=false
 offers={{'Test Ability','Rank 1','available',10,1},
         {'Test Ability','Rank 2','unavailable',20,2},{'Second Ability','','available',10,1}}
end
function GetSpellName(i) if known[i] then return known[i][1],known[i][2] end end
function UnitClass() return class,class end
function UnitLevel() return level end
function UnitAffectingCombat() return combat end
function UnitIsDeadOrGhost() return dead end
function UnitCanAttack() return false end
function UnitName() return actor end
function IsTradeskillTrainer() return profession end
function IsTrainerServiceLearnSpell() return true,pet end
function GetMoney() return money end
function GetNumTrainerServices() return table.getn(offers) end
function GetTrainerServiceInfo(i) return offers[i][1],offers[i][2],offers[i][3],true end
function GetTrainerServiceCost(i) return offers[i][4],0,0 end
function GetTrainerServiceLevelReq(i) return offers[i][5] end
function BuyTrainerService(i)
 table.insert(commands,i)
 if deliver then
  table.insert(known,{offers[i][1],offers[i][2]}); offers[i][3]='used'; money=money-offers[i][4]
  if i==1 then offers[2][3]='available' end
 end
end
ClassTrainerFrame={IsVisible=function() return frame end}
local tests=0
local function check(expected,pending)
 local s=script
 if pending then s=string.gsub(s,'local pending=0;', 'local pending='..pending..';') end
 assert(loadstring(s))()
 assert(string.sub(WOW_INTERNAL_TRAINER_RESULT,1,string.len(expected))==expected,WOW_INTERNAL_TRAINER_RESULT)
 tests=tests+1
end
reset(); check('issued|50001'); assert(table.getn(commands)==1)
check('pending',50001); assert(table.getn(commands)==1) -- issue alone never confirms
known={{'Test Ability','Rank 1'}}; check('confirmed',50001)
reset(); frame=false; check('frame_missing'); assert(table.getn(commands)==0)
reset(); class='ROGUE'; check('class_changed')
reset(); profession=true; check('wrong_trainer')
reset(); actor='Different Trainer'; check('actor_changed')
reset(); combat=true; check('unsafe')
reset(); dead=true; check('unsafe')
reset(); money=59; check('deferred_insufficient_funds'); assert(table.getn(commands)==0)
reset(); known={{'Test Ability','Rank 2'}}; check('issued|50003') -- never downgrade
reset(); offers[1][3]='used'; offers[2][3]='available'; offers[3][3]='used'; check('unresolved_services')
reset(); offers[3][3]='used'; offers[2][3]='available'; known={{'Test Ability','Rank 1'}}; check('issued|50002')
reset(); offers[1][1]='Unknown Ability'; offers[3][3]='used'; check('unresolved_services')
reset(); pet=true; check('unresolved_services')
reset(); level=1; offers[1][3]='used'; offers[2][3]='available'; offers[3][3]='used'; check('unresolved_services')
reset(); deliver=true
check('issued|50001'); check('confirmed',50001)
check('issued|50002'); check('confirmed',50002)
check('issued|50003'); check('confirmed',50003)
check('complete|'); assert(table.getn(commands)==3)
print('CLASS TRAINER LUA: '..tests..' passed')
