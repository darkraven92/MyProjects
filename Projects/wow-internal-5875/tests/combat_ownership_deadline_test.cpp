#include "../src/Bot/CombatOwnershipDeadlinePolicy.h"
#include "../src/Bot/CombatBootstrapVerificationPolicy.h"
#include "../src/Bot/CombatDefensiveContainmentPolicy.h"
#include "../src/Bot/LivingDeathRecoveryPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace Bot;
static CombatLivenessSample Sample(std::uint64_t now)
{
    CombatLivenessSample s{};
    s.nowMs=s.sampleTick=now; s.targetGuid=s.selectedGuid=7; s.targetObject=19;
    s.targetHp=s.playerHp=100; s.attackPeriodMs=2000;
    s.fresh=s.alive=s.targetValid=s.selectionKnown=s.melee=s.facing=true;
    s.actionKnown=s.attackActive=true;
    return s; // readable Attack, unknown full input; no repair permission
}
static std::string Read(const char* path)
{
    std::ifstream f(std::filesystem::path(__FILE__).parent_path().parent_path()/path);
    assert(f); return {std::istreambuf_iterator<char>(f),{}};
}
int main()
{
    CombatOwnershipDeadlinePolicy deadline;
    CombatBootstrapVerificationPolicy bootstrap;
    CombatLivenessPolicy liveness;
    // A confirmed bootstrap does not prove subsequent offensive progress.
    auto s=Sample(1000);
    bootstrap.Begin(7,1000,2000,true);
    assert(bootstrap.Observe(s,false)==CombatBootstrapResult::Confirmed);
    assert(!deadline.Observe(s)); liveness.Observe(s);
    for (unsigned i=1250; i<41000; i+=250)
    {
        s=Sample(i); s.playerHp=90;
        assert(liveness.Observe(s).action==CombatRecoveryAction::None);
        assert(!deadline.Observe(s));
    }
    assert(deadline.Observe(Sample(41000)));
    // Unknown/stale/cast/selection/facing/chase/object churn must not refund
    // the deadline or authorize unsafe offense. No new navigation owner.
    for (unsigned kind=0; kind<9; ++kind)
    {
        deadline.Reset(); assert(!deadline.Observe(Sample(1000)));
        for (unsigned i=1250; i<=41000; i+=250)
        {
            s=Sample(i);
            if (kind==0) s.fresh=false;
            if (kind==1) s.selectionKnown=false;
            if (kind==2) s.selectedGuid=8;
            if (kind==3) s.actionWait=true;
            if (kind==4) s.actionKnown=false;
            if (kind==5) s.facing=false;
            if (kind==6) s.melee=false;
            if (kind==7) { s.targetObject=i; s.targetHp=99; }
            if (kind==8) s.sampleTick=1000;
            assert(deadline.Observe(s)==(i==41000));
        }
    }
    // Fresh selected HP progress is the only ordinary renewal. Health after a
    // gap or object change is a new baseline, never borrowed damage proof.
    for (unsigned kind=0; kind<3; ++kind)
    {
        deadline.Reset(); deadline.Observe(Sample(1000));
        s=Sample(39000);
        if (kind==1) s.fresh=false;
        if (kind==2) s.targetObject=20;
        deadline.Observe(s);
        s=Sample(40000); s.targetHp=90;
        assert(!deadline.Observe(s));
        s.nowMs=s.sampleTick=41000;
        assert(deadline.Observe(s)==(kind!=0));
        if (!kind)
        {
            s.nowMs=s.sampleTick=79999; assert(!deadline.Observe(s));
            s.nowMs=s.sampleTick=80000; assert(deadline.Observe(s));
        }
    }
    deadline.Reset(); assert(!deadline.Observe(Sample(1000)));
    assert(deadline.Observe(Sample(41000))); // also bounds a stale initial chase/facing owner
    deadline.Reset(); assert(!deadline.Observe(Sample(100000)));
    assert(deadline.Observe(Sample(99999))); // monotonic-clock discontinuity
    deadline.Reset(); deadline.Observe(Sample(1000));
    deadline.Pause(40000); // external living-water owner, no elapsed offense borrowed
    assert(!deadline.Observe(Sample(79999)));
    assert(deadline.Observe(Sample(80000)));
    for (unsigned kind=0; kind<3; ++kind)
    {
        deadline.Reset(); deadline.Observe(Sample(1000));
        s=Sample(50000);
        if (kind==0) s.alive=false;
        if (kind==1) s.targetValid=false;
        if (kind==2) s.targetGuid=8;
        assert(!deadline.Observe(s));
    }

    // Terminal combat failure cannot release the living recovery owner.
    CombatDefensiveContainmentPolicy containment;
    containment.Begin(7,100,41000);
    DefensiveContainmentSample c{};
    c.nowMs=41001; c.guid=7; c.targetHp=100;
    c.playerAlive=c.targetValid=c.routeFailed=true;
    assert(containment.Observe(c).action==DefensiveContainmentAction::Fail);
    LivingDeathRecoveryPolicy living;
    assert(living.Begin(true,7,1,{},1000));
    living.Block("defense_terminal_failure");
    assert(living.Owns() && !living.Complete());

    const auto source=Read("src/Bot/CombatController.h");
    const auto observe=source.find("const bool ownershipExpired=ownershipDeadline_.Observe");
    const auto terminal=source.find("return ResolveMeleeTerminal(world,target,s,tick);",observe);
    assert(observe!=std::string::npos && terminal!=std::string::npos);
    assert(source.find("meleeDecision_.action=CombatRecoveryAction::Fail;",observe)<terminal);
    assert(source.find("ownershipDeadline_.Reset();")!=std::string::npos);
    assert(source.find("ownershipDeadline_.Pause(GetTickCount64());")!=std::string::npos);
    assert(source.find("terminalSample.actionWait=attack.waiting;")!=std::string::npos);
    const auto failure=source.find("Fail(std::string(\"defensive_containment_exhausted:\")");
    assert(source.substr(failure,250).find("attack.known && attack.inputSafe && !attack.waiting")!=std::string::npos);
    const auto owner=Read("src/Bot/LivingDeathRecoveryController.h");
    assert(owner.find("if (combat.Failed()) policy_.Block(\"defense_terminal_failure\");")!=std::string::npos);
}
