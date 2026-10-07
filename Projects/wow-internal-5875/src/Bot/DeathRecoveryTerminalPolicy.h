#pragma once

#include "DeathRecoveryPolicy.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace Bot
{
    // A strategic continuation is not an extension of the ordinary route
    // budget. It is permitted once, only with evidence that its origin or
    // Detour query will differ from the failed episode.
    struct DeathRecoveryTerminalPolicy
    {
        struct Point
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        struct Signature
        {
            std::uint32_t mapId = 0;
            int corpseX = 0;
            int corpseY = 0;
            int corpseZ = 0;
            int ghostX = 0;
            int ghostY = 0;
            int ghostZ = 0;
            int reason = 0;
            int routeVariant = 0;
            std::uint64_t failedFrom = 0;
            std::uint64_t failedTo = 0;
            std::uint64_t corridorFingerprint = 0;

            bool operator==(const Signature&) const = default;
        };

        // A fresh origin must differ by at least one existing broad reclaim
        // radius; sub-yard steering motion is not a new route episode.
        static constexpr float MinimumCorpseDistanceGain =
            DeathRecoveryPolicy::ReclaimDistance;
        static constexpr float MinimumOriginDisplacement =
            DeathRecoveryPolicy::ReclaimDistance;
        static constexpr float AlternateApproachRadius = 4.0f;

        static float Distance(Point a, Point b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        static int Bucket(float value, float width)
        {
            return static_cast<int>(std::floor(value / width));
        }

        static Signature MakeSignature(
            std::uint32_t mapId, Point corpse, Point ghost, int reason,
            int routeVariant, std::uint64_t failedFrom,
            std::uint64_t failedTo, std::uint64_t fingerprint)
        {
            return {mapId,
                Bucket(corpse.x, 8.0f), Bucket(corpse.y, 8.0f),
                Bucket(corpse.z, 8.0f),
                Bucket(ghost.x, 16.0f), Bucket(ghost.y, 16.0f),
                Bucket(ghost.z, 16.0f), reason, routeVariant,
                failedFrom, failedTo, fingerprint};
        }

        static std::uint64_t Fingerprint(const Signature& s)
        {
            std::uint64_t hash = 1469598103934665603ULL;
            const std::uint64_t parts[] = {
                s.mapId, static_cast<std::uint32_t>(s.corpseX),
                static_cast<std::uint32_t>(s.corpseY),
                static_cast<std::uint32_t>(s.corpseZ),
                static_cast<std::uint32_t>(s.ghostX),
                static_cast<std::uint32_t>(s.ghostY),
                static_cast<std::uint32_t>(s.ghostZ),
                static_cast<std::uint32_t>(s.reason),
                static_cast<std::uint32_t>(s.routeVariant),
                s.failedFrom, s.failedTo, s.corridorFingerprint};
            for (const auto part : parts)
            {
                hash ^= part;
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        static std::string SignatureLabel(bool known, const Signature& s)
        {
            return known ? std::to_string(Fingerprint(s)) : "none";
        }

        static bool MayEscalate(
            bool ghostConfirmed, bool routeRelated, bool alreadyUsed,
            bool identicalSignature, bool haveInitialGhost,
            Point initialGhost, Point currentGhost, Point corpse,
            bool provenBadDirectedTransition)
        {
            if (!ghostConfirmed || !routeRelated || alreadyUsed ||
                identicalSignature)
                return false;
            const bool materialProgress = haveInitialGhost &&
                Distance(initialGhost, currentGhost) >=
                    MinimumOriginDisplacement &&
                Distance(initialGhost, corpse) -
                    Distance(currentGhost, corpse) >=
                    MinimumCorpseDistanceGain;
            return materialProgress || provenBadDirectedTransition;
        }

        // The endpoint is four yards toward the approaching ghost, well
        // inside the existing eight-yard precision reclaim gate. Detour must
        // still project and validate the entire route before any CTM.
        static Point AlternateApproach(Point corpse, Point ghost)
        {
            const float dx = ghost.x - corpse.x;
            const float dy = ghost.y - corpse.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length < 0.001f)
                return {corpse.x + AlternateApproachRadius, corpse.y,
                    corpse.z};
            return {corpse.x + dx / length * AlternateApproachRadius,
                corpse.y + dy / length * AlternateApproachRadius,
                corpse.z};
        }
    };
}
