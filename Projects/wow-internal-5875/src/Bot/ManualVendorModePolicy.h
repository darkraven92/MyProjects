#pragma once

#include "AutonomousMaintenancePolicy.h"

#include <string>

namespace Bot
{
    enum class ManualVendorDecision
    {
        ContinueGrind,
        StartAutomaticVendor,
        EnterManualWait,
        ContinueManualWait,
        ResumeGrind
    };

    struct ManualVendorModePolicy
    {
        static std::string NeedName(bool sell, const MaintenanceNeed& need)
        {
            std::string result;
            const auto append = [&result](const char* name)
            {
                if (!result.empty()) result += '+';
                result += name;
            };
            if (sell) append("sell");
            if (need.repair) append("repair");
            if (need.food) append("food");
            if (need.drink) append("drink");
            return result.empty() ? "none" : result;
        }

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

        static constexpr ManualVendorDecision Decide(
            bool automationEnabled, bool waitingForManualVendor,
            bool vendorNeed, bool maintenanceSatisfied)
        {
            if (waitingForManualVendor)
                return maintenanceSatisfied
                    ? ManualVendorDecision::ResumeGrind
                    : ManualVendorDecision::ContinueManualWait;
            if (!vendorNeed)
                return ManualVendorDecision::ContinueGrind;
            return automationEnabled
                ? ManualVendorDecision::StartAutomaticVendor
                : ManualVendorDecision::EnterManualWait;
        }

        static constexpr bool MayStartVendor(ManualVendorDecision decision)
        {
            return decision == ManualVendorDecision::StartAutomaticVendor;
        }

        static constexpr bool NeedsOneShotHold(ManualVendorDecision decision)
        {
            return decision == ManualVendorDecision::EnterManualWait;
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
