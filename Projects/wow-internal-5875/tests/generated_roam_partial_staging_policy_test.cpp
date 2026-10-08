#include "../src/Navigation/GeneratedRoamTargetPolicy.h"
#include "../src/Navigation/PartialStagingEpisodePolicy.h"

#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using Navigation::GeneratedRoamTargetPolicy;
    using Navigation::PartialStagingEpisodeEvidence;
    using Navigation::PartialStagingEpisodePolicy;

    // Inherited player Z is only a seed: a nearby valid ground layer may
    // replace it, but not a different floor or arbitrary horizontal point.
    assert(GeneratedRoamTargetPolicy::BoundedProjection(
        2401.633f, -2452.717f, 100.330f,
        2402.0f, -2452.0f, 112.0f));
    assert(!GeneratedRoamTargetPolicy::BoundedProjection(
        2401.633f, -2452.717f, 100.330f,
        2402.0f, -2452.0f, 135.0f));
    assert(!GeneratedRoamTargetPolicy::BoundedProjection(
        2401.633f, -2452.717f, 100.330f,
        2415.0f, -2452.0f, 100.0f));
    assert(GeneratedRoamTargetPolicy::SafeConnectedGround(
        true, false, true, true, false, false));
    assert(!GeneratedRoamTargetPolicy::SafeConnectedGround(
        true, false, true, true, true, false)); // water
    assert(!GeneratedRoamTargetPolicy::SafeConnectedGround(
        true, true, true, true, false, false)); // hard hazard
    assert(!GeneratedRoamTargetPolicy::SafeConnectedGround(
        true, false, false, true, false, false)); // disconnected
    assert(!GeneratedRoamTargetPolicy::SafeConnectedGround(
        true, false, true, false, false, false)); // unsafe terrain
    assert(!GeneratedRoamTargetPolicy::SafeConnectedGround(
        true, false, true, true, false, true)); // steep fallback

    static_assert(PartialStagingEpisodePolicy::MaximumAttempts == 3);
    PartialStagingEpisodeEvidence progress{
        true, true, true, true, true, true, true,
        17, 17, 10, 12, 81.141f, 35.0f};
    const std::vector<std::uint64_t> oldCorridor{10, 11, 12, 13};
    assert(PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor));
    progress.newStartPoly = 99;
    assert(!PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor)); // lateral movement without corridor proof
    progress.newStartPoly = 12;
    progress.destinationGain = -1.0f;
    assert(!PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor));
    progress.destinationGain = 35.0f;
    progress.newGeneration = 18;
    assert(!PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor)); // stale refs
    progress.newGeneration = 17;
    progress.waterExcluded = false;
    assert(!PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor));
    progress.waterExcluded = true;
    progress.newRouteValidated = false;
    assert(!PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor));
    progress.newRouteValidated = true;
    progress.playerProjectionMatchesStart = false;
    assert(!PartialStagingEpisodePolicy::VerifiedForwardProgress(
        progress, oldCorridor));
    return 0;
}
