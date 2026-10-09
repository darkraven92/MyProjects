#pragma once

#include "QuestPlannerTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace Bot
{
    struct ObjectiveAnchorSelection
    {
        bool valid = false;
        std::size_t index = 0;
        float directDistance = 0.0f;
        const char* reason = "no_valid_same_map_anchor";
    };

    struct ObjectiveAnchorSelectionPolicy
    {
        static constexpr std::size_t MaximumCandidates = 8;

        static bool SameAnchor(const ObjectiveDestination& a,
            const ObjectiveDestination& b)
        {
            return a.valid && b.valid && a.mapId == b.mapId &&
                std::isfinite(a.x) && std::isfinite(a.y) &&
                std::isfinite(a.z) && std::isfinite(b.x) &&
                std::isfinite(b.y) && std::isfinite(b.z) &&
                std::hypot(a.x-b.x,a.y-b.y,a.z-b.z) <= 0.5f;
        }

        // Only a destination already represented by the objective's own
        // source-backed spawn list may opt into failure fallback. An authored
        // destination override outside that list keeps its precedence.
        static ObjectiveAnchorSelection PrimarySearchIndex(
            const QuestProfile& profile)
        {
            ObjectiveAnchorSelection result{};
            result.reason = "primary_not_in_search_destinations";
            for (std::size_t i=0; i<profile.searchDestinations.size(); ++i)
                if (SameAnchor(profile.destination,
                        profile.searchDestinations[i]))
                {
                    result.valid = true;
                    result.index = i;
                    result.reason = "primary_source_anchor";
                    return result;
                }
            return result;
        }

        static ObjectiveAnchorSelection SelectAlternate(
            const QuestProfile& profile, float playerX, float playerY,
            float playerZ, const std::vector<std::size_t>& attempted)
        {
            ObjectiveAnchorSelection result{};
            result.reason = "no_distinct_same_map_alternate";
            const auto primary = PrimarySearchIndex(profile);
            if (!primary.valid || attempted.empty() ||
                std::find(attempted.begin(),attempted.end(),primary.index)==
                    attempted.end() ||
                attempted.size() >= MaximumCandidates ||
                !std::isfinite(playerX) || !std::isfinite(playerY) ||
                !std::isfinite(playerZ))
            {
                result.reason = "fallback_not_authorized_or_exhausted";
                return result;
            }

            float best=std::numeric_limits<float>::infinity();
            for (std::size_t i=0; i<profile.searchDestinations.size(); ++i)
            {
                const auto& candidate=profile.searchDestinations[i];
                if (!candidate.valid ||
                    candidate.mapId!=profile.destination.mapId ||
                    !std::isfinite(candidate.x) ||
                    !std::isfinite(candidate.y) ||
                    !std::isfinite(candidate.z) ||
                    std::find(attempted.begin(),attempted.end(),i)!=
                        attempted.end())
                    continue;
                const bool duplicate=std::any_of(attempted.begin(),
                    attempted.end(),[&](std::size_t prior)
                    {
                        return prior<profile.searchDestinations.size() &&
                            SameAnchor(candidate,
                                profile.searchDestinations[prior]);
                    });
                if (duplicate) continue;
                const float distance=std::hypot(candidate.x-playerX,
                    candidate.y-playerY,candidate.z-playerZ);
                if (!std::isfinite(distance) || distance>=best) continue;
                best=distance;
                result.valid=true;
                result.index=i;
                result.directDistance=distance;
                result.reason="nearest_untried_source_anchor";
            }
            return result;
        }

        static ObjectiveAnchorSelection Select(
            const QuestProfile& profile,
            float playerX, float playerY, float playerZ)
        {
            ObjectiveAnchorSelection result{};
            if (!std::isfinite(playerX) || !std::isfinite(playerY) ||
                !std::isfinite(playerZ) || !profile.destination.valid)
            {
                result.reason = "invalid_origin_or_profile_map";
                return result;
            }

            const auto& candidates = profile.searchDestinations;
            if (candidates.empty() || candidates.size() > MaximumCandidates)
            {
                result.reason = "candidate_count_out_of_bounds";
                return result;
            }

            float best = std::numeric_limits<float>::infinity();
            for (std::size_t i = 0; i < candidates.size(); ++i)
            {
                const auto& candidate = candidates[i];
                if (!candidate.valid ||
                    candidate.mapId != profile.destination.mapId ||
                    !std::isfinite(candidate.x) ||
                    !std::isfinite(candidate.y) ||
                    !std::isfinite(candidate.z))
                {
                    continue;
                }

                const float distance = std::hypot(
                    candidate.x - playerX,
                    candidate.y - playerY,
                    candidate.z - playerZ);
                if (!std::isfinite(distance) || distance >= best)
                    continue;

                result.valid = true;
                result.index = i;
                result.directDistance = distance;
                result.reason = "nearest_valid_same_map";
                best = distance;
            }
            return result;
        }
    };
}
