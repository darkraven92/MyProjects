#pragma once

#include "TerrainTransitionPolicy.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Navigation
{
    struct TerrainPortalSegmentProvenance
    {
        std::size_t corridorFromIndex = 0;
        std::size_t corridorToIndex = 0;
        std::uint64_t fromPoly = 0;
        std::uint64_t toPoly = 0;
        bool adjacent = false;
        bool transitionKnown = false;
    };

    // Detour's ALL_CROSSINGS refs name the polygon entered at each emitted
    // point. The fork may omit a portal intersection or merge coincident
    // points, so consecutive straight points need not mean consecutive
    // corridor polygons. Preserve their ordered source indices at creation;
    // never infer a missing edge later by projecting the point coordinates.
    struct TerrainPortalProvenancePolicy
    {
        static bool ResolvePointIndices(
            const std::vector<std::uint64_t>& corridor,
            const std::vector<std::uint64_t>& pointRefs,
            std::vector<std::size_t>& indices)
        {
            indices.clear();
            if (corridor.empty() || pointRefs.size() < 2)
                return false;
            indices.reserve(pointRefs.size());
            std::size_t cursor = 0;
            for (std::size_t i = 0; i < pointRefs.size(); ++i)
            {
                const auto ref = TerrainTransitionPolicy::EnteredRef(
                    pointRefs[i], i + 1 == pointRefs.size(),
                    corridor.back());
                if (!ref)
                { indices.clear(); return false; }
                while (cursor < corridor.size() && corridor[cursor] != ref)
                    ++cursor;
                if (cursor == corridor.size())
                { indices.clear(); return false; }
                indices.push_back(cursor);
            }
            return true;
        }

        static TerrainPortalSegmentProvenance Segment(
            const std::vector<std::uint64_t>& corridor,
            const std::vector<std::size_t>& pointIndices,
            std::size_t pointIndex, bool detourLinkVerified)
        {
            TerrainPortalSegmentProvenance result{};
            if (pointIndex == 0 || pointIndex >= pointIndices.size())
                return result;
            // The first validation leg starts at the live player, not at
            // straightPath[0]. Detour may coalesce a coincident start/portal
            // point and update that point's ref to the entered polygon.
            result.corridorFromIndex = pointIndex == 1
                ? 0 : pointIndices[pointIndex - 1];
            result.corridorToIndex = pointIndices[pointIndex];
            if (result.corridorFromIndex >= corridor.size() ||
                result.corridorToIndex >= corridor.size())
                return result;
            result.adjacent = result.corridorToIndex ==
                result.corridorFromIndex + 1;
            result.transitionKnown = result.adjacent &&
                detourLinkVerified &&
                corridor[result.corridorFromIndex] != 0 &&
                corridor[result.corridorToIndex] != 0 &&
                corridor[result.corridorFromIndex] !=
                    corridor[result.corridorToIndex];
            if (result.transitionKnown)
            {
                result.fromPoly = corridor[result.corridorFromIndex];
                result.toPoly = corridor[result.corridorToIndex];
            }
            return result;
        }
    };
}
