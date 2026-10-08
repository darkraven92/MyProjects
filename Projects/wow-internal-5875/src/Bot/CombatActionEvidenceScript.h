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
 if not UIParent or not UIParent.IsVisible or not UIParent:IsVisible() then return 'unknown_ui_parent' end
 if not UnitIsDeadOrGhost or not UnitExists or not UnitIsDead then return 'unknown_life_api' end
 if UnitIsDeadOrGhost('player') then return 'blocked_player_dead' end
 if not CastingBarFrame then return 'unknown_casting_frame' end
 if not SpellIsTargeting then return 'unknown_spell_targeting' end
 if mode~='bootstrap' and not EnumerateFrames then return 'unknown_enumerate_frames' end
 if mode~='selection_probe' and
    (not IsAttackAction or not IsCurrentAction) then return 'unknown_attack_action_api' end
 if mode~='selection_probe' and mode~='bootstrap' and
    (not GetActionCooldown or not GetTime) then return 'unknown_attack_cooldown_api' end
 local frames={'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
  'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame',
  'StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}
 for _,name in ipairs(frames) do
  local f=getglobal(name)
  if f and not f.IsVisible then return 'unknown_frame_visibility_api' end
  if f and f:IsVisible() then return 'blocked_modal_frame' end
 end
 if mode~='bootstrap' then
  local f=EnumerateFrames()
  for i=1,4096 do
   if not f then break end
   if not f.IsVisible then return 'unknown_frame_visibility_api' end
   if not f.GetObjectType then return 'unknown_frame_object_type' end
   if f:IsVisible() then
    if f:GetObjectType()=='EditBox' then return 'blocked_editbox' end
    if not f.IsKeyboardEnabled then return 'unknown_frame_keyboard_api' end
    if not f.GetScript then return 'unknown_frame_script_api' end
    if f:IsKeyboardEnabled() and (f:GetScript('OnKeyDown') or f:GetScript('OnKeyUp')) then return 'blocked_keyboard_handler' end
   end
   f=EnumerateFrames(f)
   if i==4096 and f then return 'unknown_frame_iteration_limit' end
  end
 end
 if SpellIsTargeting() then return 'blocked_spell_targeting' end
 if mode=='selection_probe' then
  if CastingBarFrame.casting or CastingBarFrame.channeling then return 'waiting_cast' end
  return 'ready'
 end
 if mode~='probe' and (not UnitExists('target') or UnitIsDead('target')) then return 'blocked_target_invalid' end
 local wait=CastingBarFrame.casting or CastingBarFrame.channeling
 local a=0
 for i=1,120 do
  if a==0 and IsAttackAction(i) then a=i end
  if mode~='bootstrap' then
   local start,duration=GetActionCooldown(i)
   if start and duration and duration>0 and duration<=1.5 and start+duration>GetTime() then wait=true end
  end
 end
 if wait then return 'waiting_cast_or_gcd' end
 if mode=='abandon' then
  if a==0 or type(ClearTarget)~='function' then return 'unsupported_release' end
  if IsCurrentAction(a) then UseAction(a) end
  ClearTarget()
  return 'issued'
 end
 if mode~='probe' then
  if a>0 then
   if mode=='refresh' then
    -- Stop stage only: IsCurrentAction need not update synchronously after
    -- UseAction. A fresh later observation authorizes the bounded start.
    if IsCurrentAction(a) then UseAction(a) end
   elseif not IsCurrentAction(a) then UseAction(a) end
  elseif mode=='start' or mode=='bootstrap' then AttackTarget()
  elseif type(StopAttack)=='function' then StopAttack()
  else return 'unsupported_refresh' end
  return 'issued'
 end
 local current=-1
 if a>0 then current=IsCurrentAction(a) and 1 or 0 end
 return a..'|'..current
end
local ok,result=pcall(run)
WOW_INTERNAL_AUTOATTACK_RESULT=ok and result or 'unknown_script_error'
)lua";
    inline std::string CombatActionScript(const char* mode)
    {
        std::string script=CombatActionEvidenceScript;
        const std::string marker="local mode='probe'";
        script.replace(script.find(marker),marker.size(),std::string("local mode='")+mode+"'");
        return script;
    }
}
