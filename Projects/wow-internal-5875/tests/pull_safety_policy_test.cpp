#include "../src/Bot/PullSafetyPolicy.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
    Bot::PullSafetyUnit Unit(std::uint64_t guid, float x, float distance,
        bool eligible = true, bool nearbyHostile = true)
    {
        Bot::PullSafetyUnit unit{};
        unit.guid = guid;
        unit.entry = 3256;
        unit.x = x;
        unit.playerDistance = distance;
        unit.live = true;
        unit.eligible = eligible;
        unit.nearbyHostile = nearbyHostile;
        return unit;
    }
}

int main()
{
    using Bot::PullSafetyDecision;
    using Bot::PullSafetyPolicy;
    using Bot::PullSafetyRejectReason;

    static_assert(PullSafetyPolicy::MinimumVoluntaryHealthPercent == 70.0f);
    static_assert(PullSafetyPolicy::HighHealthPercent == 85.0f);
    static_assert(PullSafetyPolicy::NearbyHostileRadius == 12.0f);
    static_assert(PullSafetyPolicy::MaximumEvaluatedCandidates == 32);

    std::vector<Bot::PullSafetyUnit> isolated{Unit(1, 0, 8)};
    assert(PullSafetyPolicy::Select(isolated, 69.9f).decision ==
        PullSafetyDecision::WaitForHealth);
    assert(PullSafetyPolicy::Select(isolated, 70.0f).decision ==
        PullSafetyDecision::Voluntary);
    assert(PullSafetyPolicy::Select(isolated, 84.9f).selectedIndex == 0);
    assert(PullSafetyPolicy::Select(isolated, 85.0f).selectedIndex == 0);
    assert(PullSafetyPolicy::Select(isolated,
        std::numeric_limits<float>::quiet_NaN()).decision ==
        PullSafetyDecision::WaitForHealth);

    std::vector<Bot::PullSafetyUnit> oneAdd{
        Unit(1, 0, 8), Unit(9, 8, 16, false)};
    assert(PullSafetyPolicy::Assess(oneAdd, 0, 80.0f, false).predictedAdds == 1);
    assert(PullSafetyPolicy::Assess(oneAdd, 0, 80.0f, false).rejection ==
        PullSafetyRejectReason::NotIsolated);
    assert(PullSafetyPolicy::Select(oneAdd, 80.0f).decision ==
        PullSafetyDecision::NoSafeCandidate);
    assert(PullSafetyPolicy::Select(oneAdd, 85.0f).decision ==
        PullSafetyDecision::Voluntary);

    std::vector<Bot::PullSafetyUnit> twoAdds{
        Unit(1, 0, 8), Unit(8, 6, 15, false), Unit(9, -6, 15, false)};
    assert(PullSafetyPolicy::Assess(twoAdds, 0, 100.0f, false).predictedAdds == 2);
    assert(PullSafetyPolicy::Assess(twoAdds, 0, 100.0f, false).rejection ==
        PullSafetyRejectReason::TooManyPredictedAdds);
    assert(PullSafetyPolicy::Select(twoAdds, 100.0f).decision ==
        PullSafetyDecision::NoSafeCandidate);

    // Distance cannot make a packed target outrank a farther isolated one.
    std::vector<Bot::PullSafetyUnit> alternatives{
        Unit(1, 0, 5), Unit(2, 40, 25), Unit(9, 6, 12, false)};
    auto result = PullSafetyPolicy::Select(alternatives, 95.0f);
    assert(result.decision == PullSafetyDecision::Voluntary);
    assert(alternatives[result.selectedIndex].guid == 2);
    assert(result.selected.predictedAdds == 0);
    assert(result.candidateCount == 2);

    // Dead, friendly, invalid and unattackable snapshot entries do not
    // strengthen the nearby-hostile proxy.
    auto dead = Unit(10, 4, 9, false);
    dead.live = false;
    auto friendly = Unit(11, 5, 9, false, false);
    auto invalid = Unit(12, 6, 9, false);
    invalid.guid = 0;
    std::vector<Bot::PullSafetyUnit> irrelevant{
        Unit(1, 0, 8), dead, friendly, invalid};
    assert(PullSafetyPolicy::Assess(irrelevant, 0, 80.0f, false).predictedAdds == 0);
    assert(PullSafetyPolicy::Select(irrelevant, 80.0f).selectedIndex == 0);
    // Height-separated units are not predicted as immediate adds.
    auto upper = Unit(13, 0, 8, false);
    upper.z = 20.0f;
    irrelevant.push_back(upper);
    assert(PullSafetyPolicy::Assess(irrelevant, 0, 80.0f, false).predictedAdds == 0);

    // Existing attackers always outrank unrelated quest candidates, even at
    // critical health. Among attackers retain a deterministic nearest rule.
    auto attackerA = Unit(20, 30, 7, false);
    attackerA.directAggressor = true;
    auto attackerB = Unit(21, 32, 5, false);
    attackerB.directAggressor = true;
    std::vector<Bot::PullSafetyUnit> defensive{
        Unit(1, 0, 2), attackerA, attackerB};
    result = PullSafetyPolicy::Select(defensive, 12.0f);
    assert(result.decision == PullSafetyDecision::Defensive);
    assert(result.aggressorCount == 2);
    assert(defensive[result.selectedIndex].guid == 21);
    assert(PullSafetyPolicy::Assess(defensive, 0, 95.0f, true).rejection ==
        PullSafetyRejectReason::ExistingAggressor);

    // The caller's quest-entry filter remains authoritative: a nearby wrong
    // creature may be a risk neighbor but cannot become the pull target.
    std::vector<Bot::PullSafetyUnit> quest{
        Unit(1, 0, 4, false), Unit(2, 30, 18, true)};
    quest[0].entry = 9999;
    result = PullSafetyPolicy::Select(quest, 95.0f);
    assert(quest[result.selectedIndex].guid == 2);
    quest[1].eligible = false;
    assert(PullSafetyPolicy::Select(quest, 95.0f).decision ==
        PullSafetyDecision::NoEligibleCandidate);

    // Unsafe is temporary evidence, not a permanent blacklist. Re-evaluation
    // after a neighboring mob leaves can select the same original GUID.
    oneAdd[1].live = false;
    assert(PullSafetyPolicy::Select(oneAdd, 80.0f).selectedIndex == 0);
    auto tie = std::vector<Bot::PullSafetyUnit>{Unit(3, 0, 10), Unit(2, 50, 10)};
    assert(tie[PullSafetyPolicy::Select(tie, 95.0f).selectedIndex].guid == 2);

    // The candidate shortlist is bounded; a packed shortlist cannot force an
    // attack merely because farther candidates were not evaluated.
    std::vector<Bot::PullSafetyUnit> many;
    for (std::uint64_t guid = 1; guid <= 40; ++guid)
        many.push_back(Unit(guid, static_cast<float>(guid * 30),
            static_cast<float>(guid)));
    assert(PullSafetyPolicy::Select(many, 95.0f).candidateCount == 40);
    assert(PullSafetyPolicy::Select(many, 95.0f).decision ==
        PullSafetyDecision::Voluntary);
}
