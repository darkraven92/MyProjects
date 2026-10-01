#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Navigation
{
    // Pure diagnostic/decision helpers. The follower retains the existing
    // segment checks, accumulation order, and numeric safety threshold.
    struct LongPathDiagnosticPolicy
    {
        struct Measurement
        {
            bool known = false;
            double straightPathLength = 0.0;
        };

        static constexpr bool ExceedsSafetyLength(float length, float limit)
        {
            return length > limit;
        }

        static constexpr bool UsesPartialStaging(bool partial)
        {
            return partial;
        }

        static constexpr bool CapacityLimitedPartialStage(
            bool partial, bool outOfNodes, float reachableAdvance,
            float destinationProgress, float minimumAdvance,
            float minimumDestinationProgress)
        {
            return partial && outOfNodes &&
                reachableAdvance >= minimumAdvance &&
                destinationProgress >= minimumDestinationProgress;
        }

        static constexpr bool PolygonBufferFilled(int count, int maximum)
        {
            return maximum > 0 && count >= maximum;
        }

        template <typename Points>
        static Measurement MeasureStraightPath(const Points& points)
        {
            if (points.size() < 2)
                return {};

            double total = 0.0;
            for (std::size_t i = 1; i < points.size(); ++i)
            {
                const auto& a = points[i - 1];
                const auto& b = points[i];
                const double dx = static_cast<double>(b.x) - a.x;
                const double dy = static_cast<double>(b.y) - a.y;
                const double dz = static_cast<double>(b.z) - a.z;
                const double segment = std::hypot(dx, dy, dz);
                if (!std::isfinite(segment) || !std::isfinite(total + segment))
                    return {};
                total += segment;
            }
            return {true, total};
        }

        static constexpr bool ReachedDestination(
            std::uint64_t lastPoly, std::uint64_t endPoly)
        {
            return lastPoly != 0 && endPoly != 0 && lastPoly == endPoly;
        }
    };
}
