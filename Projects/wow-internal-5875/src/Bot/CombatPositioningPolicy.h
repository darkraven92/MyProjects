#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    class CombatPositioningPolicy
    {
    public:
        /*
         * Extremely small player/target separation makes atan2-based facing
         * unstable: a tiny target movement can move the desired heading by a
         * large angle. This is especially common immediately after Charge.
         */
        static constexpr float ImmediateSeparationDistance =
            1.10f;

        /*
         * A short local separation step is enough to give facing a stable
         * baseline without turning combat positioning into navigation.
         */
        static constexpr float SeparationStepDistance =
            2.25f;

        static constexpr float SeparationCompleteDistance =
            1.75f;

        /* 3 * ~250 ms = at most ~0.75 s of local separation movement. */
        static constexpr std::uint64_t SeparationMaximumTicks =
            3;

        /* Do not oscillate in/out of separation recovery. */
        static constexpr std::uint64_t SeparationCooldownTicks =
            8;

        /* ~1 second of failed facing confirmation before escalation. */
        static constexpr std::uint32_t PersistentFacingHoldSnapshots =
            4;

        /* Local recovery only; never walk away from a target at range. */
        static constexpr float PersistentRecoveryMaximumDistance =
            3.50f;

        /* Bounded per-target recovery budget. */
        static constexpr std::uint32_t MaximumSeparationAttempts =
            3;

        static bool NeedsImmediateSeparation(float targetDistance)
        {
            return
                std::isfinite(targetDistance) &&
                targetDistance >= 0.0f &&
                targetDistance < ImmediateSeparationDistance;
        }

        static bool NeedsPersistentFacingRecovery(
            float targetDistance,
            std::uint32_t facingHoldSnapshots)
        {
            return
                std::isfinite(targetDistance) &&
                targetDistance >= 0.0f &&
                targetDistance <= PersistentRecoveryMaximumDistance &&
                facingHoldSnapshots >= PersistentFacingHoldSnapshots;
        }
    };
}
