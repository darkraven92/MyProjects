#include "../src/Bot/IdleActivityPolicy.h"
#include <cassert>
#include <initializer_list>
using namespace Bot;
int main()
{
    IdleSample s; s.healthyWorld=true; s.reason=IdleReason::NoEligibleWork; s.provenSafeAction=true;
    IdleActivityPolicy p(100);
    assert(p.Update(s,0)==IdleDecision::None); assert(p.Update(s,99)==IdleDecision::None);
    assert(p.Update(s,100)==IdleDecision::Due);
    p.ActionIssued(100); assert(p.Update(s,101)==IdleDecision::None);
    assert(p.Update(s,199)==IdleDecision::None); assert(p.Update(s,200)==IdleDecision::Due);
    p.ActionBlocked(200); assert(p.Update(s,299)==IdleDecision::None);
    for(bool IdleSample::*field:{&IdleSample::combat,&IdleSample::navigation,&IdleSample::interaction,
        &IdleSample::dead,&IdleSample::vendor,&IdleSample::trainer,&IdleSample::looting,
        &IdleSample::stuckRecovery,&IdleSample::plannerOwner,&IdleSample::meaningfulActivity})
    {
        auto blocked=s; blocked.*field=true;
        assert(!IdleActivityPolicy::Safe(blocked)); assert(p.Update(blocked,1000)==IdleDecision::None);
        assert(p.Update(s,1001)==IdleDecision::None);
    }
    for(auto reason:{IdleReason::FaultedSubsystem,IdleReason::StuckSubsystem,IdleReason::EvidenceWait,
                     IdleReason::WorldUnavailable,IdleReason::ActiveOwner})
    { auto blocked=s; blocked.reason=reason; assert(!IdleActivityPolicy::Safe(blocked)); }
    p=IdleActivityPolicy(100); s.provenSafeAction=false;
    assert(p.Update(s,0)==IdleDecision::None);
    assert(p.Update(s,100)==IdleDecision::BlockedUnprovenAction);
    assert(p.Age(100)==100); // no invented activity from blocked action
    assert(p.Update(s,101)==IdleDecision::None);
    s.meaningfulActivity=true; assert(p.Update(s,102)==IdleDecision::None);
    s.meaningfulActivity=false; assert(p.Update(s,103)==IdleDecision::None);
    assert(p.Update(s,202)==IdleDecision::None);
    assert(p.Update(s,203)==IdleDecision::BlockedUnprovenAction);
}
