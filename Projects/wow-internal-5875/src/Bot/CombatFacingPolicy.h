#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    class CombatFacingPolicy
    {
    public:
        /*
         * Offensive abilities are only released when the latest
         * WorldState snapshot is very closely aligned with the
         * locked target. 0.10 rad ~= 5.7 degrees.
         */
        static constexpr float AbilityToleranceRadians =
            0.10f;

        /*
         * Autoattack may remain latched during very small corrections,
         * but must stop early enough that a moving/overlapping mob cannot
         * cross the front arc between two 250 ms snapshots.
         * 0.35 rad ~= 20 degrees.
         */
        static constexpr float AutoAttackStopToleranceRadians =
            0.35f;

        /*
         * Two consecutive fresh snapshots must agree that facing is
         * correct before a stopped/new autoattack is released again.
         */
        static constexpr std::uint32_t StableSnapshotsRequired =
            2;

        /*
         * Combat is polled at ~250 ms, so one tick allows a facing
         * correction every snapshot while misaligned.
         */
        static constexpr std::uint64_t CorrectionCooldownTicks =
            1;

        static bool IsAbilityFacingReady(
            float angularDelta)
        {
            return
                std::isfinite(angularDelta) &&
                angularDelta <= AbilityToleranceRadians;
        }

        static bool ShouldPauseAutoAttack(
            float angularDelta)
        {
            return
                !std::isfinite(angularDelta) ||
                angularDelta > AutoAttackStopToleranceRadians;
        }
    };
}
