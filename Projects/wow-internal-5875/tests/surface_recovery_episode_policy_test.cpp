#include "../src/Navigation/SurfaceRecoveryEpisodePolicy.h"
#include "../src/Navigation/CorridorReplanHysteresisPolicy.h"
#include "../src/Navigation/LocalRecoveryExhaustionPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream input(path); assert(input);
    return {std::istreambuf_iterator<char>(input),{}};
}

int main()
{
    using namespace Navigation;
    using Q=SurfaceRecoveryQuality;
    using P=SurfaceRecoveryEpisodePolicy;
    constexpr float existingGain=4.0f;
    assert(P::Assess(177.362f,161.845f,existingGain,false)==Q::Neutral);
    assert(P::Assess(177.362f,171.839f,existingGain,false)==Q::Neutral);
    assert(P::Assess(177.362f,178.0f,existingGain,false)==Q::Regression);
    assert(P::Assess(177.362f,178.0f,existingGain,true)==Q::Regression);
    assert(P::Assess(177.362f,175.0f,existingGain,true)==Q::Neutral);
    assert(P::Assess(177.362f,170.0f,existingGain,true)==Q::Progress);
    assert(P::Assess(177.362f,173.362f,existingGain,true)==Q::Neutral);
    assert(P::SameLocalTarget(-171.522f,-4334.601f,-169.688f,-4333.750f,1.5f));
    assert(!P::SameLocalTarget(-171.522f,-4334.601f,-160.0f,-4300.0f,1.5f));
    assert(!P::SameLocalTarget(0,0,0,0,0));

    CorridorFailureRecord failure{};
    CorridorReplanHysteresisPolicy::RecordFailure(failure,17,0,0,existingGain);
    CorridorReplanHysteresisPolicy::RecordFailure(
        failure,17,11,0,existingGain,false,0,0,false);
    assert(failure.anchorX==0); // sideward travel cannot move the failure anchor
    const auto noProgress=CorridorReplanHysteresisPolicy::Assess(
        failure,17,11,0,existingGain,true,false,0,0,false);
    assert(noProgress.decision==CorridorReplanDecision::SuppressRepeated);
    const auto afterPortal=CorridorReplanHysteresisPolicy::Assess(
        failure,17,11,0,existingGain,true,false,0,0,true);
    assert(afterPortal.decision==CorridorReplanDecision::AllowAfterProgress);
    assert(CorridorReplanHysteresisPolicy::Assess(
        failure,17,11,0,existingGain,false,false,0,0,false).decision==
        CorridorReplanDecision::Escalate);
    assert(LocalRecoveryExhaustionPolicy::Assess(4,4,true,0,2)==
        LocalRecoveryExhaustionDecision::TryLastSafeBacktrack);
    assert(LocalRecoveryExhaustionPolicy::Assess(4,4,true,2,2)==
        LocalRecoveryExhaustionDecision::FailExhausted);

    // Native C++ cannot instantiate this injected Windows follower. Bind the
    // pure policy to the actual Start/replan/combat/exhaustion call sites.
    const auto source=Read("src/Navigation/GenericNavMeshPathFollower.h");
    assert(source.find("BeginSurfaceRecoveryAttempt(player,best,currentFinalDistance,\n                provenSteeringFailure)")!=std::string::npos);
    assert(source.find("SurfaceRecoveryEpisodePolicy::SameLocalTarget(")!=std::string::npos);
    assert(source.find("surfaceRecoveryAttempts_=MaximumSurfaceRecoveryAttempts")!=std::string::npos);
    assert(source.find("RegisterVerifiedSurfaceTransitionFailure(\n                    player,tick,\"repeated surface recovery")!=std::string::npos);
    assert(source.find("issuedFrom!=currentFrom || issuedTo!=currentTo")!=std::string::npos);
    assert(source.find("SurfaceRecoveryEpisodePolicy::Assess(\n                    surfaceEpisodeInitialDistance_,finalDistance")!=std::string::npos);
    assert(source.find("surfaceEpisodeForwardPortal_=true")!=std::string::npos);
    assert(source.find("surfaceEpisodeRouteRefreshes_")!=std::string::npos);
    const auto pause=source.substr(source.find("bool PauseForCombat("),
        source.find("bool ResumeAfterCombat(")-source.find("bool PauseForCombat("));
    assert(pause.find("surfaceEpisodeActive_ = false")==std::string::npos);
    assert(pause.find("surfaceRecoveryAttempts_ = 0")==std::string::npos);
    assert(pause.find("blockedTransitions_.clear()")==std::string::npos);
    assert(source.find("MaximumPathLength =\n            2000.0f")!=std::string::npos);
    assert(source.find("MaximumSurfaceRecoveryAttempts = 4")!=std::string::npos);
    assert(source.find("MaximumLastSafeBacktracks = 2")!=std::string::npos);
    assert(source.find("NAV RECOVERY EPISODE intent=")!=std::string::npos);
    assert(source.find("NAV RECOVERY RESET intent=")!=std::string::npos);
    assert(source.find("NAV RECOVERY ESCALATE intent=")!=std::string::npos);
}
