#pragma once
#include <cstdint>
#include <string>

namespace Bot
{
    // Read-only command provenance. Deliberately not a command deduplicator:
    // identical destinations can be required by existing liveness recovery.
    struct MovementCommandTrace
    {
        std::uint64_t serial = 0;
        std::uint64_t issuedMs = 0;
        std::string writer = "none";
        float x = 0, y = 0, z = 0;
        void Observe(const char* origin, std::uint64_t ms, float nx, float ny, float nz)
        {
            ++serial; issuedMs = ms; writer = origin; x = nx; y = ny; z = nz;
        }
    };
}
