#pragma once
namespace Bot
{
    // Same Vanilla action APIs already used by AutoAttackController::Probe.
    // Observation only: no selected target, input/UI eligibility or action write.
    inline constexpr const char* LivingAttackEvidenceScript=R"lua(
WOW_INTERNAL_AUTOATTACK_RESULT='unknown'
local function probe()
 if type(IsAttackAction)~='function' or type(IsCurrentAction)~='function' then return 'unknown' end
 for i=1,120 do
  if IsAttackAction(i) then return IsCurrentAction(i) and 'active' or 'inactive' end
 end
 return 'unknown'
end
local ok,result=pcall(probe)
WOW_INTERNAL_AUTOATTACK_RESULT=ok and result or 'unknown'
)lua";
}
