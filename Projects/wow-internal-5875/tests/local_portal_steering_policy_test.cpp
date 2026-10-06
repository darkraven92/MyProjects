#include "../src/Navigation/LocalPortalSteeringPolicy.h"
#include "../src/Navigation/CorridorReplanHysteresisPolicy.h"
#include "../src/Navigation/SurfaceRecoveryEpisodePolicy.h"
#include "../src/Navigation/SteeringSelectionPolicy.h"
#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using Navigation::LocalPortalSteeringPolicy;
    using Navigation::LocalPortalPoint;
    using Navigation::CorridorFailureRecord;
    using Navigation::CorridorReplanDecision;
    using Navigation::CorridorReplanHysteresisPolicy;
    const std::vector<std::uint64_t> corridor{11, 22, 33};
    const LocalPortalPoint portalA{0, 0, 0}, portalB{0, 8, 0};
    const auto planned = LocalPortalSteeringPolicy::CandidateEdge(corridor,11,33);
    assert(planned.Valid() && planned.from==11 && planned.to==22);
    assert(!LocalPortalSteeringPolicy::CandidateEdge(corridor,22,22).Valid());
    assert(!LocalPortalSteeringPolicy::CandidateEdge(
        {11,22,11,33},11,33).Valid());
    const auto known = LocalPortalSteeringPolicy::AttributeRayFailure(
        corridor, 11, 33, 11, {0.3f, 4, 0}, portalA, portalB,
        Navigation::SteeringSelectionPolicy::MinimumWallClearance);
    assert(known.Valid() && known.from == 11 && known.to == 22);
    assert(!LocalPortalSteeringPolicy::AttributeRayFailure(
        corridor, 11, 33, 11, {4, 4, 0}, portalA, portalB, 0.70f).Valid());
    assert(!LocalPortalSteeringPolicy::AttributeRayFailure(
        corridor, 11, 33, 0, {0, 4, 0}, portalA, portalB, 0.70f).Valid());
    assert(!LocalPortalSteeringPolicy::AttributeRayFailure(
        corridor, 11, 99, 11, {0, 4, 0}, portalA, portalB, 0.70f).Valid());
    assert(!LocalPortalSteeringPolicy::AttributeRayFailure(
        {11, 22, 11, 33}, 11, 33, 11, {0, 4, 0}, portalA, portalB,
        0.70f).Valid());

    CorridorFailureRecord record{};
    CorridorReplanHysteresisPolicy::RecordFailure(record, 101, 0, 0, 4,
        true, known.from, known.to, false);
    assert(record.transitionKnown);
    const auto same = CorridorReplanHysteresisPolicy::Assess(record, 202,
        2, 0, 4, true, true, 11, 22, false);
    assert(same.decision == CorridorReplanDecision::SuppressRepeated);
    const auto alternate = CorridorReplanHysteresisPolicy::Assess(record, 202,
        2, 0, 4, true, true, 11, 44, false);
    assert(alternate.decision == CorridorReplanDecision::Allow);

    const auto stage = LocalPortalSteeringPolicy::AssessStage(
        {2, 4, 0}, portalA, portalB, {0, 4, 0},
        3.0f, true, 1.0f, 0.70f, 1.0f);
    assert(stage.allowed);
    assert(!LocalPortalSteeringPolicy::AssessStage(
        {2, 4, 0}, portalA, portalB, {0, 4, 0},
        0.0f, true, 1.0f, 0.70f, 1.0f).allowed);
    assert(!LocalPortalSteeringPolicy::AssessStage(
        {2, 4, 0}, portalA, portalB, {0, 4, 0},
        3.0f, true, 0.5f, 0.70f, 1.0f).allowed);
    assert(!LocalPortalSteeringPolicy::AssessStage(
        {0.2f, 4, 0}, portalA, portalB, {0, 4, 0},
        3.0f, true, 1.0f, 0.70f, 1.0f).allowed);

    assert(Navigation::SurfaceRecoveryEpisodePolicy::Assess(
        225, 216, 4, false) != Navigation::SurfaceRecoveryQuality::Progress);
    assert(Navigation::SurfaceRecoveryEpisodePolicy::Assess(
        225, 216, 4, true) == Navigation::SurfaceRecoveryQuality::Progress);
}
