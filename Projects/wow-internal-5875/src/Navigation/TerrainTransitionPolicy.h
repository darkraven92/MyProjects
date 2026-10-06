#pragma once

#include <cmath>
#include <cstdint>

namespace Navigation
{
    // The generator assigns both NAV_GROUND and NAV_STEEP_SLOPES to steep
    // terrain. A ground-only Detour filter therefore does not exclude it.
    struct TerrainTransitionPolicy
    {
        static constexpr unsigned short GroundFlag = 0x01;
        static constexpr unsigned short WaterFlag = 0x08;
        static constexpr unsigned short SteepFlag = 0x10;

        // These are the existing 12B.10 player-to-portal safety thresholds.
        // Planning and runtime steering must make the same geometry decision.
        static constexpr float MinimumVerticalDelta = 6.0f;
        static constexpr float MaximumHorizontal = 14.0f;
        static constexpr float MinimumRiseRun = 0.75f;

        struct Geometry
        {
            float horizontal = 0.0f;
            float vertical = 0.0f;
            float riseRun = 0.0f;
            bool rejected = false;
        };

        template <typename From, typename To>
        static Geometry Assess(const From& from, const To& to)
        {
            Geometry result{};
            result.horizontal = std::hypot(to.x - from.x, to.y - from.y);
            result.vertical = std::fabs(to.z - from.z);
            if (!std::isfinite(result.horizontal) ||
                !std::isfinite(result.vertical) ||
                result.horizontal < 0.10f)
                return result;
            result.riseRun = result.vertical / result.horizontal;
            result.rejected =
                result.vertical >= MinimumVerticalDelta &&
                result.horizontal <= MaximumHorizontal &&
                result.riseRun >= MinimumRiseRun;
            return result;
        }

        static bool Complete(bool queryOk, bool success, bool partial)
        {
            return queryOk && success && !partial;
        }

        static bool UsesSteep(unsigned short flags)
        {
            return (flags & SteepFlag) != 0;
        }

        static std::uint64_t EnteredRef(
            std::uint64_t straightRef, bool finalPoint,
            std::uint64_t corridorEndRef)
        {
            // This Detour fork gives the final straight-path endpoint ref=0.
            return straightRef != 0 ? straightRef
                : finalPoint ? corridorEndRef : 0;
        }

        static bool SameEndpointPolygons(
            std::uint64_t preferredStart, std::uint64_t preferredEnd,
            std::uint64_t unrestrictedStart,
            std::uint64_t unrestrictedEnd)
        {
            return preferredStart != 0 && preferredEnd != 0 &&
                preferredStart == unrestrictedStart &&
                preferredEnd == unrestrictedEnd;
        }
    };
}
