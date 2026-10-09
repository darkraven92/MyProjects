#include "../src/Bot/QuestMaintenancePolicy.h"
#include "../src/Bot/QuestPlanner.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
int main(int argc, char** argv)
{
    using namespace Bot;
    if(argc==2 && std::string(argv[1])=="--lua")
    { std::cout<<QuestMaintenancePolicy::RestrictSaleLua(); return 0; }
    MaintenanceSnapshot s; s.valid=true; s.foodCountKnown=true; s.foodCount=0;
    assert(!QuestMaintenancePolicy::Need(s).Any()); // no penniless supply trip
    s.money=1000; assert(QuestMaintenancePolicy::Need(s).food);
    s.durableItems=1; s.minimumDurabilityPercent=14;
    assert(QuestMaintenancePolicy::Need(s).urgentRepair);
    s.powerType=0; s.drinkCountKnown=true; assert(QuestMaintenancePolicy::Need(s).drink);
    AutoSellItemFacts item; item.itemId=12345; item.quality=0; item.itemType="Miscellaneous";
    assert(QuestMaintenancePolicy::SafeTrash(item));
    item.quest=true; assert(!QuestMaintenancePolicy::SafeTrash(item)); item.quest=false;
    item.locked=true; assert(!QuestMaintenancePolicy::SafeTrash(item)); item.locked=false;
    item.food=true; assert(!QuestMaintenancePolicy::SafeTrash(item)); item.food=false;
    item.drink=true; assert(!QuestMaintenancePolicy::SafeTrash(item)); item.drink=false;
    item.itemType="Armor"; assert(!QuestMaintenancePolicy::SafeTrash(item));
    item.itemType="Weapon"; assert(!QuestMaintenancePolicy::SafeTrash(item));
    item.itemType="Trade Goods"; assert(!QuestMaintenancePolicy::SafeTrash(item));
    item.itemType="Miscellaneous"; item.quality=1; assert(!QuestMaintenancePolicy::SafeTrash(item));
    item.quality=-1; assert(!QuestMaintenancePolicy::SafeTrash(item));
    for(int owner=0;owner<5;++owner)
    {
        QuestRevisitBoundary b;
        if(owner==0) b.objectiveOwned=true;
        if(owner==1) b.objectiveStartPending=true;
        if(owner==2) b.objectiveRetryPending=true;
        if(owner==3) b.discoveryOwned=true;
        if(owner==4) b.turnInOwned=true;
        assert(!CanEvaluateQuestRevisit(b));
    }
    assert(CanEvaluateQuestRevisit({}));
    std::ifstream file("src/Bot/QuestPlannerRuntimeController.h"); assert(file);
    const std::string source((std::istreambuf_iterator<char>(file)),{});
    assert(source.find("CanEvaluateQuestRevisit(acquisitionBoundary) && maintenance_.TryStart")!=std::string::npos);
    assert(source.find("relocation_.Active() || maintenance_.Active() || trainer_.Active(),")!=std::string::npos);
    assert(source.find("maintenance_.Cancel();")!=std::string::npos);
}
