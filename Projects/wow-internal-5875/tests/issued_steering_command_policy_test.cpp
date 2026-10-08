#include "../src/Navigation/IssuedSteeringCommandPolicy.h"
#include "../src/Navigation/CorridorReplanHysteresisPolicy.h"
#include "../src/Navigation/SurfaceRecoveryEpisodePolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main()
{
    using namespace Navigation;
    const std::vector<std::uint64_t> corridor{11,22,33};
    assert(IssuedSteeringCommandPolicy::ProvenRouteEdge(
        corridor,11,22,true,false,99)==(DirectedPolyTransition{11,22}));
    assert(!IssuedSteeringCommandPolicy::ProvenRouteEdge(
        corridor,11,22,false,false,99).Valid());
    assert(!IssuedSteeringCommandPolicy::ProvenRouteEdge(
        corridor,11,22,true,true,99).Valid());
    assert(IssuedSteeringCommandPolicy::ProvenRouteEdge(
        corridor,11,22,true,true,22).Valid());
    assert(!IssuedSteeringCommandPolicy::ProvenRouteEdge(
        corridor,11,99,true,false,99).Valid());
    IssuedSteeringCommand command{};
    IssuedSteeringCommandPolicy::Record(command, 17, 4, 99, 123, 42,
        NavCommandSource::OrdinarySteering, {-1, -2, 3}, {-4, -5, 6},
        {11, 22});
    auto evidence = IssuedSteeringCommandPolicy::ForHardStall(command,
        42, 17, 4, 99, {-1, -2, 3}, true);
    assert(evidence.transition.Valid());
    assert(evidence.transition.from == 11 && evidence.transition.to == 22);
    assert(evidence.reason == HardStallAttributionReason::IssuedTransitionProven);
    assert(!IssuedSteeringCommandPolicy::ForHardStall(command,
        43, 17, 4, 99, {-1, -2, 3}, true).transition.Valid());
    assert(!IssuedSteeringCommandPolicy::ForHardStall(command,
        42, 18, 4, 99, {-1, -2, 3}, true).transition.Valid());
    assert(!IssuedSteeringCommandPolicy::ForHardStall(command,
        42, 17, 4, 99, {-1, -2, 4}, true).transition.Valid());
    assert(!IssuedSteeringCommandPolicy::ForHardStall(command,
        42, 17, 4, 99, {-1, -2, 3}, false).transition.Valid());

    IssuedSteeringCommandPolicy::Record(command, 18, 4, 99, 124, 43,
        NavCommandSource::SurfaceRecovery, {5, 6, 7}, {-1, -2, 3}, {});
    evidence = IssuedSteeringCommandPolicy::ForHardStall(command,
        43, 18, 4, 99, {5, 6, 7}, true);
    assert(!evidence.transition.Valid());
    assert(evidence.reason == HardStallAttributionReason::IssuedTransitionUnknown);
    assert(!IssuedSteeringCommandPolicy::ForHardStall(command,
        42, 17, 4, 99, {-1, -2, 3}, true).transition.Valid());
    IssuedSteeringCommandPolicy::Invalidate(command);
    assert(!IssuedSteeringCommandPolicy::ForHardStall(command,
        43, 18, 4, 99, {5, 6, 7}, true).transition.Valid());

    CorridorFailureRecord record{};
    CorridorReplanHysteresisPolicy::RecordFailure(record, 17, 0, 0, 4,
        true, 11, 22, false);
    assert(CorridorReplanHysteresisPolicy::Assess(record, 18, 1, 0, 4,
        true, true, 11, 22, false).decision ==
        CorridorReplanDecision::SuppressRepeated);
    assert(CorridorReplanHysteresisPolicy::Assess(record, 18, 1, 0, 4,
        true, true, 11, 23, false).decision ==
        CorridorReplanDecision::Allow);
    assert(SurfaceRecoveryEpisodePolicy::Assess(100, 90, 4, false) !=
        SurfaceRecoveryQuality::Progress);
    assert(SurfaceRecoveryEpisodePolicy::Assess(100, 90, 4, true) ==
        SurfaceRecoveryQuality::Progress);

    std::ifstream input("src/Navigation/GenericNavMeshPathFollower.h");
    assert(input);
    const std::string source{std::istreambuf_iterator<char>(input),{}};
    assert(source.find("IssuedSteeringCommandPolicy::ForHardStall(") !=
        std::string::npos);
    assert(source.find("ProvenIssuedSteeringEdge(player,") !=
        std::string::npos);
    assert(source.find("IssueNavMovement(player,commandPoint,CtmPrecision,") !=
        std::string::npos);
    assert(source.find("IssuedSteeringCommandPolicy::Invalidate(issuedNavCommand_);") !=
        std::string::npos);
    assert(source.find("AttributeFailedSteering(player,fromIndex,firstCandidate,") !=
        std::string::npos);
    assert(source.find("SteeringCandidateDecision::RejectedClearance") !=
        std::string::npos);
    assert(source.find("IssueVerifiedPortalStage(player,tick,localFailure)") !=
        std::string::npos);
    assert(source.find("MaximumSurfaceRecoveryAttempts = 4") !=
        std::string::npos);
    assert(source.find("MaximumLastSafeBacktracks = 2") !=
        std::string::npos);
    assert(source.find("MaximumPathLength =\n            2000.0f") !=
        std::string::npos);
}
