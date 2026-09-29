#pragma once

#include <cstdint>

namespace Bot
{
    struct DeathRecoveryPolicy
    {
        static constexpr std::uint64_t ProbeIntervalTicks = 4;          // ~1 s
        static constexpr std::uint64_t ActionRetryTicks = 8;            // ~2 s
        static constexpr std::uint64_t RouteRetryTicks = 8;             // ~2 s
        static constexpr std::uint64_t StatusLogTicks = 20;             // ~5 s
        static constexpr int MaximumReleaseAttemptsPerCycle = 6;
        static constexpr int MaximumRetrieveAttemptsPerCycle = 8;
        static constexpr int MaximumRouteVariants = 9;                  // direct + 8 generated ring points
        static constexpr int AliveConfirmationProbes = 2;               // require two fresh post-reclaim probes

        static constexpr float CorpseArrivalDistance = 18.0f;
        static constexpr float GeneratedApproachRadius = 14.0f;
        static constexpr float GeneratedApproachArrivalDistance = 8.0f;
        static constexpr float ReclaimDistance = 32.0f;

        static bool CanStartFromDeath(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth)
        {
            return playerValid && maxHealth > 0 && health == 0;
        }

        static bool AliveAfterCorpseRun(
            bool probeValid,
            bool isGhost,
            std::uint32_t health,
            std::uint32_t maxHealth,
            bool ghostConfirmed,
            bool retrieveCommandIssued,
            int aliveProbeStreak)
        {
            return
                probeValid &&
                !isGhost &&
                maxHealth > 0 &&
                health > 0 &&
                ghostConfirmed &&
                retrieveCommandIssued &&
                aliveProbeStreak >= AliveConfirmationProbes;
        }

        static bool CanRetrieveCorpse(
            bool probeValid,
            bool isGhost,
            float recoveryDelaySeconds,
            float distanceToDeath)
        {
            return
                probeValid &&
                isGhost &&
                recoveryDelaySeconds <= 0.0f &&
                distanceToDeath >= 0.0f &&
                distanceToDeath <= ReclaimDistance;
        }

        static bool ShouldRetryAction(
            std::uint64_t tick,
            std::uint64_t nextActionTick)
        {
            return tick >= nextActionTick;
        }
    };
}
