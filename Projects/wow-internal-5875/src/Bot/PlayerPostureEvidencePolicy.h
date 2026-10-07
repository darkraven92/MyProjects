#pragma once

#include <cstdint>

namespace Bot
{
    struct PlayerPostureEvidencePolicy
    {
        // UNIT_FIELD_BYTES_1 = full descriptor word 0x8a. Client Unit methods
        // use [object+0x110] = [object+8] + 0x18, hence their relative 0x210
        // is NOT an offset from PlayerSnapshot::descriptors.
        static constexpr std::uintptr_t DescriptorOffset = 0x228;
        struct Evidence { bool known; std::uint8_t state; };
        static constexpr Evidence Decode(std::uint32_t bytes1)
        {
            const auto state = static_cast<std::uint8_t>(bytes1 & 0xffu);
            return {state <= 9, state};
        }
    };
}
