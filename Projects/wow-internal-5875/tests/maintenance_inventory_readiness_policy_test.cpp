#include "../src/Bot/AutoSellItemPolicy.h"
#include "../src/Bot/AutonomousMaintenancePolicy.h"
#include "../src/Bot/ConsumableClassificationPolicy.h"
#include "../src/Bot/ManualVendorModePolicy.h"

#include <cassert>
#include <string_view>

int main()
{
    using Bot::AutonomousMaintenancePolicy;
    using Bot::ConsumableClassificationPolicy;
    using Bot::MaintenanceSnapshot;
    using Bot::ManualVendorModePolicy;

    MaintenanceSnapshot afterRestart{};
    afterRestart.valid = true;
    afterRestart.powerType = 1;
    afterRestart.unresolvedItems = 4;
    afterRestart.metadataPendingItems = 4;
    assert(afterRestart.foodCount == 0 && !afterRestart.foodCountKnown);
    assert(!AutonomousMaintenancePolicy::Evaluate(afterRestart).food);
    assert(!AutonomousMaintenancePolicy::WantsFoodTopUp(afterRestart));
    Bot::MaintenanceNeed priorFoodNeed{};
    priorFoodNeed.food = true;
    assert(!ManualVendorModePolicy::RequirementsSatisfied(
        true, 2, 1, afterRestart, priorFoodNeed));
    assert(ManualVendorModePolicy::RequirementsSatisfied(
        true, 2, 1, afterRestart)); // A repair-only wait can clear independently.

    // The next normal inventory probe replaces the unknown snapshot; it does
    // not retain the lower bound of zero when cached metadata becomes ready.
    constexpr std::string_view readyFoodTooltip =
        "use: restores 300 health over 30 sec. must remain seated while eating.";
    static_assert(ConsumableClassificationPolicy::IsFood(readyFoodTooltip));
    static_assert(!ConsumableClassificationPolicy::IsFood("item name only"));
    MaintenanceSnapshot ready = afterRestart;
    ready.unresolvedItems = 0;
    ready.metadataPendingItems = 0;
    ready.foodCountKnown = true;
    ready.drinkCountKnown = true;
    for (const int stack : {20, 20, 2, 20})
        ready.foodCount += ConsumableClassificationPolicy::StackContribution(
            ConsumableClassificationPolicy::IsFood(readyFoodTooltip), stack);
    assert(ready.foodCount == 62);
    assert(!AutonomousMaintenancePolicy::Evaluate(ready).food);
    assert(ManualVendorModePolicy::RequirementsSatisfied(
        true, 2, 1, ready, priorFoodNeed));

    MaintenanceSnapshot knownEmpty = ready;
    knownEmpty.foodCount = 0;
    assert(AutonomousMaintenancePolicy::Evaluate(knownEmpty).food);
    assert(AutonomousMaintenancePolicy::WantsFoodTopUp(knownEmpty));

    MaintenanceSnapshot repairWhilePending = afterRestart;
    repairWhilePending.durableItems = 1;
    repairWhilePending.minimumDurabilityPercent = 10.0f;
    const auto repair = AutonomousMaintenancePolicy::Evaluate(repairWhilePending);
    assert(repair.repair && repair.urgentRepair && !repair.food);
    assert(ManualVendorModePolicy::Decide(false, false, true, false) ==
           Bot::ManualVendorDecision::EnterManualWait); // Independent bag pressure.

    MaintenanceSnapshot manaWhilePending = afterRestart;
    manaWhilePending.powerType = 0;
    assert(!AutonomousMaintenancePolicy::Evaluate(manaWhilePending).drink);
    manaWhilePending.drinkCountKnown = true;
    assert(AutonomousMaintenancePolicy::Evaluate(manaWhilePending).drink);

    Bot::AutoSellItemFacts unknown{};
    unknown.itemId = 4542;
    assert(Bot::AutoSellItemPolicy::Reason(unknown) == "classification_unknown");
    assert(!Bot::AutoSellItemPolicy::CanSell(unknown));
    // Recovery uses the same strict tooltip predicates, so an unresolved
    // tooltip cannot become an arbitrary food-use candidate.
    assert(!ConsumableClassificationPolicy::IsFood(""));
    assert(!ConsumableClassificationPolicy::IsDrink(""));
}
