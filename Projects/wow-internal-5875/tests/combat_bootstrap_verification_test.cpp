#include "../src/Bot/CombatBootstrapVerificationPolicy.h"
#include "../src/Bot/CombatInitiationPolicy.h"
#include "../src/Bot/CombatDefensiveContainmentPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace Bot;
using R=CombatBootstrapResult;
static CombatLivenessSample Sample(std::uint64_t now)
{
    CombatLivenessSample s{};
    s.nowMs=s.sampleTick=now; s.targetGuid=s.selectedGuid=7; s.targetObject=19;
    s.targetHp=100; s.playerHp=558; s.attackPeriodMs=2000;
    s.fresh=s.alive=s.targetValid=s.selectionKnown=s.melee=s.facing=true;
    return s; // Full probe unknown_frame_iteration_limit: no input permission.
}
int main()
{
    CombatBootstrapVerificationPolicy start;
    CombatLivenessPolicy watchdog;
    auto s=Sample(1000);
    watchdog.Observe(s);
    watchdog.Dispatched(CombatRecoveryAction::ReengageAttack,s.nowMs);
    start.Begin(s.targetGuid,s.nowMs,s.attackPeriodMs,true);
    // Replay both endurance stalls: valid selected melee target, no damage,
    // unknown full action evidence, decreasing player HP. Ordinary watchdog
    // cannot authorize a repair; bootstrap still transfers ownership at 8 s.
    for (unsigned ms=1250;ms<9000;ms+=250)
    {
        s=Sample(ms); s.playerHp=300;
        const auto d=watchdog.Observe(s);
        assert(d.action==CombatRecoveryAction::None);
        assert(start.Observe(s,d.damageObserved)==R::Pending);
    }
    s=Sample(9000);
    assert(start.Observe(s,watchdog.Observe(s).damageObserved)==R::Failed);
    assert(watchdog.Repairs()==1); // Neither failure nor structural time refunds a command.
    for (unsigned i=0;i<100;++i)
    { ++s.nowMs; assert(start.Observe(s,false)==R::None); }
    // Existing containment receives the failure, retaining its own finite
    // deadline and input-conflict failure. It cannot silently resume Grind.
    CombatDefensiveContainmentPolicy containment;
    containment.Begin(7,100,9000);
    DefensiveContainmentSample c{};
    c.nowMs=9001; c.guid=7; c.targetHp=100;
    c.playerAlive=c.targetValid=c.evidenceKnown=c.hostileEngaged=c.routeFailed=true;
    assert(containment.Observe(c).action==DefensiveContainmentAction::Fail);

    // Damage and qualified latch readback independently confirm a dispatch.
    for (bool damage:{false,true})
    {
        start.Begin(7,1000,2000,true); s=Sample(1250);
        s.actionKnown=s.attackActive=!damage;
        assert(start.Observe(s,damage)==R::Confirmed);
        s.nowMs=100000; assert(start.Observe(s,false)==R::None);
    }
    // Unknown/stale/foreign selection cannot confirm; none extends the bound.
    for (unsigned kind=0;kind<4;++kind)
    {
        start.Begin(7,1000,2000,true); s=Sample(1250);
        s.actionKnown=s.attackActive=true;
        if (kind==0) s.fresh=false;
        if (kind==1) s.selectionKnown=false;
        if (kind==2) s.selectedGuid=8;
        if (kind==3) s.actionKnown=false;
        assert(start.Observe(s,false)==R::Pending);
        s.nowMs=9000; assert(start.Observe(s,false)==R::Failed);
    }
    start.Begin(7,1000,5000,true); s=Sample(9000);
    assert(start.Observe(s,false)==R::Pending);
    s.attackPeriodMs=60000; s.nowMs=12000; // Cannot move the dispatch-time deadline.
    assert(start.Observe(s,false)==R::Failed);
    start.Begin(7,1000,2000,false); s=Sample(1001);
    assert(start.Observe(s,true)==R::Failed); // Rejection is not success, even with damage.
    start.Begin(7,1000,2000,true); s=Sample(999);
    assert(start.Observe(s,false)==R::Failed);
    for (unsigned kind=0;kind<3;++kind)
    {
        start.Begin(7,1000,2000,true); s=Sample(1250);
        if (kind==0) s.alive=false;
        if (kind==1) s.targetValid=false;
        if (kind==2) s.targetGuid=8;
        assert(start.Observe(s,false)==R::None);
        assert(start.Observe(Sample(100000),false)==R::None);
    }
    start.Begin(7,1000,2000,true); start.Reset();
    assert(start.Observe(Sample(100000),false)==R::None);

    // A cast/modal wait is still a command prohibition. Bounded failure does
    // not make the alternate probe a general latch-repair/release permission.
    CombatInitiationEvidence e{};
    e.targetFresh=e.selectionKnown=e.selectionMatches=true;
    e.actionKnown=e.actionInputSafe=e.actionWait=true;
    e.alternateKnown=e.alternateInputSafe=true;
    assert(!CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e).offensiveInputAllowed);

    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream f(root/"src/Bot/CombatController.h"); assert(f);
    const std::string source(std::istreambuf_iterator<char>(f),{});
    const auto observe=source.find("bootstrapVerification_.Observe(s,meleeDecision_.damageObserved)");
    const auto fail=source.find("else if (bootstrapResult==CombatBootstrapResult::Failed)",observe);
    const auto terminal=source.find("return ResolveMeleeTerminal(world,target,s,tick);",fail);
    assert(observe!=std::string::npos && fail!=std::string::npos && terminal!=std::string::npos);
    assert(source.find("meleeDecision_.action=CombatRecoveryAction::Fail;",fail)<terminal);
    assert(source.find("bootstrapVerification_.Begin(target.guid,GetTickCount64(),")!=std::string::npos);
    assert(source.find("bootstrapVerification_.Reset();")!=std::string::npos);
}
