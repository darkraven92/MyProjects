#pragma once
#include <cstdint>
namespace Bot
{
    struct AfkClientFlag5875Policy
    {
        // B6E5CC: explicit mark writes 1 (5EB7AE); server update writes
        // PLAYER_FLAGS & 2 (5EE9EF..5EE9F2). Clear writes 0. Native readers
        // test nonzero, not ==1. Other encodings remain unknown.
        static constexpr bool Known(std::uint32_t raw) { return raw<=2; }
        static constexpr bool Active(std::uint32_t raw) { return Known(raw) && raw!=0; }
    };
}
