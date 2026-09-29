#pragma once

#include "AdaptiveDangerMemory.h"
#include "AutonomousMaintenancePolicy.h"
#include "CombatController.h"
#include "FirstAidController.h"
#include "GrindBagMonitor.h"
#include "GrindLevelPolicy.h"
#include "GrindTargetPolicy.h"
#include "MovementController.h"
#include "PlayerPostureController.h"
#include "VendorController.h"

#include "../Debug/Logger.h"
#include "../Navigation/DetourNavigationProvider.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Navigation/NavigationHazardMemory.h"
#include "../Objects/WorldState.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bot
{
    enum class GrindModeState
    {
        Idle,
        Grinding,
        ApproachingTarget,
        Roaming,
        Vendoring,
        Failed
    };

    class GrindModeController
    {
    private:
        static constexpr std::uint32_t MapId = 1;

        // Phase 14G.2 keeps the live generated-sector roaming model, but the
        // combat legality bubble now follows the current grind anchor so gray
        // migration can move beyond the original Sen'jin start position.
        static constexpr float LocalScanRadius = 120.0f;
        static constexpr float CombatRegionRadius = 180.0f;
        static constexpr float MaximumRoamRadius = 480.0f;
        static constexpr float CombatHandoffDistance = 24.0f;
        static constexpr float ApproachArrivalDistance = 22.0f;
        static constexpr float MaximumApproachCandidateDistance = 150.0f;
        static constexpr float RoamArrivalDistance = 12.0f;
        static constexpr int GrayMigrationMinimumRing = 3;

        static constexpr int VendorTriggerFreeSlots = 1;
        static constexpr std::uint64_t BagProbeIntervalTicks = 8; // 2 s
        static constexpr std::uint64_t MaintenanceProbeIntervalTicks = 40; // 10 s
        static constexpr std::uint64_t MaintenanceRetryBackoffTicks = 2400; // 10 min
        static constexpr std::uint64_t UrgentMaintenanceRetryBackoffTicks = 480; // 2 min
        static constexpr std::uint64_t BagPressureVendorRetryBackoffTicks = 480; // 2 min
        static constexpr std::uint64_t StartupVendorGraceTicks = 480; // 2 min at 4 ticks/s
        static constexpr float PostDeathEscapeMinimumHealthPercent = 90.0f;
        static constexpr std::uint64_t ApproachRetryTicks = 4; // 1 s
        static constexpr std::uint64_t ApproachBlacklistTicks = 120; // 30 s
        static constexpr std::uint64_t RoamScanIntervalTicks = 4; // 1 s
        static constexpr std::uint64_t FirstAidIdleDwellTicks = 8; // ~2 s of genuine idle
        static constexpr int EmptySectorScanThreshold = 4; // ~4 s without work
        static constexpr std::uint64_t EmptySectorCooldownTicks = 240; // 60 s
        static constexpr std::uint64_t FailedSectorCooldownTicks = 480; // 120 s
        static constexpr std::size_t InvalidSectorIndex =
            std::numeric_limits<std::size_t>::max();
        static constexpr float Pi = 3.14159265358979323846f;

        struct GrindSector
        {
            Navigation::NavPoint center{};
            int ring = 0;
            int spoke = 0;
            std::uint64_t cooldownUntil = 0;
            std::uint64_t lastVisitedTick = 0;
            int visits = 0;
            int failures = 0;
        };

        struct LocalScan
        {
            int combatLike = 0;
            int suitable = 0;
            int gray = 0;
            int tooHigh = 0;
        };

        GrindModeState state_ = GrindModeState::Idle;
        Navigation::NavPoint grindHome_{};
        Navigation::NavPoint sectorOrigin_{};
        Navigation::NavPoint currentGrindAnchor_{};
        Navigation::NavPoint grindResumePoint_{};
        float sectorBaseAngle_ = 0.0f;
        bool grayMigrationActive_ = false;
        int migrationRegionAdvances_ = 0;
        LocalScan lastLocalScan_{};
        VendorController vendor_{};
        FirstAidController firstAid_{};
        std::uint64_t firstAidIdleSinceTick_ = 0;
        AdaptiveDangerMemory dangerMemory_{};
        MaintenanceSnapshot maintenance_{};
        std::uint64_t nextMaintenanceProbeTick_ = 0;
        std::uint64_t maintenanceSuppressedUntil_ = 0;
        std::uint64_t startupVendorGraceUntil_ = 0;
        bool startupVendorGraceSuppressionLogged_ = false;
        int startupVendorGraceSuppressions_ = 0;
        bool postDeathEscapePending_ = false;
        bool dangerEscapeActive_ = false;
        bool deathEpisodeRecorded_ = false;

        std::unique_ptr<Navigation::GenericNavMeshPathFollower> approachNavigator_{};
        std::uint64_t approachGuid_ = 0;
        std::uint32_t approachEntry_ = 0;
        std::uint64_t nextApproachScanTick_ = 0;
        std::unordered_map<std::uint64_t, std::uint64_t> approachBlacklistUntil_{};

        // Phase 14G.2 wide-area roaming. Sector centers are generated from the
        // runtime start position, so there are no Sen'jin-specific coordinates
        // or quest waypoints in the roaming layer.
        std::vector<GrindSector> sectors_{};
        std::size_t activeSectorIndex_ = InvalidSectorIndex;
        std::size_t roamDestinationIndex_ = InvalidSectorIndex;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> roamNavigator_{};
        std::uint64_t nextRoamScanTick_ = 0;
        int emptySectorScanStreak_ = 0;
        int roamLegsStarted_ = 0;
        int roamLegsArrived_ = 0;
        int roamLegsFailed_ = 0;
        int autonomyMovementRecoveries_ = 0;
        int runtimeSupervisorResets_ = 0;
        int runtimeSupervisorEscalations_ = 0;
        int antiAfkActions_ = 0;
        int antiAfkNavigationActions_ = 0;
        int antiAfkAcquisitionActions_ = 0;
        int antiAfkFallbackPulses_ = 0;

        std::uint64_t nextBagProbeTick_ = 0;
        GrindBagMonitor::Snapshot bags_{};
        int vendorTrips_ = 0;

        static const char* StateNameInternal(GrindModeState state)
        {
            switch (state)
            {
                case GrindModeState::Idle: return "Idle";
                case GrindModeState::Grinding: return "Grinding";
                case GrindModeState::ApproachingTarget: return "ApproachingTarget";
                case GrindModeState::Roaming: return "Roaming";
                case GrindModeState::Vendoring: return "Vendoring";
                case GrindModeState::Failed: return "Failed";
                default: return "Unknown";
            }
        }

        static float Distance2D(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            return std::sqrt(dx * dx + dy * dy);
        }

        bool MatchesGrindCandidate(
            const Objects::WorldState& world,
            const Objects::UnitState& unit) const
        {
            if (!GrindTargetPolicy::IsPotentialTarget(world, unit))
                return false;

            const Navigation::NavPoint point{unit.x, unit.y, unit.z};
            if (Navigation::NavigationHazardMemory::Instance().IsHardBlocked(
                    MapId, point))
            {
                return false;
            }

            return !dangerMemory_.IsQuarantined(
                MapId, point, world.player.level);
        }

        LocalScan AnalyzeLocalMobs(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            float radius) const
        {
            LocalScan scan{};

            for (const auto& unit : world.units)
            {
                if (
                    unit.distance < 0.0f ||
                    unit.distance > radius ||
                    !GrindTargetPolicy::LooksLikeCombatCreature(unit))
                {
                    continue;
                }

                const Navigation::NavPoint unitPoint{unit.x, unit.y, unit.z};
                if (Navigation::NavigationHazardMemory::Instance().IsHardBlocked(
                        MapId, unitPoint) ||
                    dangerMemory_.IsQuarantined(
                        MapId, unitPoint, world.player.level))
                {
                    continue;
                }

                if (combat.IsTemporaryGrindTargetBlacklisted(unit.guid, tick))
                    continue;

                ++scan.combatLike;

                if (GrindLevelPolicy::IsGray(world.player.level, unit.level))
                {
                    ++scan.gray;
                    continue;
                }

                if (unit.level > world.player.level + 1)
                {
                    ++scan.tooHigh;
                    continue;
                }

                if (
                    unit.targetGuid == 0 ||
                    unit.targetGuid == world.activePlayerGuid)
                {
                    ++scan.suitable;
                }
            }

            return scan;
        }

        bool IsApproachBlacklisted(
            std::uint64_t guid,
            std::uint64_t tick)
        {
            const auto it = approachBlacklistUntil_.find(guid);
            if (it == approachBlacklistUntil_.end())
                return false;

            if (tick >= it->second)
            {
                approachBlacklistUntil_.erase(it);
                return false;
            }

            return true;
        }

        const Objects::UnitState* FindUnitByGuid(
            const Objects::WorldState& world,
            std::uint64_t guid) const
        {
            if (guid == 0)
                return nullptr;

            for (const auto& unit : world.units)
            {
                if (unit.valid && unit.guid == guid)
                    return &unit;
            }

            return nullptr;
        }

        bool PlayerInsideQuarantinedArea(
            const Objects::WorldState& world) const
        {
            return dangerMemory_.IsQuarantined(
                MapId,
                Navigation::NavPoint{
                    world.player.x,
                    world.player.y,
                    world.player.z},
                world.player.level);
        }

        const Objects::UnitState* FindDirectAggressor(
            const Objects::WorldState& world) const
        {
            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.health == 0 ||
                    unit.maxHealth == 0 ||
                    unit.targetGuid != world.activePlayerGuid ||
                    unit.distance < 0.0f ||
                    unit.distance > 60.0f)
                {
                    continue;
                }

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }

            return best;
        }

        const Objects::UnitState* FindApproachCandidate(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            const Objects::UnitState* best = nullptr;
            double bestScore = std::numeric_limits<double>::infinity();
            for (const auto& unit : world.units)
            {
                if (!MatchesGrindCandidate(world, unit))
                    continue;

                if (
                    unit.distance <= CombatHandoffDistance ||
                    unit.distance > MaximumApproachCandidateDistance ||
                    IsApproachBlacklisted(unit.guid, tick) ||
                    combat.IsTemporaryGrindTargetBlacklisted(unit.guid, tick))
                {
                    continue;
                }

                const Navigation::NavPoint point{unit.x, unit.y, unit.z};
                const double score = static_cast<double>(unit.distance) +
                    static_cast<double>(dangerMemory_.RiskAt(
                        MapId, point, world.player.level)) * 25.0 +
                    static_cast<double>(
                        Navigation::NavigationHazardMemory::Instance().RiskAt(
                            MapId, point)) * 90.0;
                if (best == nullptr || score < bestScore)
                {
                    best = &unit;
                    bestScore = score;
                }
            }

            return best;
        }

        bool HasCombatRangeCandidate(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) const
        {
            for (const auto& unit : world.units)
            {
                if (
                    MatchesGrindCandidate(world, unit) &&
                    unit.distance >= 0.0f &&
                    unit.distance <= CombatHandoffDistance &&
                    !combat.IsTemporaryGrindTargetBlacklisted(unit.guid, tick))
                {
                    return true;
                }
            }

            return false;
        }

        bool HasApproachCandidate(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            return FindApproachCandidate(world, combat, tick) != nullptr;
        }

        void BuildRoamSectors()
        {
            sectors_.clear();

            GrindSector home{};
            home.center = sectorOrigin_;
            home.ring = 0;
            home.spoke = 0;
            sectors_.push_back(home);
            activeSectorIndex_ = 0;

            // Four concentric runtime-generated rings. Alternating the angular
            // offset between rings avoids retracing the same radial spokes.
            const float radii[] = {120.0f, 240.0f, 360.0f, MaximumRoamRadius};
            constexpr int spokes = 8;

            for (int ring = 0; ring < 4; ++ring)
            {
                const float offset =
                    sectorBaseAngle_ + ((ring % 2 == 0) ? 0.0f : (Pi / 8.0f));
                for (int spoke = 0; spoke < spokes; ++spoke)
                {
                    const float angle =
                        offset + (2.0f * Pi * static_cast<float>(spoke) /
                                  static_cast<float>(spokes));

                    GrindSector sector{};
                    sector.center = Navigation::NavPoint{
                        sectorOrigin_.x + std::cos(angle) * radii[ring],
                        sectorOrigin_.y + std::sin(angle) * radii[ring],
                        sectorOrigin_.z
                    };
                    sector.ring = ring + 1;
                    sector.spoke = spoke;
                    sectors_.push_back(sector);
                }
            }
        }

        std::size_t FindNearestSectorIndex(
            const Navigation::NavPoint& point) const
        {
            if (sectors_.empty())
                return InvalidSectorIndex;

            std::size_t best = 0;
            float bestDistance = std::numeric_limits<float>::max();
            for (std::size_t i = 0; i < sectors_.size(); ++i)
            {
                const float distance = Distance2D(
                    point.x,
                    point.y,
                    sectors_[i].center.x,
                    sectors_[i].center.y);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = i;
                }
            }
            return best;
        }

        std::size_t SelectNextRoamSector(
            const Objects::WorldState& world,
            std::uint64_t tick) const
        {
            std::size_t best = InvalidSectorIndex;
            double bestScore = std::numeric_limits<double>::infinity();

            for (std::size_t i = 0; i < sectors_.size(); ++i)
            {
                if (i == activeSectorIndex_)
                    continue;

                const auto& sector = sectors_[i];
                if (tick < sector.cooldownUntil)
                    continue;

                if (Navigation::NavigationHazardMemory::Instance().IsHardBlocked(
                        MapId, sector.center) ||
                    dangerMemory_.IsQuarantined(
                        MapId, sector.center, world.player.level))
                {
                    continue;
                }

                const float distance = Distance2D(
                    world.player.x,
                    world.player.y,
                    sector.center.x,
                    sector.center.y);

                if (distance < 25.0f)
                    continue;

                double score = static_cast<double>(distance);

                if (grayMigrationActive_)
                {
                    // Once local mobs have become gray, leave the immediate
                    // spawn pocket instead of orbiting the same low-level ring.
                    if (sector.ring < GrayMigrationMinimumRing)
                        score += 10000.0;
                    score -= static_cast<double>(sector.ring) * 80.0;
                }
                else
                {
                    // Preserve the live wide-area preference for unvisited
                    // sectors without forcing a level-band jump.
                    if (sector.lastVisitedTick == 0)
                        score -= 500.0;
                    else
                        score += static_cast<double>(sector.lastVisitedTick) * 0.0001;
                }

                score += static_cast<double>(sector.failures) * 250.0;
                score += static_cast<double>(dangerMemory_.RiskAt(
                    MapId, sector.center, world.player.level)) * 180.0;
                score += static_cast<double>(
                    Navigation::NavigationHazardMemory::Instance().RiskAt(
                        MapId, sector.center)) * 220.0;

                if (score < bestScore)
                {
                    bestScore = score;
                    best = i;
                }
            }

            return best;
        }

        void AdvanceMigrationSearchRegionIfNeeded(
            const Objects::WorldState& world,
            int arrivedRing)
        {
            if (!grayMigrationActive_ || arrivedRing < 4)
                return;

            const float dx = world.player.x - sectorOrigin_.x;
            const float dy = world.player.y - sectorOrigin_.y;
            if (std::fabs(dx) + std::fabs(dy) > 1.0f)
                sectorBaseAngle_ = std::atan2(dy, dx);

            sectorOrigin_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };
            ++migrationRegionAdvances_;
            BuildRoamSectors();

            Debug::Logger::Info(
                "GRIND 14G.2: MIGRATION SEARCH REGION ADVANCED; newOrigin=(" +
                std::to_string(sectorOrigin_.x) + "," +
                std::to_string(sectorOrigin_.y) + "," +
                std::to_string(sectorOrigin_.z) + ") heading=" +
                std::to_string(sectorBaseAngle_) +
                " advances=" + std::to_string(migrationRegionAdvances_));
        }

        void ResetApproach()
        {
            approachNavigator_.reset();
            approachGuid_ = 0;
            approachEntry_ = 0;
        }

        void ResetRoam()
        {
            roamNavigator_.reset();
            roamDestinationIndex_ = InvalidSectorIndex;
        }

        void SetState(GrindModeState state)
        {
            if (state_ == state)
                return;

            Debug::Logger::Info(
                std::string("GRIND 14G.2: state ") +
                StateNameInternal(state_) + " -> " +
                StateNameInternal(state));
            state_ = state;
        }

        void Fail(const std::string& reason)
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info("GRIND 14G.2: FAILED");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");
            ResetApproach();
            ResetRoam();
            SetState(GrindModeState::Failed);
        }

        bool CombatSafeForVendor(const CombatController& combat) const
        {
            return
                combat.State() == CombatState::AcquiringTarget ||
                combat.State() == CombatState::PostKillDelay;
        }

        void ProbeBags(std::uint64_t tick)
        {
            if (tick < nextBagProbeTick_)
                return;

            nextBagProbeTick_ = tick + BagProbeIntervalTicks;

            GrindBagMonitor::Snapshot next{};
            if (!GrindBagMonitor::Read(next))
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: bag probe unavailable; keeping current grind state.");
                return;
            }

            const bool changed =
                !bags_.valid ||
                bags_.totalSlots != next.totalSlots ||
                bags_.usedSlots != next.usedSlots ||
                bags_.freeSlots != next.freeSlots;

            bags_ = next;

            if (changed)
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: bags used=" +
                    std::to_string(bags_.usedSlots) +
                    " free=" + std::to_string(bags_.freeSlots) +
                    " total=" + std::to_string(bags_.totalSlots));
            }
        }


        void ProbeMaintenance(std::uint64_t tick, bool force = false)
        {
            if (!force && tick < nextMaintenanceProbeTick_)
                return;

            nextMaintenanceProbeTick_ = tick + MaintenanceProbeIntervalTicks;
            MaintenanceSnapshot next{};
            if (!VendorController::ProbeMaintenance(next))
            {
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: probe unavailable; deferring maintenance decision.");
                return;
            }

            const bool changed =
                !maintenance_.valid ||
                maintenance_.powerType != next.powerType ||
                maintenance_.foodCount != next.foodCount ||
                maintenance_.drinkCount != next.drinkCount ||
                std::fabs(
                    maintenance_.minimumDurabilityPercent -
                    next.minimumDurabilityPercent) >= 1.0f;

            maintenance_ = next;
            if (changed)
            {
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: status durability=" +
                    std::to_string(maintenance_.minimumDurabilityPercent) +
                    "% food=" + std::to_string(maintenance_.foodCount) +
                    " drink=" + std::to_string(maintenance_.drinkCount) +
                    " powerType=" + std::to_string(maintenance_.powerType) +
                    " money=" + std::to_string(maintenance_.money));
            }
        }

        MaintenanceNeed CurrentMaintenanceNeed(std::uint64_t tick) const
        {
            MaintenanceNeed need =
                AutonomousMaintenancePolicy::Evaluate(maintenance_);
            if (!need.Any())
                return need;

            if (tick >= maintenanceSuppressedUntil_ || need.urgentRepair)
                return need;

            return MaintenanceNeed{};
        }

        bool TryStartDangerEscape(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (!postDeathEscapePending_ || dangerEscapeActive_)
                return false;

            if (FindDirectAggressor(world) != nullptr)
                return false;

            if (RecoveryController::HealthPercent(world.player) <
                PostDeathEscapeMinimumHealthPercent)
            {
                return false;
            }

            const std::size_t next = SelectNextRoamSector(world, tick);
            if (next == InvalidSectorIndex)
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: post-death escape waiting for an unquarantined NavMesh sector.");
                return false;
            }

            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            auto destination = sectors_[next].center;
            destination.z = world.player.z;
            if (!nav->Start(
                    world.player,
                    tick,
                    destination,
                    MapId,
                    RoamArrivalDistance,
                    "post-death danger escape"))
            {
                auto& failed = sectors_[next];
                failed.cooldownUntil = tick + FailedSectorCooldownTicks;
                ++failed.failures;
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: post-death escape route failed to start; trying another sector later.");
                return false;
            }

            ResetApproach();
            roamNavigator_ = std::move(nav);
            roamDestinationIndex_ = next;
            dangerEscapeActive_ = true;
            ++roamLegsStarted_;
            emptySectorScanStreak_ = 0;

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "DANGER MEMORY 14G.5: POST-DEATH ESCAPE START sector=" +
                std::to_string(next) +
                " risk=" + std::to_string(dangerMemory_.RiskAt(
                    MapId, destination, world.player.level)));
            Debug::Logger::Info(
                "Death hotspot remains quarantined; normal target acquisition is suppressed until escape completes or defensive aggro occurs.");
            Debug::Logger::Info("================================");
            SetState(GrindModeState::Roaming);
            return true;
        }

        bool TryStartApproach(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (tick < nextApproachScanTick_)
                return false;

            nextApproachScanTick_ = tick + ApproachRetryTicks;

            if (FindDirectAggressor(world) != nullptr)
                return false;

            if (HasCombatRangeCandidate(world, combat, tick))
                return false;

            const auto* candidate = FindApproachCandidate(world, combat, tick);
            if (candidate == nullptr)
                return false;

            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            const Navigation::NavPoint destination{
                candidate->x,
                candidate->y,
                candidate->z
            };

            if (!nav->Start(
                    world.player,
                    tick,
                    destination,
                    MapId,
                    ApproachArrivalDistance,
                    std::string("grind approach entry=") +
                        std::to_string(candidate->entryId)))
            {
                approachBlacklistUntil_[candidate->guid] =
                    tick + ApproachBlacklistTicks;
                Debug::Logger::Info(
                    "GRIND 14G.2: approach route failed to start; temporarily skipping guid=" +
                    std::to_string(candidate->guid));
                return false;
            }

            approachNavigator_ = std::move(nav);
            approachGuid_ = candidate->guid;
            approachEntry_ = candidate->entryId;
            emptySectorScanStreak_ = 0;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("GRIND 14G.2: APPROACH DISTANT GRIND TARGET");
            Debug::Logger::Info(
                "entry=" + std::to_string(approachEntry_) +
                " distance=" + std::to_string(candidate->distance));
            Debug::Logger::Info("================================");

            SetState(GrindModeState::ApproachingTarget);
            return true;
        }

        void UpdateApproach(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            const auto* aggressor = FindDirectAggressor(world);
            if (aggressor != nullptr)
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: defensive aggro interrupted grind approach; handing exact GUID to combat.");
                ResetApproach();
                SetState(GrindModeState::Grinding);
                combat.AdoptExactTargetForDefense(
                    world,
                    aggressor->guid,
                    tick);
                return;
            }

            const auto* target = FindUnitByGuid(world, approachGuid_);
            if (
                target == nullptr ||
                !MatchesGrindCandidate(world, *target))
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: approach target disappeared/became invalid; resuming scan.");
                ResetApproach();
                SetState(GrindModeState::Grinding);
                return;
            }

            if (target->distance <= CombatHandoffDistance)
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: approach target entered combat acquisition range; handing back to CombatController.");
                currentGrindAnchor_ = Navigation::NavPoint{
                    world.player.x,
                    world.player.y,
                    world.player.z
                };
                combat.UpdateTemporaryGrindRegion(
                    currentGrindAnchor_,
                    CombatRegionRadius);
                grindResumePoint_ = currentGrindAnchor_;
                grayMigrationActive_ = false;
                ResetApproach();
                SetState(GrindModeState::Grinding);
                combat.Update(world, tick);
                return;
            }

            if (!approachNavigator_)
            {
                ResetApproach();
                SetState(GrindModeState::Grinding);
                return;
            }

            approachNavigator_->Update(world.player, tick);

            if (approachNavigator_->Failed())
            {
                approachBlacklistUntil_[approachGuid_] =
                    tick + ApproachBlacklistTicks;
                Debug::Logger::Info(
                    "GRIND 14G.2: NavMesh approach failed; target temporarily skipped.");
                ResetApproach();
                SetState(GrindModeState::Grinding);
                return;
            }

            if (approachNavigator_->Arrived())
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: staging approach arrived; rescanning live target distance.");
                ResetApproach();
                SetState(GrindModeState::Grinding);
                nextApproachScanTick_ = tick + 1;
            }
        }

        void MarkActiveSectorExhausted(std::uint64_t tick)
        {
            if (activeSectorIndex_ == InvalidSectorIndex ||
                activeSectorIndex_ >= sectors_.size())
            {
                return;
            }

            auto& sector = sectors_[activeSectorIndex_];
            sector.lastVisitedTick = tick;
            sector.cooldownUntil = tick + EmptySectorCooldownTicks;

            Debug::Logger::Info(
                "GRIND 14G.2: SECTOR EXHAUSTED index=" +
                std::to_string(activeSectorIndex_) +
                " ring=" + std::to_string(sector.ring) +
                " spoke=" + std::to_string(sector.spoke) +
                " cooldownTicks=" + std::to_string(EmptySectorCooldownTicks));
        }

        bool TryStartRoam(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (
                FindDirectAggressor(world) != nullptr ||
                HasCombatRangeCandidate(world, combat, tick) ||
                HasApproachCandidate(world, combat, tick))
            {
                emptySectorScanStreak_ = 0;
                return false;
            }

            lastLocalScan_ = AnalyzeLocalMobs(
                world, combat, tick, LocalScanRadius);

            const bool grayNow =
                lastLocalScan_.gray > 0 &&
                lastLocalScan_.suitable == 0;

            if (grayNow && !grayMigrationActive_)
            {
                grayMigrationActive_ = true;
                Debug::Logger::Info("================================");
                Debug::Logger::Info(
                    "GRIND 14G.2: GRAY SATURATION DETECTED -> LEVEL-BAND MIGRATION");
                Debug::Logger::Info(
                    "playerLevel=" + std::to_string(world.player.level) +
                    " grayBoundary=" +
                    std::to_string(GrindLevelPolicy::GrayLevel(world.player.level)) +
                    " localGray=" + std::to_string(lastLocalScan_.gray) +
                    " localSuitable=0");
                Debug::Logger::Info(
                    "Searching farther NavMesh sectors for non-gray mobs; no fixed creature IDs or quest waypoints are used.");
                Debug::Logger::Info("================================");
            }

            const std::size_t next = SelectNextRoamSector(world, tick);
            if (next == InvalidSectorIndex)
            {
                std::size_t oldest = InvalidSectorIndex;
                std::uint64_t oldestUntil = std::numeric_limits<std::uint64_t>::max();
                for (std::size_t i = 0; i < sectors_.size(); ++i)
                {
                    if (sectors_[i].cooldownUntil < oldestUntil)
                    {
                        oldestUntil = sectors_[i].cooldownUntil;
                        oldest = i;
                    }
                }
                if (oldest != InvalidSectorIndex)
                    sectors_[oldest].cooldownUntil = tick;

                Debug::Logger::Info(
                    "GRIND 14G.2: no roam sector currently available; released oldest cooldown for continued search.");
                return false;
            }

            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            auto destination = sectors_[next].center;
            destination.z = world.player.z;
            const auto& sector = sectors_[next];

            if (!nav->Start(
                    world.player,
                    tick,
                    destination,
                    MapId,
                    RoamArrivalDistance,
                    std::string("wide grind sector index=") +
                        std::to_string(next) +
                        (grayMigrationActive_ ? " gray-migration" : " empty-search")))
            {
                auto& failed = sectors_[next];
                failed.cooldownUntil = tick + FailedSectorCooldownTicks;
                ++failed.failures;
                ++roamLegsFailed_;

                Debug::Logger::Info(
                    "GRIND 14G.2: ROAM SECTOR ROUTE FAILED TO START index=" +
                    std::to_string(next) +
                    " ring=" + std::to_string(failed.ring) +
                    " spoke=" + std::to_string(failed.spoke) +
                    " failures=" + std::to_string(failed.failures));
                return false;
            }

            roamNavigator_ = std::move(nav);
            roamDestinationIndex_ = next;
            ++roamLegsStarted_;
            emptySectorScanStreak_ = 0;

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                grayMigrationActive_
                    ? "GRIND 14G.2: ROAM -> SEARCH NEW NON-GRAY LEVEL BAND"
                    : "GRIND 14G.2: WIDE AREA ROAM START");
            Debug::Logger::Info(
                "sector=" + std::to_string(next) +
                " ring=" + std::to_string(sector.ring) +
                " spoke=" + std::to_string(sector.spoke) +
                " destination=(" + std::to_string(destination.x) + "," +
                std::to_string(destination.y) + "," +
                std::to_string(destination.z) + ")");
            Debug::Logger::Info(
                "roamLegs started=" + std::to_string(roamLegsStarted_) +
                " arrived=" + std::to_string(roamLegsArrived_) +
                " failed=" + std::to_string(roamLegsFailed_));
            Debug::Logger::Info("================================");

            SetState(GrindModeState::Roaming);
            return true;
        }

        void UpdateRoam(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            const auto* aggressor = FindDirectAggressor(world);
            if (aggressor != nullptr)
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: ROAM INTERRUPTED BY AGGRO; combat takes immediate ownership.");
                if (dangerEscapeActive_)
                {
                    dangerEscapeActive_ = false;
                    postDeathEscapePending_ = true;
                }
                ResetRoam();
                SetState(GrindModeState::Grinding);
                combat.AdoptExactTargetForDefense(world, aggressor->guid, tick);
                return;
            }

            if (
                !dangerEscapeActive_ &&
                (HasCombatRangeCandidate(world, combat, tick) ||
                 HasApproachCandidate(world, combat, tick)))
            {
                currentGrindAnchor_ = Navigation::NavPoint{
                    world.player.x,
                    world.player.y,
                    world.player.z
                };
                combat.UpdateTemporaryGrindRegion(
                    currentGrindAnchor_,
                    CombatRegionRadius);
                grindResumePoint_ = currentGrindAnchor_;
                if (grayMigrationActive_)
                {
                    Debug::Logger::Info(
                        "GRIND 14G.2: ROAM INTERRUPTED BY LIVE NON-GRAY TARGET; level-band migration complete.");
                }
                grayMigrationActive_ = false;
                ResetRoam();
                SetState(GrindModeState::Grinding);
                nextApproachScanTick_ = tick;
                return;
            }

            if (!roamNavigator_ ||
                roamDestinationIndex_ == InvalidSectorIndex ||
                roamDestinationIndex_ >= sectors_.size())
            {
                ResetRoam();
                SetState(GrindModeState::Grinding);
                return;
            }

            roamNavigator_->Update(world.player, tick);

            if (roamNavigator_->Failed())
            {
                auto& sector = sectors_[roamDestinationIndex_];
                sector.cooldownUntil = tick + FailedSectorCooldownTicks;
                ++sector.failures;
                ++roamLegsFailed_;

                Debug::Logger::Info(
                    "GRIND 14G.2: ROAM LEG FAILED index=" +
                    std::to_string(roamDestinationIndex_) +
                    " ring=" + std::to_string(sector.ring) +
                    " spoke=" + std::to_string(sector.spoke) +
                    " failures=" + std::to_string(sector.failures));

                if (dangerEscapeActive_)
                {
                    dangerEscapeActive_ = false;
                    postDeathEscapePending_ = true;
                    Debug::Logger::Info(
                        "DANGER MEMORY 14G.5: post-death escape leg failed; another safe sector will be attempted.");
                }
                ResetRoam();
                SetState(GrindModeState::Grinding);
                nextRoamScanTick_ = tick + 1;
                return;
            }

            if (!roamNavigator_->Arrived())
                return;

            const std::size_t arrivedIndex = roamDestinationIndex_;
            auto& sector = sectors_[arrivedIndex];
            const int arrivedRing = sector.ring;
            sector.lastVisitedTick = tick;
            sector.cooldownUntil = 0;
            ++sector.visits;
            ++roamLegsArrived_;
            activeSectorIndex_ = arrivedIndex;

            currentGrindAnchor_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };
            combat.UpdateTemporaryGrindRegion(
                currentGrindAnchor_,
                CombatRegionRadius);

            Debug::Logger::Info(
                "GRIND 14G.2: ROAM SECTOR ARRIVED index=" +
                std::to_string(activeSectorIndex_) +
                " ring=" + std::to_string(sector.ring) +
                " spoke=" + std::to_string(sector.spoke) +
                " visits=" + std::to_string(sector.visits));

            if (dangerEscapeActive_)
            {
                dangerEscapeActive_ = false;
                postDeathEscapePending_ = false;
                grindResumePoint_ = currentGrindAnchor_;
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: POST-DEATH ESCAPE COMPLETE; normal grind acquisition re-enabled outside the death hotspot.");
            }

            ResetRoam();
            lastLocalScan_ = AnalyzeLocalMobs(
                world, combat, tick, LocalScanRadius);

            if (lastLocalScan_.suitable > 0)
            {
                grindResumePoint_ = currentGrindAnchor_;
                if (grayMigrationActive_)
                {
                    Debug::Logger::Info(
                        "GRIND 14G.2: NON-GRAY LEVEL BAND FOUND; migration complete. suitable=" +
                        std::to_string(lastLocalScan_.suitable));
                }
                grayMigrationActive_ = false;
            }
            else
            {
                AdvanceMigrationSearchRegionIfNeeded(world, arrivedRing);
                Debug::Logger::Info(
                    "GRIND 14G.2: sector arrived but no suitable mobs yet; gray=" +
                    std::to_string(lastLocalScan_.gray) +
                    " tooHigh=" + std::to_string(lastLocalScan_.tooHigh) +
                    " -> continuing sector search.");
            }

            emptySectorScanStreak_ = 0;
            nextApproachScanTick_ = tick;
            nextRoamScanTick_ = tick + RoamScanIntervalTicks;
            SetState(GrindModeState::Grinding);
        }

        void UpdateEmptySectorAndRoam(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (combat.State() != CombatState::AcquiringTarget)
            {
                emptySectorScanStreak_ = 0;
                return;
            }

            if (tick < nextRoamScanTick_)
                return;

            nextRoamScanTick_ = tick + RoamScanIntervalTicks;

            if (FindDirectAggressor(world) != nullptr ||
                HasCombatRangeCandidate(world, combat, tick) ||
                HasApproachCandidate(world, combat, tick))
            {
                emptySectorScanStreak_ = 0;
                return;
            }

            ++emptySectorScanStreak_;
            if (emptySectorScanStreak_ == 1 ||
                emptySectorScanStreak_ == EmptySectorScanThreshold)
            {
                Debug::Logger::Info(
                    "GRIND 14G.2: EMPTY SECTOR SCAN streak=" +
                    std::to_string(emptySectorScanStreak_) + "/" +
                    std::to_string(EmptySectorScanThreshold) +
                    " activeSector=" +
                    (activeSectorIndex_ == InvalidSectorIndex
                        ? std::string("none")
                        : std::to_string(activeSectorIndex_)));
            }

            if (emptySectorScanStreak_ < EmptySectorScanThreshold)
                return;

            MarkActiveSectorExhausted(tick);
            TryStartRoam(world, combat, tick);
        }

    public:
        bool Start(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (state_ != GrindModeState::Idle)
                return false;

            grindHome_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };
            sectorOrigin_ = grindHome_;
            currentGrindAnchor_ = grindHome_;
            grindResumePoint_ = grindHome_;
            sectorBaseAngle_ = 0.0f;
            grayMigrationActive_ = false;
            migrationRegionAdvances_ = 0;
            BuildRoamSectors();

            combat.EnableTemporaryGrindMode(
                world.player,
                CombatRegionRadius);

            nextBagProbeTick_ = tick;
            nextMaintenanceProbeTick_ = tick;
            maintenanceSuppressedUntil_ = 0;
            startupVendorGraceUntil_ = tick + StartupVendorGraceTicks;
            startupVendorGraceSuppressionLogged_ = false;
            startupVendorGraceSuppressions_ = 0;
            postDeathEscapePending_ = false;
            dangerEscapeActive_ = false;
            deathEpisodeRecorded_ = false;
            nextApproachScanTick_ = tick;
            nextRoamScanTick_ = tick + RoamScanIntervalTicks;
            dangerMemory_.Initialize(world.activePlayerGuid);
            dangerMemory_.ObserveLevel(world.player.level);
            Navigation::NavigationHazardMemory::Instance().InitializeForMap(MapId);
            vendor_.ObserveWorld(world);
            firstAid_.Reset();
            firstAidIdleSinceTick_ = 0;
            ProbeBags(tick);
            ProbeMaintenance(tick, true);

            Debug::Logger::Info("================================");
            Debug::Logger::Info("GRIND 14G.2: WIDE AREA NAVMESH GRIND + VENDOR LOOP STARTED");
            Debug::Logger::Info(
                "Quest planner/discovery/turn-in ownership is paused while temporary grind mode is enabled.");
            Debug::Logger::Info(
                "Region origin=(" + std::to_string(sectorOrigin_.x) + "," +
                std::to_string(sectorOrigin_.y) + "," +
                std::to_string(sectorOrigin_.z) + ") generatedRoamRadius=" +
                std::to_string(MaximumRoamRadius) +
                " sectors=" + std::to_string(sectors_.size()));
            Debug::Logger::Info(
                "Normal grind targets are discovered from live ObjectManager Units; fixed creature entry IDs are not required.");
            Debug::Logger::Info(
                "Level policy: grind non-gray mobs up to player+1. grayBoundary=" +
                std::to_string(GrindLevelPolicy::GrayLevel(world.player.level)) +
                " playerLevel=" + std::to_string(world.player.level));
            Debug::Logger::Info(
                "Local live mobs are preferred; gray-only sectors trigger outward NavMesh level-band migration and the search origin can advance beyond the initial area.");
            Debug::Logger::Info(
                "Distant live grind targets are staged to ~22 yd so CombatController acquires them inside the Warrior Charge band.");
            Debug::Logger::Info(
                "GRIND 14G.1.4 preserved: remembered multi-aggro, post-Charge autoattack latch, and post-pack deferred corpse loot remain authoritative.");
            Debug::Logger::Info(
                "Bag policy: finish current combat/deferred loot, then vendor when freeSlots <= " +
                std::to_string(VendorTriggerFreeSlots) + ".");
            Debug::Logger::Info(
                "VENDOR DISCOVERY 14L.0: Tai'tasi(3187)/Zansoa(5942) remain preferred, but any locally visible NPC can be boundedly tested and proven as a merchant by MerchantFrame; verified merchants are reused for later trips.");
            Debug::Logger::Info(
                "DANGER MEMORY 14G.5: runtime death hotspots persist per character; two deaths near the same hotspot on the same level quarantine it until level-up.");
            Debug::Logger::Info(
                "NAV HAZARD 14I.0: persistent learned stall cells influence grind target and roam-sector selection; hard cells are avoided until decay or successful traversal evidence reopens them.");
            Debug::Logger::Info(
                "MAINTENANCE 14G.5.1: <=40% durability schedules repair, <=15% is urgent; food/drink restock target=12 with cash reserve, and drink is mana-user only.");
            Debug::Logger::Info(
                "VENDOR 14L.2.1: STARTUP VENDOR GRACE armed for 120 seconds; non-urgent food/drink/repair maintenance cannot hijack bot startup, while bag pressure and <=15% urgent repair remain immediate.");
            Debug::Logger::Info(
                "FIRST AID 14J.1: bandages are crafted only after a genuine idle dwell; movement/approach/roam are never interrupted. Live gray/trivial recipes remain excluded.");
            Debug::Logger::Info(
                "ROBUSTNESS 14K.1: stale seated recovery is actively stood up and grind idle-deadlocks are recovered after a bounded no-work window.");
            Debug::Logger::Info("================================");

            SetState(GrindModeState::Grinding);
            return true;
        }

        void Update(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (state_ == GrindModeState::Failed)
                return;

            vendor_.ObserveWorld(world);
            dangerMemory_.ObserveLevel(world.player.level);
            ProbeMaintenance(tick);

            if (state_ == GrindModeState::Idle)
            {
                if (!Start(world, combat, tick))
                    Fail("temporary grind mode failed to initialize.");
                return;
            }

            if (combat.State() == CombatState::Failed)
            {
                Fail("CombatController entered Failed state.");
                return;
            }

            // Phase 14K.1.3: CombatController::Update intentionally does nothing
            // in Idle. During an active GrindMode tick, death recovery is already
            // excluded by WorldMonitor, so a live player reaching Grinding+Idle
            // has no legitimate combat owner and would otherwise stay motionless.
            // Wake the existing combat loop immediately; RuntimeRobustnessSupervisor
            // independently treats the same state as acquisition idle as a bounded
            // fallback if this handoff ever fails to make progress.
            if (state_ == GrindModeState::Grinding &&
                combat.State() == CombatState::Idle &&
                world.player.valid &&
                world.player.maxHealth > 0 &&
                world.player.health > 0)
            {
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.1.3: GRIND COMBAT IDLE WAKE; restarting CombatController acquisition from the live position.");

                if (!combat.Start(tick))
                {
                    if (combat.State() == CombatState::Failed)
                    {
                        Fail("CombatController failed while 14K.1.3 attempted an idle wake.");
                        return;
                    }

                    Debug::Logger::Info(
                        "ROBUSTNESS 14K.1.3: combat idle wake was not accepted; bounded runtime supervisor fallback remains armed.");
                }
            }

            if (
                !dangerEscapeActive_ &&
                !postDeathEscapePending_ &&
                PlayerInsideQuarantinedArea(world) &&
                FindDirectAggressor(world) == nullptr &&
                combat.State() == CombatState::AcquiringTarget)
            {
                ResetApproach();
                ResetRoam();
                postDeathEscapePending_ = true;
                emptySectorScanStreak_ = 0;
                SetState(GrindModeState::Grinding);
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: live route entered quarantined hotspot; current work aborted and safe-sector escape armed.");
            }

            if (firstAid_.IsActive())
            {
                const bool craftingStillSafe =
                    state_ == GrindModeState::Grinding &&
                    combat.State() == CombatState::AcquiringTarget &&
                    !combat.HasDeferredCorpseLootPending() &&
                    FindDirectAggressor(world) == nullptr &&
                    world.player.health > 0 &&
                    !postDeathEscapePending_ &&
                    !dangerEscapeActive_;

                if (!craftingStillSafe)
                {
                    firstAid_.Abort("higher-priority combat/navigation work appeared");
                }
                else if (firstAid_.Update(world, tick))
                {
                    return;
                }
            }

            if (state_ == GrindModeState::Vendoring)
            {
                vendor_.Update(world, tick);

                if (vendor_.Failed())
                {
                    const bool urgent =
                        AutonomousMaintenancePolicy::Evaluate(maintenance_).urgentRepair;
                    const bool bagPressure =
                        bags_.valid && bags_.freeSlots <= VendorTriggerFreeSlots;
                    maintenanceSuppressedUntil_ = tick +
                        (urgent
                            ? UrgentMaintenanceRetryBackoffTicks
                            : (bagPressure
                                ? BagPressureVendorRetryBackoffTicks
                                : MaintenanceRetryBackoffTicks));
                    Debug::Logger::Info(
                        "MAINTENANCE 14G.5.1: vendor trip failed; failure is non-terminal for unattended grind. retryAfterTick=" +
                        std::to_string(maintenanceSuppressedUntil_));
                    vendor_.Reset();
                    nextBagProbeTick_ = tick + BagProbeIntervalTicks;
                    nextMaintenanceProbeTick_ = tick + MaintenanceProbeIntervalTicks;
                    nextApproachScanTick_ = tick + 1;
                    nextRoamScanTick_ = tick + 1;
                    SetState(GrindModeState::Grinding);
                    return;
                }

                if (vendor_.IsDone())
                {
                    const bool maintenanceUnmet = vendor_.MaintenanceUnmet();
                    ++vendorTrips_;
                    vendor_.Reset();
                    maintenanceSuppressedUntil_ = maintenanceUnmet
                        ? tick + MaintenanceRetryBackoffTicks
                        : 0;
                    nextBagProbeTick_ = tick;
                    nextMaintenanceProbeTick_ = tick;
                    nextApproachScanTick_ = tick;
                    nextRoamScanTick_ = tick + RoamScanIntervalTicks;
                    emptySectorScanStreak_ = 0;
                    activeSectorIndex_ = FindNearestSectorIndex(grindResumePoint_);
                    currentGrindAnchor_ = Navigation::NavPoint{
                        world.player.x,
                        world.player.y,
                        world.player.z
                    };
                    combat.UpdateTemporaryGrindRegion(
                        currentGrindAnchor_,
                        CombatRegionRadius);
                    ProbeBags(tick);
                    ProbeMaintenance(tick, true);

                    Debug::Logger::Info(
                        "GRIND 14G.5.1: vendor/maintenance trip complete; returned to previous grind area and resuming loop. maintenanceUnmet=" +
                        std::string(maintenanceUnmet ? "yes" : "no"));
                    SetState(GrindModeState::Grinding);
                }

                return;
            }

            ProbeBags(tick);
            ProbeMaintenance(tick);
            const bool bagPressure =
                bags_.valid && bags_.freeSlots <= VendorTriggerFreeSlots;

            if (startupVendorGraceUntil_ != 0 &&
                tick >= startupVendorGraceUntil_)
            {
                startupVendorGraceUntil_ = 0;
                startupVendorGraceSuppressionLogged_ = false;
                Debug::Logger::Info(
                    "VENDOR 14L.2.1: STARTUP VENDOR GRACE expired; normal autonomous maintenance policy is now active.");
            }

            const MaintenanceNeed rawMaintenanceNeed =
                AutonomousMaintenancePolicy::Evaluate(maintenance_);
            const bool startupVendorGraceActive =
                startupVendorGraceUntil_ != 0 && tick < startupVendorGraceUntil_;
            const bool startupVendorGraceBlocksNonUrgent =
                startupVendorGraceActive &&
                !bagPressure &&
                !rawMaintenanceNeed.urgentRepair;
            const MaintenanceNeed maintenanceNeed =
                bagPressure
                    ? rawMaintenanceNeed
                    : (startupVendorGraceBlocksNonUrgent
                        ? MaintenanceNeed{}
                        : CurrentMaintenanceNeed(tick));
            const bool maintenanceRetryAllowed =
                maintenanceSuppressedUntil_ == 0 ||
                tick >= maintenanceSuppressedUntil_ ||
                rawMaintenanceNeed.urgentRepair;
            const bool startupVendorGraceAllowsVendor =
                !startupVendorGraceActive ||
                bagPressure ||
                rawMaintenanceNeed.urgentRepair;
            const bool vendorRetryAllowed =
                maintenanceRetryAllowed && startupVendorGraceAllowsVendor;

            if (startupVendorGraceBlocksNonUrgent &&
                rawMaintenanceNeed.Any() &&
                !startupVendorGraceSuppressionLogged_)
            {
                startupVendorGraceSuppressionLogged_ = true;
                ++startupVendorGraceSuppressions_;
                Debug::Logger::Info(
                    "VENDOR 14L.2.1: STARTUP VENDOR GRACE suppressing non-urgent startup maintenance; grind owns startup until grace expires. repair=" +
                    std::string(rawMaintenanceNeed.repair ? "yes" : "no") +
                    " food=" + std::string(rawMaintenanceNeed.food ? "yes" : "no") +
                    " drink=" + std::string(rawMaintenanceNeed.drink ? "yes" : "no") +
                    " remainingTicks=" +
                    std::to_string(startupVendorGraceUntil_ - tick));
            }

            if (
                vendorRetryAllowed &&
                (bagPressure || maintenanceNeed.Any()) &&
                CombatSafeForVendor(combat) &&
                !combat.HasDeferredCorpseLootPending() &&
                FindDirectAggressor(world) == nullptr &&
                RecoveryController::HealthPercent(world.player) > 70.0f &&
                !postDeathEscapePending_ &&
                !dangerEscapeActive_)
            {
                ResetApproach();
                ResetRoam();

                grindResumePoint_ = Navigation::NavPoint{
                    world.player.x,
                    world.player.y,
                    world.player.z
                };

                Debug::Logger::Info("================================");
                Debug::Logger::Info(
                    "GRIND 14G.5.1: AUTONOMOUS MAINTENANCE -> VENDOR");
                Debug::Logger::Info(
                    "bagPressure=" + std::string(bagPressure ? "yes" : "no") +
                    " repair=" + (maintenanceNeed.repair ? std::string("yes") : std::string("no")) +
                    " urgentRepair=" + (maintenanceNeed.urgentRepair ? std::string("yes") : std::string("no")) +
                    " food=" + (maintenanceNeed.food ? std::string("yes") : std::string("no")) +
                    " drink=" + (maintenanceNeed.drink ? std::string("yes") : std::string("no")));
                Debug::Logger::Info(
                    "resumePoint=(" + std::to_string(grindResumePoint_.x) + "," +
                    std::to_string(grindResumePoint_.y) + "," +
                    std::to_string(grindResumePoint_.z) + ")");
                Debug::Logger::Info("================================");

                if (!vendor_.Start(
                        world,
                        grindResumePoint_,
                        tick,
                        maintenanceNeed,
                        bagPressure))
                {
                    maintenanceSuppressedUntil_ = tick +
                        (maintenanceNeed.urgentRepair
                            ? UrgentMaintenanceRetryBackoffTicks
                            : MaintenanceRetryBackoffTicks);
                    Debug::Logger::Info(
                        "MAINTENANCE 14G.5.1: VendorController could not start; continuing grind with bounded retry backoff.");
                    SetState(GrindModeState::Grinding);
                    return;
                }

                SetState(GrindModeState::Vendoring);
                return;
            }

            if (state_ == GrindModeState::ApproachingTarget)
            {
                UpdateApproach(world, combat, tick);
                return;
            }

            if (state_ == GrindModeState::Roaming)
            {
                UpdateRoam(world, combat, tick);
                return;
            }

            // Existing CombatController owns target/chase/fight/loot/recovery.
            // Phase 14G.4.2: after corpse resurrection the player can return
            // with low health. Recovery must outrank a new distant approach so
            // GrindMode never walks into the next pull before the existing
            // RecoveryController has restored the player.
            if (
                combat.State() == CombatState::AcquiringTarget &&
                world.player.health > 0 &&
                RecoveryController::HealthPercent(world.player) <
                    RecoveryController::EnterThresholdPercent() &&
                FindDirectAggressor(world) == nullptr)
            {
                combat.Update(world, tick);
                return;
            }

            if (
                combat.State() == CombatState::AcquiringTarget &&
                TryStartDangerEscape(world, tick))
            {
                return;
            }

            // First stage a visible distant target. If there is no work, give
            // CombatController one update so deferred corpse loot and recovery
            // retain priority; only then classify the sector as empty and roam.
            if (
                combat.State() == CombatState::AcquiringTarget &&
                TryStartApproach(world, combat, tick))
            {
                return;
            }

            combat.Update(world, tick);

            if (state_ == GrindModeState::Grinding)
                UpdateEmptySectorAndRoam(world, combat, tick);

            // Phase 14J.1: First Aid crafting is opportunistic, never movement-owned.
            // Only begin a batch after the normal combat/approach/roam pipeline has
            // had a chance to claim work and the bot has remained genuinely idle.
            const bool firstAidIdleSafe =
                state_ == GrindModeState::Grinding &&
                !firstAid_.IsActive() &&
                bags_.valid &&
                bags_.freeSlots > VendorTriggerFreeSlots &&
                combat.State() == CombatState::AcquiringTarget &&
                !combat.HasDeferredCorpseLootPending() &&
                FindDirectAggressor(world) == nullptr &&
                RecoveryController::HealthPercent(world.player) >=
                    RecoveryController::ExitThresholdPercent() &&
                !postDeathEscapePending_ &&
                !dangerEscapeActive_;

            if (!firstAidIdleSafe)
            {
                firstAidIdleSinceTick_ = 0;
                return;
            }

            if (firstAidIdleSinceTick_ == 0)
            {
                firstAidIdleSinceTick_ = tick;
                return;
            }

            if (tick - firstAidIdleSinceTick_ < FirstAidIdleDwellTicks)
                return;

            if (firstAid_.Update(world, tick))
            {
                firstAidIdleSinceTick_ = 0;
                return;
            }
        }

        void SuspendForDeathRecovery(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            ResetApproach();
            ResetRoam();
            vendor_.Reset();
            firstAid_.Abort("death recovery");
            firstAidIdleSinceTick_ = 0;
            emptySectorScanStreak_ = 0;
            nextApproachScanTick_ = tick + 1;
            nextRoamScanTick_ = tick + 1;
            nextBagProbeTick_ = tick + 1;
            nextMaintenanceProbeTick_ = tick + 1;
            dangerEscapeActive_ = false;

            currentGrindAnchor_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };

            combat.UpdateTemporaryGrindRegion(
                currentGrindAnchor_,
                CombatRegionRadius);

            SetState(GrindModeState::Grinding);

            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.2: GrindMode suspended; approach/roam/vendor movement ownership cleared at death anchor.");
        }

        void ResumeAfterDeathRecovery(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            ResetApproach();
            ResetRoam();
            vendor_.Reset();
            firstAid_.Abort("death recovery");
            firstAidIdleSinceTick_ = 0;
            emptySectorScanStreak_ = 0;
            nextApproachScanTick_ = tick + 1;
            nextRoamScanTick_ = tick + 1;
            nextBagProbeTick_ = tick;
            nextMaintenanceProbeTick_ = tick;
            dangerEscapeActive_ = false;
            deathEpisodeRecorded_ = false;

            currentGrindAnchor_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };

            activeSectorIndex_ = FindNearestSectorIndex(currentGrindAnchor_);

            combat.UpdateTemporaryGrindRegion(
                currentGrindAnchor_,
                CombatRegionRadius);

            ProbeBags(tick);
            ProbeMaintenance(tick, true);
            SetState(GrindModeState::Grinding);

            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.2: GrindMode resumed at corpse position; recovery gate has priority before the next pull.");
            if (postDeathEscapePending_)
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: post-resurrection safe-sector escape armed; normal pulling remains suppressed after recovery until escape starts.");
            }
        }

        void RecordDeath(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            (void)tick;
            if (deathEpisodeRecorded_)
                return;
            deathEpisodeRecorded_ = true;
            dangerMemory_.Initialize(world.activePlayerGuid);
            dangerMemory_.ObserveLevel(world.player.level);
            dangerMemory_.RecordDeath(
                MapId,
                Navigation::NavPoint{
                    world.player.x,
                    world.player.y,
                    world.player.z},
                world.player.level);
            postDeathEscapePending_ = true;
            dangerEscapeActive_ = false;
        }

        static constexpr std::uint32_t MapIdValue()
        {
            return MapId;
        }

        bool ForceAntiAfkSafeguard(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            const std::string& reason)
        {
            /*
             * Phase 14K.1.7: this is a bounded, useful-work AFK safeguard.
             * It never steals ownership from combat/recovery/vendor/First Aid.
             * When the normal grind pipeline has remained safely stationary for
             * roughly one minute, refresh stale acquisition and prefer real
             * NavMesh work. HoldPosition is only a final client-activity fallback;
             * no claim is made that a particular private server counts it as AFK
             * activity.
             */
            const bool acquisitionState =
                combat.State() == CombatState::Idle ||
                combat.State() == CombatState::AcquiringTarget ||
                combat.State() == CombatState::WaitingForTargetSelection;

            if (state_ != GrindModeState::Grinding ||
                !world.player.valid ||
                world.player.maxHealth == 0 ||
                world.player.health == 0 ||
                !acquisitionState ||
                combat.LockedGuid() != 0 ||
                combat.HasDeferredCorpseLootPending() ||
                combat.Recovery().IsActive() ||
                firstAid_.IsActive() ||
                postDeathEscapePending_ ||
                dangerEscapeActive_ ||
                RecoveryController::HealthPercent(world.player) <
                    RecoveryController::ExitThresholdPercent() ||
                FindDirectAggressor(world) != nullptr)
            {
                return false;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "ROBUSTNESS 14K.1.7: ACTIVE BOT AFK SAFEGUARD fired state=" +
                std::string(StateNameInternal(state_)) +
                " combat=" + combat.StateName());
            Debug::Logger::Info("Reason: " + reason);

            const bool standIssued =
                PlayerPostureController::EnsureStanding(
                    world.player,
                    "14K.1.7 active bot AFK safeguard");

            bool acquisitionReset = false;
            if (combat.State() == CombatState::Idle)
            {
                acquisitionReset = combat.Start(tick);
            }
            else if (combat.State() == CombatState::WaitingForTargetSelection)
            {
                acquisitionReset = combat.ForceRuntimeSupervisorReset(
                    world,
                    tick,
                    "14K.1.7 safe-idle acquisition refresh",
                    false);
            }

            if (combat.State() == CombatState::AcquiringTarget &&
                TryStartApproach(world, combat, tick))
            {
                ++antiAfkActions_;
                ++antiAfkNavigationActions_;
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.1.7: AFK SAFEGUARD -> real NavMesh approach started; actions=" +
                    std::to_string(antiAfkActions_));
                Debug::Logger::Info("================================");
                return true;
            }

            if (combat.State() == CombatState::AcquiringTarget)
            {
                const CombatState before = combat.State();
                const std::uint64_t beforeGuid = combat.LockedGuid();
                combat.Update(world, tick);

                if (combat.State() != before || combat.LockedGuid() != beforeGuid)
                {
                    ++antiAfkActions_;
                    ++antiAfkAcquisitionActions_;
                    Debug::Logger::Info(
                        "ROBUSTNESS 14K.1.7: AFK SAFEGUARD -> acquisition work issued; combat=" +
                        std::string(combat.StateName()) +
                        " locked=" + std::to_string(combat.LockedGuid()) +
                        " actions=" + std::to_string(antiAfkActions_));
                    Debug::Logger::Info("================================");
                    return true;
                }
            }

            if (combat.State() == CombatState::AcquiringTarget &&
                TryStartRoam(world, combat, tick))
            {
                ++antiAfkActions_;
                ++antiAfkNavigationActions_;
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.1.7: AFK SAFEGUARD -> real NavMesh roam started; actions=" +
                    std::to_string(antiAfkActions_));
                Debug::Logger::Info("================================");
                return true;
            }

            const bool holdIssued = MovementController::HoldPosition(world.player);
            if (standIssued || acquisitionReset || holdIssued)
            {
                ++antiAfkActions_;
                ++antiAfkFallbackPulses_;
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.1.7: AFK SAFEGUARD fallback client pulse standIssued=" +
                    std::string(standIssued ? "yes" : "no") +
                    " acquisitionReset=" +
                    std::string(acquisitionReset ? "yes" : "no") +
                    " holdIssued=" +
                    std::string(holdIssued ? "yes" : "no") +
                    " fallbackHolds=" +
                    std::to_string(antiAfkFallbackPulses_) +
                    "; server AFK semantics remain server-specific.");
                Debug::Logger::Info("================================");
                return true;
            }

            Debug::Logger::Info(
                "ROBUSTNESS 14K.1.7: AFK SAFEGUARD could not issue safe work; will retry after bounded cooldown.");
            Debug::Logger::Info("================================");
            return false;
        }

        bool ForceAutonomyMovementRecovery(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            const std::string& reason)
        {
            if (
                state_ != GrindModeState::ApproachingTarget &&
                state_ != GrindModeState::Roaming)
            {
                return false;
            }

            ++autonomyMovementRecoveries_;

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "AUTONOMY 14G.4.1: MOVEMENT HARD-STALL RECOVERY state=" +
                std::string(StateNameInternal(state_)));
            Debug::Logger::Info("Reason: " + reason);

            if (state_ == GrindModeState::ApproachingTarget && approachGuid_ != 0)
            {
                approachBlacklistUntil_[approachGuid_] =
                    tick + ApproachBlacklistTicks;
                Debug::Logger::Info(
                    "AUTONOMY 14G.4.1: stalled approach target temporarily blacklisted guid=" +
                    std::to_string(approachGuid_));
            }

            if (
                state_ == GrindModeState::Roaming &&
                roamDestinationIndex_ != InvalidSectorIndex &&
                roamDestinationIndex_ < sectors_.size())
            {
                auto& sector = sectors_[roamDestinationIndex_];
                ++sector.failures;
                sector.cooldownUntil = tick + FailedSectorCooldownTicks;
                ++roamLegsFailed_;
                Debug::Logger::Info(
                    "AUTONOMY 14G.4.1: stalled roam sector quarantined index=" +
                    std::to_string(roamDestinationIndex_));
            }

            ResetApproach();
            ResetRoam();

            currentGrindAnchor_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };

            combat.UpdateTemporaryGrindRegion(
                currentGrindAnchor_,
                CombatRegionRadius);

            nextApproachScanTick_ = tick + 1;
            nextRoamScanTick_ = tick + 1;
            emptySectorScanStreak_ = 0;

            SetState(GrindModeState::Grinding);

            Debug::Logger::Info(
                "AUTONOMY 14G.4.1: movement ownership reset at live safe anchor; alternate work will be selected.");
            Debug::Logger::Info("================================");
            return true;
        }

        bool ForceRuntimeRobustnessRecovery(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            const std::string& reason,
            bool escalated)
        {
            if (state_ == GrindModeState::Failed)
                return false;

            ++runtimeSupervisorResets_;
            if (escalated)
                ++runtimeSupervisorEscalations_;

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "ROBUSTNESS 14K.0: GLOBAL RECOVERY state=" +
                std::string(StateNameInternal(state_)) +
                " combat=" + combat.StateName() +
                " escalated=" + (escalated ? std::string("yes") : std::string("no")));
            Debug::Logger::Info("Reason: " + reason);

            if (firstAid_.IsActive())
                firstAid_.Abort("runtime robustness recovery took priority");
            firstAidIdleSinceTick_ = 0;

            if (state_ == GrindModeState::ApproachingTarget && approachGuid_ != 0)
            {
                approachBlacklistUntil_[approachGuid_] =
                    tick + (escalated
                        ? ApproachBlacklistTicks * 2
                        : ApproachBlacklistTicks);
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.0: active approach target quarantined before global reset guid=" +
                    std::to_string(approachGuid_));
            }

            if (state_ == GrindModeState::Roaming &&
                roamDestinationIndex_ != InvalidSectorIndex &&
                roamDestinationIndex_ < sectors_.size())
            {
                auto& sector = sectors_[roamDestinationIndex_];
                ++sector.failures;
                sector.cooldownUntil = tick +
                    (escalated
                        ? FailedSectorCooldownTicks * 2
                        : FailedSectorCooldownTicks);
                ++roamLegsFailed_;
            }

            if (state_ == GrindModeState::Vendoring)
            {
                vendor_.Reset();
                maintenanceSuppressedUntil_ = tick +
                    (escalated
                        ? MaintenanceRetryBackoffTicks
                        : UrgentMaintenanceRetryBackoffTicks);
                Debug::Logger::Info(
                    "ROBUSTNESS 14K.0: stalled vendor workflow reset with bounded maintenance backoff.");
            }

            ResetApproach();
            ResetRoam();

            combat.ForceRuntimeSupervisorReset(
                world,
                tick,
                reason,
                escalated);

            PlayerPostureController::EnsureStanding(
                world.player,
                "runtime supervisor cleared stale ownership");

            currentGrindAnchor_ = Navigation::NavPoint{
                world.player.x,
                world.player.y,
                world.player.z
            };
            combat.UpdateTemporaryGrindRegion(
                currentGrindAnchor_,
                CombatRegionRadius);

            // Escalation contributes only soft evidence to the persistent
            // hazard memory. One controller loop can therefore never hard-
            // blacklist an otherwise valid location by itself.
            if (escalated)
            {
                Navigation::NavigationHazardMemory::Instance().RecordFailure(
                    MapId,
                    currentGrindAnchor_,
                    tick,
                    1,
                    "14K.0 repeated global no-progress loop");
            }

            nextApproachScanTick_ = tick + (escalated ? 8 : 2);
            nextRoamScanTick_ = tick + (escalated ? 8 : 2);
            nextBagProbeTick_ = tick + BagProbeIntervalTicks;
            nextMaintenanceProbeTick_ = tick + MaintenanceProbeIntervalTicks;
            emptySectorScanStreak_ = escalated
                ? (EmptySectorScanThreshold - 1)
                : 0;
            SetState(GrindModeState::Grinding);

            Debug::Logger::Info(
                "ROBUSTNESS 14K.0: transient ownership cleared; alternate work will be selected from the live position.");
            Debug::Logger::Info("================================");
            return true;
        }

        GrindModeState State() const { return state_; }
        const char* StateName() const { return StateNameInternal(state_); }
        bool Failed() const { return state_ == GrindModeState::Failed; }
        const GrindBagMonitor::Snapshot& Bags() const { return bags_; }
        const VendorController& Vendor() const { return vendor_; }
        bool FirstAidActive() const { return firstAid_.IsActive(); }
        int FirstAidCraftsIssued() const { return firstAid_.CraftsIssued(); }
        int VendorTrips() const { return vendorTrips_; }
        int RoamLegsStarted() const { return roamLegsStarted_; }
        int RoamLegsArrived() const { return roamLegsArrived_; }
        int RoamLegsFailed() const { return roamLegsFailed_; }
        bool GrayMigrationActive() const { return grayMigrationActive_; }
        std::uint32_t GrayLevel(std::uint32_t playerLevel) const
        {
            return GrindLevelPolicy::GrayLevel(playerLevel);
        }
        int LocalSuitableCount() const { return lastLocalScan_.suitable; }
        int LocalGrayCount() const { return lastLocalScan_.gray; }
        int LocalTooHighCount() const { return lastLocalScan_.tooHigh; }
        int ActiveSectorIndex() const
        {
            return activeSectorIndex_ == InvalidSectorIndex
                ? -1
                : static_cast<int>(activeSectorIndex_);
        }
        int RoamAttempts() const { return roamLegsStarted_; }
        int RoamArrivals() const { return roamLegsArrived_; }
        int MigrationRegionAdvances() const { return migrationRegionAdvances_; }
        int AutonomyMovementRecoveries() const { return autonomyMovementRecoveries_; }
        int RuntimeSupervisorResets() const { return runtimeSupervisorResets_; }
        int RuntimeSupervisorEscalations() const { return runtimeSupervisorEscalations_; }
        int AntiAfkActions() const { return antiAfkActions_; }
        int AntiAfkNavigationActions() const { return antiAfkNavigationActions_; }
        int AntiAfkAcquisitionActions() const { return antiAfkAcquisitionActions_; }
        int AntiAfkFallbackPulses() const { return antiAfkFallbackPulses_; }
        int DangerDeaths() const { return dangerMemory_.TotalDeaths(); }
        int DangerHotspots() const { return static_cast<int>(dangerMemory_.HotspotCount()); }
        int ActiveDangerQuarantines(std::uint32_t playerLevel) const
        {
            return dangerMemory_.ActiveQuarantines(playerLevel);
        }
        const MaintenanceSnapshot& Maintenance() const { return maintenance_; }
        bool StartupVendorGraceActive(std::uint64_t tick) const
        {
            return startupVendorGraceUntil_ != 0 && tick < startupVendorGraceUntil_;
        }
        std::uint64_t StartupVendorGraceRemainingTicks(std::uint64_t tick) const
        {
            return StartupVendorGraceActive(tick)
                ? startupVendorGraceUntil_ - tick
                : 0;
        }
        int StartupVendorGraceSuppressions() const
        {
            return startupVendorGraceSuppressions_;
        }
        bool PostDeathEscapePending() const { return postDeathEscapePending_; }
    };
}
