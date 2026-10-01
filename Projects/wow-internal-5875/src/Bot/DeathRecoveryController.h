#pragma once

#include "DeathRecoveryPolicy.h"
#include "GameThreadDispatcher.h"
#include "MovementController.h"

#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace Bot
{
    enum class DeathRecoveryState
    {
        Idle,
        ReleasingSpirit,
        WaitingForGhost,
        RoutingToCorpse,
        WaitingForReclaim,
        WaitingForAlive,
        Done,
        Failed
    };

    class DeathRecoveryController
    {
    private:
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;
        static constexpr const char* ResultVariable = "WOW_INTERNAL_DEATH_RESULT";
        static constexpr float Pi = 3.14159265358979323846f;

        // Phase 14G.4.3: do not trust the broad policy reclaim radius as
        // proof that a private-server corpse reclaim will actually succeed.
        // Walk materially closer before issuing RetrieveCorpse, then use
        // bounded precision reroutes instead of spinning the Lua command.
        static constexpr float PrecisionReclaimDistance = 8.0f;
        static constexpr float PrecisionRouteArrivalDistance = 3.0f;
        static constexpr float MeaningfulGhostMovement = 1.25f;
        static constexpr std::uint64_t DeadStateStallTicks = 160;
        static constexpr int RetrieveAttemptsBeforePrecisionReroute = 2;

        struct DeathProbe
        {
            bool valid = false;
            bool deadKnown = false;
            bool isDead = false;
            bool isGhost = false;
            float recoveryDelaySeconds = 0.0f;
        };

        DeathRecoveryState state_ = DeathRecoveryState::Idle;
        Navigation::NavPoint deathPosition_{};
        Navigation::NavPoint lastClearlyAlivePosition_{};
        bool haveLastClearlyAlivePosition_ = false;
        std::uint32_t mapId_ = 1;

        std::unique_ptr<Navigation::GenericNavMeshPathFollower> corpseNavigator_{};
        bool corpseNavigatorFullMapCounted_ = false;
        bool corpseNavigatorReadyCounted_ = false;
        bool corpseNavigatorPrecision_ = false;

        std::uint64_t stateStartedTick_ = 0;
        std::uint64_t nextActionTick_ = 0;
        std::uint64_t nextProbeTick_ = 0;
        std::uint64_t nextIdleProbeTick_ = 0;
        std::uint64_t lastIdleProbeLogTick_ = 0;
        std::uint64_t idleDeathConfirmedTick_ = 0;
        bool haveIdleProbeLog_ = false;
        std::uint64_t lastStatusTick_ = 0;

        int releaseAttempts_ = 0;
        int retrieveAttempts_ = 0;
        int routeStarts_ = 0;
        int routeVariant_ = 0;
        int recoveries_ = 0;

        // Phase 14G.4.3 dead-state liveness / precision reclaim telemetry.
        std::uint64_t lastPhysicalProgressTick_ = 0;
        std::uint64_t nextDeadStateLivenessTick_ = 0;
        Navigation::NavPoint lastPhysicalProgressPosition_{};
        bool havePhysicalProgressPosition_ = false;
        int precisionRouteStarts_ = 0;
        int deadStateStalls_ = 0;
        int reclaimPrecisionRecoveries_ = 0;

        using SteadyClock = std::chrono::steady_clock;
        SteadyClock::time_point deathEpisodeStartTime_{};
        SteadyClock::time_point lastPhysicalProgressTime_{};
        SteadyClock::time_point lastRouteAttemptTime_{};
        int routeAttempts_ = 0;
        int routeStartFailures_ = 0;
        int fullMapFallbackAttempts_ = 0;
        int fullMapFallbackAttemptsSinceProgress_ = 0;
        int consecutiveStationaryRouteFailures_ = 0;
        int failedAliveProbeStreak_ = 0;
        bool routeAttemptedThisUpdate_ = false;

        // Phase 14G.4.2.1: resurrection must never be inferred merely from
        // health becoming non-zero during the asynchronous spirit-release transition.
        bool ghostConfirmedThisRecovery_ = false;
        bool bootstrappedFromGhost_ = false;
        bool retrieveCommandIssuedThisRecovery_ = false;
        int aliveProbeStreak_ = 0;

        DeathProbe lastProbe_{};

        static const char* StateNameInternal(DeathRecoveryState state)
        {
            switch (state)
            {
                case DeathRecoveryState::Idle: return "Idle";
                case DeathRecoveryState::ReleasingSpirit: return "ReleasingSpirit";
                case DeathRecoveryState::WaitingForGhost: return "WaitingForGhost";
                case DeathRecoveryState::Failed: return "Failed";
                case DeathRecoveryState::RoutingToCorpse: return "RoutingToCorpse";
                case DeathRecoveryState::WaitingForReclaim: return "WaitingForReclaim";
                case DeathRecoveryState::WaitingForAlive: return "WaitingForAlive";
                case DeathRecoveryState::Done: return "Done";
                default: return "Unknown";
            }
        }

        static std::string Float(float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        static float Distance3D(
            float ax, float ay, float az,
            float bx, float by, float bz)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        static std::uint64_t AgeMs(
            SteadyClock::time_point now,
            SteadyClock::time_point since)
        {
            if (now < since)
                return 0;
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - since).count());
        }

        static const char* LivenessReasonName(
            DeathRecoveryLivenessPolicy::FailureReason reason)
        {
            using Reason = DeathRecoveryLivenessPolicy::FailureReason;
            switch (reason)
            {
                case Reason::EpisodeDeadline: return "episode_deadline";
                case Reason::NoPhysicalProgress: return "no_physical_progress";
                case Reason::RouteAttemptBudget: return "route_attempt_budget";
                case Reason::StationaryFailureBudget: return "stationary_failure_budget";
                case Reason::None: return "none";
            }
            return "unknown";
        }

        static bool IsExecutable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(
                    reinterpret_cast<LPCVOID>(address),
                    &info,
                    sizeof(info)) == 0)
            {
                return false;
            }

            if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0)
                return false;

            const DWORD protect = info.Protect & 0xFF;
            switch (protect)
            {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;
                default:
                    return false;
            }
        }

        static std::uintptr_t LuaDoStringAddress()
        {
            return Wow5875::Client::Base() + LuaDoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return Wow5875::Client::Base() + GetTextRva;
        }

        static bool ExecuteLuaAction(
            const char* script,
            const char* scriptName)
        {
            const auto doStringAddress = LuaDoStringAddress();
            if (!IsExecutable(doStringAddress))
                return false;

            using DoStringFunction = bool (__fastcall*)(const char*, const char*);
            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);

            bool onGameThread = false;
            bool luaExecuted = false;
            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (!onGameThread)
                        return;
                    luaExecuted = doString(script, scriptName);
                });

            return dispatched && onGameThread && luaExecuted;
        }

        static bool ProbeDeathState(DeathProbe& probe)
        {
            probe = DeathProbe{};

            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();
            if (!IsExecutable(doStringAddress) || !IsExecutable(getTextAddress))
                return false;

            using DoStringFunction = bool (__fastcall*)(const char*, const char*);
            using GetTextFunction = const char* (__fastcall*)(char*, std::uint32_t, int);
            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);
            const auto getText =
                reinterpret_cast<GetTextFunction>(getTextAddress);

            static constexpr const char* script =
                "local dead=-1; if type(UnitIsDead)=='function' then dead=UnitIsDead('player') and 1 or 0 end; "
                "local ghost=-1; if type(UnitIsGhost)=='function' then ghost=UnitIsGhost('player') and 1 or 0 end; "
                "local delay=0; if GetCorpseRecoveryDelay then delay=GetCorpseRecoveryDelay() or 0 end; "
                "WOW_INTERNAL_DEATH_RESULT=tostring(dead)..'|'..tostring(ghost)..'|'..tostring(delay);";

            char buffer[128]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (!onGameThread)
                        return;

                    luaExecuted = doString(
                        script,
                        "wow-internal/DeathRecoveryProbe.lua");
                    if (!luaExecuted)
                        return;

                    const char* raw = getText(
                        const_cast<char*>(ResultVariable),
                        0xFFFFFFFFu,
                        0);

                    if (raw != nullptr && *raw != '\0')
                    {
                        std::strncpy(buffer, raw, sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';
                        gotText = true;
                    }
                });

            if (!dispatched || !onGameThread || !luaExecuted || !gotText)
                return false;

            int dead = -1;
            int ghost = -1;
            float delay = 0.0f;
            if (std::sscanf(buffer, "%d|%d|%f", &dead, &ghost, &delay) != 3)
                return false;

            probe.valid = ghost == 0 || ghost == 1;
            probe.deadKnown = dead == 0 || dead == 1;
            probe.isDead = dead == 1;
            probe.isGhost = ghost == 1;
            probe.recoveryDelaySeconds = std::max(0.0f, delay);
            return probe.valid;
        }

        void SetState(DeathRecoveryState state, std::uint64_t tick)
        {
            if (state_ == state)
                return;

            Debug::Logger::Info(
                std::string("DeathRecoveryController state: ") +
                StateNameInternal(state_) + " -> " +
                StateNameInternal(state));

            state_ = state;
            stateStartedTick_ = tick;
        }

        Navigation::NavPoint RouteDestinationForVariant(int variant) const
        {
            if (variant <= 0)
                return deathPosition_;

            const int spoke = (variant - 1) % 8;
            const float angle =
                (2.0f * Pi * static_cast<float>(spoke)) / 8.0f;

            return Navigation::NavPoint{
                deathPosition_.x +
                    std::cos(angle) * DeathRecoveryPolicy::GeneratedApproachRadius,
                deathPosition_.y +
                    std::sin(angle) * DeathRecoveryPolicy::GeneratedApproachRadius,
                deathPosition_.z
            };
        }

        bool EnforceMonotonicLiveness(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            bool beforeNewRoute)
        {
            if (state_ == DeathRecoveryState::Failed)
                return false;

            const auto now = SteadyClock::now();
            const auto episodeAgeMs = AgeMs(now, deathEpisodeStartTime_);
            const auto noProgressAgeMs = AgeMs(now, lastPhysicalProgressTime_);
            const auto reason = DeathRecoveryLivenessPolicy::Evaluate(
                episodeAgeMs,
                noProgressAgeMs,
                routeAttempts_,
                consecutiveStationaryRouteFailures_,
                beforeNewRoute);
            if (reason == DeathRecoveryLivenessPolicy::FailureReason::None)
                return true;

            corpseNavigator_.reset();
            MovementController::HoldPosition(player);
            failedAliveProbeStreak_ = 0;
            nextProbeTick_ = tick;
            const float corpseDistance = Distance3D(
                player.x, player.y, player.z,
                deathPosition_.x, deathPosition_.y, deathPosition_.z);
            Debug::Logger::Info(
                std::string("DEATH RECOVERY 14G.4.3.2: TERMINAL LIVENESS FAILURE reason=") +
                LivenessReasonName(reason) +
                " episodeAgeMs=" + std::to_string(episodeAgeMs) +
                " noProgressAgeMs=" + std::to_string(noProgressAgeMs) +
                " routeAttempts=" + std::to_string(routeAttempts_) +
                " routeStartFailures=" + std::to_string(routeStartFailures_) +
                " fullMapFallbackAttempts=" + std::to_string(fullMapFallbackAttempts_) +
                " stationaryFullMapFallbackAttempts=" +
                std::to_string(fullMapFallbackAttemptsSinceProgress_) +
                " lastRouteAttemptAgeMs=" +
                (routeAttempts_ > 0
                    ? std::to_string(AgeMs(now, lastRouteAttemptTime_))
                    : std::string("unknown")) +
                " stationaryFailures=" +
                std::to_string(consecutiveStationaryRouteFailures_) +
                " corpseDistance=" + Float(corpseDistance));
            SetState(DeathRecoveryState::Failed, tick);
            return false;
        }

        bool TryStartRoute(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const Navigation::NavPoint& destination,
            float arrival,
            const char* label,
            int variant,
            Navigation::GenericNavMeshPathFollower& navigator)
        {
            if (!DeathRecoveryLivenessPolicy::MayAttemptRouteThisUpdate(
                    state_ == DeathRecoveryState::Failed,
                    routeAttemptedThisUpdate_))
            {
                Debug::Logger::Info(
                    "DEATH RECOVERY 14G.4.3.2: additional route attempt deferred to a later monitor update.");
                return false;
            }
            if (!EnforceMonotonicLiveness(player, tick, true))
                return false;

            const bool allowFullMapFallback =
                DeathRecoveryLivenessPolicy::AllowFullMapFallback(
                    fullMapFallbackAttemptsSinceProgress_);
            const auto began = SteadyClock::now();
            lastRouteAttemptTime_ = began;
            routeAttemptedThisUpdate_ = true;
            ++routeAttempts_;
            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.3.2 ROUTE ATTEMPT BEGIN variant=" +
                std::to_string(variant) +
                " allowFullMapFallback=" +
                (allowFullMapFallback ? std::string("yes") : std::string("no")) +
                " attempt=" + std::to_string(routeAttempts_));

            const bool started = navigator.Start(
                player, tick, destination, mapId_, arrival, label, false,
                Navigation::GenericNavMeshStartOptions{allowFullMapFallback});
            if (navigator.FullMapFallbackAttempted())
            {
                ++fullMapFallbackAttempts_;
                ++fullMapFallbackAttemptsSinceProgress_;
            }
            if (!started)
            {
                ++routeStartFailures_;
                ++consecutiveStationaryRouteFailures_;
            }

            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.3.2 ROUTE ATTEMPT END variant=" +
                std::to_string(variant) +
                " allowFullMapFallback=" +
                (allowFullMapFallback ? std::string("yes") : std::string("no")) +
                " attempt=" + std::to_string(routeAttempts_) +
                " monotonicDurationMs=" +
                std::to_string(AgeMs(SteadyClock::now(), began)) +
                " success=" + (started ? std::string("yes") : std::string("no")) +
                " initializationPending=" +
                (navigator.InitializationPending() ? std::string("yes") : std::string("no")) +
                " fullMapAttempted=" +
                (navigator.FullMapFallbackAttempted() ? std::string("yes") : std::string("no")) +
                " fullMapFallbackAttempts=" +
                std::to_string(fullMapFallbackAttempts_) +
                " stationaryFullMapFallbackAttempts=" +
                std::to_string(fullMapFallbackAttemptsSinceProgress_) +
                " routeStartFailures=" + std::to_string(routeStartFailures_) +
                " stationaryFailures=" +
                std::to_string(consecutiveStationaryRouteFailures_) +
                " failureReason=" + (started ? std::string("none") : std::string("navigator_start_failed")));

            // A synchronous full-map load can consume most of the budget by
            // itself. Recheck real elapsed time before accepting its result.
            return EnforceMonotonicLiveness(player, tick, false) && started;
        }

        bool StartCorpseRoute(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            corpseNavigator_.reset();
            corpseNavigatorFullMapCounted_ = false;
            corpseNavigatorReadyCounted_ = false;
            corpseNavigatorPrecision_ = false;
            if (!EnforceMonotonicLiveness(player, tick, true))
                return false;

            // One variant per monitor update; the next index survives failure.
            const int variant = DeathRecoveryLivenessPolicy::ConsumeNextVariant(
                routeVariant_);
            const Navigation::NavPoint destination =
                RouteDestinationForVariant(variant);
            const float arrival = variant == 0
                ? PrecisionRouteArrivalDistance
                : DeathRecoveryPolicy::GeneratedApproachArrivalDistance;
            auto nav =
                std::make_unique<Navigation::GenericNavMeshPathFollower>();

            if (TryStartRoute(
                    player, tick, destination, arrival,
                    "death recovery corpse route", variant, *nav))
            {
                corpseNavigator_ = std::move(nav);
                Debug::Logger::Info(
                    "DEATH RECOVERY 14G.4.2: corpse route initialization accepted variant=" +
                    std::to_string(variant) +
                    " destination=(" + Float(destination.x) + "," +
                    Float(destination.y) + "," + Float(destination.z) + ")");
                SetState(DeathRecoveryState::RoutingToCorpse, tick);
                return true;
            }

            if (state_ == DeathRecoveryState::Failed)
                return false;
            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.3.2: corpse route variant failed; retrying after bounded backoff.");
            nextActionTick_ = tick + DeathRecoveryPolicy::RouteRetryTicks;
            return false;
        }

        bool StartPrecisionCorpseRoute(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const char* reason)
        {
            if (routeAttemptedThisUpdate_)
            {
                nextDeadStateLivenessTick_ = tick + 1;
                return false;
            }
            corpseNavigator_.reset();
            corpseNavigatorFullMapCounted_ = false;
            corpseNavigatorReadyCounted_ = false;
            corpseNavigatorPrecision_ = true;

            auto nav =
                std::make_unique<Navigation::GenericNavMeshPathFollower>();

            if (!TryStartRoute(
                    player, tick, deathPosition_,
                    PrecisionRouteArrivalDistance,
                    "death recovery precision corpse route", -1, *nav))
            {
                if (state_ == DeathRecoveryState::Failed)
                    return false;
                Debug::Logger::Info(
                    std::string("ROBUSTNESS 14G.4.3: PRECISION CORPSE ROUTE start failed reason=") +
                    reason + "; falling back to bounded route-variant retry.");
                nextActionTick_ = tick + DeathRecoveryPolicy::RouteRetryTicks;
                SetState(DeathRecoveryState::WaitingForGhost, tick);
                return false;
            }

            corpseNavigator_ = std::move(nav);
            nextDeadStateLivenessTick_ = tick + DeadStateStallTicks;

            Debug::Logger::Info(
                std::string("ROBUSTNESS 14G.4.3: PRECISION CORPSE ROUTE initialization accepted reason=") +
                reason +
                " destination=(" + Float(deathPosition_.x) + "," +
                Float(deathPosition_.y) + "," + Float(deathPosition_.z) +
                ") arrival=" + Float(PrecisionRouteArrivalDistance));

            SetState(DeathRecoveryState::RoutingToCorpse, tick);
            return true;
        }

        void ObservePhysicalProgress(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            const Navigation::NavPoint now{player.x, player.y, player.z};
            if (!havePhysicalProgressPosition_)
            {
                lastPhysicalProgressPosition_ = now;
                havePhysicalProgressPosition_ = true;
                lastPhysicalProgressTick_ = tick;
                lastPhysicalProgressTime_ = SteadyClock::now();
                nextDeadStateLivenessTick_ = tick + DeadStateStallTicks;
                return;
            }

            const float moved = Distance3D(
                lastPhysicalProgressPosition_.x,
                lastPhysicalProgressPosition_.y,
                lastPhysicalProgressPosition_.z,
                now.x, now.y, now.z);

            if (moved >= MeaningfulGhostMovement)
            {
                lastPhysicalProgressPosition_ = now;
                lastPhysicalProgressTick_ = tick;
                lastPhysicalProgressTime_ = SteadyClock::now();
                consecutiveStationaryRouteFailures_ = 0;
                fullMapFallbackAttemptsSinceProgress_ = 0;
                nextDeadStateLivenessTick_ = tick + DeadStateStallTicks;
            }
        }

        void CompleteRecovery(std::uint64_t tick)
        {
            ++recoveries_;
            corpseNavigator_.reset();
            Debug::Logger::Info("================================");
            Debug::Logger::Info("DEATH RECOVERY 14G.4.2: RESURRECTION CONFIRMED");
            Debug::Logger::Info(
                "releaseAttempts=" + std::to_string(releaseAttempts_) +
                " retrieveAttempts=" + std::to_string(retrieveAttempts_) +
                " routeStarts=" + std::to_string(routeStarts_));
            Debug::Logger::Info("================================");
            SetState(DeathRecoveryState::Done, tick);
        }

    public:
        void ObserveClearlyAlivePosition(const Objects::WorldState& world)
        {
            if (state_ != DeathRecoveryState::Idle ||
                !DeathRecoveryPolicy::ShouldRememberClearlyAlivePosition(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth) ||
                !std::isfinite(world.player.x) ||
                !std::isfinite(world.player.y) ||
                !std::isfinite(world.player.z))
            {
                return;
            }

            lastClearlyAlivePosition_ = Navigation::NavPoint{
                world.player.x, world.player.y, world.player.z};
            haveLastClearlyAlivePosition_ = true;
        }

        bool ConfirmDeathWhileIdle(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (state_ != DeathRecoveryState::Idle ||
                !DeathRecoveryPolicy::ShouldProbeIdleGhost(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth) ||
                tick < nextIdleProbeTick_)
            {
                return false;
            }

            nextIdleProbeTick_ = tick + DeathRecoveryPolicy::ProbeIntervalTicks;
            DeathProbe probe{};
            const bool probeRead = ProbeDeathState(probe);
            const bool confirmedGhost = probeRead &&
                DeathRecoveryPolicy::CanBootstrapFromGhost(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth,
                    probe.valid,
                    probe.isGhost);
            const bool confirmedDead = probeRead &&
                DeathRecoveryPolicy::CanBootstrapFromDead(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth,
                    probe.valid,
                    probe.deadKnown,
                    probe.isDead,
                    probe.isGhost);

            if (!haveIdleProbeLog_ ||
                tick >= lastIdleProbeLogTick_ + DeathRecoveryPolicy::StatusLogTicks ||
                confirmedDead || confirmedGhost)
            {
                haveIdleProbeLog_ = true;
                lastIdleProbeLogTick_ = tick;
                Debug::Logger::Info(
                    "DEATH 14G.4.3.1 PROBE: hp=" +
                    std::to_string(world.player.health) + "/" +
                    std::to_string(world.player.maxHealth) +
                    " valid=" + (probe.valid ? "yes" : "no") +
                    " dead=" + (probe.deadKnown
                        ? (probe.isDead ? "yes" : "no") : "unknown") +
                    " ghost=" + (probe.valid
                        ? (probe.isGhost ? "yes" : "no") : "unknown") +
                    " reclaimDelay=" +
                    (probe.valid ? Float(probe.recoveryDelaySeconds) : "unknown"));
            }

            if (!probeRead)
                return false;

            lastProbe_ = probe;
            if (confirmedDead || confirmedGhost)
                idleDeathConfirmedTick_ = tick;
            return confirmedDead || confirmedGhost;
        }

        bool Start(
            const Objects::WorldState& world,
            std::uint64_t tick,
            std::uint32_t mapId,
            bool deathBootstrap = false)
        {
            if (state_ != DeathRecoveryState::Idle)
                return false;

            const bool confirmedGhost =
                deathBootstrap && idleDeathConfirmedTick_ == tick &&
                DeathRecoveryPolicy::CanBootstrapFromGhost(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth,
                    lastProbe_.valid,
                    lastProbe_.isGhost);
            const bool confirmedDead =
                deathBootstrap && idleDeathConfirmedTick_ == tick &&
                DeathRecoveryPolicy::CanBootstrapFromDead(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth,
                    lastProbe_.valid,
                    lastProbe_.deadKnown,
                    lastProbe_.isDead,
                    lastProbe_.isGhost);
            if (!DeathRecoveryPolicy::CanStartFromDeath(
                    world.player.valid,
                    world.player.health,
                    world.player.maxHealth) &&
                !confirmedDead && !confirmedGhost)
            {
                return false;
            }

            if (
                !std::isfinite(world.player.x) ||
                !std::isfinite(world.player.y) ||
                !std::isfinite(world.player.z))
            {
                return false;
            }

            const auto entry = DeathRecoveryPolicy::EntryFor(
                confirmedDead, confirmedGhost, haveLastClearlyAlivePosition_);
            if (entry == DeathRecoveryPolicy::EntryState::FailedMissingAnchor)
            {
                MovementController::HoldPosition(world.player);
                Debug::Logger::Info(
                    std::string("ROBUSTNESS 14G.4.3.1: DEATH OWNERSHIP BOOTSTRAP corpsePosition=unknown state=") +
                    (confirmedGhost ? "ghost" : "dead"));
                Debug::Logger::Info(
                    "ROBUSTNESS 14G.4.3.1: DEATH OWNERSHIP BOOTSTRAP FAILED; no clearly-alive anchor, corpse routing and reclaim disabled until bot restart.");
                SetState(DeathRecoveryState::Failed, tick);
                return true;
            }

            mapId_ = mapId;
            deathPosition_ = (confirmedDead || confirmedGhost)
                ? lastClearlyAlivePosition_
                : Navigation::NavPoint{
                    world.player.x, world.player.y, world.player.z};

            releaseAttempts_ = 0;
            retrieveAttempts_ = 0;
            routeStarts_ = 0;
            routeVariant_ = 0;
            precisionRouteStarts_ = 0;
            deadStateStalls_ = 0;
            reclaimPrecisionRecoveries_ = 0;
            deathEpisodeStartTime_ = SteadyClock::now();
            lastPhysicalProgressTime_ = deathEpisodeStartTime_;
            lastRouteAttemptTime_ = SteadyClock::time_point{};
            routeAttempts_ = 0;
            routeStartFailures_ = 0;
            fullMapFallbackAttempts_ = 0;
            fullMapFallbackAttemptsSinceProgress_ = 0;
            consecutiveStationaryRouteFailures_ = 0;
            failedAliveProbeStreak_ = 0;
            routeAttemptedThisUpdate_ = false;
            lastPhysicalProgressTick_ = tick;
            nextDeadStateLivenessTick_ = tick + DeadStateStallTicks;
            lastPhysicalProgressPosition_ = Navigation::NavPoint{
                world.player.x, world.player.y, world.player.z};
            havePhysicalProgressPosition_ = true;
            nextActionTick_ = tick;
            nextProbeTick_ = (confirmedDead || confirmedGhost)
                ? tick + DeathRecoveryPolicy::ProbeIntervalTicks : tick;
            lastStatusTick_ = tick;
            ghostConfirmedThisRecovery_ = confirmedGhost;
            bootstrappedFromGhost_ = confirmedGhost;
            retrieveCommandIssuedThisRecovery_ = false;
            aliveProbeStreak_ = 0;
            if (!confirmedDead && !confirmedGhost)
                lastProbe_ = DeathProbe{};
            corpseNavigator_.reset();
            corpseNavigatorFullMapCounted_ = false;
            corpseNavigatorReadyCounted_ = false;
            corpseNavigatorPrecision_ = false;

            MovementController::HoldPosition(world.player);

            Debug::Logger::Info("================================");
            if (confirmedGhost)
            {
                Debug::Logger::Info(
                    "ROBUSTNESS 14G.4.3.1: GHOST OWNERSHIP BOOTSTRAP corpsePosition=last clearly-alive position");
            }
            else if (confirmedDead)
            {
                Debug::Logger::Info(
                    "ROBUSTNESS 14G.4.3.1: DEAD OWNERSHIP BOOTSTRAP corpsePosition=last clearly-alive position");
            }
            else
            {
                Debug::Logger::Info("DEATH RECOVERY 14G.4.2: PLAYER DEAD -> CORPSE RUN OWNERSHIP");
            }
            Debug::Logger::Info(
                "deathPosition=(" + Float(deathPosition_.x) + "," +
                Float(deathPosition_.y) + "," + Float(deathPosition_.z) + ") mapId=" +
                std::to_string(mapId_));
            Debug::Logger::Info(
                confirmedGhost
                    ? "Policy: confirmed ghost -> NavMesh corpse route -> wait reclaim delay -> RetrieveCorpse -> verify alive."
                    : "Policy: ReleaseSpirit -> verify ghost -> NavMesh corpse route -> wait reclaim delay -> RetrieveCorpse -> verify alive.");
            Debug::Logger::Info("================================");

            SetState(entry == DeathRecoveryPolicy::EntryState::WaitingForGhost
                ? DeathRecoveryState::WaitingForGhost
                : DeathRecoveryState::ReleasingSpirit, tick);
            return true;
        }

        void Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (state_ == DeathRecoveryState::Idle ||
                state_ == DeathRecoveryState::Done)
            {
                return;
            }

            routeAttemptedThisUpdate_ = false;

            if (state_ == DeathRecoveryState::Failed)
            {
                // Terminal death ownership remains in place until two fresh
                // probes positively confirm a clearly-alive player. HP alone
                // must not release a ghost to normal grind ownership.
                if (DeathRecoveryPolicy::ShouldRememberClearlyAlivePosition(
                        world.player.valid, world.player.health,
                        world.player.maxHealth) && tick >= nextProbeTick_)
                {
                    nextProbeTick_ = tick + DeathRecoveryPolicy::ProbeIntervalTicks;
                    DeathProbe probe{};
                    if (ProbeDeathState(probe) && probe.valid && probe.deadKnown &&
                        !probe.isDead && !probe.isGhost)
                    {
                        ++failedAliveProbeStreak_;
                        if (DeathRecoveryLivenessPolicy::ManualAliveConfirmed(
                                world.player.valid, world.player.health,
                                world.player.maxHealth, probe.valid,
                                probe.deadKnown, probe.isDead, probe.isGhost,
                                failedAliveProbeStreak_))
                        {
                            Debug::Logger::Info(
                                "DEATH RECOVERY 14G.4.3.2: MANUAL ALIVE CONFIRMED after terminal failure; releasing death ownership.");
                            SetState(DeathRecoveryState::Done, tick);
                        }
                    }
                    else
                    {
                        failedAliveProbeStreak_ = 0;
                    }
                }
                else if (!DeathRecoveryPolicy::ShouldRememberClearlyAlivePosition(
                             world.player.valid, world.player.health,
                             world.player.maxHealth))
                {
                    failedAliveProbeStreak_ = 0;
                }
                return;
            }

            ObservePhysicalProgress(world.player, tick);
            if (!EnforceMonotonicLiveness(world.player, tick, false))
                return;

            bool freshProbe = false;
            if (tick >= nextProbeTick_)
            {
                nextProbeTick_ = tick + DeathRecoveryPolicy::ProbeIntervalTicks;
                DeathProbe probe{};
                if (ProbeDeathState(probe))
                {
                    lastProbe_ = probe;
                    freshProbe = true;

                    if (probe.isGhost)
                    {
                        if (!ghostConfirmedThisRecovery_)
                        {
                            Debug::Logger::Info(
                                "DEATH RECOVERY 14G.4.2.1: ghost state positively confirmed; resurrection completion is now armed only after a real corpse-reclaim command.");
                        }
                        ghostConfirmedThisRecovery_ = true;
                        aliveProbeStreak_ = 0;
                    }
                    else if (state_ == DeathRecoveryState::WaitingForAlive &&
                             ghostConfirmedThisRecovery_ &&
                             retrieveCommandIssuedThisRecovery_ &&
                             world.player.maxHealth > 0 &&
                             world.player.health > 0)
                    {
                        ++aliveProbeStreak_;
                        Debug::Logger::Info(
                            "DEATH RECOVERY 14G.4.2.1: post-reclaim alive probe " +
                            std::to_string(aliveProbeStreak_) + "/" +
                            std::to_string(DeathRecoveryPolicy::AliveConfirmationProbes) +
                            " hp=" + std::to_string(world.player.health) + "/" +
                            std::to_string(world.player.maxHealth));
                    }
                    else
                    {
                        aliveProbeStreak_ = 0;
                    }
                }
            }

            // Do not perform a global health>0 resurrection check here. In Vanilla the
            // spirit-release transition can expose a transient non-zero player health
            // snapshot before UnitIsGhost() has positively transitioned. Completion is
            // legal only in WaitingForAlive after we have observed ghost state, issued
            // RetrieveCorpse successfully, and received consecutive fresh non-ghost probes.

            if (state_ == DeathRecoveryState::ReleasingSpirit)
            {
                if (lastProbe_.valid && lastProbe_.isGhost)
                {
                    SetState(DeathRecoveryState::WaitingForGhost, tick);
                    nextActionTick_ = tick;
                }
                else if (DeathRecoveryPolicy::CanAttemptSpiritRelease(
                             bootstrappedFromGhost_, tick, nextActionTick_))
                {
                    const bool issued = ExecuteLuaAction(
                        "if RepopMe then RepopMe(); end;",
                        "wow-internal/DeathRecoveryReleaseSpirit.lua");

                    ++releaseAttempts_;
                    Debug::Logger::Info(
                        "DEATH RECOVERY 14G.4.2: ReleaseSpirit command " +
                        std::string(issued ? "issued" : "failed") +
                        " attempt=" + std::to_string(releaseAttempts_));

                    nextActionTick_ = tick + DeathRecoveryPolicy::ActionRetryTicks;
                    SetState(DeathRecoveryState::WaitingForGhost, tick);
                }
            }
            else if (state_ == DeathRecoveryState::WaitingForGhost)
            {
                if (lastProbe_.valid && lastProbe_.isGhost)
                {
                    const float distance = Distance3D(
                        world.player.x,
                        world.player.y,
                        world.player.z,
                        deathPosition_.x,
                        deathPosition_.y,
                        deathPosition_.z);

                    Debug::Logger::Info(
                        "DEATH RECOVERY 14G.4.2: ghost confirmed distanceToCorpse=" +
                        Float(distance));

                    if (distance <= PrecisionReclaimDistance)
                    {
                        MovementController::HoldPosition(world.player);
                        SetState(DeathRecoveryState::WaitingForReclaim, tick);
                        nextActionTick_ = tick;
                    }
                    else if (DeathRecoveryPolicy::ShouldRetryAction(
                                 tick,
                                 nextActionTick_))
                    {
                        if (distance <= DeathRecoveryPolicy::ReclaimDistance)
                        {
                            StartPrecisionCorpseRoute(
                                world.player,
                                tick,
                                "inside broad reclaim radius but outside precision radius");
                        }
                        else
                        {
                            StartCorpseRoute(world.player, tick);
                        }
                    }
                }
                else if (DeathRecoveryPolicy::CanAttemptSpiritRelease(
                             bootstrappedFromGhost_, tick, nextActionTick_))
                {
                    if (releaseAttempts_ >=
                        DeathRecoveryPolicy::MaximumReleaseAttemptsPerCycle)
                    {
                        Debug::Logger::Info(
                            "DEATH RECOVERY 14G.4.2: release retry cycle exhausted; resetting bounded release budget.");
                        releaseAttempts_ = 0;
                    }

                    SetState(DeathRecoveryState::ReleasingSpirit, tick);
                    nextActionTick_ = tick;
                }
            }
            else if (state_ == DeathRecoveryState::RoutingToCorpse)
            {
                const float distance = Distance3D(
                    world.player.x,
                    world.player.y,
                    world.player.z,
                    deathPosition_.x,
                    deathPosition_.y,
                    deathPosition_.z);

                if (distance <= PrecisionReclaimDistance)
                {
                    if (corpseNavigator_)
                        corpseNavigator_.reset();
                    MovementController::HoldPosition(world.player);
                    Debug::Logger::Info(
                        "ROBUSTNESS 14G.4.3: entered precision corpse reclaim radius distance=" +
                        Float(distance));
                    SetState(DeathRecoveryState::WaitingForReclaim, tick);
                    nextActionTick_ = tick;
                }
                else if (corpseNavigator_)
                {
                    corpseNavigator_->Update(world.player, tick);

                    // Initialization now spans monitor updates. Consume the
                    // existing death-route fallback budget when full-map is
                    // actually entered, not when Start accepted a pending route.
                    if (corpseNavigator_->FullMapFallbackAttempted() &&
                        !corpseNavigatorFullMapCounted_)
                    {
                        ++fullMapFallbackAttempts_;
                        ++fullMapFallbackAttemptsSinceProgress_;
                        corpseNavigatorFullMapCounted_ = true;
                    }
                    if (corpseNavigator_->HasUsablePath() &&
                        !corpseNavigatorReadyCounted_)
                    {
                        ++routeStarts_;
                        if (corpseNavigatorPrecision_)
                            ++precisionRouteStarts_;
                        corpseNavigatorReadyCounted_ = true;
                    }

                    if (corpseNavigator_->Arrived())
                    {
                        corpseNavigator_.reset();
                        if (distance <= PrecisionReclaimDistance)
                        {
                            MovementController::HoldPosition(world.player);
                            SetState(DeathRecoveryState::WaitingForReclaim, tick);
                            nextActionTick_ = tick;
                        }
                        else
                        {
                            StartPrecisionCorpseRoute(
                                world.player,
                                tick,
                                "coarse corpse route arrived outside precision radius");
                        }
                    }
                    else if (corpseNavigator_->Failed())
                    {
                        if (!corpseNavigatorReadyCounted_)
                            ++routeStartFailures_;
                        corpseNavigator_.reset();
                        ++consecutiveStationaryRouteFailures_;
                        if (!EnforceMonotonicLiveness(world.player, tick, false))
                            return;
                        Debug::Logger::Info(
                            "DEATH RECOVERY 14G.4.2: corpse route failed; selecting another generated approach variant.");
                        nextActionTick_ = tick + DeathRecoveryPolicy::RouteRetryTicks;
                        SetState(DeathRecoveryState::WaitingForGhost, tick);
                    }
                }
                else
                {
                    SetState(DeathRecoveryState::WaitingForGhost, tick);
                    nextActionTick_ = tick + DeathRecoveryPolicy::RouteRetryTicks;
                }
            }
            else if (state_ == DeathRecoveryState::WaitingForReclaim)
            {
                const float distance = Distance3D(
                    world.player.x,
                    world.player.y,
                    world.player.z,
                    deathPosition_.x,
                    deathPosition_.y,
                    deathPosition_.z);

                if (distance > PrecisionReclaimDistance + 2.0f)
                {
                    Debug::Logger::Info(
                        "ROBUSTNESS 14G.4.3: drifted outside precision corpse reclaim radius; rerouting closer.");
                    StartPrecisionCorpseRoute(
                        world.player,
                        tick,
                        "drift outside precision reclaim radius");
                }
                else if (distance <= PrecisionReclaimDistance &&
                         DeathRecoveryPolicy::CanRetrieveCorpse(
                             lastProbe_.valid,
                             lastProbe_.isGhost,
                             lastProbe_.recoveryDelaySeconds,
                             distance) &&
                         DeathRecoveryPolicy::ShouldRetryAction(tick, nextActionTick_))
                {
                    retrieveCommandIssuedThisRecovery_ = false;
                    aliveProbeStreak_ = 0;

                    const bool issued = ExecuteLuaAction(
                        "if RetrieveCorpse then RetrieveCorpse(); end;",
                        "wow-internal/DeathRecoveryRetrieveCorpse.lua");

                    ++retrieveAttempts_;
                    retrieveCommandIssuedThisRecovery_ = issued;
                    Debug::Logger::Info(
                        "DEATH RECOVERY 14G.4.2: RetrieveCorpse command " +
                        std::string(issued ? "issued" : "failed") +
                        " attempt=" + std::to_string(retrieveAttempts_) +
                        " distance=" + Float(distance));

                    nextActionTick_ = tick + DeathRecoveryPolicy::ActionRetryTicks;
                    SetState(DeathRecoveryState::WaitingForAlive, tick);
                }
            }
            else if (state_ == DeathRecoveryState::WaitingForAlive)
            {
                if (freshProbe && DeathRecoveryPolicy::AliveAfterCorpseRun(
                        lastProbe_.valid,
                        lastProbe_.isGhost,
                        world.player.health,
                        world.player.maxHealth,
                        ghostConfirmedThisRecovery_,
                        retrieveCommandIssuedThisRecovery_,
                        aliveProbeStreak_))
                {
                    Debug::Logger::Info(
                        "DEATH RECOVERY 14G.4.2.1: resurrection accepted only after ghost + RetrieveCorpse + consecutive alive probes.");
                    CompleteRecovery(tick);
                    return;
                }

                if (lastProbe_.valid && lastProbe_.isGhost &&
                    DeathRecoveryPolicy::ShouldRetryAction(tick, nextActionTick_))
                {
                    retrieveCommandIssuedThisRecovery_ = false;
                    aliveProbeStreak_ = 0;

                    const float distance = Distance3D(
                        world.player.x,
                        world.player.y,
                        world.player.z,
                        deathPosition_.x,
                        deathPosition_.y,
                        deathPosition_.z);

                    if (retrieveAttempts_ >= RetrieveAttemptsBeforePrecisionReroute)
                    {
                        ++reclaimPrecisionRecoveries_;
                        Debug::Logger::Info(
                            "ROBUSTNESS 14G.4.3: CORPSE RETRIEVE STALLED after " +
                            std::to_string(retrieveAttempts_) +
                            " issued attempts at distance=" + Float(distance) +
                            "; forcing closer precision reposition before retry.");
                        retrieveAttempts_ = 0;
                        StartPrecisionCorpseRoute(
                            world.player,
                            tick,
                            "RetrieveCorpse remained ghost after bounded attempts");
                    }
                    else
                    {
                        SetState(DeathRecoveryState::WaitingForReclaim, tick);
                        nextActionTick_ = tick;
                    }
                }
            }

            if (lastProbe_.valid && lastProbe_.isGhost &&
                !routeAttemptedThisUpdate_ &&
                tick >= nextDeadStateLivenessTick_ &&
                tick >= lastPhysicalProgressTick_ + DeadStateStallTicks &&
                (state_ == DeathRecoveryState::RoutingToCorpse ||
                 state_ == DeathRecoveryState::WaitingForGhost ||
                 state_ == DeathRecoveryState::WaitingForReclaim ||
                 state_ == DeathRecoveryState::WaitingForAlive))
            {
                const float distance = Distance3D(
                    world.player.x,
                    world.player.y,
                    world.player.z,
                    deathPosition_.x,
                    deathPosition_.y,
                    deathPosition_.z);

                ++deadStateStalls_;
                nextDeadStateLivenessTick_ = tick + DeadStateStallTicks;
                retrieveCommandIssuedThisRecovery_ = false;
                aliveProbeStreak_ = 0;

                Debug::Logger::Info(
                    "ROBUSTNESS 14G.4.3: DEATH RECOVERY STALLED state=" +
                    std::string(StateNameInternal(state_)) +
                    " corpseDistance=" + Float(distance) +
                    " noPhysicalProgressTicks=" +
                    std::to_string(tick - lastPhysicalProgressTick_) +
                    "; restarting a bounded precision corpse route.");

                StartPrecisionCorpseRoute(
                    world.player,
                    tick,
                    "dead-state physical-progress watchdog");
            }

            if (tick >= lastStatusTick_ + DeathRecoveryPolicy::StatusLogTicks)
            {
                lastStatusTick_ = tick;
                const float distance = Distance3D(
                    world.player.x,
                    world.player.y,
                    world.player.z,
                    deathPosition_.x,
                    deathPosition_.y,
                    deathPosition_.z);

                Debug::Logger::Info(
                    std::string("Death14G4.2: state=") + StateNameInternal(state_) +
                    " ghost=" +
                    (lastProbe_.valid ? (lastProbe_.isGhost ? "yes" : "no") : "unknown") +
                    " reclaimDelay=" +
                    (lastProbe_.valid ? Float(lastProbe_.recoveryDelaySeconds) : "unknown") +
                    " corpseDistance=" + Float(distance) +
                    " releaseAttempts=" + std::to_string(releaseAttempts_) +
                    " retrieveAttempts=" + std::to_string(retrieveAttempts_) +
                    " routeStarts=" + std::to_string(routeStarts_) +
                    " precisionRoutes=" + std::to_string(precisionRouteStarts_) +
                    " deathStalls=" + std::to_string(deadStateStalls_) +
                    " reclaimPrecisionRecoveries=" +
                    std::to_string(reclaimPrecisionRecoveries_) +
                    " deathProgressAgeTicks=" +
                    std::to_string(tick - lastPhysicalProgressTick_) +
                    " ghostConfirmed=" + (ghostConfirmedThisRecovery_ ? "yes" : "no") +
                    " retrieveIssued=" + (retrieveCommandIssuedThisRecovery_ ? "yes" : "no") +
                    " aliveProbeStreak=" + std::to_string(aliveProbeStreak_));
            }
        }

        void Reset()
        {
            corpseNavigator_.reset();
            corpseNavigatorFullMapCounted_ = false;
            corpseNavigatorReadyCounted_ = false;
            corpseNavigatorPrecision_ = false;
            state_ = DeathRecoveryState::Idle;
            deathPosition_ = Navigation::NavPoint{};
            lastClearlyAlivePosition_ = Navigation::NavPoint{};
            haveLastClearlyAlivePosition_ = false;
            stateStartedTick_ = 0;
            nextActionTick_ = 0;
            nextProbeTick_ = 0;
            nextIdleProbeTick_ = 0;
            lastIdleProbeLogTick_ = 0;
            idleDeathConfirmedTick_ = 0;
            haveIdleProbeLog_ = false;
            lastStatusTick_ = 0;
            releaseAttempts_ = 0;
            retrieveAttempts_ = 0;
            routeStarts_ = 0;
            routeVariant_ = 0;
            lastPhysicalProgressTick_ = 0;
            nextDeadStateLivenessTick_ = 0;
            lastPhysicalProgressPosition_ = Navigation::NavPoint{};
            havePhysicalProgressPosition_ = false;
            precisionRouteStarts_ = 0;
            deadStateStalls_ = 0;
            reclaimPrecisionRecoveries_ = 0;
            deathEpisodeStartTime_ = SteadyClock::time_point{};
            lastPhysicalProgressTime_ = SteadyClock::time_point{};
            lastRouteAttemptTime_ = SteadyClock::time_point{};
            routeAttempts_ = 0;
            routeStartFailures_ = 0;
            fullMapFallbackAttempts_ = 0;
            fullMapFallbackAttemptsSinceProgress_ = 0;
            consecutiveStationaryRouteFailures_ = 0;
            failedAliveProbeStreak_ = 0;
            routeAttemptedThisUpdate_ = false;
            ghostConfirmedThisRecovery_ = false;
            bootstrappedFromGhost_ = false;
            retrieveCommandIssuedThisRecovery_ = false;
            aliveProbeStreak_ = 0;
            lastProbe_ = DeathProbe{};
        }

        DeathRecoveryState State() const { return state_; }
        const char* StateName() const { return StateNameInternal(state_); }
        bool IsActive() const
        {
            return state_ != DeathRecoveryState::Idle &&
                   state_ != DeathRecoveryState::Done &&
                   state_ != DeathRecoveryState::Failed;
        }
        bool IsDone() const { return state_ == DeathRecoveryState::Done; }
        bool IsFailed() const { return state_ == DeathRecoveryState::Failed; }
        bool LastClearlyAlivePosition(Navigation::NavPoint& position) const
        {
            if (!haveLastClearlyAlivePosition_)
                return false;
            position = lastClearlyAlivePosition_;
            return true;
        }
        int Recoveries() const { return recoveries_; }
        int ReleaseAttempts() const { return releaseAttempts_; }
        int RetrieveAttempts() const { return retrieveAttempts_; }
        int RouteStarts() const { return routeStarts_; }
        int PrecisionRouteStarts() const { return precisionRouteStarts_; }
        int DeadStateStalls() const { return deadStateStalls_; }
        int ReclaimPrecisionRecoveries() const { return reclaimPrecisionRecoveries_; }
        std::uint64_t DeathProgressAgeTicks(std::uint64_t tick) const
        {
            return lastPhysicalProgressTick_ == 0 || tick < lastPhysicalProgressTick_
                ? 0
                : tick - lastPhysicalProgressTick_;
        }
    };
}
