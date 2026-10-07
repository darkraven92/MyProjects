#pragma once

#include "NavigationInitializationLivenessPolicy.h"
#include "../Navigation/NavigationInitTelemetryPolicy.h"

#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class AutonomyActivity
    {
        None,
        Combat,
        Movement
    };

    enum class AutonomyEventKind
    {
        None,
        SoftStall,
        HardStall
    };

    struct AutonomySample
    {
        AutonomyActivity activity = AutonomyActivity::None;
        std::uint64_t identity = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        std::uint32_t targetHealth = 0;
        Navigation::NavigationInitializationObservation navigationInitialization{};
        std::uint64_t monotonicMs = 0;
    };

    struct AutonomyEvent
    {
        AutonomyEventKind kind = AutonomyEventKind::None;
        AutonomyActivity activity = AutonomyActivity::None;
        std::uint64_t stalledTicks = 0;
        const char* classification = "no_physical_or_hp_progress";
    };

    class AutonomySupervisor
    {
    private:
        static constexpr float CombatProgressDistance = 0.50f;
        static constexpr float MovementProgressDistance = 0.90f;

        // 250 ms polling cadence: combat warns at ~4 s and recovers at ~8 s.
        static constexpr std::uint64_t CombatSoftTicks = 16;
        static constexpr std::uint64_t CombatHardTicks = 32;

        // Long-distance movement gets a little more tolerance.
        static constexpr std::uint64_t MovementSoftTicks = 24;
        static constexpr std::uint64_t MovementHardTicks = 48;

        bool initialized_ = false;
        bool softLatched_ = false;
        AutonomyActivity activity_ = AutonomyActivity::None;
        std::uint64_t identity_ = 0;
        std::uint64_t lastProgressTick_ = 0;
        float anchorX_ = 0.0f;
        float anchorY_ = 0.0f;
        float anchorZ_ = 0.0f;
        std::uint32_t lastTargetHealth_ = 0;
        int softEvents_ = 0;
        int hardEvents_ = 0;
        Navigation::NavigationInitializationObservation planning_{};
        std::uint64_t planningStartedMs_ = 0;
        std::uint64_t planningProgressTick_ = 0;
        bool planningFailureLatched_ = false;

        static float Distance3D(
            float ax, float ay, float az,
            float bx, float by, float bz)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        static std::uint64_t SoftTicks(AutonomyActivity activity)
        {
            return activity == AutonomyActivity::Combat
                ? CombatSoftTicks
                : MovementSoftTicks;
        }

        static std::uint64_t HardTicks(AutonomyActivity activity)
        {
            return activity == AutonomyActivity::Combat
                ? CombatHardTicks
                : MovementHardTicks;
        }

        static float ProgressDistance(AutonomyActivity activity)
        {
            return activity == AutonomyActivity::Combat
                ? CombatProgressDistance
                : MovementProgressDistance;
        }

        void Baseline(const AutonomySample& sample, std::uint64_t tick)
        {
            initialized_ = sample.activity != AutonomyActivity::None;
            softLatched_ = false;
            activity_ = sample.activity;
            identity_ = sample.identity;
            lastProgressTick_ = tick;
            anchorX_ = sample.x;
            anchorY_ = sample.y;
            anchorZ_ = sample.z;
            lastTargetHealth_ = sample.targetHealth;
        }

    public:
        AutonomyEvent Update(const AutonomySample& sample, std::uint64_t tick)
        {
            if (sample.activity == AutonomyActivity::None)
            {
                initialized_ = false;
                softLatched_ = false;
                activity_ = AutonomyActivity::None;
                identity_ = 0;
                planning_ = {};
                planningFailureLatched_ = false;
                return {};
            }

            if (sample.activity == AutonomyActivity::Movement &&
                sample.navigationInitialization.Valid())
            {
                const auto& work = sample.navigationInitialization;
                const bool regressed = planning_.pending && work.intent == planning_.intent &&
                    (static_cast<int>(work.tier) < static_cast<int>(planning_.tier) ||
                     (work.tier == planning_.tier &&
                      (work.tilesProcessed < planning_.tilesProcessed ||
                       work.tilesTotal != planning_.tilesTotal)));
                if (!planning_.pending || work.intent != planning_.intent)
                {
                    planningStartedMs_ = sample.monotonicMs;
                    planningProgressTick_ = tick;
                    planningFailureLatched_ = false;
                }
                else if ((work.tier == planning_.tier &&
                          work.tilesProcessed > planning_.tilesProcessed) ||
                         static_cast<int>(work.tier) > static_cast<int>(planning_.tier))
                {
                    // Tile/tier work is planning liveness ONLY. Never refund
                    // a follower recovery budget or report physical progress.
                    planningProgressTick_ = tick;
                }
                planning_ = work;
                const bool budgetExhausted = sample.monotonicMs < planningStartedMs_ ||
                    sample.monotonicMs - planningStartedMs_ >=
                        static_cast<std::uint64_t>(
                            NavigationInitializationLivenessPolicy::MaximumPendingMs);
                const bool frozen = tick < planningProgressTick_ ||
                    tick - planningProgressTick_ >= MovementHardTicks;
                if ((budgetExhausted || frozen || regressed) && !planningFailureLatched_)
                {
                    planningFailureLatched_ = true;
                    ++hardEvents_;
                    return {AutonomyEventKind::HardStall, AutonomyActivity::Movement,
                        tick >= planningProgressTick_ ? tick - planningProgressTick_ : 0,
                        regressed ? "initialization_evidence_regressed" :
                        (budgetExhausted ? "initialization_budget_exhausted" : "initialization_frozen")};
                }
                return {};
            }
            if (planning_.pending)
            {
                planning_ = {};
                planningFailureLatched_ = false;
                // Start the unchanged execution watchdog when movement can
                // actually begin, not when the owner requested tile loading.
                Baseline(sample, tick);
                return {};
            }

            if (!initialized_ ||
                sample.activity != activity_ ||
                sample.identity != identity_)
            {
                Baseline(sample, tick);
                return {};
            }

            const float displacement = Distance3D(
                anchorX_, anchorY_, anchorZ_,
                sample.x, sample.y, sample.z);

            const bool physicalProgress =
                displacement >= ProgressDistance(sample.activity);

            const bool combatDamageProgress =
                sample.activity == AutonomyActivity::Combat &&
                lastTargetHealth_ > 0 &&
                sample.targetHealth > 0 &&
                sample.targetHealth < lastTargetHealth_;

            if (physicalProgress || combatDamageProgress)
            {
                Baseline(sample, tick);
                return {};
            }

            // Track healing/other target-health changes without counting them
            // as forward progress so the next real damage event is observable.
            lastTargetHealth_ = sample.targetHealth;

            const std::uint64_t stalledTicks = tick - lastProgressTick_;

            if (stalledTicks >= HardTicks(sample.activity))
            {
                ++hardEvents_;
                Baseline(sample, tick);
                return {
                    AutonomyEventKind::HardStall,
                    sample.activity,
                    stalledTicks
                };
            }

            if (!softLatched_ && stalledTicks >= SoftTicks(sample.activity))
            {
                softLatched_ = true;
                ++softEvents_;
                return {
                    AutonomyEventKind::SoftStall,
                    sample.activity,
                    stalledTicks
                };
            }

            return {};
        }

        const char* ActivityName() const
        {
            if (planning_.pending)
                return "NavigationInitialization";
            switch (activity_)
            {
                case AutonomyActivity::Combat: return "Combat";
                case AutonomyActivity::Movement: return "Movement";
                default: return "None";
            }
        }

        std::uint64_t ProgressAgeTicks(std::uint64_t tick) const
        {
            if (!initialized_ || tick < lastProgressTick_)
                return 0;
            return tick - lastProgressTick_;
        }

        int SoftEvents() const { return softEvents_; }
        int HardEvents() const { return hardEvents_; }
    };
}
