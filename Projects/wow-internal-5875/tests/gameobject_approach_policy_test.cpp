#include "../src/Bot/GameObjectApproachPolicy.h"
#include "../src/Navigation/RouteCostProbePolicy.h"

#include <cassert>
#include <cmath>

int main()
{
    using Bot::GameObjectApproachPoint;
    using Bot::GameObjectApproachPolicy;
    constexpr GameObjectApproachPoint player{10.0f, 0.0f, 0.0f};
    constexpr GameObjectApproachPoint object{0.0f, 0.0f, 0.0f};
    const auto candidates = GameObjectApproachPolicy::Generate(player, object);
    const auto repeated = GameObjectApproachPolicy::Generate(player, object);
    static_assert(GameObjectApproachPolicy::MaximumCandidates == 8);
    static_assert(GameObjectApproachPolicy::CandidateRadius +
        GameObjectApproachPolicy::ArrivalDistance <
        GameObjectApproachPolicy::UseDistance);
    assert(std::abs(candidates[0].x - 3.5f) < 0.001f);
    assert(std::abs(candidates[0].y) < 0.001f);
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        assert(candidates[i].x == repeated[i].x);
        assert(candidates[i].y == repeated[i].y);
        assert(std::abs(GameObjectApproachPolicy::Distance2D(
            candidates[i], object) - 3.5f) < 0.001f);
        assert(GameObjectApproachPolicy::AcceptProjection(object,
            candidates[i], candidates[i], true));
    }

    // A valid Detour projection alone is not sufficient: the full route
    // must complete, remain on the expected height layer, and end in range.
    assert(!GameObjectApproachPolicy::AcceptProjection(object,
        candidates[0], candidates[0], false));
    assert(!GameObjectApproachPolicy::AcceptProjection(object,
        candidates[0], {3.5f, 0.0f, 3.0f}, true));
    assert(!GameObjectApproachPolicy::AcceptProjection(object,
        candidates[0], {8.0f, 0.0f, 0.0f}, true));
    assert(!GameObjectApproachPolicy::WithinUseRange(
        {6.0f, 0.0f, 0.0f}, object));
    assert(GameObjectApproachPolicy::WithinUseRange(
        {5.0f, 0.0f, 0.0f}, object));

    assert(GameObjectApproachPolicy::ShouldRefine(
        true, false, object, {1.0f, 0.0f, 0.0f}));
    assert(!GameObjectApproachPolicy::ShouldRefine(
        true, true, object, {1.0f, 0.0f, 0.0f}));
    assert(!GameObjectApproachPolicy::ShouldRefine(
        false, false, object, {1.0f, 0.0f, 0.0f}));
    assert(!Navigation::RouteCostProbePolicy::MayIssueMovement(true));
    assert(Navigation::RouteCostProbePolicy::MayIssueMovement(false));
}
