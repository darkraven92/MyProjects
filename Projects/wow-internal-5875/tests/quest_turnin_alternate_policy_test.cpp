#include "../src/Bot/QuestTurnInAlternatePolicy.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    struct Point
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };
}

int main()
{
    using Bot::QuestTurnInAlternatePolicy;
    using Navigation::NavigationPlanFailure;
    const Point origin{100.0f, 200.0f, 10.0f};
    const Point npc{500.0f, 200.0f, 10.0f};
    assert(QuestTurnInAlternatePolicy::CanGenerate(origin, npc));
    assert(!QuestTurnInAlternatePolicy::CanGenerate(origin, origin));
    const auto candidates = QuestTurnInAlternatePolicy::Generate(origin, npc);
    static_assert(candidates.size() ==
        QuestTurnInAlternatePolicy::MaximumCandidates);
    assert(candidates[0].y > origin.y);
    assert(candidates[1].y < origin.y);
    for (const Point& candidate : candidates)
        assert(std::fabs(QuestTurnInAlternatePolicy::Distance2D(
            origin, candidate) -
            QuestTurnInAlternatePolicy::EgressDistance) < 0.01f);

    // Normal navigation has no reason to start strategic recovery. Once a
    // follower really fails, at most two distinct strategic legs can run.
    assert(QuestTurnInAlternatePolicy::MayStartLeg(0));
    assert(QuestTurnInAlternatePolicy::MayStartLeg(1));
    assert(!QuestTurnInAlternatePolicy::MayStartLeg(2));
    assert(!QuestTurnInAlternatePolicy::MayStartLeg(-1));
    assert(!QuestTurnInAlternatePolicy::MayRecover(
        false, NavigationPlanFailure::ReplanBudgetExhausted, 0));
    assert(QuestTurnInAlternatePolicy::MayRecover(
        true, NavigationPlanFailure::ReplanBudgetExhausted, 0));
    assert(QuestTurnInAlternatePolicy::MayRecover(
        true, NavigationPlanFailure::SurfaceRecoveryExhausted, 1));
    assert(!QuestTurnInAlternatePolicy::MayRecover(
        true, NavigationPlanFailure::ReplanBudgetExhausted, 2));
    assert(!QuestTurnInAlternatePolicy::MayRecover(
        true, NavigationPlanFailure::None, 0));
    assert(!QuestTurnInAlternatePolicy::MayRecover(
        true, NavigationPlanFailure::NoPath, 0));

    const std::vector<Point> none{};
    const Point stage = candidates[0];
    assert(QuestTurnInAlternatePolicy::Accept(
        origin, stage, 42.0f, true, false, 2.0f, 2.0f, none));
    // Geometric difference is not enough: a complete reachable Detour route
    // must avoid the known failed directed edge.
    assert(!QuestTurnInAlternatePolicy::Accept(
        origin, stage, 42.0f, false, false, 2.0f, 2.0f, none));
    assert(!QuestTurnInAlternatePolicy::Accept(
        origin, stage, 42.0f, true, true, 2.0f, 2.0f, none));
    assert(!QuestTurnInAlternatePolicy::Accept(
        origin, stage, 181.0f, true, false, 2.0f, 2.0f, none));
    assert(!QuestTurnInAlternatePolicy::Accept(
        origin, stage, 42.0f, true, false, 2.0f, 3.1f, none));
    assert(!QuestTurnInAlternatePolicy::Accept(
        origin, stage, 42.0f, true, false, 2.0f, 2.0f, {stage}));
    assert(!QuestTurnInAlternatePolicy::Accept(
        origin, stage, std::numeric_limits<float>::quiet_NaN(),
        true, false, 2.0f, 2.0f, none));

    // A lateral route must actually move the player before retrying the NPC.
    assert(!QuestTurnInAlternatePolicy::StageReached(
        origin, Point{110.0f, 200.0f, 10.0f}));
    assert(QuestTurnInAlternatePolicy::StageReached(origin, stage));
    // A second strategic attempt cannot choose effectively the same stage.
    assert(QuestTurnInAlternatePolicy::Accept(
        origin, candidates[1], 43.0f, true, false, 2.0f, 1.5f,
        {stage}));
}
