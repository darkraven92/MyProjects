#pragma once
#include <cstdint>
namespace Bot
{
    struct CombatTargetConsistencyPolicy
    {
        static constexpr bool ChaseMatches(std::uint64_t combat,std::uint64_t chase)
        { return combat!=0 && combat==chase; }
    };
}
