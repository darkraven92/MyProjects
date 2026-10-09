#pragma once
#include <cstdint>

namespace Bot
{
    struct MaintenanceCombatHandoffPolicy
    {
        static constexpr bool RetireDelay(bool postKillDelay, std::uint64_t tick,
            std::uint64_t deadline, bool locked, bool deferredLoot)
        {
            return postKillDelay && tick >= deadline && !locked && !deferredLoot;
        }
    };
}
