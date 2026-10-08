#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Navigation
{
    struct PartialStagingEpisodeEvidence
    {
        bool sameIntent = false;
        bool freshRoute = false;
        bool oldRouteValidated = false;
        bool newRouteValidated = false;
        bool newCorridorConnected = false;
        bool playerProjectionMatchesStart = false;
        bool waterExcluded = false;
        std::uint64_t oldGeneration = 0;
        std::uint64_t newGeneration = 0;
        std::uint64_t oldStartPoly = 0;
        std::uint64_t newStartPoly = 0;
        float physicalProgress = 0.0f;
        float destinationGain = 0.0f;
    };

    struct PartialStagingEpisodePolicy
    {
        static constexpr int MaximumAttempts = 3;
        static constexpr float MinimumForwardGain = 4.0f;

        static bool VerifiedForwardProgress(
            const PartialStagingEpisodeEvidence& evidence,
            const std::vector<std::uint64_t>& previousCorridor)
        {
            if (!evidence.sameIntent || !evidence.freshRoute ||
                !evidence.oldRouteValidated || !evidence.newRouteValidated ||
                !evidence.newCorridorConnected ||
                !evidence.playerProjectionMatchesStart ||
                !evidence.waterExcluded || evidence.oldGeneration == 0 ||
                evidence.oldGeneration != evidence.newGeneration ||
                evidence.oldStartPoly == 0 || evidence.newStartPoly == 0 ||
                evidence.oldStartPoly == evidence.newStartPoly ||
                evidence.physicalProgress < MinimumForwardGain ||
                evidence.destinationGain < MinimumForwardGain ||
                previousCorridor.empty() ||
                previousCorridor.front() != evidence.oldStartPoly)
                return false;

            // Only a later polygon on the previously validated directed
            // corridor can start another three-attempt staging episode.
            const auto next = std::find(previousCorridor.begin() + 1,
                previousCorridor.end(), evidence.newStartPoly);
            return next != previousCorridor.end();
        }
    };
}
