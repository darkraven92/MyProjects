#pragma once

#include "AutonomousMaintenancePolicy.h"

namespace Bot
{
    struct ManualVendorModePolicy
    {
        static bool RequirementsSatisfied(
            bool bagsValid, int freeSlots, int vendorTriggerFreeSlots,
            const MaintenanceSnapshot& maintenance,
            const MaintenanceNeed& requestedNeed = {})
        {
            return bagsValid && freeSlots > vendorTriggerFreeSlots &&
                maintenance.valid &&
                (!requestedNeed.food || maintenance.foodCountKnown ||
                 maintenance.foodCount >= AutonomousMaintenancePolicy::FoodTripThreshold) &&
                (!requestedNeed.drink ||
                 maintenance.drinkCountKnown ||
                 maintenance.drinkCount >= AutonomousMaintenancePolicy::DrinkTripThreshold) &&
                !AutonomousMaintenancePolicy::Evaluate(maintenance).Any();
        }

        // A failed automatic trip must not return to target acquisition when
        // the current bag is full. An unavailable post-trip read cannot prove
        // space is available, so keep the owner paused until it can be read.
        static constexpr bool HoldAfterFailedTrip(
            bool freshBagKnown, int freshFreeSlots)
        {
            return freshBagKnown
                ? freshFreeSlots <= 0
                : true;
        }
    };
}
