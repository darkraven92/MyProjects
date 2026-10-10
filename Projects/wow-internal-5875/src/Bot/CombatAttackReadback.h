#pragma once
#include <charconv>
#include <string_view>

namespace Bot
{
    struct CombatAttackStatus
    {
        bool valid=false, actionSlotFound=false, active=false;
        int actionSlot=0;
    };

    // Targetless observation of the existing Vanilla action APIs. No UI guard
    // or input dispatch: a readable latch never grants permission to act.
    inline constexpr const char* CombatAttackReadbackScript=R"lua(
WOW_INTERNAL_AUTOATTACK_RESULT='unknown_readback'
local function probe()
 if type(IsAttackAction)~='function' or type(IsCurrentAction)~='function' then return 'unknown_attack_action_api' end
 for i=1,120 do
  if IsAttackAction(i) then return i..'|'..(IsCurrentAction(i) and 1 or 0) end
 end
 return '0|-1'
end
local ok,result=pcall(probe)
WOW_INTERNAL_AUTOATTACK_RESULT=ok and result or 'unknown_script_error'
)lua";

    inline bool ParseCombatAttackReadback(std::string_view text, CombatAttackStatus& status)
    {
        status={};
        const auto separator=text.find('|');
        if (separator==std::string_view::npos) return false;
        int slot=0, current=-1;
        const auto a=std::from_chars(text.data(),text.data()+separator,slot);
        const auto b=std::from_chars(text.data()+separator+1,text.data()+text.size(),current);
        if (a.ec!=std::errc{} || a.ptr!=text.data()+separator ||
            b.ec!=std::errc{} || b.ptr!=text.data()+text.size() ||
            slot<0 || slot>120 || (slot==0 ? current!=-1 : current<0 || current>1))
            return false;
        status={true,slot>0,current==1,slot};
        return true;
    }
}
