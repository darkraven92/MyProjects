#pragma once

#include <cstdint>

namespace Bot
{
    struct DeathRecoveryPolicy
    {
        enum class EntryState
        {
            ReleasingSpirit,
            WaitingForGhost,
            FailedMissingAnchor
        };

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

        static bool ShouldRememberClearlyAlivePosition(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth)
        {
            return playerValid && maxHealth > 0 && health > 1;
        }

        static bool ShouldProbeIdleGhost(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth)
        {
            return playerValid && maxHealth > 0 && health == 1;
        }

        static bool CanStartFromDeath(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth)
        {
            return playerValid && maxHealth > 0 && health == 0;
        }

        static bool CanBootstrapFromGhost(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth,
            bool probeValid,
            bool isGhost)
        {
            return ShouldProbeIdleGhost(playerValid, health, maxHealth) &&
                probeValid && isGhost;
        }

        static bool CanBootstrapFromDead(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth,
            bool probeValid,
            bool deadKnown,
            bool isDead,
            bool isGhost)
        {
            return ShouldProbeIdleGhost(playerValid, health, maxHealth) &&
                probeValid && deadKnown && isDead && !isGhost;
        }

        static EntryState EntryFor(
            bool confirmedDead,
            bool confirmedGhost,
            bool haveClearlyAlivePosition)
        {
            if (!confirmedDead && !confirmedGhost)
                return EntryState::ReleasingSpirit;
            if (!haveClearlyAlivePosition)
                return EntryState::FailedMissingAnchor;
            return confirmedGhost
                ? EntryState::WaitingForGhost
                : EntryState::ReleasingSpirit;
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

        static bool CanAttemptSpiritRelease(
            bool bootstrappedFromGhost,
            std::uint64_t tick,
            std::uint64_t nextActionTick)
        {
            return !bootstrappedFromGhost &&
                ShouldRetryAction(tick, nextActionTick);
        }
    };

    // Monotonic milliseconds are supplied by the controller. Tick cadence is
    // deliberately absent: synchronous path loading can take many seconds.
    struct DeathRecoveryLivenessPolicy
    {
        enum class FailureReason
        {
            None,
            EpisodeDeadline,
            NoPhysicalProgress,
            RouteAttemptBudget,
            StationaryFailureBudget
        };

        static constexpr std::uint64_t MaximumEpisodeAgeMs = 300000;
        static constexpr std::uint64_t MaximumNoProgressAgeMs = 180000;
        static constexpr int MaximumRouteAttempts = 18;
        static constexpr int MaximumStationaryRouteFailures = 9;
        static constexpr int MaximumFullMapFallbackAttempts = 1;

        static FailureReason Evaluate(
            std::uint64_t episodeAgeMs,
            std::uint64_t noProgressAgeMs,
            int routeAttempts,
            int stationaryFailures,
            bool beforeNewRoute)
        {
            if (episodeAgeMs >= MaximumEpisodeAgeMs)
                return FailureReason::EpisodeDeadline;
            if (noProgressAgeMs >= MaximumNoProgressAgeMs)
                return FailureReason::NoPhysicalProgress;
            if (stationaryFailures >= MaximumStationaryRouteFailures)
                return FailureReason::StationaryFailureBudget;
            if (beforeNewRoute && routeAttempts >= MaximumRouteAttempts)
                return FailureReason::RouteAttemptBudget;
            return FailureReason::None;
        }

        static bool AllowFullMapFallback(int attempts)
        {
            return attempts < MaximumFullMapFallbackAttempts;
        }

        static bool MayAttemptRouteThisUpdate(
            bool terminalFailed,
            bool alreadyAttempted)
        {
            return !terminalFailed && !alreadyAttempted;
        }

        static int ConsumeNextVariant(int& nextVariant)
        {
            const int variant = nextVariant % DeathRecoveryPolicy::MaximumRouteVariants;
            nextVariant = (variant + 1) % DeathRecoveryPolicy::MaximumRouteVariants;
            return variant;
        }

        static bool ManualAliveConfirmed(
            bool playerValid,
            std::uint32_t health,
            std::uint32_t maxHealth,
            bool probeValid,
            bool deadKnown,
            bool isDead,
            bool isGhost,
            int freshAliveProbes)
        {
            return DeathRecoveryPolicy::ShouldRememberClearlyAlivePosition(
                       playerValid, health, maxHealth) &&
                probeValid && deadKnown && !isDead && !isGhost &&
                freshAliveProbes >= DeathRecoveryPolicy::AliveConfirmationProbes;
        }
    };
}
