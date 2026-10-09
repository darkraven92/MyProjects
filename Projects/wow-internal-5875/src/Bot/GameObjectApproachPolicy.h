#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace Bot
{
    struct GameObjectApproachPoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // A GO origin is an identity/search hint, not necessarily a walkable
    // destination. Candidate points are inside the existing 5.5-yard use
    // radius even after the follower's one-yard arrival tolerance.
    struct GameObjectApproachPolicy
    {
        static constexpr std::size_t MaximumCandidates = 8;
        static constexpr float CandidateRadius = 3.5f;
        static constexpr float ArrivalDistance = 1.0f;
        static constexpr float UseDistance = 5.5f;
        static constexpr float MaximumProjectionHeightError = 1.5f;
        static constexpr float MaximumLiveRefinementDistance = 0.5f;
        static constexpr float Pi = 3.14159265358979323846f;

        static bool Finite(GameObjectApproachPoint point)
        {
            return std::isfinite(point.x) && std::isfinite(point.y) &&
                std::isfinite(point.z);
        }

        static float Distance2D(GameObjectApproachPoint a,
            GameObjectApproachPoint b)
        {
            return std::hypot(a.x - b.x, a.y - b.y);
        }

        static bool WithinUseRange(GameObjectApproachPoint player,
            GameObjectApproachPoint object)
        {
            return Finite(player) && Finite(object) &&
                std::hypot(player.x - object.x, player.y - object.y,
                    player.z - object.z) <= UseDistance;
        }

        static std::array<GameObjectApproachPoint, MaximumCandidates>
        Generate(GameObjectApproachPoint player,
            GameObjectApproachPoint object)
        {
            std::array<GameObjectApproachPoint, MaximumCandidates> result{};
            if (!Finite(player) || !Finite(object))
                return result;
            const float towardPlayer = std::atan2(
                player.y - object.y, player.x - object.x);
            constexpr std::array<int, MaximumCandidates> order{
                0, 1, -1, 2, -2, 3, -3, 4};
            for (std::size_t i = 0; i < result.size(); ++i)
            {
                const float angle = towardPlayer +
                    static_cast<float>(order[i]) * Pi / 4.0f;
                result[i] = {object.x + CandidateRadius * std::cos(angle),
                    object.y + CandidateRadius * std::sin(angle), object.z};
            }
            return result;
        }

        // A complete, terrain-validated Detour probe must also project onto
        // the object's height layer and remain within physical use range.
        static bool AcceptProjection(GameObjectApproachPoint object,
            GameObjectApproachPoint requested,
            GameObjectApproachPoint projected,
            bool completeRoute)
        {
            return completeRoute && Finite(object) && Finite(requested) &&
                Finite(projected) &&
                std::abs(projected.z - requested.z) <=
                    MaximumProjectionHeightError &&
                std::hypot(projected.x - object.x,
                    projected.y - object.y, projected.z - object.z) +
                    ArrivalDistance <= UseDistance;
        }

        static bool ShouldRefine(bool sourceIsStatic,
            bool refinementAlreadyUsed, GameObjectApproachPoint oldPosition,
            GameObjectApproachPoint livePosition)
        {
            return sourceIsStatic && !refinementAlreadyUsed &&
                Finite(oldPosition) && Finite(livePosition) &&
                std::hypot(oldPosition.x - livePosition.x,
                    oldPosition.y - livePosition.y,
                    oldPosition.z - livePosition.z) >
                    MaximumLiveRefinementDistance;
        }
    };
}
