#pragma once
#include "DeathRecoveryPolicy.h"
#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class CorpseAnchorSource
    {
        Unknown,
        ObservedBody,
        RecentBodyRecord,
        ServerCorpseLocation
    };

    struct DeathRecoveryEvidencePolicy
    {
        struct Point { float x, y, z; };
        enum class Wait { Pending, Ready, Expired };
        // Diagnostic publication bound, NOT a measured network delay. Uses
        // the existing six two-second release opportunities, not a new
        // extension of the 180/300-second liveness budgets.
        static constexpr std::uint64_t AnchorWaitMs =
            DeathRecoveryPolicy::MaximumReleaseAttemptsPerCycle *
            DeathRecoveryPolicy::ActionRetryTicks * 250;

        static Wait AnchorWait(std::uint64_t ageMs, bool known)
        {
            return known ? Wait::Ready : ageMs >= AnchorWaitMs
                ? Wait::Expired : Wait::Pending;
        }
        static bool ServerAnchorEligible(bool worldValid, bool ghost,
            std::uint64_t episodeGuid, std::uint64_t liveGuid,
            std::uint32_t currentMap, std::int32_t displayMap,
            std::int32_t corpseMap, Point point)
        {
            return worldValid && ghost && episodeGuid != 0 &&
                liveGuid == episodeGuid && displayMap >= 0 &&
                static_cast<std::uint32_t>(displayMap) == currentMap &&
                corpseMap == displayMap && std::isfinite(point.x) &&
                std::isfinite(point.y) && std::isfinite(point.z);
        }
        static bool MayRestartForStall(bool initializationPending)
        {
            return !initializationPending;
        }
        static bool CanReclaim(bool currentServerAnchor, bool freshProbe,
            bool ghost, float delay, float distance)
        {
            return currentServerAnchor && freshProbe && std::isfinite(delay) && delay >= 0 &&
                DeathRecoveryPolicy::CanRetrieveCorpse(
                    true, ghost, delay, distance);
        }
        static const char* SourceName(CorpseAnchorSource source)
        {
            switch (source)
            {
                case CorpseAnchorSource::ObservedBody: return "observed_body_routing_only";
                case CorpseAnchorSource::RecentBodyRecord: return "recent_body_record_routing_only";
                case CorpseAnchorSource::ServerCorpseLocation: return "server_corpse_location";
                case CorpseAnchorSource::Unknown: return "unknown";
            }
            return "unknown";
        }
    };
}
