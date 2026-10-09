#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Bot
{
    enum class ServiceKnowledge : std::uint8_t
    {
        Unknown,
        Available,
        Unavailable
    };

    enum class ServiceHubSource : std::uint8_t
    {
        UnverifiedVisible,
        Seeded,
        Persisted,
        MerchantFrameVerified,
        SourceBacked
    };

    struct ServiceSelectionOrigin
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct ServiceHubCandidate
    {
        std::uint32_t entry = 0;
        ServiceHubSource source = ServiceHubSource::UnverifiedVisible;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        // Entries known only by NPC ID are eligible only once a live snapshot
        // supplies an actual position; never route to an invented (0,0,0).
        bool positionKnown = false;
        ServiceKnowledge sell = ServiceKnowledge::Unknown;
        ServiceKnowledge repair = ServiceKnowledge::Unknown;
        bool live = false;
        std::uint64_t guid = 0;
        float euclideanDistance = 0.0f;
        float plannedPathLength = 0.0f;
        float navigationCost = 0.0f;
        bool reachable = false;
        bool directFallbackOnly = false;
        bool evaluated = false;
        bool failed = false;
        std::string probeFailureReason = "none";
    };

    class ServiceHubSelectionPolicy
    {
    public:
        static constexpr bool MayTryAnotherCandidate(
            std::size_t failedCount, std::size_t maximumFailures)
        {
            return failedCount < maximumFailures;
        }

        static float DistanceFrom(const ServiceSelectionOrigin& origin,
                                  const ServiceHubCandidate& candidate)
        {
            const float dx = candidate.x - origin.x;
            const float dy = candidate.y - origin.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        static bool Suitable(const ServiceHubCandidate& candidate,
                             bool needSell, bool needRepair)
        {
            return candidate.entry != 0 &&
                candidate.positionKnown &&
                candidate.source != ServiceHubSource::UnverifiedVisible &&
                (!needSell || candidate.sell != ServiceKnowledge::Unavailable) &&
                (!needRepair || candidate.repair != ServiceKnowledge::Unavailable);
        }

        static constexpr const char* SourceName(ServiceHubSource source)
        {
            switch (source)
            {
                case ServiceHubSource::SourceBacked: return "source_backed_unverified";
                case ServiceHubSource::Seeded: return "seeded";
                case ServiceHubSource::Persisted: return "persisted";
                case ServiceHubSource::MerchantFrameVerified: return "merchant_frame_verified";
                default: return "unverified_visible";
            }
        }

        static std::vector<std::size_t> Shortlist(
            const std::vector<ServiceHubCandidate>& candidates,
            bool needSell, bool needRepair, std::size_t limit)
        {
            std::vector<std::size_t> result;
            for (std::size_t i = 0; i < candidates.size(); ++i)
            {
                const auto& candidate = candidates[i];
                if (Suitable(candidate, needSell, needRepair) &&
                    std::isfinite(candidate.euclideanDistance) &&
                    candidate.euclideanDistance >= 0.0f && !candidate.failed)
                    result.push_back(i);
            }
            std::sort(result.begin(), result.end(), [&](std::size_t a, std::size_t b) {
                const auto& left = candidates[a];
                const auto& right = candidates[b];
                if (left.euclideanDistance != right.euclideanDistance)
                    return left.euclideanDistance < right.euclideanDistance;
                if (left.entry != right.entry)
                    return left.entry < right.entry;
                return left.guid < right.guid;
            });
            if (result.size() > limit)
                result.resize(limit);
            return result;
        }

        static std::size_t BestReachable(
            const std::vector<ServiceHubCandidate>& candidates,
            const std::vector<std::size_t>& shortlist)
        {
            std::size_t best = candidates.size();
            for (const std::size_t index : shortlist)
            {
                if (index >= candidates.size())
                    continue;
                const auto& candidate = candidates[index];
                if (!candidate.evaluated || !candidate.reachable ||
                    candidate.failed || !std::isfinite(candidate.navigationCost))
                    continue;
                if (best == candidates.size())
                {
                    best = index;
                    continue;
                }
                const auto& current = candidates[best];
                if ((current.directFallbackOnly && !candidate.directFallbackOnly) ||
                    (candidate.directFallbackOnly == current.directFallbackOnly &&
                     (candidate.navigationCost < current.navigationCost ||
                      (candidate.navigationCost == current.navigationCost &&
                       (candidate.euclideanDistance < current.euclideanDistance ||
                        (candidate.euclideanDistance == current.euclideanDistance &&
                         candidate.entry < current.entry))))))
                    best = index;
            }
            return best;
        }
    };
}
