#pragma once

namespace Navigation
{
    // Shared existing navigation bounds; emergency egress gets no extra budget.
    struct LocalRecoveryLimits
    {
        static constexpr int MaximumSurfaceRecoveryAttempts = 4;
        static constexpr int MaximumLastSafeBacktracks = 2;
    };
}
