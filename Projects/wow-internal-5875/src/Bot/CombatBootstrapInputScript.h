#pragma once

namespace Bot
{
    // Initial offensive input has a narrower contract than target release:
    // named Blizzard modal frames, spell cursor and Vanilla cast-bar state.
    // It does not depend on the generic frame iterator or Attack action-slot
    // readback. The game-thread native GUID/health/range/facing guard remains
    // mandatory at the eventual command dispatch.
    inline constexpr const char* CombatBootstrapInputScript=R"lua(
WOW_INTERNAL_AUTOATTACK_RESULT='unknown_readback'
local function run()
 if not UIParent or not UIParent.IsVisible or not UIParent:IsVisible() then return 'unknown_ui_parent' end
 if not UnitExists or not UnitIsDeadOrGhost then return 'unknown_life_api' end
 if not UnitExists('player') or UnitIsDeadOrGhost('player') then return 'blocked_player_dead' end
 if not CastingBarFrame then return 'unknown_casting_frame' end
 if not SpellIsTargeting then return 'unknown_spell_targeting' end
 if type(getglobal)~='function' then return 'unknown_modal_lookup_api' end
 local frames={'GossipFrame','QuestFrame','MerchantFrame','ClassTrainerFrame','LootFrame',
  'TalentFrame','TradeFrame','MailFrame','ItemTextFrame','TaxiFrame','GameMenuFrame',
  'StaticPopup1','StaticPopup2','StaticPopup3','StaticPopup4'}
 for _,name in ipairs(frames) do
  local f=getglobal(name)
  if f and not f.IsVisible then return 'unknown_frame_visibility_api' end
  if f and f:IsVisible() then return 'blocked_modal_frame' end
 end
 if SpellIsTargeting() then return 'blocked_spell_targeting' end
 if CastingBarFrame.casting or CastingBarFrame.channeling then return 'waiting_cast' end
 return 'ready'
end
local ok,result=pcall(run)
WOW_INTERNAL_AUTOATTACK_RESULT=ok and result or 'unknown_script_error'
)lua";
}
