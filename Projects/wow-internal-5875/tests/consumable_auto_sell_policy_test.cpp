#include "../src/Bot/AutoSellItemPolicy.h"
#include "../src/Bot/ConsumableClassificationPolicy.h"
#include "../src/Bot/ManualVendorModePolicy.h"

#include <cassert>
#include <string>

int main()
{
    using Bot::AutoSellItemFacts;
    using Bot::AutoSellItemPolicy;
    using Bot::ConsumableClassificationPolicy;
    using Bot::ManualVendorModePolicy;

    // Text can be split across tooltip lines. Stack counts, not slot counts,
    // are the maintenance unit.
    constexpr std::string_view food =
        "use: restores 300 health over 30 sec. must remain seated while eating.";
    static_assert(ConsumableClassificationPolicy::IsFood(food));
    static_assert(!ConsumableClassificationPolicy::IsDrink(food));
    static_assert(ConsumableClassificationPolicy::IsDrink(
        "restores 300 mana over 30 sec. must remain seated while drinking."));
    static_assert(!ConsumableClassificationPolicy::IsFood(
        "restores 300 health instantly."));
    const std::string splitTooltip =
        std::string("restores 300 health over 30 sec.") +
        " must remain seated while eating.";
    assert(ConsumableClassificationPolicy::IsFood(splitTooltip));
    static_assert(ConsumableClassificationPolicy::StackContribution(true, 5) == 5);
    static_assert(ConsumableClassificationPolicy::StackContribution(false, 5) == 0);
    static_assert(ConsumableClassificationPolicy::StackContribution(true, 0) == 0);

    AutoSellItemFacts item{};
    item.itemId = 100;
    item.quality = 0;
    item.itemType = "Miscellaneous";
    assert(AutoSellItemPolicy::CanSell(item));
    item.food = true;
    assert(AutoSellItemPolicy::Reason(item) == "food");
    assert(!AutoSellItemPolicy::CanSell(item));
    item.food = false;
    item.drink = true;
    assert(AutoSellItemPolicy::Reason(item) == "drink");
    item.drink = false;
    item.itemType = "Armor";
    for (const int quality : {2, 3, 4, 5, 6})
    {
        item.quality = quality;
        assert(AutoSellItemPolicy::Reason(item) == "quality_uncommon_or_higher");
    }
    item.quality = 1;
    item.itemType = "Trade Goods";
    for (const std::string_view subtype : {"Herb", "Metal & Stone", "Leather"})
    {
        item.itemSubType = subtype;
        assert(AutoSellItemPolicy::Reason(item) == "gathering_material");
    }
    item.itemType = "Tradeskill";
    assert(AutoSellItemPolicy::Reason(item) == "gathering_material");
    item.itemType = "Quest";
    assert(AutoSellItemPolicy::Reason(item) == "quest_item");
    item.itemType = "Armor";
    item.quest = true;
    assert(!AutoSellItemPolicy::CanSell(item));
    item.quest = false;
    item.locked = true;
    assert(AutoSellItemPolicy::Reason(item) == "locked");
    item.locked = false;
    item.equipped = true;
    assert(AutoSellItemPolicy::Reason(item) == "equipped");
    item.equipped = false;
    item.explicitProtection = true;
    assert(!AutoSellItemPolicy::CanSell(item));
    item.explicitProtection = false;
    item.quality = -1;
    assert(AutoSellItemPolicy::Reason(item) == "classification_unknown");
    item.quality = 1;
    item.itemType = "Consumable";
    assert(AutoSellItemPolicy::Reason(item) == "classification_unknown");
    item.itemType = "";
    assert(!AutoSellItemPolicy::CanSell(item));
    assert(AutoSellItemPolicy::PlanStillMatches("item:100:0", 100, 4, false,
                                               "item:100:0", 100, 4, false));
    assert(!AutoSellItemPolicy::PlanStillMatches("item:100:0", 100, 4, false,
                                                "item:101:0", 101, 4, false));
    assert(!AutoSellItemPolicy::PlanStillMatches("item:100:0", 100, 4, false,
                                                "item:100:1", 100, 4, false));
    assert(!AutoSellItemPolicy::PlanStillMatches("item:100:0", 100, 4, false,
                                                "item:100:0", 100, 5, false));
    assert(!AutoSellItemPolicy::PlanStillMatches("item:100:0", 100, 4, false,
                                                "item:100:0", 100, 4, true));

    Bot::MaintenanceSnapshot maintenance{};
    maintenance.valid = true;
    maintenance.powerType = 1;
    maintenance.foodCount = 0;
    maintenance.foodCountKnown = true;
    maintenance.drinkCountKnown = true;
    assert(!ManualVendorModePolicy::RequirementsSatisfied(true, 2, 1, maintenance));
    maintenance.foodCount = 5;
    assert(ManualVendorModePolicy::RequirementsSatisfied(true, 2, 1, maintenance));
    Bot::MaintenanceNeed need{};
    need.food = true;
    assert(ManualVendorModePolicy::NeedName(false, need) == "food");
    assert(ManualVendorModePolicy::NeedName(true, {}) == "sell");
    need.drink = true;
    assert(ManualVendorModePolicy::NeedName(true, need) == "sell+food+drink");
    need.repair = true;
    assert(ManualVendorModePolicy::NeedName(true, need) == "sell+repair+food+drink");
}
