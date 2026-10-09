#pragma once
#include "AutonomousMaintenancePolicy.h"
#include "AutoSellItemPolicy.h"
#include "EquipmentUpgradePolicy.h"

namespace Bot
{
    struct QuestMaintenancePolicy
    {
        static constexpr std::uint64_t ProbeTicks=40;
        static constexpr std::uint64_t RetryTicks=2400;
        static constexpr int BagTriggerFreeSlots=1;
        static MaintenanceNeed Need(const MaintenanceSnapshot& snapshot)
        {
            auto need=AutonomousMaintenancePolicy::Evaluate(snapshot);
            // Do not make a penniless fresh character travel to buy supplies.
            // Merchant affordability remains authoritative at the vendor.
            if(snapshot.money<=AutonomousMaintenancePolicy::ConsumableReserve(snapshot.money))
                need.food=need.drink=false;
            return need;
        }
        static bool SafeTrash(const AutoSellItemFacts& item)
        {
            // Preserve ALL equipment, including gray/common starter upgrades.
            return !EquipmentUpgradePolicy::ProtectFromSale(item.itemType) &&
                AutoSellItemPolicy::CanSell(item) && item.quality==0 && item.itemType=="Miscellaneous";
        }
        static std::string RestrictSaleLua()
        {
            return EquipmentUpgradePolicy::VendorProtectionLua()+"local baseSaleReason=saleReason; "
                "saleReason=function(b,s,link,count,locked) "
                "local reason,id,quality,itype=baseSaleReason(b,s,link,count,locked); "
                "if reason=='sell' and (gearProtected(itype) or not (quality==0 and itype=='Miscellaneous')) then "
                "reason='questing_conservative_preservation'; end; "
                "return reason,id,quality,itype; end; ";
        }
    };
}
