#pragma once

#include <cmath>
#include <cstdint>

namespace Navigation
{
    enum class CorridorReplanDecision
    {
        Allow,
        AllowAfterProgress,
        SuppressRepeated,
        Escalate
    };

    enum class CorridorReplanReason
    {
        NoFailure,
        PhysicalProgress,
        ExactFingerprint,
        LocalTransitionMatch,
        TransitionUnknown,
        DifferentLocalTransition
    };

    struct CorridorFailureRecord
    {
        std::uint64_t fingerprint = 0;
        float anchorX = 0.0f;
        float anchorY = 0.0f;
        std::uint64_t failedFromPoly = 0;
        std::uint64_t failedToPoly = 0;
        bool transitionKnown = false;
        bool active = false;
    };

    struct CorridorReplanAssessment
    {
        CorridorReplanDecision decision = CorridorReplanDecision::Allow;
        CorridorReplanReason reason = CorridorReplanReason::NoFailure;
        float anchorDistance = 0.0f;
        bool localTransitionMatch = false;
    };

    class CorridorReplanHysteresisPolicy
    {
    public:
        static void RecordFailure(
            CorridorFailureRecord& record,
            std::uint64_t fingerprint,
            float x,
            float y,
            float meaningfulProgressDistance,
            bool transitionKnown = false,
            std::uint64_t fromPoly = 0,
            std::uint64_t toPoly = 0)
        {
            if (fingerprint == 0)
                return;

            transitionKnown = transitionKnown && fromPoly != 0 &&
                toPoly != 0 && fromPoly != toPoly;
            const float moved = std::hypot(x - record.anchorX, y - record.anchorY);
            const bool sameFailure = record.active &&
                (record.fingerprint == fingerprint ||
                 (record.transitionKnown && transitionKnown &&
                  record.failedFromPoly == fromPoly &&
                  record.failedToPoly == toPoly));
            if (sameFailure && std::isfinite(moved) &&
                moved < meaningfulProgressDistance)
            {
                // A second failure at the same passage cannot move the anchor.
                // It may, however, resolve a previously unknown transition.
                if (!record.transitionKnown && transitionKnown)
                {
                    record.failedFromPoly = fromPoly;
                    record.failedToPoly = toPoly;
                    record.transitionKnown = true;
                }
                return;
            }
            record = {fingerprint, x, y,
                transitionKnown ? fromPoly : 0,
                transitionKnown ? toPoly : 0,
                transitionKnown, true};
        }

        static CorridorReplanAssessment Assess(
            const CorridorFailureRecord& record,
            std::uint64_t candidateFingerprint,
            float x,
            float y,
            float meaningfulProgressDistance,
            bool boundedRecoveryAvailable,
            bool candidateTransitionKnown = false,
            std::uint64_t candidateFromPoly = 0,
            std::uint64_t candidateToPoly = 0)
        {
            if (!record.active)
                return {};

            const float moved = std::hypot(x - record.anchorX, y - record.anchorY);
            if (std::isfinite(moved) && moved >= meaningfulProgressDistance)
                return {CorridorReplanDecision::AllowAfterProgress,
                    CorridorReplanReason::PhysicalProgress, moved, false};

            CorridorReplanReason reason = CorridorReplanReason::ExactFingerprint;
            bool localMatch = false;
            if (candidateFingerprint == 0 ||
                candidateFingerprint != record.fingerprint)
            {
                if (!record.transitionKnown || !candidateTransitionKnown ||
                    candidateFromPoly == 0 || candidateToPoly == 0 ||
                    candidateFromPoly == candidateToPoly)
                    return {CorridorReplanDecision::Allow,
                        CorridorReplanReason::TransitionUnknown, moved, false};

                localMatch = record.failedFromPoly == candidateFromPoly &&
                    record.failedToPoly == candidateToPoly;
                if (!localMatch)
                    return {CorridorReplanDecision::Allow,
                        CorridorReplanReason::DifferentLocalTransition,
                        moved, false};
                reason = CorridorReplanReason::LocalTransitionMatch;
            }
            return {boundedRecoveryAvailable
                    ? CorridorReplanDecision::SuppressRepeated
                    : CorridorReplanDecision::Escalate,
                reason, moved, localMatch};
        }

        static const char* ReasonName(CorridorReplanReason reason)
        {
            switch (reason)
            {
                case CorridorReplanReason::NoFailure: return "no_failure";
                case CorridorReplanReason::PhysicalProgress: return "physical_progress";
                case CorridorReplanReason::ExactFingerprint: return "exact_fingerprint";
                case CorridorReplanReason::LocalTransitionMatch:
                    return "local_transition_match";
                case CorridorReplanReason::TransitionUnknown:
                    return "transition_unknown";
                case CorridorReplanReason::DifferentLocalTransition:
                    return "different_local_transition";
            }
            return "unknown";
        }
    };
}
