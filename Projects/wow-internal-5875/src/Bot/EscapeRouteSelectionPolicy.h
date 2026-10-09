#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace Bot
{
    struct EscapeRoutePoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    enum class EscapeRouteDecisionKind
    {
        CandidateAvailable,
        NoReachableCandidate,
        InsufficientData
    };

    struct EscapeRouteCandidate
    {
        EscapeRoutePoint destination{};
        float minimumThreatDistance = 0.0f;
    };

    struct EscapeRouteGeneration
    {
        bool threatsKnown = false;
        float currentMinimumThreatDistance = 0.0f;
        std::vector<EscapeRouteCandidate> candidates{};
    };

    struct EscapeRouteProbeEvidence
    {
        bool reachable = false;
        float routeCost = 0.0f;
        bool deathRiskKnown = false;
        float currentDeathRisk = 0.0f;
        float destinationDeathRisk = 0.0f;
        bool navHazardRiskKnown = false;
        float currentNavHazardRisk = 0.0f;
        float destinationNavHazardRisk = 0.0f;
    };

    struct EscapeRouteSelection
    {
        EscapeRouteDecisionKind decision =
            EscapeRouteDecisionKind::InsufficientData;
        std::size_t selectedIndex = 0;
        std::size_t reachableCount = 0;
        float currentMinimumThreatDistance = 0.0f;
        float selectedMinimumThreatDistance = 0.0f;
        float routeCost = 0.0f;
        const char* reason = "insufficient_threat_positions";
    };

    // This is endpoint evidence, not a guarantee about the whole Detour
    // corridor. Reachability is supplied only by a separate planning-only
    // follower. No operation here can issue movement or spend route budgets.
    class EscapeRouteSelectionPolicy
    {
    public:
        static constexpr std::size_t MaximumThreats = 8;
        static constexpr std::size_t MaximumCandidates = 3;
        // One short local leg, matching the existing grind combat handoff
        // distance; neither a full-map search nor a flee movement order.
        static constexpr float CandidateDistance = 24.0f;
        // A visible separation gain must exceed small position jitter.
        static constexpr float MinimumSeparationGain = 4.0f;

        // Probe the strongest known endpoint separation first. The stable
        // index tie preserves deterministic route-cost comparisons.
        static std::vector<std::size_t> ProbeOrder(
            const EscapeRouteGeneration& generation)
        {
            std::vector<std::size_t> order;
            for (std::size_t i = 0; i < generation.candidates.size(); ++i)
                order.push_back(i);
            std::stable_sort(order.begin(), order.end(),
                [&](std::size_t a, std::size_t b) {
                    return generation.candidates[a].minimumThreatDistance >
                        generation.candidates[b].minimumThreatDistance;
                });
            return order;
        }

        // A completed, validated route may finish before other probes only
        // when its separation strictly dominates every unprobed endpoint.
        // Reachability and endpoint-risk vetoes are still enforced by Select.
        static bool CanFinishEarly(
            const EscapeRouteGeneration& generation,
            const std::vector<EscapeRouteProbeEvidence>& probes,
            const std::vector<bool>& evaluated)
        {
            if (evaluated.size() != generation.candidates.size())
                return false;
            const auto selected = Select(generation, probes);
            if (selected.decision != EscapeRouteDecisionKind::CandidateAvailable ||
                !evaluated[selected.selectedIndex])
                return false;
            for (std::size_t i = 0; i < evaluated.size(); ++i)
                if (!evaluated[i] &&
                    selected.selectedMinimumThreatDistance <=
                        generation.candidates[i].minimumThreatDistance + 0.001f)
                    return false;
            return true;
        }

        static const char* DecisionName(EscapeRouteDecisionKind decision)
        {
            switch (decision)
            {
                case EscapeRouteDecisionKind::CandidateAvailable:
                    return "candidate_available";
                case EscapeRouteDecisionKind::NoReachableCandidate:
                    return "no_reachable_candidate";
                case EscapeRouteDecisionKind::InsufficientData:
                    return "insufficient_data";
            }
            return "insufficient_data";
        }

        static bool Finite(const EscapeRoutePoint& point)
        {
            return std::isfinite(point.x) && std::isfinite(point.y) &&
                std::isfinite(point.z);
        }

        static float MinimumThreatDistance(
            const EscapeRoutePoint& point,
            const std::vector<EscapeRoutePoint>& threats)
        {
            float minimum = std::numeric_limits<float>::infinity();
            for (const auto& threat : threats)
                minimum = std::min(minimum, std::hypot(
                    point.x - threat.x, point.y - threat.y));
            return minimum;
        }

        static EscapeRouteGeneration Generate(
            const EscapeRoutePoint& origin,
            const std::vector<EscapeRoutePoint>& threats)
        {
            EscapeRouteGeneration result{};
            if (!Finite(origin) || threats.empty() ||
                threats.size() > MaximumThreats)
                return result;

            float awayX = 0.0f;
            float awayY = 0.0f;
            for (const auto& threat : threats)
            {
                if (!Finite(threat))
                    return result;
                const float dx = origin.x - threat.x;
                const float dy = origin.y - threat.y;
                const float distance = std::hypot(dx, dy);
                // An overlapping threat has no defensible away direction.
                if (!std::isfinite(distance) || distance <= 0.001f)
                    return result;
                awayX += dx / distance;
                awayY += dy / distance;
            }

            const float pressure = std::hypot(awayX, awayY);
            if (!std::isfinite(pressure) || pressure <= 0.001f)
                return result;
            awayX /= pressure;
            awayY /= pressure;
            result.threatsKnown = true;
            result.currentMinimumThreatDistance =
                MinimumThreatDistance(origin, threats);

            // Straight away and two lateral-away options. These are only
            // Detour probe destinations; the current Z is projected by the
            // ordinary planner, not assumed to be traversable terrain.
            const float lateralX = -awayY;
            const float lateralY = awayX;
            constexpr float diagonal = 0.70710678118f;
            const float directions[MaximumCandidates][2] = {
                {awayX, awayY},
                {(awayX + lateralX) * diagonal,
                 (awayY + lateralY) * diagonal},
                {(awayX - lateralX) * diagonal,
                 (awayY - lateralY) * diagonal}
            };
            for (const auto& direction : directions)
            {
                const EscapeRoutePoint point{
                    origin.x + direction[0] * CandidateDistance,
                    origin.y + direction[1] * CandidateDistance,
                    origin.z};
                const float separation = MinimumThreatDistance(point, threats);
                if (std::isfinite(separation) &&
                    separation >= result.currentMinimumThreatDistance +
                        MinimumSeparationGain)
                    result.candidates.push_back({point, separation});
            }
            return result;
        }

        static EscapeRouteSelection Select(
            const EscapeRouteGeneration& generation,
            const std::vector<EscapeRouteProbeEvidence>& probes)
        {
            EscapeRouteSelection result{};
            result.currentMinimumThreatDistance =
                generation.currentMinimumThreatDistance;
            if (!generation.threatsKnown ||
                probes.size() != generation.candidates.size())
                return result;

            result.decision = EscapeRouteDecisionKind::NoReachableCandidate;
            result.reason = generation.candidates.empty()
                ? "no_safer_local_candidate" : "no_safer_reachable_candidate";
            for (std::size_t index = 0; index < probes.size(); ++index)
            {
                const auto& probe = probes[index];
                if (!probe.reachable || !std::isfinite(probe.routeCost) ||
                    probe.routeCost <= 0.0f)
                    continue;
                ++result.reachableCount;
                if ((probe.deathRiskKnown &&
                     (!std::isfinite(probe.currentDeathRisk) ||
                      !std::isfinite(probe.destinationDeathRisk) ||
                      probe.destinationDeathRisk > probe.currentDeathRisk)) ||
                    (probe.navHazardRiskKnown &&
                     (!std::isfinite(probe.currentNavHazardRisk) ||
                      !std::isfinite(probe.destinationNavHazardRisk) ||
                      probe.destinationNavHazardRisk >
                          probe.currentNavHazardRisk)))
                    continue;

                const auto& candidate = generation.candidates[index];
                if (!std::isfinite(candidate.minimumThreatDistance) ||
                    candidate.minimumThreatDistance <
                        generation.currentMinimumThreatDistance +
                            MinimumSeparationGain)
                    continue;

                bool better = result.decision !=
                    EscapeRouteDecisionKind::CandidateAvailable;
                if (!better)
                {
                    const auto& best = probes[result.selectedIndex];
                    const float difference = candidate.minimumThreatDistance -
                        result.selectedMinimumThreatDistance;
                    if (difference > 0.001f)
                        better = true;
                    else if (std::fabs(difference) <= 0.001f)
                    {
                        if (probe.deathRiskKnown && best.deathRiskKnown &&
                            probe.destinationDeathRisk != best.destinationDeathRisk)
                            better = probe.destinationDeathRisk <
                                best.destinationDeathRisk;
                        else if (probe.navHazardRiskKnown &&
                                 best.navHazardRiskKnown &&
                                 probe.destinationNavHazardRisk !=
                                     best.destinationNavHazardRisk)
                            better = probe.destinationNavHazardRisk <
                                best.destinationNavHazardRisk;
                        else
                            better = probe.routeCost < best.routeCost;
                    }
                }
                if (better)
                {
                    result.decision = EscapeRouteDecisionKind::CandidateAvailable;
                    result.selectedIndex = index;
                    result.selectedMinimumThreatDistance =
                        candidate.minimumThreatDistance;
                    result.routeCost = probe.routeCost;
                    result.reason = "greater_minimum_threat_separation";
                }
            }
            return result;
        }
    };
}
