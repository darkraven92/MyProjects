#include "../src/Navigation/MapNavMeshSessionCache.h"
#include <cassert>
#include <vector>

struct Topology { int additions = 0; };
using Cache = Navigation::MapNavMeshSessionCache<Topology>;

static int Load(const Cache::Handle& entry, int count)
{
    int missing = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto key = std::to_string(i);
        if (entry->tiles.contains(key)) { ++entry->cacheHits; continue; }
        entry->tiles.emplace(key, Cache::Tile{{1, 100}, static_cast<std::uint64_t>(i + 1)});
        ++entry->topology.additions;
        ++entry->coldTileLoads;
        ++missing;
    }
    return missing;
}

int main()
{
    assert(!Navigation::SameNavMeshGeneration(0, 0));
    assert(!Navigation::SameNavMeshGeneration(0, 1));
    assert(!Navigation::SameNavMeshGeneration(1, 0));
    assert(!Navigation::SameNavMeshGeneration(1, 2));
    assert(Navigation::SameNavMeshGeneration(1, 1));
    Cache cache;
    const Navigation::NavMeshCacheIdentity map1{1, "mesh", "parameters", 1, 28};
    auto first = cache.Acquire(map1);
    assert(Load(first, 30) == 30);
    assert(Load(cache.Acquire(map1), 30) == 0);
    assert(Load(cache.Acquire(map1), 90) == 60);
    assert(Load(cache.Acquire(map1), 704) == 614);
    assert(Load(cache.Acquire(map1), 25) == 0);
    assert(first->topology.additions == 704);
    assert(cache.Acquire(map1) == first); // Destination/intent not cache keys.
    // Dropping a provider handle or a failed route does not delete topology.
    auto detached = cache.Acquire(map1);
    detached.reset();
    assert(cache.Acquire(map1) == first);
    // Filter/hazard state is deliberately not a cache identity dimension.
    const auto oldHits = first->cacheHits;
    assert(Load(cache.Acquire(map1), 25) == 0);
    assert(first->cacheHits == oldHits + 25);
    const auto ref = first->tiles.at("0").reference;
    auto later = cache.Acquire(map1);
    assert(later->tiles.at("0").reference == ref);
    // Route-specific corridors live OUTSIDE the topology entry.
    std::vector<std::uint64_t> corridor{ref};
    corridor.clear();
    assert(cache.Acquire(map1)->tiles.size() == 704);
    for (std::uint32_t map : {0u, 530u, 1u})
    {
        auto identity = map1;
        identity.map = map;
        const auto next = cache.Acquire(identity);
        assert(!cache.IsCurrent(later));
        assert(!later->valid);
        assert(next->generation > later->generation);
        assert(next->tiles.empty());
        later = next;
    }
    cache.Invalidate(); // World unload / explicit teardown.
    assert(!cache.IsCurrent(later));
    assert(!later->valid); // Owned memory survives, NOT usable references.
    auto next = cache.Acquire(map1);
    assert(next->generation > later->generation);
    for (int changed = 0; changed < 4; ++changed)
    {
        auto identity = map1;
        if (changed == 0) identity.directory += "_new";
        if (changed == 1) identity.mapParameters += "_new";
        if (changed == 2) ++identity.metadataStamp;
        if (changed == 3) ++identity.metadataSize;
        const auto replaced = cache.Acquire(identity);
        assert(!cache.IsCurrent(next));
        assert(replaced->generation > next->generation);
        next = replaced;
    }
}
