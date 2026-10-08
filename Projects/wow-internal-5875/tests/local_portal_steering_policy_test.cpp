#include "../src/Navigation/LocalPortalSteeringPolicy.h"
#include "../src/Navigation/CorridorReplanHysteresisPolicy.h"
#include "../src/Navigation/SurfaceRecoveryEpisodePolicy.h"
#include "../src/Navigation/SteeringSelectionPolicy.h"
#include "../src/Navigation/TerrainTransitionPolicy.h"
#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using Navigation::LocalPortalSteeringPolicy;
    using Navigation::LocalPortalPoint;
    using Navigation::LocalPortalRayClass;
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

    // Detour visits the next polygon only when its directed link actually
    // passes the filter and clipped tile-seam interval. A near hit is a wall,
    // not an accepted portal crossing.
    assert(LocalPortalSteeringPolicy::ClassifyRay(
        true,11,22,22,1.0f,0.0f,0.70f,false)==
        LocalPortalRayClass::IntendedPortal);
    assert(LocalPortalSteeringPolicy::ClassifyRay(
        true,11,22,11,0.50f,0.0f,0.70f,false)==
        LocalPortalRayClass::CorridorBoundary);
    assert(LocalPortalSteeringPolicy::ClassifyRay(
        true,11,22,11,0.50f,2.0f,0.70f,false)==
        LocalPortalRayClass::BlockedGeometry);
    assert(LocalPortalSteeringPolicy::ClassifyRay(
        false,11,22,11,0.50f,0.0f,0.70f,false)==
        LocalPortalRayClass::UnknownProvenance);
    assert(LocalPortalSteeringPolicy::ClassifyRay(
        true,11,22,22,1.0f,0.0f,0.70f,true)==
        LocalPortalRayClass::InsufficientClearance);
    const auto seamInterior=LocalPortalSteeringPolicy::InteriorStage(
        {0,0,0},{0,1,0},{4,0.5f,0},0.25f);
    assert(seamInterior.x==1.0f && seamInterior.y==0.5f);
    const auto narrowInterior=LocalPortalSteeringPolicy::InteriorStage(
        {0,0,0},{0,0.1f,0},{0.2f,0.05f,0},1.0f);
    assert(narrowInterior.x==0.2f);

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
    using Navigation::SurfaceRecoveryEpisodePolicy;
    assert(SurfaceRecoveryEpisodePolicy::VerifiedPortalStageProgress(
        true,11,22,11,22,22,true,true,true,225,216,4));
    assert(!SurfaceRecoveryEpisodePolicy::VerifiedPortalStageProgress(
        false,11,22,11,22,22,true,true,true,225,216,4));
    assert(!SurfaceRecoveryEpisodePolicy::VerifiedPortalStageProgress(
        true,11,22,11,11,22,true,true,true,225,216,4));
    assert(!SurfaceRecoveryEpisodePolicy::VerifiedPortalStageProgress(
        true,11,22,11,22,22,true,false,true,225,216,4));
    assert(!SurfaceRecoveryEpisodePolicy::VerifiedPortalStageProgress(
        true,11,22,11,22,22,true,true,false,225,216,4));
    assert(!SurfaceRecoveryEpisodePolicy::VerifiedPortalStageProgress(
        true,11,22,11,22,22,true,true,true,225,222,4));
    assert(Navigation::TerrainTransitionPolicy::Assess(
        LocalPortalPoint{0,0,0},LocalPortalPoint{3.851f,0,6.040f}).rejected);
    assert(Navigation::TerrainTransitionPolicy::Assess(
        LocalPortalPoint{0,0,0},LocalPortalPoint{8.996f,0,10.0f}).rejected);
}
