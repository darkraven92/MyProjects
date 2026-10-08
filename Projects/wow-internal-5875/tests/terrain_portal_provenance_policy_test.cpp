#include "../src/Navigation/TerrainPortalProvenancePolicy.h"
#include "../src/Navigation/DeathRouteTransitionMemory.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    struct Point { float x, y, z; };
}

int main()
{
    using Navigation::TerrainPortalProvenancePolicy;
    using Navigation::TerrainTransitionPolicy;
    const std::vector<std::uint64_t> corridor{101,202,303,404};
    std::vector<std::size_t> pointIndices;

    // pointIndex names an ALL_CROSSINGS point, not a corridor polygon.
    assert(TerrainPortalProvenancePolicy::ResolvePointIndices(
        corridor,{101,202,303,0},pointIndices));
    assert((pointIndices==std::vector<std::size_t>{0,1,2,3}));
    const auto exact=TerrainPortalProvenancePolicy::Segment(
        corridor,pointIndices,2,true);
    assert(exact.transitionKnown && exact.adjacent);
    assert(exact.corridorFromIndex==1 && exact.corridorToIndex==2);
    assert(exact.fromPoly==202 && exact.toPoly==303);
    const auto unlinked=TerrainPortalProvenancePolicy::Segment(
        corridor,pointIndices,2,false);
    assert(!unlinked.transitionKnown && !unlinked.fromPoly && !unlinked.toPoly);

    // Detour can omit an intersection or merge coincident portal points.
    // The unsafe straight leg is still rejected, but it cannot prove one
    // particular edge in the ordered multi-link span.
    assert(TerrainPortalProvenancePolicy::ResolvePointIndices(
        corridor,{101,303,0},pointIndices));
    const auto skipped=TerrainPortalProvenancePolicy::Segment(
        corridor,pointIndices,1,true);
    assert(skipped.corridorFromIndex==0 && skipped.corridorToIndex==2);
    assert(!skipped.adjacent && !skipped.transitionKnown);
    assert(!skipped.fromPoly && !skipped.toPoly);
    const auto capturedStyle=TerrainTransitionPolicy::Assess(
        Point{0,0,0},Point{8.996f,0,10.0f});
    assert(capturedStyle.rejected);
    const auto later=TerrainPortalProvenancePolicy::Segment(
        corridor,pointIndices,2,true);
    assert(later.transitionKnown && later.fromPoly==303 && later.toPoly==404);

    // A coincident first portal may update straightPath[0]'s ref. The first
    // live-player leg nevertheless starts at corridor index zero.
    assert(TerrainPortalProvenancePolicy::ResolvePointIndices(
        corridor,{202,303,0},pointIndices));
    const auto mergedStart=TerrainPortalProvenancePolicy::Segment(
        corridor,pointIndices,1,true);
    assert(!mergedStart.transitionKnown && mergedStart.corridorToIndex==2);
    assert(!TerrainPortalProvenancePolicy::ResolvePointIndices(
        corridor,{101,999,0},pointIndices)); // no nearest-poly inference
    assert(pointIndices.empty());

    // The existing DeathRecovery memory carries multiple exact edges across
    // query tiers and variants, and clears them on generation/episode change.
    Navigation::DeathRouteTransitionMemory deathEpisode;
    assert(!deathEpisode.ObserveGeneration(17));
    assert(deathEpisode.Learn({101,202}));
    assert(deathEpisode.Learn({303,404}));
    assert(deathEpisode.Size()==2);
    assert(deathEpisode.Contains({101,202}) &&
        deathEpisode.Contains({303,404}));
    assert(deathEpisode.ObserveGeneration(18));
    assert(deathEpisode.Size()==0);
    assert(deathEpisode.Learn({101,202}));
    deathEpisode.Reset();
    assert(deathEpisode.Size()==0);
}
