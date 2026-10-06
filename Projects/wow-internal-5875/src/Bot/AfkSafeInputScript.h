#pragma once
namespace Bot
{
    inline constexpr const char* AfkSafeInputScript = R"lua(
WOW_INTERNAL_AFK_INPUT='unknown'
local function gate()
 if not UnitAffectingCombat or not UnitIsDeadOrGhost or not GetBindingAction or
    not EnumerateFrames or not UIParent or not UIParent.IsVisible then return 'api_missing' end
 if not UIParent:IsVisible() or UnitIsDeadOrGhost('player') or
    UnitAffectingCombat('player') then return 'world_dead_or_combat' end
 local binding=GetBindingAction('F12')
 if binding==nil then return 'binding_unknown' end
 if binding~='' then return 'key_bound' end
 local names={'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
  'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame','StaticPopup1'}
 for _,name in ipairs(names) do
  local f=getglobal(name)
  if f and (not f.IsVisible or f:IsVisible()) then return 'dialog_visible' end
 end
 local f=EnumerateFrames()
 for i=1,4096 do
  if not f then return 'ready' end
  if not f.IsVisible or not f.GetObjectType then return 'frame_unknown' end
  if f:IsVisible() then
   if f:GetObjectType()=='EditBox' then return 'edit_box_visible' end
   if not f.IsKeyboardEnabled or not f.GetScript then return 'keyboard_state_unknown' end
   if f:IsKeyboardEnabled() and (f:GetScript('OnKeyDown') or f:GetScript('OnKeyUp')) then
    return 'keyboard_handler_active'
   end
  end
  f=EnumerateFrames(f)
 end
 return 'frame_limit'
end
local ok,result=pcall(gate)
WOW_INTERNAL_AFK_INPUT=ok and result or 'guard_error'
)lua";
}
