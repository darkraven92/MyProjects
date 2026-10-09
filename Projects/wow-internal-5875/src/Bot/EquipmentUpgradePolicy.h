#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <string>

namespace Bot
{
    struct EquipmentStats
    {
        double strength=0, agility=0, stamina=0, intellect=0, spirit=0;
        double armor=0, attackPower=0, dps=0;
    };
    struct EquipmentClassWeights
    {
        std::string_view token;
        EquipmentStats weights;
    };
    struct EquipmentUpgradePolicy
    {
        // Conservative low-level heuristics, not specialization theorycraft.
        // A weighted win alone is insufficient: no modeled stat may decrease.
        static constexpr std::array<EquipmentClassWeights,9> Classes{{
            {"WARRIOR",{2,.8,1.2,0,.2,.02,.5,12}},
            {"PALADIN",{1.5,.5,1.2,1,.5,.02,.4,8}},
            {"HUNTER",{.3,2,1,1,.3,.01,.5,1}},
            {"ROGUE",{1,2,1.2,0,.1,.01,.5,12}},
            {"PRIEST",{0,0,1,2,1.5,.01,0,0}},
            {"SHAMAN",{1,.5,1.2,1.5,1,.02,.3,4}},
            {"MAGE",{0,0,1,2,1,.01,0,0}},
            {"WARLOCK",{0,0,1.5,2,.5,.01,0,0}},
            {"DRUID",{1,1,1,1.5,1,.01,.2,0}}
        }};
        static constexpr double MinimumGain=.75, RelativeGain=.05;
        static const EquipmentStats* Weights(std::string_view token)
        {
            for (const auto& entry:Classes) if(entry.token==token) return &entry.weights;
            return nullptr;
        }
        static std::array<double,8> Values(const EquipmentStats& s)
        { return {s.strength,s.agility,s.stamina,s.intellect,s.spirit,s.armor,s.attackPower,s.dps}; }
        static double Score(const EquipmentStats& s,const EquipmentStats& weights)
        {
            const auto values=Values(s), w=Values(weights);
            double result=0;
            for(std::size_t i=0;i<values.size();++i) result+=values[i]*w[i];
            return result;
        }
        static bool ClearUpgrade(const EquipmentStats& candidate,const EquipmentStats& current,
                                 std::string_view classToken)
        {
            const auto* weights=Weights(classToken);
            if(!weights) return false;
            const auto a=Values(candidate), b=Values(current);
            for(std::size_t i=0;i<a.size();++i)
                if(!std::isfinite(a[i]) || !std::isfinite(b[i]) || a[i]<b[i] || b[i]<0) return false;
            const auto before=Score(current,*weights), after=Score(candidate,*weights);
            return after>before+std::max(MinimumGain,before*RelativeGain);
        }
        // Vendor protection is deliberately stronger than upgrade approval.
        static constexpr std::array<std::string_view,2> ProtectedItemTypes{"Armor","Weapon"};
        static bool ProtectFromSale(std::string_view itemType)
        { return std::find(ProtectedItemTypes.begin(),ProtectedItemTypes.end(),itemType)!=ProtectedItemTypes.end(); }
        static std::string VendorProtectionLua()
        {
            std::string script="local protectedEquipmentTypes={";
            for(const auto type:ProtectedItemTypes) script+="['"+std::string(type)+"']=true,";
            return script+"}; local function gearProtected(t) return protectedEquipmentTypes[t or ''] end; ";
        }
    };
}
