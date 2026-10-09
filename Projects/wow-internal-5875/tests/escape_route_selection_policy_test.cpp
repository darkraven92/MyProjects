#include "../src/Bot/EscapeRouteSelectionPolicy.h"
#include "../src/Navigation/RouteCostProbePolicy.h"

#include <cassert>
#include <cmath>
#include <vector>

using namespace Bot;

static EscapeRouteProbeEvidence Reachable(float cost)
{
    EscapeRouteProbeEvidence result{};
    result.reachable = true;
    result.routeCost = cost;
    return result;
}

int main()
{
    const EscapeRoutePoint origin{0.0f, 0.0f, 5.0f};
    const auto oneThreat = EscapeRouteSelectionPolicy::Generate(
        origin, {{5.0f, 0.0f, 5.0f}});
    assert(oneThreat.threatsKnown);
    assert(!oneThreat.candidates.empty());
    assert(oneThreat.candidates.size() <=
        EscapeRouteSelectionPolicy::MaximumCandidates);
    assert(oneThreat.candidates[0].destination.x < 0.0f);
    std::vector<EscapeRouteProbeEvidence> oneProbes(
        oneThreat.candidates.size(), Reachable(30.0f));
    auto selected = EscapeRouteSelectionPolicy::Select(oneThreat, oneProbes);
    assert(selected.decision == EscapeRouteDecisionKind::CandidateAvailable);
    assert(selected.selectedMinimumThreatDistance >
        selected.currentMinimumThreatDistance);
    const auto oneOrder = EscapeRouteSelectionPolicy::ProbeOrder(oneThreat);
    assert(oneOrder.size() == oneThreat.candidates.size());
    assert(oneOrder[0] == 0); // direct away has best separation in 1v1
    std::vector<EscapeRouteProbeEvidence> firstReady(oneOrder.size());
    std::vector<bool> firstEvaluated(oneOrder.size(), false);
    firstReady[0] = Reachable(30.0f); // complete Detour route only
    firstEvaluated[0] = true;
    assert(EscapeRouteSelectionPolicy::CanFinishEarly(
        oneThreat, firstReady, firstEvaluated));
    firstReady[0].reachable = false; // partial corridor is not proof
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        oneThreat, firstReady, firstEvaluated));
    if (oneOrder.size() > 1)
    {
        firstEvaluated[oneOrder[1]] = true;
        firstReady[oneOrder[1]] = Reachable(34.0f);
        assert(EscapeRouteSelectionPolicy::Select(oneThreat, firstReady)
            .decision == EscapeRouteDecisionKind::CandidateAvailable);
    }

    // Euclidean separation alone is insufficient: only Detour-validated
    // planning-only results may mark a destination reachable.
    oneProbes[0].reachable = false;
    for (std::size_t i = 1; i < oneProbes.size(); ++i)
        oneProbes[i].reachable = false;
    selected = EscapeRouteSelectionPolicy::Select(oneThreat, oneProbes);
    assert(selected.decision == EscapeRouteDecisionKind::NoReachableCandidate);
    assert(selected.reachableCount == 0);

    const auto twoThreats = EscapeRouteSelectionPolicy::Generate(
        origin, {{5.0f, 1.0f, 5.0f}, {5.0f, -1.0f, 5.0f}});
    assert(twoThreats.threatsKnown);
    assert(!twoThreats.candidates.empty());
    std::vector<EscapeRouteProbeEvidence> twoProbes(
        twoThreats.candidates.size(), Reachable(40.0f));
    selected = EscapeRouteSelectionPolicy::Select(twoThreats, twoProbes);
    assert(selected.decision == EscapeRouteDecisionKind::CandidateAvailable);
    assert(selected.selectedMinimumThreatDistance >=
        selected.currentMinimumThreatDistance +
            EscapeRouteSelectionPolicy::MinimumSeparationGain);

    // Known greater death/hazard risk is vetoed even when Detour can plan.
    for (auto& probe : twoProbes)
    {
        probe.deathRiskKnown = true;
        probe.currentDeathRisk = 1.0f;
        probe.destinationDeathRisk = 2.0f;
    }
    selected = EscapeRouteSelectionPolicy::Select(twoThreats, twoProbes);
    assert(selected.decision == EscapeRouteDecisionKind::NoReachableCandidate);
    for (auto& probe : twoProbes)
    {
        probe.destinationDeathRisk = 0.0f;
        probe.navHazardRiskKnown = true;
        probe.currentNavHazardRisk = 0.0f;
        probe.destinationNavHazardRisk = 1.0f;
    }
    selected = EscapeRouteSelectionPolicy::Select(twoThreats, twoProbes);
    assert(selected.decision == EscapeRouteDecisionKind::NoReachableCandidate);

    assert(!EscapeRouteSelectionPolicy::Generate(origin, {}).threatsKnown);
    assert(!EscapeRouteSelectionPolicy::Generate(origin, {{0, 0, 5}})
        .threatsKnown);
    assert(!EscapeRouteSelectionPolicy::Generate(
        origin, {{5, 0, 5}, {-5, 0, 5}}).threatsKnown);
    assert(!EscapeRouteSelectionPolicy::Generate(
        {NAN, 0, 0}, {{5, 0, 0}}).threatsKnown);
    std::vector<EscapeRoutePoint> tooMany(
        EscapeRouteSelectionPolicy::MaximumThreats + 1,
        {5.0f, 0.0f, 5.0f});
    assert(!EscapeRouteSelectionPolicy::Generate(origin, tooMany).threatsKnown);

    // Deliberately asymmetric, synthetic evidence isolates lexicographic
    // ordering: greater separation beats modestly shorter path cost.
    EscapeRouteGeneration ranked{};
    ranked.threatsKnown = true;
    ranked.currentMinimumThreatDistance = 5.0f;
    ranked.candidates = {{{-20, 0, 5}, 25.0f},
                         {{-24, 0, 5}, 29.0f}};
    std::vector<EscapeRouteProbeEvidence> rankProbes{
        Reachable(25.0f), Reachable(32.0f)};
    selected = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    assert(selected.selectedIndex == 1);
    assert(selected.routeCost == 32.0f);
    rankProbes[1].reachable = false;
    selected = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    assert(selected.selectedIndex == 0);

    ranked.candidates[1].minimumThreatDistance = 25.0f;
    rankProbes[1] = Reachable(25.0f);
    selected = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    assert(selected.selectedIndex == 0); // stable index tie
    rankProbes[1].routeCost = 24.0f;
    selected = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    assert(selected.selectedIndex == 1);
    rankProbes[0].deathRiskKnown = true;
    rankProbes[0].currentDeathRisk = 3.0f;
    rankProbes[0].destinationDeathRisk = 1.0f;
    rankProbes[1].deathRiskKnown = true;
    rankProbes[1].currentDeathRisk = 3.0f;
    rankProbes[1].destinationDeathRisk = 2.0f;
    selected = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    assert(selected.selectedIndex == 0); // lower known danger before cost

    const auto first = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    const auto second = EscapeRouteSelectionPolicy::Select(ranked, rankProbes);
    assert(first.decision == second.decision);
    assert(first.selectedIndex == second.selectedIndex);
    assert(first.routeCost == second.routeCost);

    // The strongest known separation is probed first, irrespective of
    // generation order. A complete safe result can terminate only when no
    // unevaluated candidate can outrank it under the existing primary key.
    ranked.candidates[1].minimumThreatDistance = 29.0f;
    const auto order = EscapeRouteSelectionPolicy::ProbeOrder(ranked);
    assert(order.size() == 2);
    assert(order[0] == 1 && order[1] == 0);
    assert(order == EscapeRouteSelectionPolicy::ProbeOrder(ranked));
    std::vector<bool> evaluated{false, true};
    rankProbes[0].reachable = false;
    assert(EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));
    // The first candidate can fail; evaluating the second is then required.
    rankProbes[1].reachable = false;
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));
    rankProbes[1] = Reachable(32.0f);
    rankProbes[1].deathRiskKnown = true;
    rankProbes[1].currentDeathRisk = 1.0f;
    rankProbes[1].destinationDeathRisk = 2.0f;
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated)); // reachable but unsafe endpoint
    rankProbes[1].destinationDeathRisk = 0.0f;
    assert(EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));

    // A tied or stronger unprobed endpoint may still have a better route
    // cost. Strict dominance is required; partial/unproven probes must not
    // set reachable evidence and therefore cannot be accepted.
    ranked.candidates[0].minimumThreatDistance = 29.0f;
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));
    ranked.candidates[0].minimumThreatDistance = 30.0f;
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));
    evaluated = {true, false};
    rankProbes[0] = Reachable(25.0f);
    rankProbes[0].deathRiskKnown = false;
    assert(EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));
    rankProbes[0].reachable = false;
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, evaluated));
    assert(!EscapeRouteSelectionPolicy::CanFinishEarly(
        ranked, rankProbes, {true}));

    // Pure generation/selection never dispatch movement. The only runtime
    // planning adapter is explicitly planning-only and cannot issue CTM.
    assert(!Navigation::RouteCostProbePolicy::MayIssueMovement(true));
}
