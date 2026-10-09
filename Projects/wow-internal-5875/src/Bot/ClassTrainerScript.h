#pragma once
#include "ClassTrainerCatalogue.h"
#include <set>

namespace Bot
{
    inline std::string TrainerLuaString(const std::string& value)
    {
        std::string out="\"";
        for(unsigned char c:value)
        {
            if(c=='\\' || c=='\"') { out+='\\'; out+=static_cast<char>(c); }
            else if(c<32 || c==127) { out+='\\'; out+=char('0'+c/100); out+=char('0'+(c/10)%10); out+=char('0'+c%10); }
            else out+=static_cast<char>(c);
        }
        return out+'\"';
    }
    // Values are sent to the existing game-thread Lua bridge; no UI state is
    // cached across reads. Each purchase rechecks identity, price and spellbook.
    inline std::string TrainerScript(const ClassTrainerCatalogue& db, unsigned entry, unsigned classId,
        const std::string& classToken, const std::string& npcName, std::uint64_t reserve,
        unsigned pendingSpell, bool purchase, unsigned expectedSpell=0)
    {
        std::string out="local defs={}; local offers={}; ";
        for(const auto& [id,s]:db.spells)
            out+="defs["+std::to_string(id)+"]={"+TrainerLuaString(s.name)+","+TrainerLuaString(s.rankText)+","+
                std::to_string(s.chain)+","+std::to_string(s.rank)+"};";
        for(const auto& s:db.services)
            if(s.entry==entry && s.classId==classId)
                out+="table.insert(offers,{"+std::to_string(s.spell)+","+std::to_string(s.previous)+","+
                    std::to_string(s.required)+","+std::to_string(s.level)+"});";
        out+="local expectedClass="+TrainerLuaString(classToken)+"; local expectedNpc="+TrainerLuaString(npcName)+
             "; local reserve="+std::to_string(reserve)+"; local pending="+std::to_string(pendingSpell)+
             "; local purchase="+(purchase?std::string("true"):std::string("false"))+
             "; local expectedSpell="+std::to_string(expectedSpell)+";";
        out+=R"lua(
local function run()
 if not GetSpellName or not UnitClass or not UnitLevel or not UnitAffectingCombat or
    not UnitIsDeadOrGhost or UnitIsDeadOrGhost('player') or UnitAffectingCombat('player') then return 'unsafe' end
 local _,class=UnitClass('player'); if class~=expectedClass then return 'class_changed' end
 local known={}; local complete=false
 for i=1,1024 do
  local name,rank=GetSpellName(i,BOOKTYPE_SPELL or 'spell')
  if not name then complete=true; break end
  for id,d in pairs(defs) do if name==d[1] and (rank or '')==d[2] then known[id]=true end end
 end
 if not complete then return 'spellbook_unresolved' end
 local function has(id)
  if id==0 then return true end
  if known[id] then return true end
  local d=defs[id]; if not d then return false end
  for k in pairs(known) do local other=defs[k]
   if d[4]>0 and other[3]==d[3] and other[4]>=d[4] then return true end
  end
  return false
 end
 if pending>0 then return has(pending) and 'confirmed' or 'pending' end
 if not ClassTrainerFrame or not ClassTrainerFrame:IsVisible() then return 'frame_missing' end
 if not UnitName or UnitName('npc')~=expectedNpc or not UnitCanAttack or UnitCanAttack('player','npc') then return 'actor_changed' end
 if not IsTradeskillTrainer or IsTradeskillTrainer() then return 'wrong_trainer' end
 if not GetNumTrainerServices or not GetTrainerServiceInfo or not GetTrainerServiceCost or
    not GetTrainerServiceLevelReq or not IsTrainerServiceLearnSpell or not BuyTrainerService or not GetMoney then return 'api_missing' end
 local n=GetNumTrainerServices(); if not n or n<=0 or n>512 then return 'services_unresolved' end
 local budget=false; local unknown=false; local best=nil; local bestSpell=nil; local bestCost=nil
 for i=1,n do
  local name,rank,status,expanded=GetTrainerServiceInfo(i)
  if status=='header' and not expanded then return 'collapsed_services' end
  if status=='available' then
   local match=nil
   for _,o in ipairs(offers) do local d=defs[o[1]]
    if d and d[1]==name and d[2]==(rank or '') then
     if match and match[1]~=o[1] then return 'ambiguous_service' end
     match=o
    end
   end
   if not match then unknown=true
   elseif not has(match[1]) then
    local cost,cp1,cp2=GetTrainerServiceCost(i); local level=GetTrainerServiceLevelReq(i)
    local learn,pet=IsTrainerServiceLearnSpell(i)
    if not learn or pet or not cost or cost<0 or (cp1 or 0)~=0 or (cp2 or 0)~=0 or not level or
       level>UnitLevel('player') or match[4]>UnitLevel('player') or not has(match[2]) or not has(match[3]) then
     unknown=true
    elseif GetMoney()-cost<reserve then budget=true
    elseif not bestSpell or match[1]<bestSpell then best=i; bestSpell=match[1]; bestCost=cost end
   end
  end
 end
 if best then
  if purchase and expectedSpell>0 and bestSpell~=expectedSpell then return 'candidate_changed' end
  if purchase then BuyTrainerService(best) end
  return (purchase and 'issued|' or 'candidate|')..bestSpell..'|'..bestCost..'|'..n
 end
 if budget then return 'deferred_insufficient_funds' end
 if unknown then return 'unresolved_services' end
 return 'complete|'..n
end
WOW_INTERNAL_TRAINER_RESULT=run()
)lua";
        return out;
    }
}
