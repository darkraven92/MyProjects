#pragma once

#include "ClickToMoveController.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    enum class QuestObjectiveTravelState
    {
        Idle,
        Travelling,
        Done,
        Failed
    };

    class QuestObjectiveTravelController
    {
    private:
        struct Waypoint
        {
            float x;
            float y;
            float z;
            const char* name;
        };

        struct Position2D
        {
            float x;
            float y;
        };

        /*
         * Phase 9B objective routing for:
         *
         *   792 Vile Familiars
         *   target entry 3101
         *
         * The first part of this route reuses Valley of
         * Trials positions that have already been traversed
         * successfully by the live 7B.4 / 8A routing tests.
         *
         * The final objective positions come from existing
         * quest-profile hunting grounds for Vile Familiars.
         *
         * This remains CTM waypoint routing with bounded
         * local obstacle recovery. It is not a navmesh.
         */
        static constexpr float WaypointTolerance =
            4.0f;

        static constexpr float ProgressThreshold =
            0.75f;

        static constexpr std::uint64_t StuckTicks =
            16;

        static constexpr std::uint64_t DetourTimeoutTicks =
            12;

        static constexpr float DetourReachedTolerance =
            2.5f;

        static constexpr float DetourMovementSuccess =
            3.25f;

        static constexpr float SmallDetourRadius =
            7.0f;

        static constexpr float LargeDetourRadius =
            11.0f;

        static constexpr float DetourBackoff =
            1.5f;

        static constexpr int MaximumDetourAttempts =
            4;

        static constexpr float ObstacleClearedProgress =
            6.0f;

        static constexpr int MaximumTotalDetours =
            12;

        static constexpr std::uint32_t VileFamiliarEntry =
            3101;

        static constexpr float ObjectiveAreaX =
            -246.56f;

        static constexpr float ObjectiveAreaY =
            -4279.26f;

        static constexpr float ObjectiveAreaRadius =
            75.0f;

        static constexpr float PatrolArrivalRadius =
            7.0f;

        static constexpr std::uint64_t PatrolCommandCooldownTicks =
            16;

        /*
         * Reactive patrol navigation. Unlike the Phase 9B
         * patrol, we do not blindly reissue the same CTM
         * destination forever.
         */
        static constexpr float PatrolProgressThreshold =
            0.75f;

        static constexpr std::uint64_t PatrolStuckTicks =
            16;

        static constexpr std::uint64_t PatrolDetourTimeoutTicks =
            12;

        static constexpr float PatrolDetourMovementSuccess =
            2.75f;

        static constexpr float PatrolDetourReachedTolerance =
            2.25f;

        static constexpr int MaximumPatrolDetourAttempts =
            6;

        static constexpr int MaximumPatrolHotspotSkipsPerCycle =
            4;

        /*
         * Route from the Zureetha / The Den area into the
         * western edge of the Vile Familiar hunting grounds.
         *
         * The first three points are shared with already
         * proven Valley of Trials routing.
         */
        inline static constexpr Waypoint VileFamiliarsRoute[] =
        {
            {
                -610.073f,
                -4253.520f,
                39.03930f,
                "The Den south exit"
            },
            {
                -516.5089f,
                -4292.180f,
                39.22541f,
                "Open Valley staging 1"
            },
            {
                -475.5253f,
                -4280.907f,
                44.80814f,
                "Open Valley staging 2"
            },
            {
                -246.56f,
                -4279.26f,
                61.64f,
                "Vile Familiar west hunting ground"
            }
        };

        /*
         * If there is temporarily no Vile Familiar within
         * combat-selection range, patrol these objective
         * hotspots instead of standing still indefinitely.
         */
        inline static constexpr Waypoint VileFamiliarsPatrol[] =
        {
            {
                -246.56f,
                -4279.26f,
                61.64f,
                "Vile Familiar patrol 1"
            },
            {
                -253.59f,
                -4316.13f,
                56.04f,
                "Vile Familiar patrol 2"
            },
            {
                -198.09f,
                -4333.76f,
                68.35f,
                "Vile Familiar patrol 3"
            },
            {
                -211.55f,
                -4351.95f,
                64.49f,
                "Vile Familiar patrol 4"
            },
            {
                -213.20f,
                -4382.11f,
                63.66f,
                "Vile Familiar patrol 5"
            },
            {
                -252.91f,
                -4381.57f,
                62.57f,
                "Vile Familiar patrol 6"
            }
        };

        QuestObjectiveTravelState state_ =
            QuestObjectiveTravelState::Idle;

        std::size_t waypointIndex_ =
            0;

        std::uint64_t lastCommandTick_ =
            0;

        std::uint64_t lastProgressTick_ =
            0;

        float bestDistance_ =
            0.0f;

        int commands_ =
            0;

        int reissues_ =
            0;

        int detours_ =
            0;

        int detourAttempt_ =
            0;

        int totalDetours_ =
            0;

        bool obstacleBaselineValid_ =
            false;

        float obstacleStartMainDistance_ =
            0.0f;

        std::size_t patrolIndex_ =
            0;

        std::uint64_t lastPatrolCommandTick_ =
            0;

        int patrolCommands_ =
            0;

        bool patrolMoveActive_ =
            false;

        bool patrolDetourActive_ =
            false;

        std::uint64_t patrolLastProgressTick_ =
            0;

        std::uint64_t patrolDetourStartTick_ =
            0;

        std::uint64_t patrolLastUpdateTick_ =
            0;

        float patrolBestDistance_ =
            0.0f;

        int patrolDetourAttempt_ =
            0;

        int patrolDetours_ =
            0;

        int patrolHotspotSkips_ =
            0;

        Position2D patrolDetourStart_{};

        Position2D patrolDetourDestination_{};

        bool detourActive_ =
            false;

        std::uint64_t detourStartTick_ =
            0;

        Position2D detourStart_{};

        Position2D detourDestination_{};

        static float Distance2D(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx =
                bx - ax;

            const float dy =
                by - ay;

            return std::sqrt(
                dx * dx +
                dy * dy
            );
        }

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(3)
                << value;

            return stream.str();
        }

        static const char* StateNameInternal(
            QuestObjectiveTravelState state)
        {
            switch (state)
            {
                case QuestObjectiveTravelState::Idle:
                    return "Idle";

                case QuestObjectiveTravelState::Travelling:
                    return "Travelling";

                case QuestObjectiveTravelState::Done:
                    return "Done";

                case QuestObjectiveTravelState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            QuestObjectiveTravelState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "QuestObjectiveTravelController state: "
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

        static constexpr std::size_t RouteCount()
        {
            return
                sizeof(
                    VileFamiliarsRoute
                ) /
                sizeof(
                    VileFamiliarsRoute[0]
                );
        }

        static constexpr std::size_t PatrolCount()
        {
            return
                sizeof(
                    VileFamiliarsPatrol
                ) /
                sizeof(
                    VileFamiliarsPatrol[0]
                );
        }

        void LogPlayerPosition(
            const Objects::PlayerState& player,
            const char* prefix)
        {
            Debug::Logger::Info(
                std::string(
                    prefix
                ) +
                " (" +
                Float(
                    player.x
                ) +
                "," +
                Float(
                    player.y
                ) +
                "," +
                Float(
                    player.z
                ) +
                ")"
            );
        }

        bool IssuePosition(
            const Objects::PlayerState& player,
            float x,
            float y,
            float z,
            const char* reason)
        {
            Debug::Logger::Info(
                std::string(
                    "QUEST 792 TRAVEL CTM: "
                ) +
                reason
            );

            Debug::Logger::Info(
                "Destination: (" +
                Float(x) +
                "," +
                Float(y) +
                "," +
                Float(z) +
                ")"
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        x,
                        y,
                        z,
                        0.75f
                    ))
            {
                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: CTM command failed."
                );

                return false;
            }

            ++commands_;

            lastCommandTick_ =
                GetTickCount();

            Debug::Logger::Info(
                "QUEST 792 TRAVEL: CTM command issued."
            );

            return true;
        }

        bool IssueCurrentWaypoint(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            bool reissue)
        {
            if (
                waypointIndex_ >=
                    RouteCount())
            {
                SetState(
                    QuestObjectiveTravelState::Done
                );

                return true;
            }

            const auto& waypoint =
                VileFamiliarsRoute[
                    waypointIndex_
                ];

            const float distance =
                Distance2D(
                    player.x,
                    player.y,
                    waypoint.x,
                    waypoint.y
                );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                reissue
                    ? "QUEST 792 TRAVEL: retrying original waypoint after detour."
                    : "QUEST 792 TRAVEL: moving to egress waypoint."
            );

            Debug::Logger::Info(
                "Waypoint " +
                std::to_string(
                    waypointIndex_ + 1
                ) +
                "/" +
                std::to_string(
                    RouteCount()
                ) +
                ": " +
                waypoint.name
            );

            LogPlayerPosition(
                player,
                "Player position:"
            );

            Debug::Logger::Info(
                "Destination: (" +
                Float(
                    waypoint.x
                ) +
                "," +
                Float(
                    waypoint.y
                ) +
                "," +
                Float(
                    waypoint.z
                ) +
                ")"
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    distance
                )
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        waypoint.x,
                        waypoint.y,
                        waypoint.z,
                        0.75f
                    ))
            {
                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: CTM command failed."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            ++commands_;

            if (reissue)
            {
                ++reissues_;
            }

            lastCommandTick_ =
                tick;

            lastProgressTick_ =
                tick;

            bestDistance_ =
                distance;

            Debug::Logger::Info(
                "QUEST 792 TRAVEL: CTM command issued."
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        bool BeginDetour(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (
                waypointIndex_ >=
                    RouteCount())
            {
                return false;
            }

            if (
                detourAttempt_ >=
                    MaximumDetourAttempts)
            {
                return false;
            }

            if (
                totalDetours_ >=
                    MaximumTotalDetours)
            {
                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: total detour safety "
                    "limit reached."
                );

                return false;
            }

            const auto& waypoint =
                VileFamiliarsRoute[
                    waypointIndex_
                ];

            const float dx =
                waypoint.x -
                player.x;

            const float dy =
                waypoint.y -
                player.y;

            const float length =
                std::sqrt(
                    dx * dx +
                    dy * dy
                );

            if (length < 0.001f)
            {
                return false;
            }

            if (!obstacleBaselineValid_)
            {
                obstacleBaselineValid_ =
                    true;

                obstacleStartMainDistance_ =
                    length;

                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: new local obstacle "
                    "baseline distance: " +
                    Float(
                        obstacleStartMainDistance_
                    )
                );
            }

            const float ux =
                dx / length;

            const float uy =
                dy / length;

            /*
             * Perpendicular unit vector.
             */
            const float px =
                -uy;

            const float py =
                ux;

            /*
             * Try alternating sides:
             *
             *  attempt 1 -> right/small
             *  attempt 2 -> left/small
             *  attempt 3 -> right/large
             *  attempt 4 -> left/large
             *
             * The slight backwards component prevents the
             * detour from aiming directly across the same
             * obstacle face.
             */
            const bool rightSide =
                (detourAttempt_ % 2) == 0;

            const float side =
                rightSide
                    ? -1.0f
                    : 1.0f;

            const float radius =
                detourAttempt_ < 2
                    ? SmallDetourRadius
                    : LargeDetourRadius;

            detourDestination_.x =
                player.x +
                px * side * radius -
                ux * DetourBackoff;

            detourDestination_.y =
                player.y +
                py * side * radius -
                uy * DetourBackoff;

            detourStart_.x =
                player.x;

            detourStart_.y =
                player.y;

            detourStartTick_ =
                tick;

            detourActive_ =
                true;

            ++detourAttempt_;

            ++detours_;
            ++totalDetours_;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST 792 TRAVEL: OBSTACLE DETOUR"
            );

            Debug::Logger::Info(
                "Main waypoint appears blocked."
            );

            Debug::Logger::Info(
                "Detour attempt: " +
                std::to_string(
                    detourAttempt_
                ) +
                "/" +
                std::to_string(
                    MaximumDetourAttempts
                )
            );

            Debug::Logger::Info(
                std::string(
                    "Detour side: "
                ) +
                (
                    rightSide
                        ? "right"
                        : "left"
                )
            );

            Debug::Logger::Info(
                "Detour radius: " +
                Float(
                    radius
                )
            );

            Debug::Logger::Info(
                "Total route detours: " +
                std::to_string(
                    totalDetours_
                ) +
                "/" +
                std::to_string(
                    MaximumTotalDetours
                )
            );

            LogPlayerPosition(
                player,
                "Detour start:"
            );

            Debug::Logger::Info(
                "Detour destination: (" +
                Float(
                    detourDestination_.x
                ) +
                "," +
                Float(
                    detourDestination_.y
                ) +
                "," +
                Float(
                    player.z
                ) +
                ")"
            );

            Debug::Logger::Info(
                "================================"
            );

            if (!ClickToMoveController::
                    MoveTo(
                        player,
                        detourDestination_.x,
                        detourDestination_.y,
                        player.z,
                        0.75f
                    ))
            {
                detourActive_ =
                    false;

                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: detour CTM command failed."
                );

                return false;
            }

            ++commands_;

            lastCommandTick_ =
                tick;

            return true;
        }

        bool UpdateDetour(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!detourActive_)
            {
                return false;
            }

            const float moved =
                Distance2D(
                    detourStart_.x,
                    detourStart_.y,
                    player.x,
                    player.y
                );

            const float remaining =
                Distance2D(
                    player.x,
                    player.y,
                    detourDestination_.x,
                    detourDestination_.y
                );

            if (
                remaining <=
                    DetourReachedTolerance ||
                moved >=
                    DetourMovementSuccess)
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: detour movement observed."
                );

                Debug::Logger::Info(
                    "Moved: " +
                    Float(
                        moved
                    )
                );

                Debug::Logger::Info(
                    "Remaining to detour point: " +
                    Float(
                        remaining
                    )
                );

                LogPlayerPosition(
                    player,
                    "Position after detour:"
                );

                Debug::Logger::Info(
                    "Retrying original route waypoint."
                );

                Debug::Logger::Info(
                    "================================"
                );

                detourActive_ =
                    false;

                lastProgressTick_ =
                    tick;

                bestDistance_ =
                    Distance2D(
                        player.x,
                        player.y,
                        VileFamiliarsRoute[
                            waypointIndex_
                        ].x,
                        VileFamiliarsRoute[
                            waypointIndex_
                        ].y
                    );

                if (!IssueCurrentWaypoint(
                        player,
                        tick,
                        true))
                {
                    SetState(
                        QuestObjectiveTravelState::Failed
                    );
                }

                return true;
            }

            if (
                tick <
                    detourStartTick_ +
                    DetourTimeoutTicks)
            {
                return true;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST 792 TRAVEL: detour produced insufficient movement."
            );

            Debug::Logger::Info(
                "Moved: " +
                Float(
                    moved
                )
            );

            LogPlayerPosition(
                player,
                "Current position:"
            );

            Debug::Logger::Info(
                "Trying the next bounded detour."
            );

            Debug::Logger::Info(
                "================================"
            );

            detourActive_ =
                false;

            if (!BeginDetour(
                    player,
                    tick))
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: FAILED"
                );

                Debug::Logger::Info(
                    "Reason: all bounded obstacle "
                    "detours were exhausted."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestObjectiveTravelState::Failed
                );
            }

            return true;
        }

    public:
        static bool ShouldStart(
            const Objects::PlayerState& player,
            std::uint32_t desiredQuestEntry)
        {
            if (
                !player.valid ||
                desiredQuestEntry !=
                    VileFamiliarEntry)
            {
                return false;
            }

            return
                Distance2D(
                    player.x,
                    player.y,
                    ObjectiveAreaX,
                    ObjectiveAreaY
                ) >
                ObjectiveAreaRadius;
        }

        static bool IsInObjectiveArea(
            const Objects::PlayerState& player)
        {
            if (!player.valid)
            {
                return false;
            }

            return
                Distance2D(
                    player.x,
                    player.y,
                    ObjectiveAreaX,
                    ObjectiveAreaY
                ) <=
                ObjectiveAreaRadius;
        }

        bool Start(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (
                state_ !=
                    QuestObjectiveTravelState::Idle)
            {
                return false;
            }

            if (!player.valid)
            {
                return false;
            }

            waypointIndex_ =
                0;

            commands_ =
                0;

            reissues_ =
                0;

            detours_ =
                0;

            detourAttempt_ =
                0;

            totalDetours_ =
                0;

            obstacleBaselineValid_ =
                false;

            obstacleStartMainDistance_ =
                0.0f;

            patrolIndex_ =
                0;

            lastPatrolCommandTick_ =
                0;

            patrolCommands_ =
                0;

            patrolMoveActive_ =
                false;

            patrolDetourActive_ =
                false;

            patrolLastProgressTick_ =
                0;

            patrolDetourStartTick_ =
                0;

            patrolLastUpdateTick_ =
                0;

            patrolBestDistance_ =
                0.0f;

            patrolDetourAttempt_ =
                0;

            patrolDetours_ =
                0;

            patrolHotspotSkips_ =
                0;

            patrolDetourStart_ =
                Position2D{};

            patrolDetourDestination_ =
                Position2D{};

            detourActive_ =
                false;

            lastCommandTick_ =
                tick;

            lastProgressTick_ =
                tick;

            bestDistance_ =
                0.0f;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST 792 TRAVEL: START"
            );

            Debug::Logger::Info(
                "Objective: 792 Vile Familiars."
            );

            Debug::Logger::Info(
                "Reason: quest 792 is active and the "
                "player is outside the Vile Familiar "
                "objective area."
            );

            Debug::Logger::Info(
                "Policy: proven Valley corridor -> "
                "Vile Familiar hunting ground, with "
                "bounded local obstacle recovery."
            );

            LogPlayerPosition(
                player,
                "Travel start position:"
            );

            Debug::Logger::Info(
                "Route destination: Vile Familiar hunting "
                "ground after " +
                std::to_string(
                    RouteCount()
                ) +
                " waypoints."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestObjectiveTravelState::Travelling
            );

            if (!IssueCurrentWaypoint(
                    player,
                    tick,
                    false))
            {
                SetState(
                    QuestObjectiveTravelState::Failed
                );

                return false;
            }

            return true;
        }

        void Update(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (
                state_ !=
                    QuestObjectiveTravelState::Travelling)
            {
                return;
            }

            if (!player.valid)
            {
                SetState(
                    QuestObjectiveTravelState::Failed
                );

                return;
            }

            if (UpdateDetour(
                    player,
                    tick))
            {
                return;
            }

            if (
                waypointIndex_ >=
                    RouteCount())
            {
                SetState(
                    QuestObjectiveTravelState::Done
                );

                return;
            }

            const auto& waypoint =
                VileFamiliarsRoute[
                    waypointIndex_
                ];

            const float distance =
                Distance2D(
                    player.x,
                    player.y,
                    waypoint.x,
                    waypoint.y
                );

            /*
             * Phase 7B.2 exposed an important distinction:
             * four detours may span more than one physical
             * obstacle. If a detour is followed by strong
             * progress toward the main waypoint, that
             * obstacle is cleared and the next obstacle must
             * receive a fresh local detour budget.
             */
            if (
                obstacleBaselineValid_ &&
                distance <=
                    obstacleStartMainDistance_ -
                    ObstacleClearedProgress)
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: local obstacle cleared."
                );

                Debug::Logger::Info(
                    "Main waypoint distance improved: " +
                    Float(
                        obstacleStartMainDistance_
                    ) +
                    " -> " +
                    Float(
                        distance
                    )
                );

                Debug::Logger::Info(
                    "Resetting local detour attempts "
                    "for the next obstacle."
                );

                Debug::Logger::Info(
                    "================================"
                );

                detourAttempt_ =
                    0;

                obstacleBaselineValid_ =
                    false;

                obstacleStartMainDistance_ =
                    0.0f;
            }

            if (
                distance <=
                    WaypointTolerance)
            {
                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: reached waypoint " +
                    std::to_string(
                        waypointIndex_ + 1
                    ) +
                    "/" +
                    std::to_string(
                        RouteCount()
                    ) +
                    " (" +
                    waypoint.name +
                    ")."
                );

                LogPlayerPosition(
                    player,
                    "Reached position:"
                );

                ++waypointIndex_;

                detourAttempt_ =
                    0;

                obstacleBaselineValid_ =
                    false;

                obstacleStartMainDistance_ =
                    0.0f;

                detourActive_ =
                    false;

                if (
                    waypointIndex_ >=
                        RouteCount())
                {
                    Debug::Logger::Info(
                        "================================"
                    );

                    Debug::Logger::Info(
                        "QUEST 792 TRAVEL: PASS"
                    );

                    Debug::Logger::Info(
                        "Valley of Trials egress corridor "
                        "completed."
                    );

                    Debug::Logger::Info(
                        "Combat target acquisition may resume."
                    );

                    Debug::Logger::Info(
                        "================================"
                    );

                    SetState(
                        QuestObjectiveTravelState::Done
                    );

                    return;
                }

                IssueCurrentWaypoint(
                    player,
                    tick,
                    false
                );

                return;
            }

            if (
                distance <
                    bestDistance_ -
                    ProgressThreshold)
            {
                bestDistance_ =
                    distance;

                lastProgressTick_ =
                    tick;
            }

            if (
                tick <
                    lastProgressTick_ +
                    StuckTicks)
            {
                return;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST 792 TRAVEL: no forward progress."
            );

            Debug::Logger::Info(
                "Waypoint: " +
                std::string(
                    waypoint.name
                )
            );

            Debug::Logger::Info(
                "Remaining distance: " +
                Float(
                    distance
                )
            );

            LogPlayerPosition(
                player,
                "Stuck position:"
            );

            Debug::Logger::Info(
                "Starting local obstacle bypass instead "
                "of repeating the blocked CTM line."
            );

            Debug::Logger::Info(
                "================================"
            );

            if (!BeginDetour(
                    player,
                    tick))
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "QUEST 792 TRAVEL: FAILED"
                );

                Debug::Logger::Info(
                    "Reason: local obstacle detour budget "
                    "or total route detour safety limit "
                    "was exhausted."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestObjectiveTravelState::Failed
                );
            }
        }

        bool Patrol(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!player.valid)
            {
                return false;
            }

            if (PatrolCount() == 0)
            {
                return false;
            }

            if (
                patrolIndex_ >=
                    PatrolCount())
            {
                patrolIndex_ =
                    0;
            }

            /*
             * A long gap normally means combat temporarily
             * interrupted patrol updates. Re-baseline from
             * the current post-combat position instead of
             * immediately declaring a stale movement stuck.
             */
            if (
                patrolLastUpdateTick_ != 0 &&
                tick >
                    patrolLastUpdateTick_ +
                    PatrolStuckTicks * 2)
            {
                patrolMoveActive_ =
                    false;

                patrolDetourActive_ =
                    false;

                patrolDetourAttempt_ =
                    0;

                patrolBestDistance_ =
                    0.0f;
            }

            patrolLastUpdateTick_ =
                tick;

            const auto issueMainDestination =
                [&](const char* reason)
                {
                    const auto& destination =
                        VileFamiliarsPatrol[
                            patrolIndex_
                        ];

                    const float distance =
                        Distance2D(
                            player.x,
                            player.y,
                            destination.x,
                            destination.y
                        );

                    Debug::Logger::Info(
                        "================================"
                    );

                    Debug::Logger::Info(
                        std::string(
                            "QUEST 792 NAV: "
                        ) +
                        reason
                    );

                    Debug::Logger::Info(
                        "Hotspot " +
                        std::to_string(
                            patrolIndex_ + 1
                        ) +
                        "/" +
                        std::to_string(
                            PatrolCount()
                        ) +
                        " (" +
                        destination.name +
                        ")."
                    );

                    LogPlayerPosition(
                        player,
                        "Player position:"
                    );

                    Debug::Logger::Info(
                        "Destination: (" +
                        Float(
                            destination.x
                        ) +
                        "," +
                        Float(
                            destination.y
                        ) +
                        "," +
                        Float(
                            destination.z
                        ) +
                        ")"
                    );

                    Debug::Logger::Info(
                        "Distance: " +
                        Float(
                            distance
                        )
                    );

                    const bool issued =
                        ClickToMoveController::
                            MoveTo(
                                player,
                                destination.x,
                                destination.y,
                                destination.z,
                                1.0f
                            );

                    if (issued)
                    {
                        ++patrolCommands_;

                        lastPatrolCommandTick_ =
                            tick;

                        patrolLastProgressTick_ =
                            tick;

                        patrolBestDistance_ =
                            distance;

                        patrolMoveActive_ =
                            true;
                    }

                    Debug::Logger::Info(
                        issued
                            ? "QUEST 792 NAV: CTM command issued."
                            : "QUEST 792 NAV: CTM command failed."
                    );

                    Debug::Logger::Info(
                        "================================"
                    );

                    return issued;
                };

            const auto beginPatrolDetour =
                [&]()
                {
                    if (
                        patrolDetourAttempt_ >=
                            MaximumPatrolDetourAttempts)
                    {
                        ++patrolHotspotSkips_;

                        Debug::Logger::Info(
                            "================================"
                        );

                        Debug::Logger::Info(
                            "QUEST 792 NAV: hotspot blocked."
                        );

                        Debug::Logger::Info(
                            "Skipping hotspot " +
                            std::to_string(
                                patrolIndex_ + 1
                            ) +
                            " after " +
                            std::to_string(
                                patrolDetourAttempt_
                            ) +
                            " failed detour attempts."
                        );

                        patrolIndex_ =
                            (
                                patrolIndex_ +
                                1
                            ) %
                            PatrolCount();

                        patrolDetourAttempt_ =
                            0;

                        patrolMoveActive_ =
                            false;

                        patrolDetourActive_ =
                            false;

                        patrolBestDistance_ =
                            0.0f;

                        Debug::Logger::Info(
                            "Next hotspot: " +
                            std::to_string(
                                patrolIndex_ + 1
                            ) +
                            "/" +
                            std::to_string(
                                PatrolCount()
                            )
                        );

                        Debug::Logger::Info(
                            "================================"
                        );

                        return true;
                    }

                    const auto& destination =
                        VileFamiliarsPatrol[
                            patrolIndex_
                        ];

                    const float dx =
                        destination.x -
                        player.x;

                    const float dy =
                        destination.y -
                        player.y;

                    const float length =
                        std::sqrt(
                            dx * dx +
                            dy * dy
                        );

                    if (length < 0.001f)
                    {
                        return false;
                    }

                    const float ux =
                        dx / length;

                    const float uy =
                        dy / length;

                    /*
                     * Fan search around the blocked heading.
                     * Alternating both sides and widening the
                     * angle is more useful on ridges/slopes
                     * than repeatedly trying one perpendicular
                     * sidestep.
                     */
                    static constexpr float AnglesDegrees[] =
                    {
                         55.0f,
                        -55.0f,
                         90.0f,
                        -90.0f,
                        130.0f,
                       -130.0f
                    };

                    static constexpr float Radii[] =
                    {
                         8.0f,
                         8.0f,
                        10.0f,
                        10.0f,
                        12.0f,
                        12.0f
                    };

                    const int attempt =
                        patrolDetourAttempt_;

                    const float angle =
                        AnglesDegrees[
                            attempt
                        ] *
                        3.14159265358979323846f /
                        180.0f;

                    const float radius =
                        Radii[
                            attempt
                        ];

                    const float cosA =
                        std::cos(
                            angle
                        );

                    const float sinA =
                        std::sin(
                            angle
                        );

                    const float rx =
                        ux * cosA -
                        uy * sinA;

                    const float ry =
                        ux * sinA +
                        uy * cosA;

                    patrolDetourStart_.x =
                        player.x;

                    patrolDetourStart_.y =
                        player.y;

                    patrolDetourDestination_.x =
                        player.x +
                        rx * radius;

                    patrolDetourDestination_.y =
                        player.y +
                        ry * radius;

                    patrolDetourStartTick_ =
                        tick;

                    patrolDetourActive_ =
                        true;

                    ++patrolDetourAttempt_;
                    ++patrolDetours_;

                    Debug::Logger::Info(
                        "================================"
                    );

                    Debug::Logger::Info(
                        "QUEST 792 NAV: REACTIVE DETOUR"
                    );

                    Debug::Logger::Info(
                        "Detour attempt: " +
                        std::to_string(
                            patrolDetourAttempt_
                        ) +
                        "/" +
                        std::to_string(
                            MaximumPatrolDetourAttempts
                        )
                    );

                    Debug::Logger::Info(
                        "Angle: " +
                        Float(
                            AnglesDegrees[
                                attempt
                            ]
                        ) +
                        " degrees"
                    );

                    Debug::Logger::Info(
                        "Radius: " +
                        Float(
                            radius
                        )
                    );

                    LogPlayerPosition(
                        player,
                        "Detour start:"
                    );

                    Debug::Logger::Info(
                        "Detour destination: (" +
                        Float(
                            patrolDetourDestination_.x
                        ) +
                        "," +
                        Float(
                            patrolDetourDestination_.y
                        ) +
                        "," +
                        Float(
                            player.z
                        ) +
                        ")"
                    );

                    const bool issued =
                        ClickToMoveController::
                            MoveTo(
                                player,
                                patrolDetourDestination_.x,
                                patrolDetourDestination_.y,
                                player.z,
                                0.75f
                            );

                    if (issued)
                    {
                        ++patrolCommands_;

                        lastPatrolCommandTick_ =
                            tick;
                    }
                    else
                    {
                        patrolDetourActive_ =
                            false;
                    }

                    Debug::Logger::Info(
                        issued
                            ? "QUEST 792 NAV: detour CTM issued."
                            : "QUEST 792 NAV: detour CTM failed."
                    );

                    Debug::Logger::Info(
                        "================================"
                    );

                    return issued;
                };

            /*
             * Finish/timeout an active local detour.
             */
            if (patrolDetourActive_)
            {
                const float moved =
                    Distance2D(
                        patrolDetourStart_.x,
                        patrolDetourStart_.y,
                        player.x,
                        player.y
                    );

                const float remaining =
                    Distance2D(
                        player.x,
                        player.y,
                        patrolDetourDestination_.x,
                        patrolDetourDestination_.y
                    );

                if (
                    moved >=
                        PatrolDetourMovementSuccess ||
                    remaining <=
                        PatrolDetourReachedTolerance)
                {
                    Debug::Logger::Info(
                        "================================"
                    );

                    Debug::Logger::Info(
                        "QUEST 792 NAV: detour movement observed."
                    );

                    Debug::Logger::Info(
                        "Moved: " +
                        Float(
                            moved
                        )
                    );

                    LogPlayerPosition(
                        player,
                        "Position after detour:"
                    );

                    Debug::Logger::Info(
                        "Retrying original hotspot from "
                        "the new position."
                    );

                    Debug::Logger::Info(
                        "================================"
                    );

                    patrolDetourActive_ =
                        false;

                    patrolMoveActive_ =
                        false;

                    patrolLastProgressTick_ =
                        tick;

                    return
                        issueMainDestination(
                            "retrying hotspot after detour"
                        );
                }

                if (
                    tick <
                        patrolDetourStartTick_ +
                        PatrolDetourTimeoutTicks)
                {
                    return true;
                }

                Debug::Logger::Info(
                    "QUEST 792 NAV: detour produced "
                    "insufficient movement."
                );

                patrolDetourActive_ =
                    false;

                patrolMoveActive_ =
                    false;

                return beginPatrolDetour();
            }

            const auto& destination =
                VileFamiliarsPatrol[
                    patrolIndex_
                ];

            const float distance =
                Distance2D(
                    player.x,
                    player.y,
                    destination.x,
                    destination.y
                );

            if (
                distance <=
                    PatrolArrivalRadius)
            {
                Debug::Logger::Info(
                    "QUEST 792 NAV: reached patrol hotspot " +
                    std::to_string(
                        patrolIndex_ + 1
                    ) +
                    "/" +
                    std::to_string(
                        PatrolCount()
                    ) +
                    "."
                );

                patrolIndex_ =
                    (
                        patrolIndex_ +
                        1
                    ) %
                    PatrolCount();

                patrolDetourAttempt_ =
                    0;

                patrolMoveActive_ =
                    false;

                patrolBestDistance_ =
                    0.0f;

                return
                    issueMainDestination(
                        "advancing to next patrol hotspot"
                    );
            }

            if (!patrolMoveActive_)
            {
                if (
                    lastPatrolCommandTick_ != 0 &&
                    tick <
                        lastPatrolCommandTick_ +
                        2)
                {
                    return false;
                }

                return
                    issueMainDestination(
                        "no quest target in range"
                    );
            }

            if (
                distance <
                    patrolBestDistance_ -
                    PatrolProgressThreshold)
            {
                patrolBestDistance_ =
                    distance;

                patrolLastProgressTick_ =
                    tick;
            }

            if (
                tick <
                    patrolLastProgressTick_ +
                    PatrolStuckTicks)
            {
                return true;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST 792 NAV: STUCK"
            );

            Debug::Logger::Info(
                "Hotspot: " +
                std::to_string(
                    patrolIndex_ + 1
                ) +
                "/" +
                std::to_string(
                    PatrolCount()
                )
            );

            Debug::Logger::Info(
                "Remaining distance: " +
                Float(
                    distance
                )
            );

            LogPlayerPosition(
                player,
                "Stuck position:"
            );

            Debug::Logger::Info(
                "Switching from blind CTM reissue to "
                "reactive fan detour."
            );

            Debug::Logger::Info(
                "================================"
            );

            patrolMoveActive_ =
                false;

            if (
                patrolHotspotSkips_ >=
                    MaximumPatrolHotspotSkipsPerCycle)
            {
                /*
                 * Reset the skip counter after a bounded
                 * number of skipped hotspots. The route
                 * continues rather than failing the entire
                 * quest because one profile hotspot is bad.
                 */
                patrolHotspotSkips_ =
                    0;
            }

            return beginPatrolDetour();
        }

        void Reset()
        {
            state_ =
                QuestObjectiveTravelState::Idle;

            waypointIndex_ =
                0;

            lastCommandTick_ =
                0;

            lastProgressTick_ =
                0;

            bestDistance_ =
                0.0f;

            commands_ =
                0;

            reissues_ =
                0;

            detours_ =
                0;

            detourAttempt_ =
                0;

            totalDetours_ =
                0;

            obstacleBaselineValid_ =
                false;

            obstacleStartMainDistance_ =
                0.0f;

            patrolIndex_ =
                0;

            lastPatrolCommandTick_ =
                0;

            patrolCommands_ =
                0;

            patrolMoveActive_ =
                false;

            patrolDetourActive_ =
                false;

            patrolLastProgressTick_ =
                0;

            patrolDetourStartTick_ =
                0;

            patrolLastUpdateTick_ =
                0;

            patrolBestDistance_ =
                0.0f;

            patrolDetourAttempt_ =
                0;

            patrolDetours_ =
                0;

            patrolHotspotSkips_ =
                0;

            patrolDetourStart_ =
                Position2D{};

            patrolDetourDestination_ =
                Position2D{};

            detourActive_ =
                false;

            detourStartTick_ =
                0;

            detourStart_ =
                Position2D{};

            detourDestination_ =
                Position2D{};
        }

        QuestObjectiveTravelState State() const
        {
            return state_;
        }

        const char* StateName() const
        {
            return
                StateNameInternal(
                    state_
                );
        }

        bool IsActive() const
        {
            return
                state_ ==
                    QuestObjectiveTravelState::Travelling;
        }

        bool IsDone() const
        {
            return
                state_ ==
                    QuestObjectiveTravelState::Done;
        }

        bool Failed() const
        {
            return
                state_ ==
                    QuestObjectiveTravelState::Failed;
        }

        bool DetourActive() const
        {
            return detourActive_;
        }

        std::size_t WaypointIndex() const
        {
            return waypointIndex_;
        }

        int Commands() const
        {
            return commands_;
        }

        int Reissues() const
        {
            return reissues_;
        }

        int Detours() const
        {
            return detours_;
        }

        int LocalDetourAttempt() const
        {
            return detourAttempt_;
        }

        int TotalDetours() const
        {
            return totalDetours_;
        }

        std::size_t PatrolIndex() const
        {
            return patrolIndex_;
        }

        int PatrolCommands() const
        {
            return patrolCommands_;
        }

        int PatrolDetours() const
        {
            return patrolDetours_;
        }

        int PatrolHotspotSkips() const
        {
            return patrolHotspotSkips_;
        }

        bool PatrolDetourActive() const
        {
            return patrolDetourActive_;
        }
    };
}
