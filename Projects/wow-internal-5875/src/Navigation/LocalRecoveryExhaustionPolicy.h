#pragma once

namespace Navigation
{
    enum class LocalRecoveryExhaustionDecision
    {
        NotExhausted,
        TryLastSafeBacktrack,
        FailExhausted
    };

    struct LocalRecoveryExhaustionPolicy
    {
        static constexpr bool EarnedProgressReset(
            float currentFinalDistance, float bestFinalDistance,
            float meaningfulProgress)
        {
            return currentFinalDistance + meaningfulProgress <
                bestFinalDistance;
        }

        static constexpr LocalRecoveryExhaustionDecision Assess(
            int surfaceAttempts, int maximumSurfaceAttempts,
            bool lastSafeKnown,
            int backtrackAttempts, int maximumBacktracks)
        {
            if (surfaceAttempts < maximumSurfaceAttempts)
                return LocalRecoveryExhaustionDecision::NotExhausted;
            if (lastSafeKnown &&
                backtrackAttempts < maximumBacktracks)
                return LocalRecoveryExhaustionDecision::TryLastSafeBacktrack;
            return LocalRecoveryExhaustionDecision::FailExhausted;
        }
    };
}
