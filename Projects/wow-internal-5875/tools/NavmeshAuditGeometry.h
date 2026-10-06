#pragma once

#include <cmath>
#include <optional>
#include <utility>

namespace NavmeshAudit
{
    // Detour external links cover only bmin..bmax of the owning edge.
    // Diagnostic geometry must use the same clipping as getPortalPoints.
    template <typename Point>
    std::optional<std::pair<Point, Point>> ClipPortal(
        Point a, Point b, unsigned side, unsigned minimum, unsigned maximum)
    {
        if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z) ||
            !std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z))
            return std::nullopt;
        if (side == 0xff)
            return std::pair{a, b};
        if (side > 7 || minimum > maximum || maximum > 255)
            return std::nullopt;
        const auto interpolate = [&](unsigned value)
        {
            const float t = static_cast<float>(value) / 255.0f;
            return Point{a.x + (b.x - a.x) * t,
                         a.y + (b.y - a.y) * t,
                         a.z + (b.z - a.z) * t};
        };
        return std::pair{interpolate(minimum), interpolate(maximum)};
    }
}
