#include "../src/Bot/CombatGeometry.h"
#include "../src/Bot/CombatFacingPolicy.h"
#include "../src/Bot/MovementCommandTrace.h"
#include "../src/Bot/CombatTargetConsistencyPolicy.h"
#include <cassert>
#include <limits>
int main()
{
    using G = Bot::CombatGeometry;
    assert(std::fabs(G::SignedError(0,0)) < 0.0001f);
    assert(std::fabs(G::SignedError(0,0.5f)-0.5f) < 0.0001f);
    assert(std::fabs(std::fabs(G::SignedError(0,G::Pi))-G::Pi) < 0.0001f);
    assert(G::SignedError(6.2f,0.1f)>0);
    assert(G::SignedError(0.1f,6.2f)<0);
    assert(std::fabs(G::Normalize(-0.2f)-(2*G::Pi-0.2f))<0.0001f);
    assert(std::isnan(G::Normalize(std::numeric_limits<float>::infinity())));
    assert(!Bot::CombatFacingPolicy::IsAbilityFacingReady(G::SignedError(0, NAN)));
    Bot::MovementCommandTrace trace;
    trace.Observe("navigation",10,1,2,3);
    trace.Observe("combat_chase",11,1,2,3);
    assert(trace.serial==2 && trace.writer=="combat_chase" && trace.issuedMs==11);
    // Observations do not suppress an identical liveness refresh.
    trace.Observe("combat_chase",12,1,2,3);
    assert(trace.serial==3);
    static_assert(Bot::CombatFacingPolicy::StableSnapshotsRequired==2);
    static_assert(Bot::CombatFacingPolicy::AbilityToleranceRadians==0.10f);
    static_assert(Bot::CombatTargetConsistencyPolicy::ChaseMatches(1,1));
    static_assert(!Bot::CombatTargetConsistencyPolicy::ChaseMatches(1,2));
    static_assert(!Bot::CombatTargetConsistencyPolicy::ChaseMatches(0,0));
    // Defensive switch must converge on the new GUID, not the old chase.
    static_assert(!Bot::CombatTargetConsistencyPolicy::ChaseMatches(2,1));
    static_assert(Bot::CombatTargetConsistencyPolicy::ChaseMatches(2,2));
}
