#include "../src/Bot/ServiceHubSelectionPolicy.h"
#include "../src/Navigation/RouteCostProbePolicy.h"

#include <cassert>
#include <cstddef>
#include <vector>

using Bot::ServiceHubCandidate;
using Bot::ServiceHubSelectionPolicy;
using Bot::ServiceHubSource;
using Bot::ServiceKnowledge;
using Bot::ServiceSelectionOrigin;

static ServiceHubCandidate Hub(std::uint32_t entry, float distance,
                               float cost, ServiceKnowledge sell,
                               ServiceKnowledge repair, bool reachable = true)
{
    ServiceHubCandidate candidate{};
    candidate.entry = entry;
    candidate.source = ServiceHubSource::MerchantFrameVerified;
    candidate.positionKnown = true;
    candidate.euclideanDistance = distance;
    candidate.navigationCost = cost;
    candidate.sell = sell;
    candidate.repair = repair;
    candidate.reachable = reachable;
    candidate.evaluated = true;
    return candidate;
}

int main()
{
    constexpr auto yes = ServiceKnowledge::Available;
    constexpr auto no = ServiceKnowledge::Unavailable;
    constexpr auto unknown = ServiceKnowledge::Unknown;
    std::vector<ServiceHubCandidate> hubs{
        Hub(3167, 100.0f, 120.0f, yes, yes),
        Hub(9001, 20.0f, 24.0f, yes, yes),
        Hub(9002, 10.0f, 12.0f, yes, no),
        Hub(9003, 25.0f, 28.0f, yes, yes)
    };
    hubs[0].source = ServiceHubSource::Seeded; // Wuark has no rank privilege.

    auto shortlist = ServiceHubSelectionPolicy::Shortlist(hubs, true, true, 4);
    assert(shortlist.size() == 3);
    assert(shortlist[0] == 1); // nearer suitable merchant, not Wuark
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 1);
    hubs[1].reachable = false;
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 3);
    hubs[3].reachable = false;
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 0);
    hubs[0].reachable = false;
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == hubs.size());

    // A sell-only trip may use a known non-repair merchant.
    shortlist = ServiceHubSelectionPolicy::Shortlist(hubs, true, false, 4);
    assert(shortlist[0] == 2);
    hubs[2].reachable = true;
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 2);
    assert(!ServiceHubSelectionPolicy::Suitable(hubs[2], true, true));

    // Only seeded, persisted, or MerchantFrame-proven merchants enter ranking.
    ServiceHubCandidate arbitrary = Hub(9876, 1.0f, 1.0f, unknown, unknown);
    arbitrary.source = ServiceHubSource::UnverifiedVisible;
    hubs.push_back(arbitrary);
    assert(!ServiceHubSelectionPolicy::Suitable(hubs.back(), true, false));
    hubs.back().source = ServiceHubSource::Persisted;
    assert(ServiceHubSelectionPolicy::Suitable(hubs.back(), true, false));
    hubs.pop_back();

    const ServiceSelectionOrigin origin{100.0f, 200.0f, 5.0f};
    ServiceHubCandidate first = Hub(100, 0.0f, 0.0f, yes, yes);
    ServiceHubCandidate second = Hub(101, 0.0f, 0.0f, yes, yes);
    first.x = 103.0f;
    first.y = 204.0f;
    second.x = 106.0f;
    second.y = 208.0f;
    assert(ServiceHubSelectionPolicy::DistanceFrom(origin, first) == 5.0f);
    assert(ServiceHubSelectionPolicy::DistanceFrom(origin, second) == 10.0f);

    // Unknown capabilities are eligible for bounded MerchantFrame probing.
    hubs.push_back(Hub(9004, 5.0f, 40.0f, unknown, unknown));
    shortlist = ServiceHubSelectionPolicy::Shortlist(hubs, true, true, 4);
    assert(shortlist[0] == 4);

    // Navigation cost, not Euclidean distance or insertion order, wins.
    hubs[0].reachable = true;
    hubs[1].reachable = true;
    hubs[3].reachable = true;
    hubs[4].navigationCost = 200.0f;
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 1);

    hubs[4].directFallbackOnly = true;
    hubs[4].navigationCost = 5.0f;
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 1);
    hubs[4].directFallbackOnly = false;
    hubs[4].navigationCost = 200.0f;

    // Failed candidates are removed; deterministic tie breaks use entry ID.
    hubs[1].failed = true;
    hubs[3].navigationCost = 120.0f;
    shortlist = ServiceHubSelectionPolicy::Shortlist(hubs, true, true, 4);
    assert(ServiceHubSelectionPolicy::BestReachable(hubs, shortlist) == 3);
    hubs[0].failed = true;
    hubs[3].failed = true;
    hubs[4].failed = true;
    assert(ServiceHubSelectionPolicy::Shortlist(hubs, true, true, 4).empty());

    // Shortlist caps per selection pass and no failed candidate loops back.
    std::vector<ServiceHubCandidate> many;
    for (std::uint32_t id = 1; id <= 20; ++id)
        many.push_back(Hub(id, static_cast<float>(id),
                           static_cast<float>(id), yes, yes));
    shortlist = ServiceHubSelectionPolicy::Shortlist(many, true, true, 4);
    assert(shortlist.size() == 4 && shortlist[0] == 0 && shortlist[3] == 3);
    for (const std::size_t index : shortlist)
        many[index].failed = true;
    shortlist = ServiceHubSelectionPolicy::Shortlist(many, true, true, 4);
    assert(shortlist.size() == 4 && shortlist[0] == 4);
    assert(ServiceHubSelectionPolicy::MayTryAnotherCandidate(9, 10));
    assert(!ServiceHubSelectionPolicy::MayTryAnotherCandidate(10, 10));

    // The frozen-origin Euclidean shortlist does not override validated
    // planning-only route length when choosing the final merchant.
    std::vector<ServiceHubCandidate> routeCosts{
        Hub(7001, 100.0f, 0.0f, yes, unknown),
        Hub(3167, 150.0f, 0.0f, yes, unknown)};
    routeCosts[0].navigationCost =
        Navigation::RouteCostProbePolicy::Ready(300.0f).pathLength;
    routeCosts[1].navigationCost =
        Navigation::RouteCostProbePolicy::Ready(180.0f).pathLength;
    shortlist = ServiceHubSelectionPolicy::Shortlist(routeCosts, true, false, 4);
    assert(shortlist[0] == 0);
    assert(ServiceHubSelectionPolicy::BestReachable(routeCosts, shortlist) == 1);
    routeCosts[1].failed = true;
    shortlist = ServiceHubSelectionPolicy::Shortlist(routeCosts, true, false, 4);
    assert(ServiceHubSelectionPolicy::BestReachable(routeCosts, shortlist) == 0);

    // Historical merchant IDs without observed coordinates are not guessed
    // into a navigation target; a live snapshot can supply the position.
    ServiceHubCandidate knownIdOnly = Hub(3187, 0.0f, 0.0f, unknown, unknown);
    knownIdOnly.source = ServiceHubSource::Seeded;
    knownIdOnly.positionKnown = false;
    assert(!ServiceHubSelectionPolicy::Suitable(knownIdOnly, true, false));
    knownIdOnly.positionKnown = true;
    assert(ServiceHubSelectionPolicy::Suitable(knownIdOnly, true, false));
}
