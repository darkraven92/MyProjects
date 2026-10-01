#pragma once

#include <cstdint>

namespace Bot
{
    struct NavigationInitializationLivenessPolicy
    {
        // Planning is genuine bounded work, not physical progress. The normal
        // owner watchdog is deferred only for one continuous planning window.
        static constexpr std::int64_t MaximumPendingMs = 4 * 60 * 1000;

        static constexpr bool DeferOwnerRecovery(bool pending,
                                                 std::int64_t elapsedMs)
        {
            return pending && elapsedMs >= 0 && elapsedMs < MaximumPendingMs;
        }
    };
}
