#pragma once
#include <string>
namespace Bot
{
    // Vanilla 1.12.1 CastingBarFrame tracks SPELLCAST_*; no modern casting API.
    // A short live action cooldown is conservatively a wait, not proof of a cast.
    inline constexpr const char* CombatActionEvidenceScript=R"lua(
WOW_INTERNAL_AUTOATTACK_RESULT='unknown'
local mode='probe'
local function run()
 if not UIParent or not UIParent.IsVisible or not UIParent:IsVisible() or
    not UnitIsDeadOrGhost or UnitIsDeadOrGhost('player') or
    not UnitExists or not UnitIsDead or
    not CastingBarFrame or not IsAttackAction or not IsCurrentAction or
    not GetActionCooldown or not GetTime or not SpellIsTargeting or
    not EnumerateFrames then return 'unknown' end
 local frames={'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
  'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame',
  'StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}
 for _,name in ipairs(frames) do
  local f=getglobal(name)
  if f and (not f.IsVisible or f:IsVisible()) then return 'blocked' end
 end
 local f=EnumerateFrames()
 for i=1,4096 do
  if not f then break end
  if not f.IsVisible or not f.GetObjectType then return 'unknown' end
  if f:IsVisible() then
   if f:GetObjectType()=='EditBox' then return 'blocked' end
   if not f.IsKeyboardEnabled or not f.GetScript then return 'unknown' end
   if f:IsKeyboardEnabled() and (f:GetScript('OnKeyDown') or f:GetScript('OnKeyUp')) then return 'blocked' end
  end
  f=EnumerateFrames(f)
  if i==4096 and f then return 'unknown' end
 end
 if SpellIsTargeting() then return 'blocked' end
 if mode~='probe' and (not UnitExists('target') or UnitIsDead('target')) then return 'blocked' end
 local wait=CastingBarFrame.casting or CastingBarFrame.channeling
 local a=0
 for i=1,120 do
  if a==0 and IsAttackAction(i) then a=i end
  local start,duration=GetActionCooldown(i)
  if start and duration and duration>0 and duration<=1.5 and start+duration>GetTime() then wait=true end
 end
 if wait then return 'wait' end
 if mode~='probe' then
  if a>0 then
   if mode=='refresh' and IsCurrentAction(a) then UseAction(a) end
   if not IsCurrentAction(a) then UseAction(a) end
  elseif mode=='start' then AttackTarget()
  elseif type(StopAttack)=='function' then StopAttack(); AttackTarget()
  else return 'unsupported_refresh' end
  return 'issued'
 end
 local current=-1
 if a>0 then current=IsCurrentAction(a) and 1 or 0 end
 return a..'|'..current
end
local ok,result=pcall(run)
WOW_INTERNAL_AUTOATTACK_RESULT=ok and result or 'unknown'
)lua";
    inline std::string CombatActionScript(const char* mode)
    {
        std::string script=CombatActionEvidenceScript;
        const std::string marker="local mode='probe'";
        script.replace(script.find(marker),marker.size(),std::string("local mode='")+mode+"'");
        return script;
    }
}
