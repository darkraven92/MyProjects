#pragma once

#include "DetourNavigationProvider.h"
#include "../Bot/ClickToMoveController.h"
#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Navigation
{
    enum class NavMeshFollowState
    {
        Idle,
        Planning,
        Moving,
        Arrived,
        Failed
    };

    class NavMeshPathFollower
    {
    private:
        static constexpr std::uint32_t KalimdorMapId = 1;
        static constexpr NavPoint ZureethaFargaze{-629.052f, -4228.060f, 38.2334f};

        static constexpr float CornerArrivalDistance = 3.00f;
        static constexpr float NearCornerRecoveryDistance = 5.00f;
        static constexpr float FinalArrivalDistance = 5.50f;
        static constexpr float CtmPrecision = 0.75f;
        static constexpr float ProgressThreshold = 0.75f;
        static constexpr float MinimumSafetyHealthPercent = 35.0f;

        // WorldMonitor runs every 250 ms.
        static constexpr std::uint64_t StuckTicks = 24;   // ~6 s
        static constexpr std::uint64_t ReissueTicks = 40; // ~10 s
        static constexpr int MaximumReplans = 4;

        // Dense portal steering should normally be short, but do not reject a
        // legitimate Detour route because of an arbitrary 130 yd threshold.
        // Total-path and vertical sanity checks remain in force.
        static constexpr float MaximumCornerSegment = 300.0f;
        static constexpr float MaximumVerticalSegment = 30.0f;
        static constexpr float MaximumPathLength = 1200.0f;

        DetourNavigationProvider provider_;
        NavMeshFollowState state_ = NavMeshFollowState::Idle;
        std::vector<NavPoint> points_;
        std::size_t pointIndex_ = 0;
        int commands_ = 0;
        int replans_ = 0;
        std::uint64_t lastCommandTick_ = 0;
        std::uint64_t lastProgressTick_ = 0;
        std::uint64_t lastStatusTick_ = 0;
        float bestCornerDistance_ = 0.0f;
        float plannedPathLength_ = 0.0f;
        std::string mmapsDirectory_;

        static float Distance2D(float ax, float ay, float bx, float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            return std::sqrt(dx * dx + dy * dy);
        }

        static float Distance3D(const NavPoint& a, const NavPoint& b)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float dz = b.z - a.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        static std::string Float(float value)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        static const char* StateNameInternal(NavMeshFollowState state)
        {
            switch (state)
            {
                case NavMeshFollowState::Idle: return "Idle";
                case NavMeshFollowState::Planning: return "Planning";
                case NavMeshFollowState::Moving: return "Moving";
                case NavMeshFollowState::Arrived: return "Arrived";
                case NavMeshFollowState::Failed: return "Failed";
                default: return "Unknown";
            }
        }

        void SetState(NavMeshFollowState next)
        {
            if (state_ == next) return;
            Debug::Logger::Info(
                std::string("NavMeshPathFollower state: ") +
                StateNameInternal(state_) + " -> " + StateNameInternal(next));
            state_ = next;
        }

        static NavPoint PlayerPoint(const Objects::PlayerState& player)
        {
            return NavPoint{player.x, player.y, player.z};
        }

        static float HealthPercent(const Objects::PlayerState& player)
        {
            if (player.maxHealth == 0) return 0.0f;
            return static_cast<float>(player.health) * 100.0f /
                   static_cast<float>(player.maxHealth);
        }

        void StopAtCurrentPosition(const Objects::PlayerState& player)
        {
            // Bounded stop request for an aborted Phase 10B run.
            Bot::ClickToMoveController::MoveTo(
                player, player.x, player.y, player.z, 0.25f);
        }

        bool ValidatePath(const NavPathResult& path, std::string& error, float& length) const
        {
            length = 0.0f;
            if (!path.success)
            {
                error = "Detour did not return a successful path.";
                return false;
            }
            if (path.partial)
            {
                error = "Detour returned a partial path.";
                return false;
            }
            if (path.points.size() < 2)
            {
                error = "Detour returned fewer than two straight-path points.";
                return false;
            }

            for (std::size_t i = 1; i < path.points.size(); ++i)
            {
                const float segment = Distance3D(path.points[i - 1], path.points[i]);
                const float vertical = std::fabs(path.points[i].z - path.points[i - 1].z);
                if (!std::isfinite(segment) || segment > MaximumCornerSegment)
                {
                    error = "Implausible corner segment " + std::to_string(i) +
                            ": " + Float(segment);
                    return false;
                }
                if (!std::isfinite(vertical) || vertical > MaximumVerticalSegment)
                {
                    error = "Implausible vertical segment " + std::to_string(i) +
                            ": " + Float(vertical);
                    return false;
                }
                length += segment;
                if (length > MaximumPathLength)
                {
                    error = "NavMesh path exceeds the Phase 10B safety length.";
                    return false;
                }
            }
            error.clear();
            return true;
        }

        void LogPath(const NavPathResult& path, float length, bool replan) const
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info(replan
                ? "NAVMESH 10B.2: REPLAN COMPLETE"
                : "NAVMESH 10B.2: INITIAL PLAN COMPLETE");
            Debug::Logger::Info("Loaded tiles: " + std::to_string(path.loadedTiles));
            Debug::Logger::Info("Polygon path count: " + std::to_string(path.polygonCount));
            Debug::Logger::Info("Dense portal steering points: " + std::to_string(path.points.size()));
            Debug::Logger::Info("Path length: " + Float(length));
            for (std::size_t i = 0; i < path.points.size(); ++i)
            {
                const auto& p = path.points[i];
                Debug::Logger::Info(
                    "NAV10B2 point[" + std::to_string(i) + "]: (" +
                    Float(p.x) + "," + Float(p.y) + "," + Float(p.z) + ")");
            }
            Debug::Logger::Info("================================");
        }

        bool IssueCurrentCorner(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const char* reason)
        {
            while (pointIndex_ < points_.size())
            {
                const auto& p = points_[pointIndex_];
                const float distance = Distance2D(player.x, player.y, p.x, p.y);

                if (distance <= CornerArrivalDistance && pointIndex_ + 1 < points_.size())
                {
                    Debug::Logger::Info(
                        "NAVMESH 10B.2: corner within normal arrival radius; advancing " +
                        std::to_string(pointIndex_) + " -> " +
                        std::to_string(pointIndex_ + 1));
                    ++pointIndex_;
                    continue;
                }

                Debug::Logger::Info("================================");
                Debug::Logger::Info(std::string("NAVMESH 10B.2: CTM CORNER - ") + reason);
                Debug::Logger::Info(
                    "Corner " + std::to_string(pointIndex_ + 1) + "/" +
                    std::to_string(points_.size()));
                Debug::Logger::Info(
                    "Corner destination: (" + Float(p.x) + "," +
                    Float(p.y) + "," + Float(p.z) + ")");
                Debug::Logger::Info("Corner distance: " + Float(distance));

                if (!Bot::ClickToMoveController::MoveTo(
                        player, p.x, p.y, p.z, CtmPrecision))
                {
                    Debug::Logger::Info("NAVMESH 10B.2: CTM command failed.");
                    Debug::Logger::Info("================================");
                    return false;
                }

                ++commands_;
                lastCommandTick_ = tick;
                lastProgressTick_ = tick;
                bestCornerDistance_ = distance;
                Debug::Logger::Info("NAVMESH 10B.2: CTM command issued.");
                Debug::Logger::Info("================================");
                return true;
            }
            return true;
        }

        bool PlanFrom(const Objects::PlayerState& player, std::uint64_t tick, bool replan)
        {
            SetState(NavMeshFollowState::Planning);

            NavPathResult path{};
            if (!provider_.FindPath(PlayerPoint(player), ZureethaFargaze, path))
            {
                Debug::Logger::Info("NAVMESH 10B.2: path query failed.");
                Debug::Logger::Info("Reason: " + path.error);
                SetState(NavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return false;
            }

            std::string error;
            float length = 0.0f;
            if (!ValidatePath(path, error, length))
            {
                Debug::Logger::Info("NAVMESH 10B.2: path validation failed.");
                Debug::Logger::Info("Reason: " + error);
                SetState(NavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return false;
            }

            if (replan) ++replans_;
            points_ = path.points;
            pointIndex_ = 1; // point 0 is start projection
            plannedPathLength_ = length;
            LogPath(path, length, replan);
            SetState(NavMeshFollowState::Moving);

            if (!IssueCurrentCorner(player, tick, replan ? "after replan" : "initial path"))
            {
                SetState(NavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return false;
            }
            return true;
        }

        bool Replan(const Objects::PlayerState& player, std::uint64_t tick)
        {
            if (replans_ >= MaximumReplans)
            {
                Debug::Logger::Info("NAVMESH 10B.2: FAILED - replan safety limit reached.");
                SetState(NavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return false;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info("NAVMESH 10B.2: STUCK - replanning from live player position.");
            Debug::Logger::Info(
                "Replan attempt: " + std::to_string(replans_ + 1) + "/" +
                std::to_string(MaximumReplans));
            Debug::Logger::Info("================================");
            return PlanFrom(player, tick, true);
        }

    public:
        void CancelForLivingWater()
        {
            if (!OwnsMovement()) return;
            points_.clear();
            provider_.Shutdown(); // detaches query, preserves cached topology
            SetState(NavMeshFollowState::Failed);
        }

        bool StartReturnToZureetha(const Objects::PlayerState& player, std::uint64_t tick)
        {
            if (state_ != NavMeshFollowState::Idle)
                return state_ != NavMeshFollowState::Failed;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("NAVMESH PHASE 10B.2: DENSE PORTAL STEERING");
            Debug::Logger::Info("Goal: return to Zureetha after quest 792.");
            Debug::Logger::Info(
                "Detour steering mode: DT_STRAIGHTPATH_ALL_CROSSINGS; "
                "CTM receives short corridor portal targets instead of sparse funnel corners.");
            Debug::Logger::Info(
                "Movement ownership: NavMesh follower only; combat/quest CTM is paused during this test.");
            Debug::Logger::Info(
                "Destination: Zureetha Fargaze (-629.052,-4228.060,38.233)");

            mmapsDirectory_ = DetourNavigationProvider::ResolveMmapsDirectory();
            Debug::Logger::Info("MMAP directory: " + mmapsDirectory_);
            SetState(NavMeshFollowState::Planning);

            std::string error;
            if (!provider_.Initialize(mmapsDirectory_, KalimdorMapId, error))
            {
                Debug::Logger::Info("NAVMESH 10B.2: initialization failed.");
                Debug::Logger::Info("Reason: " + error);
                SetState(NavMeshFollowState::Failed);
                Debug::Logger::Info("================================");
                return false;
            }

            Debug::Logger::Info("Loaded tiles: " + std::to_string(provider_.LoadedTiles()));
            return PlanFrom(player, tick, false);
        }

        void Update(const Objects::PlayerState& player, std::uint64_t tick)
        {
            if (state_ != NavMeshFollowState::Moving) return;

            const float hp = HealthPercent(player);
            if (hp <= MinimumSafetyHealthPercent)
            {
                Debug::Logger::Info(
                    "NAVMESH 10B.2: safety abort - player HP fell to " + Float(hp) + "%.");
                SetState(NavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return;
            }

            const float finalDistance = Distance2D(
                player.x, player.y, ZureethaFargaze.x, ZureethaFargaze.y);
            if (finalDistance <= FinalArrivalDistance)
            {
                Debug::Logger::Info("================================");
                Debug::Logger::Info("NAVMESH 10B.2: PASS");
                Debug::Logger::Info("Arrived within Zureetha interaction range.");
                Debug::Logger::Info("Final distance: " + Float(finalDistance));
                Debug::Logger::Info("CTM commands: " + std::to_string(commands_));
                Debug::Logger::Info("Replans: " + std::to_string(replans_));
                Debug::Logger::Info("================================");
                SetState(NavMeshFollowState::Arrived);
                return;
            }

            if (pointIndex_ >= points_.size())
            {
                Replan(player, tick);
                return;
            }

            const auto& p = points_[pointIndex_];
            const float cornerDistance = Distance2D(player.x, player.y, p.x, p.y);

            if (cornerDistance <= CornerArrivalDistance)
            {
                Debug::Logger::Info(
                    "NAVMESH 10B.2: reached corner " +
                    std::to_string(pointIndex_ + 1) + "/" +
                    std::to_string(points_.size()) +
                    " distance=" + Float(cornerDistance));
                ++pointIndex_;

                if (pointIndex_ >= points_.size())
                {
                    Replan(player, tick);
                    return;
                }

                if (!IssueCurrentCorner(player, tick, "next Detour corner"))
                {
                    SetState(NavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                }
                return;
            }

            if (cornerDistance + ProgressThreshold < bestCornerDistance_)
            {
                bestCornerDistance_ = cornerDistance;
                lastProgressTick_ = tick;
            }

            if (tick >= lastProgressTick_ && tick - lastProgressTick_ >= StuckTicks)
            {
                /*
                 * Detour straight-path corners are steering/portal
                 * points, not exact stop coordinates.
                 *
                 * Phase 10B repeatedly stalled only a few yards from
                 * the first corner and then replanned back to the same
                 * portal. If we are already close to that portal,
                 * advance to the next Detour corner instead of wasting
                 * a replan on exact-corner convergence.
                 */
                if (cornerDistance <= NearCornerRecoveryDistance &&
                    pointIndex_ + 1 < points_.size())
                {
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "NAVMESH 10B.2: NEAR-CORNER LOOKAHEAD");
                    Debug::Logger::Info(
                        "Stalled within " + Float(cornerDistance) +
                        " yd of corner " +
                        std::to_string(pointIndex_ + 1) + "/" +
                        std::to_string(points_.size()) +
                        "; advancing to next Detour corner.");

                    ++pointIndex_;

                    if (!IssueCurrentCorner(
                            player,
                            tick,
                            "near-corner recovery lookahead"))
                    {
                        SetState(NavMeshFollowState::Failed);
                        StopAtCurrentPosition(player);
                    }

                    Debug::Logger::Info("================================");
                    return;
                }

                Replan(player, tick);
                return;
            }

            if (tick >= lastCommandTick_ && tick - lastCommandTick_ >= ReissueTicks)
            {
                if (!IssueCurrentCorner(player, tick, "periodic command refresh"))
                {
                    SetState(NavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                }
                return;
            }

            if (tick >= lastStatusTick_ && tick - lastStatusTick_ >= 20)
            {
                lastStatusTick_ = tick;
                Debug::Logger::Info(
                    "NavMesh10B.2: state=" + std::string(StateNameInternal(state_)) +
                    " corner=" + std::to_string(pointIndex_ + 1) + "/" +
                    std::to_string(points_.size()) +
                    " cornerDistance=" + Float(cornerDistance) +
                    " finalDistance=" + Float(finalDistance) +
                    " commands=" + std::to_string(commands_) +
                    " replans=" + std::to_string(replans_));
            }
        }

        bool OwnsMovement() const
        {
            return state_ == NavMeshFollowState::Planning ||
                   state_ == NavMeshFollowState::Moving;
        }

        bool Arrived() const { return state_ == NavMeshFollowState::Arrived; }
        bool Failed() const { return state_ == NavMeshFollowState::Failed; }
        const char* StateName() const { return StateNameInternal(state_); }
        std::size_t PointIndex() const { return pointIndex_; }
        std::size_t PointCount() const { return points_.size(); }
        int Commands() const { return commands_; }
        int Replans() const { return replans_; }
        float PlannedPathLength() const { return plannedPathLength_; }
    };
}
