#pragma once

#include <algorithm>
#include <cstdint>

namespace Bot
{
    struct MaintenanceSnapshot
    {
        bool valid = false;
        int powerType = -1;
        std::uint32_t money = 0;
        float minimumDurabilityPercent = 100.0f;
        int durableItems = 0;
        int foodCount = 0;
        int drinkCount = 0;
    };

    struct MaintenanceNeed
    {
        bool repair = false;
        bool urgentRepair = false;
        bool food = false;
        bool drink = false;

        bool Any() const
        {
            return repair || food || drink;
        }
    };

    class AutonomousMaintenancePolicy
    {
    public:
        static constexpr float RepairTripThresholdPercent = 40.0f;
        static constexpr float UrgentRepairThresholdPercent = 15.0f;
        static constexpr int FoodTripThreshold = 5;
        static constexpr int FoodTarget = 12;
        static constexpr int DrinkTripThreshold = 5;
        static constexpr int DrinkTarget = 12;
        static constexpr std::uint32_t MinimumCashReserveCopper = 50;

        static bool IsManaUser(const MaintenanceSnapshot& snapshot)
        {
            return snapshot.powerType == 0;
        }

        static MaintenanceNeed Evaluate(const MaintenanceSnapshot& snapshot)
        {
            MaintenanceNeed need{};
            if (!snapshot.valid)
                return need;

            if (snapshot.durableItems > 0)
            {
                need.repair =
                    snapshot.minimumDurabilityPercent <=
                    RepairTripThresholdPercent;
                need.urgentRepair =
                    snapshot.minimumDurabilityPercent <=
                    UrgentRepairThresholdPercent;
            }

            need.food = snapshot.foodCount < FoodTripThreshold;
            need.drink =
                IsManaUser(snapshot) &&
                snapshot.drinkCount < DrinkTripThreshold;
            return need;
        }

        static bool WantsFoodTopUp(const MaintenanceSnapshot& snapshot)
        {
            return snapshot.valid && snapshot.foodCount < FoodTarget;
        }

        static bool WantsDrinkTopUp(const MaintenanceSnapshot& snapshot)
        {
            return snapshot.valid && IsManaUser(snapshot) &&
                   snapshot.drinkCount < DrinkTarget;
        }

        static std::uint32_t ConsumableReserve(std::uint32_t money)
        {
            // Keep at least half the current cash and never less than 50 copper.
            // Repairs run before restocking, so this protects the next repair cycle
            // without allowing food/drink to consume the entire purse.
            return std::max(MinimumCashReserveCopper, money / 2u);
        }

        static bool CanBuyConsumable(
            std::uint32_t money,
            std::uint32_t unitPrice)
        {
            if (unitPrice == 0 || money < unitPrice)
                return false;

            const std::uint32_t reserve = ConsumableReserve(money);
            return money - unitPrice >= reserve;
        }
    };
}
