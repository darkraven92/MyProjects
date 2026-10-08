#pragma once

#include <string_view>

namespace Bot
{
    // The same tooltip predicates are used by recovery, maintenance and
    // auto-sell. Text is aggregated across tooltip lines before matching.
    struct ConsumableClassificationPolicy
    {
        static constexpr bool IsFood(std::string_view lowerTooltip)
        {
            return lowerTooltip.find("health over") != std::string_view::npos &&
                lowerTooltip.find("eating") != std::string_view::npos;
        }

        static constexpr bool IsDrink(std::string_view lowerTooltip)
        {
            return lowerTooltip.find("mana over") != std::string_view::npos &&
                lowerTooltip.find("drinking") != std::string_view::npos;
        }

        static constexpr int StackContribution(bool recognized, int count)
        {
            return recognized && count > 0 ? count : 0;
        }

        static constexpr const char* LuaDefinition()
        {
            return
                "local function classifyConsumable(tt) "
                "local text=''; local prefix=tt:GetName(); "
                "for i=1,tt:NumLines() do "
                "local l=getglobal(prefix..'TextLeft'..i); "
                "local r=getglobal(prefix..'TextRight'..i); "
                "if l and l:GetText() then text=text..' '..string.lower(l:GetText()) end; "
                "if r and r:GetText() then text=text..' '..string.lower(r:GetText()) end; "
                "end; "
                "local h=string.find(text,'health over')~=nil; "
                "local e=string.find(text,'eating')~=nil; "
                "local m=string.find(text,'mana over')~=nil; "
                "local d=string.find(text,'drinking')~=nil; "
                "return h and e,m and d,h,e,m,d,string.find(text,'quest item')~=nil,text; "
                "end; ";
        }
    };
}
