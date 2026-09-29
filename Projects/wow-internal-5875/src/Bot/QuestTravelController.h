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
    enum class QuestTravelState
    {
        Idle,
        Travelling,
        Done,
        Failed
    };

    class QuestTravelController
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
         * Phase 7B.1 proved that a raw CTM line from Gornek
         * toward the Rwag-side corridor can collide with a
         * static object inside The Den.
         *
         * Phase 7B.2 therefore keeps the high-level route
         * but adds a local obstacle-bypass routine. When
         * forward progress stops, the controller makes a
         * bounded perpendicular sidestep, then retries the
         * original waypoint.
         *
         * This is still not a navmesh. It is deliberately
         * limited to escaping small static obstacles in the
         * Valley of Trials questgiver area.
         */
        static constexpr float GornekX =
            -600.132f;

        static constexpr float GornekY =
            -4186.190f;

        static constexpr float GornekZ =
            41.08921f;

        static constexpr float TriggerRadius =
            70.0f;

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
         * Hard safety ceiling for the whole egress route.
         * This prevents an endless series of local detours.
         */
        static constexpr int MaximumTotalDetours =
            12;

        static constexpr std::uint32_t ScorpidWorkerEntry =
            3124;

        /*
         * Phase 7B.3 used the wrong side of The Den as
         * its egress corridor. The live log showed Y moving
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
        inline static constexpr Waypoint ScorpidEgressRoute[] =
        {
            /*
             * Exit The Den toward Kaltunk / the outdoor
             * Valley of Trials area.
             */
            {
                -610.073f,
                -4253.520f,
                39.03930f,
                "The Den south exit / Kaltunk side"
            },

            /*
             * Move well clear of the questgiver cave before
             * steering back toward the northern Scorpid area.
             */
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

            /*
             * Follow outdoor quest-ground points around the
             * terrain rather than cutting back through The Den.
             */
            {
                -449.3994f,
                -4240.989f,
                51.25065f,
                "East Valley turn"
            },
            {
                -454.7112f,
                -4221.499f,
                49.21489f,
                "North-east approach"
            },
            {
                -476.7746f,
                -4178.261f,
                50.77428f,
                "Scorpid approach ridge"
            },

            /*
             * Known Scorpid Worker quest hotspot.
             */
            {
                -457.8277f,
                -4156.172f,
                47.58189f,
                "Scorpid Worker staging area"
            }
        };

        QuestTravelState state_ =
            QuestTravelState::Idle;

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
            QuestTravelState state)
        {
            switch (state)
            {
                case QuestTravelState::Idle:
                    return "Idle";

                case QuestTravelState::Travelling:
                    return "Travelling";

                case QuestTravelState::Done:
                    return "Done";

                case QuestTravelState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            QuestTravelState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "QuestTravelController state: "
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
                    ScorpidEgressRoute
                ) /
                sizeof(
                    ScorpidEgressRoute[0]
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
                    "QUEST TRAVEL CTM: "
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
                    "QUEST TRAVEL: CTM command failed."
                );

                return false;
            }

            ++commands_;

            lastCommandTick_ =
                GetTickCount();

            Debug::Logger::Info(
                "QUEST TRAVEL: CTM command issued."
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
                    QuestTravelState::Done
                );

                return true;
            }

            const auto& waypoint =
                ScorpidEgressRoute[
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
                    ? "QUEST TRAVEL: retrying original waypoint after detour."
                    : "QUEST TRAVEL: moving to egress waypoint."
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
                    "QUEST TRAVEL: CTM command failed."
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
                "QUEST TRAVEL: CTM command issued."
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
                    "QUEST TRAVEL: total detour safety "
                    "limit reached."
                );

                return false;
            }

            const auto& waypoint =
                ScorpidEgressRoute[
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
                    "QUEST TRAVEL: new local obstacle "
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
                "QUEST TRAVEL: OBSTACLE DETOUR"
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
                    "QUEST TRAVEL: detour CTM command failed."
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
                    "QUEST TRAVEL: detour movement observed."
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
                        ScorpidEgressRoute[
                            waypointIndex_
                        ].x,
                        ScorpidEgressRoute[
                            waypointIndex_
                        ].y
                    );

                if (!IssueCurrentWaypoint(
                        player,
                        tick,
                        true))
                {
                    SetState(
                        QuestTravelState::Failed
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
                "QUEST TRAVEL: detour produced insufficient movement."
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
                    "QUEST TRAVEL: FAILED"
                );

                Debug::Logger::Info(
                    "Reason: all bounded obstacle "
                    "detours were exhausted."
                );

                Debug::Logger::Info(
                    "================================"
                );

                SetState(
                    QuestTravelState::Failed
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
                    ScorpidWorkerEntry)
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
                distanceFromGornek <=
                    TriggerRadius;
        }

        bool Start(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (
                state_ !=
                    QuestTravelState::Idle)
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
                "QUEST TRAVEL: START"
            );

            Debug::Logger::Info(
                "Objective: 789 Sting of the Scorpid."
            );

            Debug::Logger::Info(
                "Reason: player is still inside the "
                "Gornek/questgiver routing radius."
            );

            Debug::Logger::Info(
                "Policy: southbound The Den exit, "
                "outdoor Valley route, then bounded local "
                "obstacle detours before mob selection."
            );

            LogPlayerPosition(
                player,
                "Travel start position:"
            );

            Debug::Logger::Info(
                "Exit direction check: first waypoint Y=" +
                Float(
                    ScorpidEgressRoute[0].y
                ) +
                " (must be more negative than the Gornek "
                "interior area)."
            );

            Debug::Logger::Info(
                "Route destination: Scorpid Worker staging "
                "area after " +
                std::to_string(
                    RouteCount()
                ) +
                " waypoints."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                QuestTravelState::Travelling
            );

            if (!IssueCurrentWaypoint(
                    player,
                    tick,
                    false))
            {
                SetState(
                    QuestTravelState::Failed
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
                    QuestTravelState::Travelling)
            {
                return;
            }

            if (!player.valid)
            {
                SetState(
                    QuestTravelState::Failed
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
                    QuestTravelState::Done
                );

                return;
            }

            const auto& waypoint =
                ScorpidEgressRoute[
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
                    "QUEST TRAVEL: local obstacle cleared."
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
                    "QUEST TRAVEL: reached waypoint " +
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
                        "QUEST TRAVEL: PASS"
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
                        QuestTravelState::Done
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
                "QUEST TRAVEL: no forward progress."
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
                    "QUEST TRAVEL: FAILED"
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
                    QuestTravelState::Failed
                );
            }
        }

        void Reset()
        {
            state_ =
                QuestTravelState::Idle;

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

        QuestTravelState State() const
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
                    QuestTravelState::Travelling;
        }

        bool IsDone() const
        {
            return
                state_ ==
                    QuestTravelState::Done;
        }

        bool Failed() const
        {
            return
                state_ ==
                    QuestTravelState::Failed;
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
