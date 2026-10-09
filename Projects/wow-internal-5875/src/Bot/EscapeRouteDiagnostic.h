#pragma once

#include "EscapeDecisionPolicy.h"
#include "EscapeEvaluationEpisodePolicy.h"
#include "EscapeRouteSelectionPolicy.h"
#include "GrindModeController.h"
#include "GrindTargetPolicy.h"

#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Navigation/NavigationHazardMemory.h"
#include "../Objects/WorldState.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Bot
{
    // Diagnostic ownership only: one independent planning-only follower at a
    // time, all using the frozen WorldMonitor snapshot origin. This class
    // never sees or changes the active combat/grind navigation follower.
    class EscapeRouteDiagnostic
    {
    private:
        static constexpr float ProbeArrivalDistance = 2.0f;

        EscapeRouteGeneration generation_{};
        std::vector<EscapeRouteProbeEvidence> evidence_{};
        std::vector<std::size_t> probeOrder_{};
        std::vector<bool> evaluated_{};
        Objects::PlayerState originPlayer_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> probe_{};
        std::size_t probePosition_ = 0;
        std::uint64_t episodeStartedTick_ = 0;
        std::uint64_t probeStartedTick_ = 0;
        std::uint64_t lastProbeTicks_ = 0;
        std::uint64_t lastEpisodeTicks_ = 0;
        std::size_t lastCandidateIndex_ = 0;
        std::size_t evaluatedCount_ = 0;
        std::uint64_t targetGuid_ = 0;
        bool active_ = false;
        bool finished_ = false;
        bool incomplete_ = false;
        const char* incompleteReason_ = "probe_incomplete";
        int movementCommands_ = 0;

        static Navigation::NavPoint Nav(const EscapeRoutePoint& point)
        {
            return {point.x, point.y, point.z};
        }

        EscapeRouteSelection Finish()
        {
            active_ = false;
            finished_ = true;
            if (probe_)
                movementCommands_ += probe_->Commands();
            probe_.reset();
            EscapeRouteSelection selected =
                EscapeRouteSelectionPolicy::Select(generation_, evidence_);
            // An incomplete probe cannot establish the best reachable route.
            if (incomplete_ || movementCommands_ != 0)
            {
                selected.decision = EscapeRouteDecisionKind::InsufficientData;
                selected.reason = movementCommands_ != 0
                    ? "planning_probe_movement_violation"
                    : incompleteReason_;
            }
            return selected;
        }

    public:
        void Reset()
        {
            probe_.reset();
            generation_ = {};
            evidence_.clear();
            probeOrder_.clear();
            evaluated_.clear();
            originPlayer_ = {};
            probePosition_ = 0;
            episodeStartedTick_ = 0;
            probeStartedTick_ = 0;
            lastProbeTicks_ = 0;
            lastEpisodeTicks_ = 0;
            lastCandidateIndex_ = 0;
            evaluatedCount_ = 0;
            targetGuid_ = 0;
            active_ = false;
            finished_ = false;
            incomplete_ = false;
            incompleteReason_ = "probe_incomplete";
            movementCommands_ = 0;
        }

        bool Active() const { return active_; }
        bool HasEpisode() const { return active_ || finished_; }
        bool StartedFor(std::uint64_t targetGuid) const
        {
            return targetGuid_ == targetGuid && (active_ || finished_);
        }
        std::size_t CandidateCount() const
        {
            return generation_.candidates.size();
        }
        int MovementCommands() const { return movementCommands_; }
        std::size_t EvaluatedCount() const { return evaluatedCount_; }
        std::size_t LastCandidateIndex() const { return lastCandidateIndex_; }
        std::uint64_t LastProbeTicks() const { return lastProbeTicks_; }
        std::uint64_t LastEpisodeTicks() const { return lastEpisodeTicks_; }
        const EscapeRouteGeneration& Generation() const
        {
            return generation_;
        }
        const std::vector<EscapeRouteProbeEvidence>& Evidence() const
        {
            return evidence_;
        }

        std::optional<EscapeRouteSelection> Begin(
            const Objects::WorldState& world,
            const GrindModeController& grind,
            std::uint64_t targetGuid,
            std::uint64_t tick)
        {
            Reset();
            targetGuid_ = targetGuid;
            episodeStartedTick_ = tick;
            originPlayer_ = world.player;
            std::vector<EscapeRoutePoint> threats;
            if (world.player.valid && world.activePlayerGuid != 0 &&
                targetGuid != 0 &&
                EscapeRouteSelectionPolicy::Finite({
                    world.player.x, world.player.y, world.player.z}))
            {
                for (const auto& unit : world.units)
                {
                    if (!GrindTargetPolicy::LooksLikeCombatCreature(unit) ||
                        unit.targetGuid != world.activePlayerGuid ||
                        !std::isfinite(unit.distance) || unit.distance < 0.0f ||
                        unit.distance > EscapeDecisionPolicy::LocalAggressorRadius)
                        continue;
                    threats.push_back({unit.x, unit.y, unit.z});
                    if (threats.size() > EscapeRouteSelectionPolicy::MaximumThreats)
                        break; // fail closed in Generate(), never truncate.
                }
            }

            generation_ = EscapeRouteSelectionPolicy::Generate(
                {world.player.x, world.player.y, world.player.z}, threats);
            evidence_.resize(generation_.candidates.size());
            probeOrder_ = EscapeRouteSelectionPolicy::ProbeOrder(generation_);
            evaluated_.resize(generation_.candidates.size(), false);
            if (!generation_.threatsKnown || generation_.candidates.empty())
                return Finish();

            const Navigation::NavPoint origin{
                world.player.x, world.player.y, world.player.z};
            const float currentDeathRisk = grind.EscapeDiagnosticDangerRiskAt(
                origin, world.player.level);
            const float currentNavRisk =
                Navigation::NavigationHazardMemory::Instance().RiskAt(
                    GrindModeController::MapIdValue(), origin);
            for (std::size_t i = 0; i < evidence_.size(); ++i)
            {
                auto& evidence = evidence_[i];
                const auto point = Nav(generation_.candidates[i].destination);
                evidence.currentDeathRisk = currentDeathRisk;
                evidence.destinationDeathRisk =
                    grind.EscapeDiagnosticDangerRiskAt(point, world.player.level);
                evidence.deathRiskKnown = std::isfinite(currentDeathRisk) &&
                    std::isfinite(evidence.destinationDeathRisk);
                evidence.currentNavHazardRisk = currentNavRisk;
                evidence.destinationNavHazardRisk =
                    Navigation::NavigationHazardMemory::Instance().RiskAt(
                        GrindModeController::MapIdValue(), point);
                evidence.navHazardRiskKnown = std::isfinite(currentNavRisk) &&
                    std::isfinite(evidence.destinationNavHazardRisk);
            }
            active_ = true;
            return std::nullopt;
        }

        std::optional<EscapeRouteSelection> Step(std::uint64_t tick)
        {
            if (!active_)
                return std::nullopt;
            lastEpisodeTicks_ = tick >= episodeStartedTick_
                ? tick - episodeStartedTick_ : 0;
            if (EscapeEvaluationEpisodePolicy::EpisodeTimedOut(
                    episodeStartedTick_, tick))
            {
                incomplete_ = true;
                incompleteReason_ = "episode_timeout";
                return Finish();
            }
            if (probePosition_ >= probeOrder_.size())
                return Finish();

            const std::size_t candidateIndex = probeOrder_[probePosition_];

            if (!probe_)
            {
                probe_ = std::make_unique<
                    Navigation::GenericNavMeshPathFollower>();
                probeStartedTick_ = tick;
                const Navigation::GenericNavMeshStartOptions options{
                    false, true}; // no full-map fallback; planning only.
                if (!probe_->Start(
                        originPlayer_, tick,
                        Nav(generation_.candidates[candidateIndex].destination),
                        GrindModeController::MapIdValue(),
                        ProbeArrivalDistance, "escape diagnostic probe", true,
                        options))
                {
                    probe_.reset();
                    evaluated_[candidateIndex] = true;
                    ++evaluatedCount_;
                    ++probePosition_;
                }
                return std::nullopt;
            }

            probe_->Update(originPlayer_, tick);
            const auto result = probe_->PlanningOnlyResult();
            if (result.status == Navigation::RouteCostProbeStatus::Pending &&
                !EscapeEvaluationEpisodePolicy::ProbeTimedOut(
                    probeStartedTick_, tick))
                return std::nullopt;
            movementCommands_ += probe_->Commands();
            lastCandidateIndex_ = candidateIndex;
            lastProbeTicks_ = tick >= probeStartedTick_
                ? tick - probeStartedTick_ : 0;
            if (result.status == Navigation::RouteCostProbeStatus::Reachable)
            {
                if (probe_->PlanningOnlyReachedDestination())
                {
                    evidence_[candidateIndex].reachable = true;
                    evidence_[candidateIndex].routeCost = result.pathLength;
                }
                else
                {
                    incomplete_ = true; // valid prefix, unproven endpoint.
                    incompleteReason_ = "unproven_partial_prefix";
                }
            }
            else if (result.status == Navigation::RouteCostProbeStatus::Pending)
            {
                incomplete_ = true;
                incompleteReason_ = "probe_timeout";
            }
            probe_.reset();
            evaluated_[candidateIndex] = true;
            ++evaluatedCount_;
            ++probePosition_;
            // An incomplete earlier probe may be better than this route;
            // only strict dominance over all unprobed endpoints is enough.
            if (!incomplete_ && movementCommands_ == 0 &&
                EscapeRouteSelectionPolicy::CanFinishEarly(
                    generation_, evidence_, evaluated_) &&
                probePosition_ < probeOrder_.size())
            {
                auto selected = Finish();
                selected.reason = "early_dominant_complete_route";
                return selected;
            }
            if (probePosition_ >= probeOrder_.size())
                return Finish();
            return std::nullopt;
        }
    };
}
