#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace Bot
{
    // Snapshot facts only. "Nearby hostile" is a conservative, potentially
    // attackable creature proxy; the client does not expose an exact aggro
    // radius or UnitCanAttack result for every unselected GUID.
    struct PullSafetyUnit
    {
        std::uint64_t guid = 0;
        std::uint32_t entry = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float playerDistance = 0.0f;
        float existingRouteRisk = 0.0f;
        bool live = false;
        bool eligible = false;
        bool nearbyHostile = false;
        bool directAggressor = false;
    };

    enum class PullSafetyDecision
    {
        Defensive,
        Voluntary,
        WaitForHealth,
        NoEligibleCandidate,
        NoSafeCandidate
    };

    enum class PullSafetyRejectReason
    {
        None,
        LowHealth,
        ExistingAggressor,
        TooManyPredictedAdds,
        NotIsolated,
        InvalidCandidate
    };

    struct PullSafetyAssessment
    {
        PullSafetyRejectReason rejection = PullSafetyRejectReason::InvalidCandidate;
        int predictedAdds = 0;
        float nearestHostileDistance =
            std::numeric_limits<float>::infinity();
    };

    struct PullSafetySelection
    {
        PullSafetyDecision decision = PullSafetyDecision::NoEligibleCandidate;
        std::size_t selectedIndex = std::numeric_limits<std::size_t>::max();
        PullSafetyAssessment selected{};
        std::size_t candidateCount = 0;
        std::size_t rejectedCount = 0;
        std::size_t aggressorCount = 0;
    };

    class PullSafetyPolicy
    {
    public:
        static constexpr float MinimumVoluntaryHealthPercent = 70.0f;
        static constexpr float HighHealthPercent = 85.0f;
        static constexpr float NearbyHostileRadius = 12.0f;
        static constexpr float IsolationRankingCap = 24.0f;
        static constexpr std::size_t MaximumEvaluatedCandidates = 32;

        static bool HealthKnown(float healthPercent)
        {
            return std::isfinite(healthPercent) &&
                healthPercent >= 0.0f && healthPercent <= 100.0f;
        }

        static bool Live(const PullSafetyUnit& unit)
        {
            return unit.live && unit.guid != 0 &&
                std::isfinite(unit.x) && std::isfinite(unit.y) &&
                std::isfinite(unit.z) &&
                std::isfinite(unit.playerDistance) &&
                unit.playerDistance >= 0.0f;
        }

        static PullSafetyAssessment Assess(
            std::span<const PullSafetyUnit> units, std::size_t index,
            float healthPercent, bool anyAggressor)
        {
            PullSafetyAssessment result{};
            if (index >= units.size() || !Live(units[index]))
                return result;
            const auto& candidate = units[index];
            if (candidate.directAggressor)
            {
                result.rejection = PullSafetyRejectReason::None;
                return result;
            }
            if (!candidate.eligible)
                return result;
            if (anyAggressor)
            {
                result.rejection = PullSafetyRejectReason::ExistingAggressor;
                return result;
            }
            if (!HealthKnown(healthPercent) ||
                healthPercent < MinimumVoluntaryHealthPercent)
            {
                result.rejection = PullSafetyRejectReason::LowHealth;
                return result;
            }

            for (const auto& other : units)
            {
                if (!Live(other) || !other.nearbyHostile ||
                    other.guid == candidate.guid)
                    continue;
                const float dx = candidate.x - other.x;
                const float dy = candidate.y - other.y;
                const float dz = candidate.z - other.z;
                const float separation = std::sqrt(dx * dx + dy * dy + dz * dz);
                result.nearestHostileDistance =
                    std::min(result.nearestHostileDistance, separation);
                if (separation <= NearbyHostileRadius)
                    ++result.predictedAdds;
            }

            if (result.predictedAdds >= 2)
                result.rejection = PullSafetyRejectReason::TooManyPredictedAdds;
            else if (result.predictedAdds == 1 &&
                healthPercent < HighHealthPercent)
                result.rejection = PullSafetyRejectReason::NotIsolated;
            else
                result.rejection = PullSafetyRejectReason::None;
            return result;
        }

        static PullSafetySelection Select(
            std::span<const PullSafetyUnit> units, float healthPercent)
        {
            PullSafetySelection result{};
            std::vector<std::size_t> candidates;
            candidates.reserve(std::min(units.size(), MaximumEvaluatedCandidates));
            for (std::size_t i = 0; i < units.size(); ++i)
            {
                if (!Live(units[i]))
                    continue;
                if (units[i].directAggressor)
                {
                    ++result.aggressorCount;
                    if (result.selectedIndex == std::numeric_limits<std::size_t>::max() ||
                        units[i].playerDistance < units[result.selectedIndex].playerDistance ||
                        (units[i].playerDistance == units[result.selectedIndex].playerDistance &&
                         units[i].guid < units[result.selectedIndex].guid))
                        result.selectedIndex = i;
                }
                if (units[i].eligible)
                    candidates.push_back(i);
            }
            result.candidateCount = candidates.size();
            if (result.aggressorCount != 0)
            {
                result.decision = PullSafetyDecision::Defensive;
                result.selected.rejection = PullSafetyRejectReason::None;
                return result;
            }
            if (!HealthKnown(healthPercent) ||
                healthPercent < MinimumVoluntaryHealthPercent)
            {
                result.decision = PullSafetyDecision::WaitForHealth;
                return result;
            }
            if (candidates.empty())
                return result;

            std::sort(candidates.begin(), candidates.end(),
                [&units](std::size_t a, std::size_t b)
                {
                    if (units[a].playerDistance != units[b].playerDistance)
                        return units[a].playerDistance < units[b].playerDistance;
                    return units[a].guid < units[b].guid;
                });
            if (candidates.size() > MaximumEvaluatedCandidates)
                candidates.resize(MaximumEvaluatedCandidates);

            for (const auto index : candidates)
            {
                const auto assessment = Assess(units, index, healthPercent, false);
                if (assessment.rejection != PullSafetyRejectReason::None)
                {
                    ++result.rejectedCount;
                    continue;
                }
                if (result.selectedIndex == std::numeric_limits<std::size_t>::max())
                {
                    result.selectedIndex = index;
                    result.selected = assessment;
                    continue;
                }
                const auto& chosen = units[result.selectedIndex];
                const auto& contender = units[index];
                const float contenderIsolation = std::min(
                    assessment.nearestHostileDistance, IsolationRankingCap);
                const float chosenIsolation = std::min(
                    result.selected.nearestHostileDistance, IsolationRankingCap);
                if (assessment.predictedAdds < result.selected.predictedAdds ||
                    (assessment.predictedAdds == result.selected.predictedAdds &&
                     (contenderIsolation > chosenIsolation ||
                      (contenderIsolation == chosenIsolation &&
                       (contender.existingRouteRisk < chosen.existingRouteRisk ||
                        (contender.existingRouteRisk == chosen.existingRouteRisk &&
                         (contender.playerDistance < chosen.playerDistance ||
                          (contender.playerDistance == chosen.playerDistance &&
                           contender.guid < chosen.guid))))))))
                {
                    result.selectedIndex = index;
                    result.selected = assessment;
                }
            }
            result.decision = result.selectedIndex ==
                    std::numeric_limits<std::size_t>::max()
                ? PullSafetyDecision::NoSafeCandidate
                : PullSafetyDecision::Voluntary;
            return result;
        }

        static const char* DecisionName(PullSafetyDecision decision)
        {
            switch (decision)
            {
                case PullSafetyDecision::Defensive: return "defensive";
                case PullSafetyDecision::Voluntary: return "select";
                case PullSafetyDecision::WaitForHealth: return "wait";
                case PullSafetyDecision::NoEligibleCandidate: return "no_eligible_candidate";
                case PullSafetyDecision::NoSafeCandidate: return "no_safe_candidate";
            }
            return "unknown";
        }
    };
}
