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
    enum class QuestReturnState
    {
        Idle,
        Travelling,
        Done,
        Failed
    };

    class QuestReturnController
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
         * WoW 1.12.1 Valley of Trials.
         *
         * Phase 8A uses the already-verified outdoor route
         * in reverse so a completed Sting of the Scorpid
         * objective can return from the Scorpid area to
         * Gornek in The Den.
         *
         * The local obstacle-bypass logic from the outbound
         * route is kept intact. This remains waypoint + CTM
         * navigation, not general navmesh pathfinding.
         */
        static constexpr float GornekX =
            -600.132f;

        static constexpr float GornekY =
            -4186.190f;

        static constexpr float GornekZ =
            41.08921f;

        static constexpr float InteractionReadyDistance =
            5.5f;

        static constexpr float WaypointTolerance =
            4.0f;

        static constexpr float ProgressThreshold =
            0.75f;

        /*
         * Main-route stuck detection:
         * 16 ticks * 250 ms ~= 4 seconds.
         */
        static constexpr std::uint64_t StuckTicks =
            16;

        /*
         * Detour observation window:
         * 12 ticks * 250 ms ~= 3 seconds.
         */
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

        /*
         * A successful sidestep followed by >=6 yards of
         * renewed progress toward the main waypoint means
         * that local obstacle is considered cleared.
         *
         * The next obstacle gets a fresh 1/4 detour budget.
         */
        static constexpr float ObstacleClearedProgress =
            6.0f;

        /*
         * Hard safety ceiling for the whole return route.
         * This prevents an endless series of local detours.
         */
        static constexpr int MaximumTotalDetours =
            12;

        /*
         * Phase 7B.3 used the wrong side of The Den as
         * its return corridor. The live log showed Y moving
         * from roughly -4185 toward -4165, while the actual
         * outside/start-valley route is toward more-negative
         * Y first.
         *
         * 7B.4 therefore uses a southbound exit through the
         * Kaltunk side of The Den, then follows known outdoor
         * Valley of Trials quest hotspots toward the Scorpid
         * Worker area.
         *
         * Local obstacle recovery still applies between each
         * pair of route waypoints.
         */
        inline static constexpr Waypoint GornekReturnRoute[] =
        {
            {
                -476.7746f,
                -4178.261f,
                50.77428f,
                "Leave Scorpid staging"
            },
            {
                -454.7112f,
                -4221.499f,
                49.21489f,
                "North-east outdoor return"
            },
            {
                -449.3994f,
                -4240.989f,
                51.25065f,
                "East Valley return turn"
            },
            {
                -475.5253f,
                -4280.907f,
                44.80814f,
                "Open Valley return 2"
            },
            {
                -516.5089f,
                -4292.180f,
                39.22541f,
                "Open Valley return 1"
            },
            {
                -610.073f,
                -4253.520f,
                39.03930f,
                "The Den south entrance / Kaltunk side"
            },
            {
                -600.132f,
                -4186.190f,
                41.08921f,
                "Gornek"
            }
        };

        QuestReturnState state_ =
            QuestReturnState::Idle;

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
            QuestReturnState state)
        {
            switch (state)
            {
                case QuestReturnState::Idle:
                    return "Idle";

                case QuestReturnState::Travelling:
                    return "Travelling";

                case QuestReturnState::Done:
                    return "Done";

                case QuestReturnState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            QuestReturnState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "QuestReturnController state: "
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
                    GornekReturnRoute
                ) /
                sizeof(
                    GornekReturnRoute[0]
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
                    "QUEST RETURN CTM: "
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
                    "QUEST RETURN: CTM command failed."
                );

                return false;
            }

            ++commands_;

            lastCommandTick_ =
                GetTickCount();

            Debug::Logger::Info(
                "QUEST RETURN: CTM command issued."
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
                    QuestReturnState::Done
                );

                return true;
            }

            const auto& waypoint =
                GornekReturnRoute[
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
                    ? "QUEST RETURN: retrying return waypoint after detour."
                    : "QUEST RETURN: moving to return waypoint."
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
                    "QUEST RETURN: CTM command failed."
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
                "QUEST RETURN: CTM command issued."
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
                    "QUEST RETURN: total detour safety "
                    "limit reached."
                );

                return false;
            }

            const auto& waypoint =
                GornekReturnRoute[
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
                    "QUEST RETURN: new local obstacle "
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
                "QUEST RETURN: OBSTACLE DETOUR"
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
                    "QUEST RETURN: detour CTM command failed."
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
                    "QUEST RETURN: detour movement observed."
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
                        GornekReturnRoute[
                            waypointIndex_
                        ].x,
                        GornekReturnRoute[
                            waypointIndex_
                        ].y
                    );

                if (!IssueCurrentWaypoint(
                        player,
                        tick,
                        true))
                {
                    SetState(
                        QuestReturnState::Failed
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
                "QUEST RETURN: detour produced insufficient movement."
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
                    "QUEST RETURN: FAILED"
                );

                Debug::Logger::Info(
                    "Reason: all bounded obstacle "
                    "detours were exhausted."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestReturnState::Failed
                );
            }

            return true;
        }

    public:
        static bool ShouldStart(
            const Objects::PlayerState& player)
        {
            if (!player.valid)
            {
                return false;
            }

            const float distanceFromGornek =
                Distance2D(
                    player.x,
                    player.y,
                    GornekX,
                    GornekY
                );

            return
                distanceFromGornek >
                    InteractionReadyDistance;
        }

        static bool IsAtGornek(
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
                    GornekX,
                    GornekY
                ) <=
                InteractionReadyDistance;
        }

        bool Start(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (
                state_ !=
                    QuestReturnState::Idle)
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
                "QUEST RETURN: START"
            );

            Debug::Logger::Info(
                "Objective complete: 789 Sting of the Scorpid."
            );

            Debug::Logger::Info(
                "Reason: quest 789 is complete and must be turned in."
            );

            Debug::Logger::Info(
                "Policy: reverse the verified outdoor Valley route, "
                "enter The Den from the Kaltunk side, then "
                "stop at Gornek for interaction."
            );

            LogPlayerPosition(
                player,
                "Travel start position:"
            );

            Debug::Logger::Info(
                "Return direction check: first waypoint=(" +
                Float(
                    GornekReturnRoute[0].x
                ) +
                "," +
                Float(
                    GornekReturnRoute[0].y
                ) +
                ")."
            );

            Debug::Logger::Info(
                "Route destination: Gornek after " +
                std::to_string(
                    RouteCount()
                ) +
                " waypoints."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestReturnState::Travelling
            );

            if (!IssueCurrentWaypoint(
                    player,
                    tick,
                    false))
            {
                SetState(
                    QuestReturnState::Failed
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
                    QuestReturnState::Travelling)
            {
                return;
            }

            if (!player.valid)
            {
                SetState(
                    QuestReturnState::Failed
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
                    QuestReturnState::Done
                );

                return;
            }

            const auto& waypoint =
                GornekReturnRoute[
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
                    "QUEST RETURN: local obstacle cleared."
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
                    "QUEST RETURN: reached waypoint " +
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
                        "QUEST RETURN: PASS"
                    );

                    Debug::Logger::Info(
                        "Valley of Trials return corridor "
                        "completed."
                    );

                    Debug::Logger::Info(
                        "Gornek interaction may begin."
                    );

                    Debug::Logger::Info(
                        "================================"
                    );

                    SetState(
                        QuestReturnState::Done
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
                "QUEST RETURN: no forward progress."
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
                    "QUEST RETURN: FAILED"
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
                    QuestReturnState::Failed
                );
            }
        }

        void Reset()
        {
            state_ =
                QuestReturnState::Idle;

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

            detourActive_ =
                false;

            detourStartTick_ =
                0;

            detourStart_ =
                Position2D{};

            detourDestination_ =
                Position2D{};
        }

        QuestReturnState State() const
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
                    QuestReturnState::Travelling;
        }

        bool IsDone() const
        {
            return
                state_ ==
                    QuestReturnState::Done;
        }

        bool Failed() const
        {
            return
                state_ ==
                    QuestReturnState::Failed;
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
    };
}
