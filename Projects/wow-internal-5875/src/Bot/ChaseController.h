#pragma once

#include "MovementController.h"
#include "ChaseProgressWatchdog.h"
#include "TargetSelector.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    enum class ChaseState
    {
        Idle,
        Chasing,
        InRange,
        TargetLost,
        TargetDead,
        Failed
    };

    enum class ChaseFailureReason
    {
        None, TargetInvalid, TargetUnreachable, ApproachPlanRejected,
        MovementCommandRejected, RecoveryExhausted
    };

    inline const char* ChaseFailureName(ChaseFailureReason reason)
    {
        switch (reason)
        {
            case ChaseFailureReason::TargetUnreachable: return "target_unreachable";
            case ChaseFailureReason::TargetInvalid: return "target_invalid";
            case ChaseFailureReason::ApproachPlanRejected: return "approach_plan_rejected";
            case ChaseFailureReason::MovementCommandRejected: return "movement_command_rejected";
            case ChaseFailureReason::RecoveryExhausted: return "chase_no_physical_progress";
            default: return "none";
        }
    }

    class ChaseController
    {
    private:
        // =============================================
        // Range configuration
        // =============================================

        static constexpr float StandOffDistance =
            3.5f;

        /*
         * När target är <= 5 yards betraktar vi
         * spelaren som inom melee-range för denna fas.
         */
        static constexpr float InRangeDistance =
            5.0f;

        /*
         * Hysteresis:
         *
         * <= 5.0 yd  -> InRange
         * 5.0-6.0 yd -> håll nuvarande state
         * > 6.0 yd   -> börja chase igen
         *
         * Detta förhindrar att botten startar/stannar
         * CTM hela tiden runt samma gränsvärde.
         */
        static constexpr float ResumeChaseDistance =
            6.0f;

        /*
         * Vi tillåter inte att movement-only chase
         * följer ett target hur långt som helst.
         *
         * Senare blir navigation/navmesh ansvarig för
         * långdistansförflyttning.
         */
        static constexpr float MaximumChaseDistance =
            80.0f;

        // =============================================
        // Replanning
        // =============================================

        /*
         * Om target flyttat minst 1.5 yd sedan
         * senaste approach-planen byggdes replannar vi.
         */
        static constexpr float TargetMoveReplanDistance =
            1.5f;

        /*
         * Om spelaren redan nått gamla destinationen
         * men target fortfarande är utanför chase-range
         * måste destinationen uppdateras.
         */
        static constexpr float DestinationReachedDistance =
            1.25f;

        /*
         * WorldMonitor kör 4 gånger per sekund.
         *
         * 4 ticks = cirka 1 sekund.
         *
         * Vi skickar alltså maximalt ungefär ett nytt
         * CTM-kommando per sekund.
         */
        static constexpr std::uint64_t CommandCooldownTicks =
            4;

        // =============================================
        // Stuck detection
        // =============================================

        /*
         * Phase 14G.3.1: use net physical progress plus gain toward the live
         * target instead of per-tick jitter. Tiny back/forth movement must not
         * reset stuck detection indefinitely.
         */
        static constexpr float WatchdogMeaningfulNetMovement =
            0.65f;

        static constexpr float WatchdogMeaningfulTargetGain =
            0.75f;

        // 8 * 250 ms ~= 2 s: replan early.
        static constexpr std::uint64_t SuspectedStallTicks =
            8;

        // 16 * 250 ms ~= 4 s: count a hard recovery episode.
        static constexpr std::uint64_t HardStallTicks =
            16;

        /*
         * Recovery remains bounded. If several fresh CTM plans produce no
         * measurable net progress, CombatController will blacklist the mob
         * instead of hammering the same obstacle forever.
         */
        static constexpr int MaximumConsecutiveStuckRecoveries =
            5;

        // =============================================
        // Runtime state
        // =============================================

        ChaseState state_ =
            ChaseState::Idle;
        ChaseFailureReason failureReason_ = ChaseFailureReason::None;

        std::uint64_t targetGuid_ =
            0;

        std::uint64_t lastCommandTick_ =
            0;

        ApproachPlan plan_{};

        float lastPlayerX_ =
            0.0f;

        float lastPlayerY_ =
            0.0f;

        float lastPlayerZ_ =
            0.0f;

        int stuckTicks_ =
            0;

        int consecutiveStuckRecoveries_ =
            0;

        int commands_ =
            0;

        int replans_ =
            0;

        ChaseProgressWatchdog movementWatchdog_{
            MovementProgressWatchdogConfig{
                WatchdogMeaningfulNetMovement,
                WatchdogMeaningfulTargetGain,
                SuspectedStallTicks,
                HardStallTicks
            }
        };

        // =============================================
        // Helpers
        // =============================================

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(2)
                << value;

            return stream.str();
        }

        static std::string Hex64(
            std::uint64_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(16)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static float Distance(
            float ax,
            float ay,
            float az,
            float bx,
            float by,
            float bz)
        {
            const float dx =
                bx - ax;

            const float dy =
                by - ay;

            const float dz =
                bz - az;

            return std::sqrt(
                dx * dx +
                dy * dy +
                dz * dz
            );
        }

        static const char* StateNameInternal(
            ChaseState state)
        {
            switch (state)
            {
                case ChaseState::Idle:
                    return "Idle";

                case ChaseState::Chasing:
                    return "Chasing";

                case ChaseState::InRange:
                    return "InRange";

                case ChaseState::TargetLost:
                    return "TargetLost";

                case ChaseState::TargetDead:
                    return "TargetDead";

                case ChaseState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            ChaseState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "ChaseController state: "
                ) +
                StateNameInternal(
                    state_
                ) +
                " -> " +
                StateNameInternal(
                    newState
                )
            );

            state_ =
                newState;
        }

        void ResetMovementTracking(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            lastPlayerX_ = player.x;
            lastPlayerY_ = player.y;
            lastPlayerZ_ = player.z;
            stuckTicks_ = 0;
            movementWatchdog_.Reset(
                player.x,
                player.y,
                player.z,
                target.x,
                target.y,
                target.z,
                target.distance,
                tick);
        }

        bool CooldownReady(
            std::uint64_t tick) const
        {
            /*
             * commands_ == 0 gör att första CTM-kommandot
             * får skickas direkt.
             */
            return
                commands_ == 0 ||
                tick >=
                    lastCommandTick_ +
                    CommandCooldownTicks;
        }

        bool IssuePlan(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick,
            bool replan)
        {
            ApproachPlan newPlan{};

            if (!MovementController::
                    BuildApproachPlan(
                        player,
                        target,
                        StandOffDistance,
                        newPlan
                    ))
            {
                failureReason_ = ChaseFailureReason::ApproachPlanRejected;
                Debug::Logger::Info(
                    "ChaseController: "
                    "BuildApproachPlan failed."
                );

                return false;
            }

            Debug::Logger::Info(
                replan
                    ? "CHASE: issuing replan."
                    : "CHASE: issuing movement plan."
            );

            Debug::Logger::Info(
                "CHASE locked GUID: " +
                Hex64(
                    targetGuid_
                )
            );

            Debug::Logger::Info(
                "CHASE target distance: " +
                Float(
                    target.distance
                )
            );

            Debug::Logger::Info(
                "CHASE destination: (" +
                Float(
                    newPlan.destinationX
                ) +
                "," +
                Float(
                    newPlan.destinationY
                ) +
                "," +
                Float(
                    newPlan.destinationZ
                ) +
                ")"
            );

            if (!MovementController::
                    MoveToApproachPoint(
                        player,
                        newPlan
                    ))
            {
                failureReason_ = ChaseFailureReason::MovementCommandRejected;
                Debug::Logger::Info(
                    "ChaseController: "
                    "ClickToMove command rejected."
                );

                return false;
            }

            plan_ =
                newPlan;

            lastCommandTick_ =
                tick;

            ++commands_;

            if (replan)
            {
                ++replans_;
            }

            return true;
        }

        void Fail(
            const std::string& reason,
            ChaseFailureReason kind)
        {
            failureReason_ = kind;
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "CHASE: FAILED"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );
            Debug::Logger::Info(std::string("COMBAT CHASE TERMINAL reason=") +
                ChaseFailureName(kind) + " guid=" + Hex64(targetGuid_));

            Debug::Logger::Info(
                "Locked GUID: " +
                Hex64(
                    targetGuid_
                )
            );

            Debug::Logger::Info(
                "Commands: " +
                std::to_string(
                    commands_
                )
            );

            Debug::Logger::Info(
                "Replans: " +
                std::to_string(
                    replans_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                ChaseState::Failed
            );
        }

    public:
        // =============================================
        // Start
        // =============================================

        bool Start(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            failureReason_ = ChaseFailureReason::None;
            if (
                state_ == ChaseState::Chasing ||
                state_ == ChaseState::InRange)
            {
                Debug::Logger::Info(
                    "ChaseController: "
                    "Start rejected because chase "
                    "is already active."
                );

                return false;
            }

            if (!player.valid)
            {
                Debug::Logger::Info(
                    "ChaseController: "
                    "invalid player."
                );

                return false;
            }

            if (
                !target.valid ||
                target.guid == 0 ||
                target.health == 0)
            {
                failureReason_ = ChaseFailureReason::TargetInvalid;
                Debug::Logger::Info(
                    "ChaseController: "
                    "invalid target."
                );

                return false;
            }

            if (
                !TargetSelector::
                    IsAllowedEntry(
                        target.entryId
                    ))
            {
                failureReason_ = ChaseFailureReason::TargetInvalid;
                Debug::Logger::Info(
                    "ChaseController: "
                    "target entry is not allowed."
                );

                return false;
            }

            if (
                target.distance >
                    MaximumChaseDistance)
            {
                failureReason_ = ChaseFailureReason::TargetUnreachable;
                Debug::Logger::Info(
                    "ChaseController: "
                    "target is beyond maximum "
                    "chase distance."
                );

                return false;
            }

            targetGuid_ =
                target.guid;
            failureReason_ = ChaseFailureReason::None;

            lastCommandTick_ =
                tick;

            plan_ =
                ApproachPlan{};

            stuckTicks_ =
                0;

            consecutiveStuckRecoveries_ =
                0;

            commands_ =
                0;

            replans_ =
                0;

            ResetMovementTracking(
                player,
                target,
                tick
            );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "CONTINUOUS CHASE: START"
            );

            Debug::Logger::Info(
                "Locked GUID: " +
                Hex64(
                    targetGuid_
                )
            );

            Debug::Logger::Info(
                "Initial target distance: " +
                Float(
                    target.distance
                )
            );

            Debug::Logger::Info(
                "Range hysteresis: "
                "<=5.0 InRange, "
                ">6.0 resume chase."
            );

            Debug::Logger::Info(
                "No automatic chase timeout."
            );

            /*
             * Target är redan nära.
             *
             * Skicka ingen CTM. Vänta tills mobben
             * faktiskt går utanför ResumeChaseDistance.
             */
            if (
                target.distance <=
                    InRangeDistance)
            {
                SetState(
                    ChaseState::InRange
                );

                /*
                 * Phase 14G.3.2: combat can inherit a still-active CTM from
                 * NavMesh staging or a rapidly completed approach. Replace
                 * it with a zero-distance verified Move command before the
                 * facing guard takes ownership.
                 */
                if (!MovementController::HoldPosition(player))
                {
                    Debug::Logger::Info(
                        "CHASE 14G.3.2: MELEE HOLD command failed; facing guard will continue defensively."
                    );
                }

                Debug::Logger::Info(
                    "CONTINUOUS CHASE: "
                    "target already in range."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return true;
            }

            /*
             * Om vi startar på 5-6 yd vill vi fortfarande
             * närma oss target första gången.
             */
            SetState(
                ChaseState::Chasing
            );

            if (!IssuePlan(
                    player,
                    target,
                    tick,
                    false))
            {
                Fail(
                    "initial movement command failed.",
                    failureReason_
                );

                return false;
            }

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        // =============================================
        // Update
        // =============================================

        void Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (
                state_ != ChaseState::Chasing &&
                state_ != ChaseState::InRange)
            {
                return;
            }

            const auto* target =
                TargetSelector::
                    FindByGuid(
                        world,
                        targetGuid_
                    );

            if (target == nullptr)
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "CONTINUOUS CHASE: "
                    "target lost."
                );

                Debug::Logger::Info(
                    "Locked GUID: " +
                    Hex64(
                        targetGuid_
                    )
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    ChaseState::TargetLost
                );

                return;
            }

            if (target->health == 0)
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "CONTINUOUS CHASE: "
                    "target dead."
                );

                Debug::Logger::Info(
                    "Locked GUID: " +
                    Hex64(
                        targetGuid_
                    )
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    ChaseState::TargetDead
                );

                return;
            }

            if (
                target->distance >
                    MaximumChaseDistance)
            {
                Fail(
                    "target exceeded maximum "
                    "chase distance.",
                    ChaseFailureReason::TargetUnreachable
                );

                return;
            }

            // =========================================
            // Enter melee range
            // =========================================

            if (
                target->distance <=
                    InRangeDistance)
            {
                if (
                    state_ !=
                        ChaseState::InRange)
                {
                    Debug::Logger::Info(
                        "================================"
                    );

                    Debug::Logger::Info(
                        "CONTINUOUS CHASE: "
                        "entered range."
                    );

                    Debug::Logger::Info(
                        "Locked GUID: " +
                        Hex64(
                            targetGuid_
                        )
                    );

                    Debug::Logger::Info(
                        "Target distance: " +
                        Float(
                            target->distance
                        )
                    );

                    Debug::Logger::Info(
                        "Commands: " +
                        std::to_string(
                            commands_
                        )
                    );

                    Debug::Logger::Info(
                        "Replans: " +
                        std::to_string(
                            replans_
                        )
                    );

                    Debug::Logger::Info(
                        "================================"
                    );

                    SetState(
                        ChaseState::InRange
                    );

                    if (!MovementController::HoldPosition(world.player))
                    {
                        Debug::Logger::Info(
                            "CHASE 14G.3.2: MELEE HOLD command failed after chase arrival; facing guard will continue defensively."
                        );
                    }
                }

                stuckTicks_ =
                    0;

                consecutiveStuckRecoveries_ =
                    0;

                movementWatchdog_.Suspend(
                    world.player.x,
                    world.player.y,
                    world.player.z,
                    target->x,
                    target->y,
                    target->z,
                    target->distance,
                    tick);

                lastPlayerX_ =
                    world.player.x;

                lastPlayerY_ =
                    world.player.y;

                lastPlayerZ_ =
                    world.player.z;

                return;
            }

            // =========================================
            // InRange hysteresis
            // =========================================

            if (
                state_ ==
                    ChaseState::InRange)
            {
                /*
                 * Target har lämnat 5 yd men är ännu
                 * inte längre bort än 6 yd.
                 *
                 * Gör ingenting.
                 */
                if (
                    target->distance <=
                        ResumeChaseDistance)
                {
                    lastPlayerX_ =
                        world.player.x;

                    lastPlayerY_ =
                        world.player.y;

                    lastPlayerZ_ =
                        world.player.z;

                    movementWatchdog_.Suspend(
                        world.player.x,
                        world.player.y,
                        world.player.z,
                        target->x,
                        target->y,
                        target->z,
                        target->distance,
                        tick);

                    return;
                }

                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "CONTINUOUS CHASE: "
                    "target left melee range."
                );

                Debug::Logger::Info(
                    "Target distance: " +
                    Float(
                        target->distance
                    )
                );

                Debug::Logger::Info(
                    "Resuming chase."
                );

                Debug::Logger::Info(
                    "================================"
                );

                /*
                 * Gamla planen är nu stale.
                 */
                plan_ =
                    ApproachPlan{};

                stuckTicks_ =
                    0;

                SetState(
                    ChaseState::Chasing
                );
            }

            // =========================================
            // Chasing
            // =========================================

            if (
                state_ ==
                    ChaseState::Chasing)
            {
                /*
                 * Chase progress is target-relative. Sideways motion or a
                 * dispatched command cannot refund a stalled chase episode.
                 */
                const auto movementObservation =
                    movementWatchdog_.Update(
                        world.player.x,
                        world.player.y,
                        world.player.z,
                        target->x,
                        target->y,
                        target->z,
                        target->distance,
                        tick,
                        target->distance > ResumeChaseDistance);

                stuckTicks_ = static_cast<int>(
                    movementObservation.stalledTicks);

                if (movementObservation.progressed)
                {
                    if (consecutiveStuckRecoveries_ > 0)
                    {
                        Debug::Logger::Info(
                            "CHASE 14G.3.1: movement watchdog observed real progress; "
                            "recovery episode cleared.");
                    }
                    consecutiveStuckRecoveries_ = 0;
                }

                const bool suspectedStall =
                    movementObservation.severity ==
                        MovementStallSeverity::Suspected;

                const bool hardStall =
                    movementObservation.severity ==
                        MovementStallSeverity::Hard;

                const bool cooldownReady =
                    CooldownReady(
                        tick
                    );

                float targetMovement =
                    0.0f;

                float destinationDistance =
                    0.0f;

                bool targetMovedEnough =
                    false;

                bool oldDestinationReached =
                    false;

                if (plan_.valid)
                {
                    targetMovement =
                        Distance(
                            target->x,
                            target->y,
                            target->z,

                            plan_.targetX,
                            plan_.targetY,
                            plan_.targetZ
                        );

                    destinationDistance =
                        Distance(
                            world.player.x,
                            world.player.y,
                            world.player.z,

                            plan_.destinationX,
                            plan_.destinationY,
                            plan_.destinationZ
                        );

                    targetMovedEnough =
                        targetMovement >=
                            TargetMoveReplanDistance;

                    oldDestinationReached =
                        destinationDistance <=
                            DestinationReachedDistance;
                }

                const bool noPlan =
                    !plan_.valid;

                const bool stuckRecovery =
                    suspectedStall || hardStall;

                /*
                 * Replan behövs bara om target är
                 * utanför ResumeChaseDistance.
                 *
                 * Mellan 5 och 6 yd får befintlig
                 * approach-rörelse slutföras.
                 */
                const bool needsMovementUpdate =
                    target->distance >
                        ResumeChaseDistance &&
                    (
                        noPlan ||
                        targetMovedEnough ||
                        oldDestinationReached ||
                        stuckRecovery
                    );

                if (
                    cooldownReady &&
                    needsMovementUpdate)
                {
                    Debug::Logger::Info(
                        "CHASE: replan condition."
                    );

                    Debug::Logger::Info(
                        "Target distance: " +
                        Float(
                            target->distance
                        )
                    );

                    if (plan_.valid)
                    {
                        Debug::Logger::Info(
                            "Target moved since plan: " +
                            Float(
                                targetMovement
                            )
                        );

                        Debug::Logger::Info(
                            "Old destination distance: " +
                            Float(
                                destinationDistance
                            )
                        );
                    }
                    else
                    {
                        Debug::Logger::Info(
                            "No active approach plan."
                        );
                    }

                    Debug::Logger::Info(
                        "Stuck ticks: " +
                        std::to_string(
                            stuckTicks_
                        )
                    );

                    if (stuckRecovery)
                    {
                        ++consecutiveStuckRecoveries_;

                        Debug::Logger::Info(
                            std::string("CHASE 14G.3.1: ") +
                            (hardStall ? "HARD STALL" : "SUSPECTED STALL") +
                            " recovery attempt=" +
                            std::to_string(consecutiveStuckRecoveries_) +
                            "/" +
                            std::to_string(MaximumConsecutiveStuckRecoveries) +
                            " stalledTicks=" +
                            std::to_string(movementObservation.stalledTicks) +
                            " netMovement=" +
                            Float(movementObservation.netMovement) +
                            " targetGain=" +
                            Float(movementObservation.goalGain));

                        if (
                            consecutiveStuckRecoveries_ >
                                MaximumConsecutiveStuckRecoveries)
                        {
                            Fail(
                                "maximum stuck recovery "
                                "attempts exceeded.",
                                ChaseFailureReason::RecoveryExhausted
                            );

                            return;
                        }
                    }

                    const bool isReplan =
                        commands_ > 0;

                    if (!IssuePlan(
                            world.player,
                            *target,
                            tick,
                            isReplan))
                    {
                        Fail(
                            "movement command failed.",
                            failureReason_
                        );

                        return;
                    }

                    if (stuckRecovery)
                    {
                        movementWatchdog_.BeginRecoveryWindow(
                            world.player.x,
                            world.player.y,
                            world.player.z,
                            target->x,
                            target->y,
                            target->z,
                            target->distance,
                            tick);
                    }
                }
            }

            lastPlayerX_ =
                world.player.x;

            lastPlayerY_ =
                world.player.y;

            lastPlayerZ_ =
                world.player.z;
        }

        // =============================================
        // Stop
        // =============================================

        void Stop()
        {
            Debug::Logger::Info(
                "ChaseController: stopped."
            );

            /*
             * Vi skickar INTE CTM Stop (0x03) här.
             *
             * Den funktionen är ännu inte separat
             * verifierad i klienten.
             */
            state_ =
                ChaseState::Idle;
            failureReason_ = ChaseFailureReason::None;

            targetGuid_ =
                0;

            plan_ =
                ApproachPlan{};

            stuckTicks_ =
                0;

            consecutiveStuckRecoveries_ =
                0;

            commands_ =
                0;

            replans_ =
                0;
        }

        // =============================================
        // State queries
        // =============================================

        ChaseState State() const
        {
            return state_;
        }

        const char* StateName() const
        {
            return StateNameInternal(
                state_
            );
        }

        bool IsActive() const
        {
            return
                state_ ==
                    ChaseState::Chasing ||
                state_ ==
                    ChaseState::InRange;
        }

        bool IsChasing() const
        {
            return
                state_ ==
                    ChaseState::Chasing;
        }

        bool IsInRange() const
        {
            return
                state_ ==
                    ChaseState::InRange;
        }

        bool TargetLost() const
        {
            return
                state_ ==
                    ChaseState::TargetLost;
        }

        bool TargetDead() const
        {
            return
                state_ ==
                    ChaseState::TargetDead;
        }

        bool Failed() const
        {
            return
                state_ ==
                    ChaseState::Failed;
        }

        ChaseFailureReason FailureReason() const { return failureReason_; }

        std::uint64_t TargetGuid() const
        {
            return targetGuid_;
        }

        int Commands() const
        {
            return commands_;
        }

        int Replans() const
        {
            return replans_;
        }
    };
}
