#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace Navigation
{
    // Evidence is supplied by the caller; this policy knows nothing about
    // Detour, WoW coordinates, or ClickToMove.
    struct SteeringCandidateEvidence
    {
        std::size_t index = 0;
        bool distanceAndRiseValid = false;
        bool raycastValid = false;
        float raycastFraction = 0.0f;
        bool clearanceKnown = false;
        float candidateClearance = 0.0f;
        bool adjusted = false;
        bool adjustedClearanceKnown = false;
        float adjustedClearance = 0.0f;
        bool adjustedRaycastValid = false;
        float adjustedRaycastFraction = 0.0f;
    };

    enum class SteeringCandidateDecision
    {
        Accepted,
        RejectedGeometry,
        RejectedRaycast,
        RejectedClearance
    };

    struct SteeringSelectionPolicy
    {
        static constexpr float MinimumWallClearance = 0.70f;
        static constexpr float MinimumRaycastFraction = 0.985f;

        static bool MeetsMinimumClearance(float clearance)
        {
            return std::isfinite(clearance) &&
                clearance >= MinimumWallClearance;
        }

        static SteeringCandidateDecision Assess(
            const SteeringCandidateEvidence& candidate)
        {
            if (!candidate.distanceAndRiseValid)
                return SteeringCandidateDecision::RejectedGeometry;
            if (!candidate.raycastValid ||
                !std::isfinite(candidate.raycastFraction) ||
                candidate.raycastFraction < MinimumRaycastFraction)
            {
                return SteeringCandidateDecision::RejectedRaycast;
            }
            if (!candidate.clearanceKnown ||
                !std::isfinite(candidate.candidateClearance))
            {
                return SteeringCandidateDecision::RejectedClearance;
            }

            if (candidate.adjusted)
            {
                if (!candidate.adjustedClearanceKnown ||
                    !MeetsMinimumClearance(candidate.adjustedClearance))
                {
                    return SteeringCandidateDecision::RejectedClearance;
                }
                if (!candidate.adjustedRaycastValid ||
                    !std::isfinite(candidate.adjustedRaycastFraction) ||
                    candidate.adjustedRaycastFraction < MinimumRaycastFraction)
                {
                    return SteeringCandidateDecision::RejectedRaycast;
                }
            }
            else if (!MeetsMinimumClearance(candidate.candidateClearance))
            {
                return SteeringCandidateDecision::RejectedClearance;
            }

            return SteeringCandidateDecision::Accepted;
        }

        // Input is ordered farthest to nearest. The first safe point wins.
        static std::optional<std::size_t> SelectFarthestSafe(
            const std::vector<SteeringCandidateEvidence>& candidates)
        {
            for (const auto& candidate : candidates)
            {
                if (Assess(candidate) == SteeringCandidateDecision::Accepted)
                    return candidate.index;
            }
            return std::nullopt;
        }

        static std::size_t CommitIndex(
            std::size_t fromIndex,
            std::size_t candidateIndex,
            bool accepted,
            bool ctmSucceeded)
        {
            return accepted && ctmSucceeded ? candidateIndex : fromIndex;
        }
    };
}
