#pragma once

#include "PullSafetyPolicy.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace Bot
{
    enum class PullSafetyCandidateDecision
    {
        Eligible,
        Reject,
        Selected
    };

    struct PullSafetyCandidateDiagnostic
    {
        std::size_t index = std::numeric_limits<std::size_t>::max();
        PullSafetyAssessment assessment{};
        PullSafetyCandidateDecision decision =
            PullSafetyCandidateDecision::Reject;
    };

    // Read-only mirror of the candidate shortlist. Select remains the sole
    // owner of ranking and safety decisions; this only explains its result.
    struct PullSafetyDiagnosticPolicy
    {
        static std::vector<PullSafetyCandidateDiagnostic> Evaluate(
            std::span<const PullSafetyUnit> units, float healthPercent,
            const PullSafetySelection& selection)
        {
            if (selection.decision != PullSafetyDecision::Voluntary &&
                selection.decision != PullSafetyDecision::NoSafeCandidate)
                return {};

            std::vector<std::size_t> indices;
            indices.reserve(std::min(
                units.size(), PullSafetyPolicy::MaximumEvaluatedCandidates));
            for (std::size_t i = 0; i < units.size(); ++i)
                if (PullSafetyPolicy::Live(units[i]) && units[i].eligible)
                    indices.push_back(i);
            std::sort(indices.begin(), indices.end(),
                [&units](std::size_t a, std::size_t b)
                {
                    if (units[a].playerDistance != units[b].playerDistance)
                        return units[a].playerDistance < units[b].playerDistance;
                    return units[a].guid < units[b].guid;
                });
            if (indices.size() > PullSafetyPolicy::MaximumEvaluatedCandidates)
                indices.resize(PullSafetyPolicy::MaximumEvaluatedCandidates);

            std::vector<PullSafetyCandidateDiagnostic> result;
            result.reserve(indices.size());
            for (const auto index : indices)
            {
                const auto assessment = PullSafetyPolicy::Assess(
                    units, index, healthPercent, false);
                const auto decision = assessment.rejection !=
                        PullSafetyRejectReason::None
                    ? PullSafetyCandidateDecision::Reject
                    : index == selection.selectedIndex
                        ? PullSafetyCandidateDecision::Selected
                        : PullSafetyCandidateDecision::Eligible;
                result.push_back({index, assessment, decision});
            }
            return result;
        }

        static bool ShouldEmit(
            const PullSafetySelection& selection,
            std::span<const PullSafetyCandidateDiagnostic> candidates)
        {
            if (selection.candidateCount >= 2 && !candidates.empty())
                return true;
            for (const auto& candidate : candidates)
                if (candidate.decision == PullSafetyCandidateDecision::Reject &&
                    candidate.assessment.predictedAdds > 0)
                    return true;
            return false;
        }

        static const char* DecisionName(PullSafetyCandidateDecision decision)
        {
            switch (decision)
            {
                case PullSafetyCandidateDecision::Eligible: return "eligible";
                case PullSafetyCandidateDecision::Reject: return "reject";
                case PullSafetyCandidateDecision::Selected: return "selected";
            }
            return "unknown";
        }

        static const char* ReasonName(
            const PullSafetyCandidateDiagnostic& candidate)
        {
            if (candidate.decision == PullSafetyCandidateDecision::Selected)
                return "safest_candidate";
            if (candidate.decision == PullSafetyCandidateDecision::Eligible)
                return "eligible_not_selected";
            switch (candidate.assessment.rejection)
            {
                case PullSafetyRejectReason::LowHealth: return "low_health";
                case PullSafetyRejectReason::ExistingAggressor:
                    return "existing_aggressor";
                case PullSafetyRejectReason::TooManyPredictedAdds:
                    return "too_many_predicted_adds";
                case PullSafetyRejectReason::NotIsolated:
                    return "not_isolated";
                case PullSafetyRejectReason::InvalidCandidate:
                    return "invalid_candidate";
                case PullSafetyRejectReason::None: return "none";
            }
            return "unknown";
        }
    };
}
