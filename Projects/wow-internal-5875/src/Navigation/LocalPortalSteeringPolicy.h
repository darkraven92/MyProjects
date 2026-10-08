#pragma once

#include "EpisodeBadTransitionPolicy.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Navigation
{
    struct LocalPortalPoint { float x = 0, y = 0, z = 0; };
    struct LocalPortalStageDecision { bool allowed = false; };
    enum class LocalPortalRayClass
    {
        IntendedPortal,
        CorridorBoundary,
        BlockedGeometry,
        UnknownProvenance,
        InsufficientClearance
    };

    // A straight-path point identifies the polygon ENTERED at that point,
    // not corridor[pointIndex]. The local directed pair must be found from
    // actual raycast/corridor refs and a hit at that pair's portal.
    struct LocalPortalSteeringPolicy
    {
        static const char* RayClassName(LocalPortalRayClass value)
        {
            switch (value)
            {
                case LocalPortalRayClass::IntendedPortal: return "intended_portal";
                case LocalPortalRayClass::CorridorBoundary: return "corridor_boundary";
                case LocalPortalRayClass::BlockedGeometry: return "blocked_geometry";
                case LocalPortalRayClass::UnknownProvenance: return "unknown";
                case LocalPortalRayClass::InsufficientClearance: return "insufficient_clearance";
            }
            return "unknown";
        }

        // Detour raycast reports a wall hit when no eligible link crosses the
        // exit edge. A hit near a linked portal is NOT proof of passage.
        static LocalPortalRayClass ClassifyRay(bool portalKnown,
            std::uint64_t fromPoly, std::uint64_t toPoly,
            std::uint64_t lastVisitedPoly, float fraction,
            float hitToClippedPortal, float maximumPortalDistance,
            bool clearanceRejected)
        {
            if (!portalKnown || !fromPoly || !toPoly || fromPoly == toPoly ||
                !std::isfinite(fraction))
                return LocalPortalRayClass::UnknownProvenance;
            if (fraction >= 0.985f)
                return clearanceRejected
                    ? LocalPortalRayClass::InsufficientClearance
                    : lastVisitedPoly == toPoly
                        ? LocalPortalRayClass::IntendedPortal
                        : LocalPortalRayClass::UnknownProvenance;
            if (lastVisitedPoly == fromPoly &&
                std::isfinite(hitToClippedPortal) &&
                hitToClippedPortal <= maximumPortalDistance)
                return LocalPortalRayClass::CorridorBoundary;
            return LocalPortalRayClass::BlockedGeometry;
        }

        static LocalPortalPoint InteriorStage(LocalPortalPoint portalA,
            LocalPortalPoint portalB, LocalPortalPoint interior, float fraction)
        {
            const LocalPortalPoint midpoint{(portalA.x+portalB.x)*0.5f,
                (portalA.y+portalB.y)*0.5f,(portalA.z+portalB.z)*0.5f};
            return {midpoint.x+(interior.x-midpoint.x)*fraction,
                midpoint.y+(interior.y-midpoint.y)*fraction,
                midpoint.z+(interior.z-midpoint.z)*fraction};
        }

        static DirectedPolyTransition CandidateEdge(
            const std::vector<std::uint64_t>& corridor,
            std::uint64_t playerPoly, std::uint64_t candidateEnteredPoly)
        {
            if (!playerPoly || !candidateEnteredPoly) return {};
            const auto player=std::find(corridor.begin(),corridor.end(),playerPoly);
            const auto candidate=std::find(corridor.begin(),corridor.end(),
                candidateEnteredPoly);
            if (player==corridor.end() || candidate==corridor.end() ||
                player+1>=corridor.end() || player>=candidate ||
                std::find(player+1,corridor.end(),playerPoly)!=corridor.end() ||
                std::find(candidate+1,corridor.end(),candidateEnteredPoly)!=
                    corridor.end()) return {};
            const DirectedPolyTransition edge{*player,*(player+1)};
            return edge.Valid()?edge:DirectedPolyTransition{};
        }

        static float SegmentDistance2D(LocalPortalPoint point,
            LocalPortalPoint a, LocalPortalPoint b)
        {
            const float dx = b.x-a.x, dy = b.y-a.y;
            const float length2 = dx*dx+dy*dy;
            if (!std::isfinite(length2) || length2 < 0.0001f)
                return INFINITY;
            const float t = std::clamp(((point.x-a.x)*dx+(point.y-a.y)*dy)/length2,
                0.0f, 1.0f);
            return std::hypot(point.x-(a.x+t*dx),point.y-(a.y+t*dy));
        }

        static DirectedPolyTransition AttributeRayFailure(
            const std::vector<std::uint64_t>& corridor,
            std::uint64_t playerPoly, std::uint64_t candidateEnteredPoly,
            std::uint64_t rayLastPoly, LocalPortalPoint hit,
            LocalPortalPoint portalA, LocalPortalPoint portalB,
            float maximumPortalDistance)
        {
            if (!playerPoly || !candidateEnteredPoly || !rayLastPoly ||
                !std::isfinite(maximumPortalDistance) || maximumPortalDistance < 0 ||
                !std::isfinite(hit.x) || !std::isfinite(hit.y)) return {};
            const auto uniqueIndex = [&](std::uint64_t ref)
            {
                const auto first = std::find(corridor.begin(),corridor.end(),ref);
                if (first == corridor.end() ||
                    std::find(first+1,corridor.end(),ref) != corridor.end())
                    return corridor.size();
                return static_cast<std::size_t>(first-corridor.begin());
            };
            const auto playerIndex=uniqueIndex(playerPoly);
            const auto hitIndex=uniqueIndex(rayLastPoly);
            const auto candidateIndex=uniqueIndex(candidateEnteredPoly);
            if (playerIndex>=corridor.size() || hitIndex>=corridor.size() ||
                candidateIndex>=corridor.size() || playerIndex>hitIndex ||
                hitIndex+1>candidateIndex ||
                SegmentDistance2D(hit,portalA,portalB)>maximumPortalDistance)
                return {};
            const DirectedPolyTransition pair{corridor[hitIndex],
                corridor[hitIndex+1]};
            return pair.Valid() ? pair : DirectedPolyTransition{};
        }

        static LocalPortalStageDecision AssessStage(LocalPortalPoint player,
            LocalPortalPoint portalA, LocalPortalPoint portalB,
            LocalPortalPoint stage, float clearance, bool raycastKnown,
            float raycastFraction, float minimumClearance,
            float minimumTravelDistance)
        {
            if (!std::isfinite(player.x) || !std::isfinite(player.y) ||
                !std::isfinite(stage.x) || !std::isfinite(stage.y) ||
                !std::isfinite(clearance) ||
                !std::isfinite(raycastFraction) ||
                !std::isfinite(minimumClearance) ||
                !std::isfinite(minimumTravelDistance) ||
                minimumClearance<=0 || minimumTravelDistance<=0)
                return {};
            return {clearance>=minimumClearance && raycastKnown &&
                raycastFraction>=0.985f &&
                std::hypot(stage.x-player.x,stage.y-player.y)>=minimumTravelDistance &&
                SegmentDistance2D(stage,portalA,portalB)<=minimumClearance};
        }
    };
}
