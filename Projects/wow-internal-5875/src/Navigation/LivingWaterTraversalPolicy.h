#pragma once

#include "TerrainTransitionPolicy.h"

namespace Navigation
{
    // Temporary P0.4 guard. Topology is shared; only route-local query filters
    // differ. An unknown/living owner never receives the Ghost exception.
    enum class WaterTraversalMode { AvoidUntilQualified, GhostDeathRecovery };

    struct LivingWaterTraversalPolicy
    {
        static constexpr unsigned short Include(WaterTraversalMode mode)
        {
            return mode == WaterTraversalMode::GhostDeathRecovery
                ? TerrainTransitionPolicy::GroundFlag |
                      TerrainTransitionPolicy::WaterFlag
                : TerrainTransitionPolicy::GroundFlag;
        }

        static constexpr unsigned short Exclude(WaterTraversalMode mode,
                                                 unsigned short existing = 0)
        {
            return mode == WaterTraversalMode::GhostDeathRecovery
                ? existing
                : static_cast<unsigned short>(existing |
                      TerrainTransitionPolicy::WaterFlag);
        }

        static constexpr bool Traversable(unsigned short polygonFlags,
                                          WaterTraversalMode mode)
        {
            return (polygonFlags & Include(mode)) != 0 &&
                (polygonFlags & Exclude(mode)) == 0;
        }
    };
}
