#include "../src/Bot/EquipmentUpgradeScript.h"
#include "../src/Bot/QuestMaintenancePolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main(int argc,char** argv)
{
    using namespace Bot;
    if(argc==2 && std::string(argv[1])=="--lua") {std::cout<<EquipmentUpgradeScript(); return 0;}
    EquipmentStats before; before.armor=50; before.stamina=1;
    auto after=before; after.armor=100;
    assert(EquipmentUpgradePolicy::ClearUpgrade(after,before,"WARRIOR"));
    assert(!EquipmentUpgradePolicy::ClearUpgrade(after,before,"UNKNOWN"));
    after.armor=51; assert(!EquipmentUpgradePolicy::ClearUpgrade(after,before,"WARRIOR"));
    after.armor=200; after.stamina=0;
    assert(!EquipmentUpgradePolicy::ClearUpgrade(after,before,"WARRIOR"));
    before={}; after={}; before.dps=3; after.dps=4;
    assert(EquipmentUpgradePolicy::ClearUpgrade(after,before,"ROGUE"));
    assert(!EquipmentUpgradePolicy::ClearUpgrade(after,before,"PRIEST"));
    for(const auto& entry:EquipmentUpgradePolicy::Classes)
        assert(EquipmentUpgradePolicy::Weights(entry.token));
    after.dps=std::numeric_limits<double>::quiet_NaN();
    assert(!EquipmentUpgradePolicy::ClearUpgrade(after,before,"WARRIOR"));
    AutoSellItemFacts item; item.itemId=50000; item.quality=0; item.itemType="Armor";
    assert(EquipmentUpgradePolicy::ProtectFromSale(item.itemType));
    assert(!QuestMaintenancePolicy::SafeTrash(item));
    item.itemType="Weapon"; assert(!QuestMaintenancePolicy::SafeTrash(item));
}
