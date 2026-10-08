#include "../src/Navigation/DeathRouteTransitionMemory.h"
#include "../src/Navigation/DirectedTransitionQueryPolicy.h"

#include <cassert>
#include <cstdint>
#include <vector>

struct TestLink
{
    std::uint64_t ref = 0;
    unsigned int next = 0xffffffffu;
};

int main()
{
    using Navigation::DirectedPolyTransition;
    using Navigation::DirectedTransitionQueryPolicy;
    using Navigation::DeathRouteTransitionMemory;
    constexpr unsigned int nullLink = 0xffffffffu;
    TestLink aLinks[]{{2, 1}, {4, nullLink}};
    TestLink cLinks[]{{2, nullLink}};
    std::vector<DirectedTransitionQueryPolicy::SavedLink<TestLink>> saved;
    assert(DirectedTransitionQueryPolicy::Mask(
        aLinks, 0, 2, nullLink, 2, saved));
    assert(aLinks[0].ref == 0); // unsafe A->B is unavailable to findPath
    assert(aLinks[1].ref == 4);
    assert(cLinks[0].ref == 2); // safe C->B remains queryable
    assert(!DirectedTransitionQueryPolicy::Mask(
        aLinks, 0, 2, nullLink, 7, saved));
    DirectedTransitionQueryPolicy::Restore(saved);
    assert(aLinks[0].ref == 2);
    assert(!DirectedTransitionQueryPolicy::Mask(
        aLinks, 2, 2, nullLink, 2, saved)); // stale chain
    assert(saved.empty());

    DeathRouteTransitionMemory episode;
    assert(!episode.ObserveGeneration(17));
    assert(episode.Learn({1, 2}));
    assert(episode.Learn({3, 4}));
    assert(!episode.Learn({1, 2}));
    assert(episode.SharedTransitions({{1, 2}, {9, 10}}) == 1);
    assert(episode.RememberCorridor(123));
    assert(!episode.RememberCorridor(123)); // same unsafe route family
    assert(episode.RememberCorridor(456)); // different corridor remains viable
    // The memory is intentionally independent of initialization tier and
    // corpse-anchor variant; only episode reset or generation change clears it.
    assert(episode.Size() == 2);
    assert(episode.Contains({1, 2}));
    assert(episode.ObserveGeneration(18));
    assert(episode.Size() == 0);
    assert(episode.Learn({5, 6}));
    episode.Reset();
    assert(episode.Generation() == 0);
    assert(episode.Size() == 0);
    assert(!episode.Learn({5, 6})); // unknown generation fails closed

    Navigation::EpisodeBadTransitionPolicy follower;
    for (std::uint64_t i = 1;
         i <= Navigation::EpisodeBadTransitionPolicy::MaximumLearned; ++i)
        assert(follower.Learn({i, i + 1}));
    assert(follower.Learn({100, 101})); // existing bounded follower policy
    assert(follower.Learned({100, 101}));
}
