#pragma once

#include <cstdint>

namespace Objects
{
    // Candidate descriptor layout for the build-5875-gated client. The AFK
    // interpretation still requires comparison with the in-game UI at runtime.
    inline constexpr std::uint32_t Vanilla5875PlayerFlagsOffset = 0x2F8;
    inline constexpr std::uint32_t Vanilla5875PlayerFlagAfkCandidate = 0x00000002;

    constexpr bool DecodeAfkCandidate(std::uint32_t rawFlags)
    {
        return (rawFlags & Vanilla5875PlayerFlagAfkCandidate) != 0;
    }
}
