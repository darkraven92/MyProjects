#include "../src/Bot/ManualVendorModePolicy.h"

#include <cassert>

int main()
{
    using Bot::ManualVendorDecision;
    using Bot::ManualVendorModePolicy;
    constexpr auto automatic = ManualVendorModePolicy::Decide(
        true, false, true, false);
    static_assert(automatic == ManualVendorDecision::StartAutomaticVendor);
    static_assert(ManualVendorModePolicy::MayStartVendor(automatic));

    constexpr auto manual = ManualVendorModePolicy::Decide(
        false, false, true, false);
    static_assert(manual == ManualVendorDecision::EnterManualWait);
    static_assert(!ManualVendorModePolicy::MayStartVendor(manual));
    static_assert(ManualVendorModePolicy::NeedsOneShotHold(manual));

    constexpr auto waiting = ManualVendorModePolicy::Decide(
        false, true, true, false);
    static_assert(waiting == ManualVendorDecision::ContinueManualWait);
    static_assert(!ManualVendorModePolicy::MayStartVendor(waiting));
    static_assert(!ManualVendorModePolicy::NeedsOneShotHold(waiting));

    constexpr auto cleared = ManualVendorModePolicy::Decide(
        false, true, false, true);
    static_assert(cleared == ManualVendorDecision::ResumeGrind);
    static_assert(!ManualVendorModePolicy::MayStartVendor(cleared));

    // A toggled setting cannot silently seize movement from an existing
    // manual intervention: finish it first, then resume normal policy.
    assert(ManualVendorModePolicy::Decide(true, true, true, false) ==
           ManualVendorDecision::ContinueManualWait);
    assert(ManualVendorModePolicy::Decide(false, false, false, true) ==
           ManualVendorDecision::ContinueGrind);

    Bot::MaintenanceSnapshot maintenance{};
    maintenance.valid = true;
    maintenance.durableItems = 1;
    maintenance.minimumDurabilityPercent = 100.0f;
    maintenance.foodCount = 12;
    maintenance.foodCountKnown = true;
    maintenance.drinkCountKnown = true;
    maintenance.powerType = 1;
    assert(ManualVendorModePolicy::RequirementsSatisfied(
        true, 2, 1, maintenance));
    assert(!ManualVendorModePolicy::RequirementsSatisfied(
        true, 1, 1, maintenance));
    assert(!ManualVendorModePolicy::RequirementsSatisfied(
        false, 2, 1, maintenance));
    maintenance.minimumDurabilityPercent = 35.0f;
    assert(!ManualVendorModePolicy::RequirementsSatisfied(
        true, 2, 1, maintenance));
    maintenance.minimumDurabilityPercent = 100.0f;
    maintenance.foodCount = 0;
    assert(!ManualVendorModePolicy::RequirementsSatisfied(
        true, 2, 1, maintenance));
    Bot::MaintenanceNeed foodOnly{};
    foodOnly.food = true;
    assert(ManualVendorModePolicy::NeedName(false, foodOnly) == "food");
    assert(ManualVendorModePolicy::NeedName(true, {}) == "sell");
}
