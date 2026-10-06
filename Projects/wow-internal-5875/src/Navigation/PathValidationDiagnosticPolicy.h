#pragma once

#include <cstddef>
#include <cstdint>

namespace Navigation
{
    enum class PathValidationSubreason
    {
        None,
        TooFewSteeringPoints,
        ImplausibleSegment,
        ImplausibleVerticalSegment,
        SafetyLengthExceeded,
        UnsafeTerrainNoAlternative,
        UnsafeTerrainAttemptLimit
    };

    struct PathValidationDetail
    {
        PathValidationSubreason reason=PathValidationSubreason::None;
        std::size_t pointIndex=0;
        std::uint64_t fromPoly=0, toPoly=0;
        float segmentLength=0, verticalDelta=0;
        std::uint16_t fromFlags=0, toFlags=0;
        int alternativeAttempt=0;
    };

    struct PathValidationDiagnosticPolicy
    {
        static constexpr const char* Name(PathValidationSubreason reason)
        {
            switch(reason)
            {
                case PathValidationSubreason::None: return "none";
                case PathValidationSubreason::TooFewSteeringPoints:
                    return "too_few_steering_points";
                case PathValidationSubreason::ImplausibleSegment:
                    return "implausible_segment";
                case PathValidationSubreason::ImplausibleVerticalSegment:
                    return "implausible_vertical_segment";
                case PathValidationSubreason::SafetyLengthExceeded:
                    return "safety_length_exceeded";
                case PathValidationSubreason::UnsafeTerrainNoAlternative:
                    return "unsafe_terrain_no_alternative";
                case PathValidationSubreason::UnsafeTerrainAttemptLimit:
                    return "unsafe_terrain_attempt_limit";
            }
            return "none";
        }

        static constexpr PathValidationDetail UnsafeTerrainNoAlternative(
            std::uint64_t from, std::uint64_t to, float horizontal,
            float vertical, std::uint16_t fromFlags,
            std::uint16_t toFlags, int attempt)
        {
            return {PathValidationSubreason::UnsafeTerrainNoAlternative,0,
                from,to,horizontal,vertical,fromFlags,toFlags,attempt};
        }

        // Diagnostics must never grant a path that terrain validation rejected.
        static constexpr bool ChangesSafetyDecision(const PathValidationDetail&)
        {
            return false;
        }
    };
}
