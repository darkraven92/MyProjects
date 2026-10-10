#include "../src/Bot/DeathRecoveryPolicy.h"
#include "../src/Bot/DeathRecoveryEvidencePolicy.h"
#include "../src/Bot/DeathRecoveryRepeatDeathPolicy.h"
#include "../src/Bot/LivingDeathRecoveryPolicy.h"
#include "../src/Bot/LivingWaterEmergencyPolicy.h"
#include "../src/Bot/AfkDeadGhostPolicy.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace Bot;
using P=LivingDeathRecoveryPolicy;
using A=P::Action;
static P::Sample Sample(std::uint64_t now)
{
    P::Sample s{};
    s.now=now; s.guid=42; s.map=1; s.identity=s.alive=s.evidenceKnown=s.movementKnown=true;
    s.hp=95; s.maxHp=100; return s;
}
static std::string Read(const char* file)
{
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream f(root/file); assert(f);
    return {std::istreambuf_iterator<char>(f),{}};
}
int main()
{
    // Historical reclaim eligibility is independent of living candidate geometry.
    assert(DeathRecoveryEvidencePolicy::CanReclaim(true,true,true,0,6.301f));
    assert(!DeathRecoveryEvidencePolicy::CanReclaim(true,false,true,0,6.301f));
    assert(!DeathRecoveryEvidencePolicy::CanReclaim(false,true,true,0,6.301f));
    assert(!DeathRecoveryPolicy::AliveAfterCorpseRun(true,false,50,100,true,true,1));
    assert(DeathRecoveryPolicy::AliveAfterCorpseRun(true,false,50,100,true,true,2));
    assert(!DeathRecoveryPolicy::AliveAfterCorpseRun(true,false,50,100,true,false,2));
    static_assert(DeathRecoveryLivenessPolicy::MaximumEpisodeAgeMs==300000);
    static_assert(DeathRecoveryLivenessPolicy::MaximumNoProgressAgeMs==180000);
    static_assert(DeathRecoveryPolicy::MaximumReleaseAttemptsPerCycle==6);
    static_assert(DeathRecoveryPolicy::MaximumRetrieveAttemptsPerCycle==8);

    DeathRecoveryRepeatDeathPolicy repeat;
    repeat.RecordAutomaticAlive(42,1,{0,0,0},1000);
    P living; assert(living.Begin(true,42,1,{0,0,0},1000));
    assert(living.Update(Sample(1001))==A::Hold && living.Owns());
    auto s=Sample(1100); s.aggressor=true;
    assert(living.Update(s)==A::Defense && living.Owns() && !living.Attempts());
    s=Sample(3200); assert(living.Update(s)==A::Plan);
    assert(living.PlanReady(true,{18,0,0}));
    s.now=3250; assert(living.Update(s)==A::Navigate);

    // Actual water policy preempts the living route; no budget or quiet-proof renewal.
    LivingWaterEmergencyPolicy water;
    WaterEgressSample ws{true,true,false,true,false,true,42,100,200,3200,0,0,0};
    water.Update(ws);
    ws.nowMs=3300; ws.x=2; ws.swimming=true;
    const auto entered=water.Update(ws);
    assert(entered.entered && entered.action==WaterEgressAction::Backtrack && water.Blocked());
    s.now=3300; s.water=true; s.aggressor=true;
    assert(living.Update(s)==A::Water && living.Attempts()==1 && living.Owns());
    for (unsigned i=0;i<3;++i)
    { ws.nowMs+=250; ws.swimming=false; water.Update(ws); }
    assert(!water.Blocked());
    s=Sample(5600); assert(living.Update(s)==A::Plan && living.Attempts()==2);
    assert(living.PlanReady(true,{18,0,0})); living.Arrived({18,0,0});
    s.position={18,0,0};
    for (unsigned i=0;i<2;++i) { s.now+=250; assert(living.Update(s)==A::Recover && living.Owns()); }
    s.now+=250; assert(living.Update(s)==A::Release && living.Owns());
    assert(living.Complete()); living.Reset();
    // Completed living egress never resets automatic-alive history.
    repeat.BeginDeath(42,1,9567);
    assert(repeat.ObserveCorpse({0,0,0}) && repeat.Latched());
    assert(!repeat.ShouldBlock(false) && repeat.ShouldBlock(true));
    for (unsigned i=0;i<100;++i) assert(repeat.ShouldBlock(true));
    repeat.Reset(); repeat.BeginDeath(42,1,10000); assert(!repeat.Latched());
    assert(!living.Begin(false,42,1,{},11000) && !living.Owns());

    // Each combination preserves normal-mode exclusion and the same absolute timeout.
    for (unsigned mask=0;mask<16;++mask)
    {
        living.Begin(true,42,1,{},1000);
        s=Sample(3000); s.aggressor=mask&1; s.defense=mask&2;
        s.water=mask&4; s.evidenceKnown=!(mask&8);
        const auto action=living.Update(s);
        const auto expected=s.water ? A::Water : (s.aggressor||s.defense) ? A::Defense :
            !s.evidenceKnown ? A::Hold : A::Plan;
        assert(action==expected && living.Owns());
        s.now=91000; living.Update(s);
        assert(living.Current()==P::State::Blocked && living.Owns());
        s=Sample(92000);
        for (unsigned i=0;i<100;++i) { ++s.now; assert(living.Update(s)==A::Block); }
        assert(!living.Complete());
    }

    // AFK owner guard remains authoritative for every active/terminal living state.
    AfkObservation observation{true,false,false,250000,1000,300000,"test"};
    AfkProtectionPolicy afk;
    AfkSafety safety{}; safety.healthyIdle=true; safety.recovery=true;
    assert(afk.Update(observation,safety,250000).action==AfkAction::None);
    safety.recovery=false;
    assert(afk.Update(observation,safety,250000).action==AfkAction::InputPulse);
    AfkDeadGhostPolicy ghost;
    observation.clientNow=310000;
    assert(std::strcmp(ghost.Blocker(AfkLifeState::Ghost,AfkDeathGap::Failed,observation),
        "dead_ghost_afk_recovery_not_qualified")==0);
    observation.clientNow=250000;
    assert(std::strcmp(ghost.Blocker(AfkLifeState::Ghost,AfkDeathGap::CommandInFlight,observation),
        "death_recovery_command_in_flight")==0);

    // Adapter/ownership sentinels supplement executable cross-policy scenarios above.
    const auto monitor=Read("src/Bot/WorldMonitor.h");
    const auto begin=monitor.find("if (livingDeathRecovery.Owns())\n");
    const auto end=monitor.find("continue; // even completion",begin);
    assert(begin!=std::string::npos && end!=std::string::npos);
    const auto held=monitor.substr(begin,end-begin);
    assert(held.find("livingAfkSafety.recovery=true")!=std::string::npos);
    for (const auto forbidden:{"grindMode.Update(","combat.Update(","questPlannerRuntime.Update("})
        assert(held.find(forbidden)==std::string::npos);
    assert(monitor.find("if (waterEmergency.Blocked())")<begin);
    const auto redeath=monitor.find("if (deathShouldOwn && livingDeathRecovery.Owns())");
    const auto redeathEnd=monitor.find("livingDeathRecovery.ObserveDeadline",redeath);
    const auto redeathBlock=monitor.substr(redeath,redeathEnd-redeath);
    assert(redeathBlock.find("livingDeathRecovery.YieldDeath(world.player)")!=std::string::npos);
    assert(redeathBlock.find("YieldWater")==std::string::npos);
    assert(monitor.find("!livingDeathRecovery.SameWorldIdentity(world)")!=std::string::npos);
    const auto gap=monitor.find("if (livingDeathRecovery.ConsumeEvidenceGap())");
    assert(gap!=std::string::npos && gap<monitor.find("const bool deathShouldOwn"));
    const auto owner=Read("src/Bot/LivingDeathRecoveryController.h");
    assert(owner.find("policy_.Observe(Sample(world,e,w))")!=std::string::npos);
    assert(owner.find("bool SameWorld(")!=std::string::npos);
    const auto handoff=owner.substr(owner.find("bool AllowsWaterHandoff"),400);
    assert(handoff.find("!Recovering()")==std::string::npos);
    assert(handoff.find("e.QuietObservation()")!=std::string::npos);
    assert(owner.find("repeatDeath_")==std::string::npos);
    const auto death=Read("src/Bot/DeathRecoveryController.h");
    assert(death.find("repeatDeath_.ShouldBlock(freshProbe")!=std::string::npos);
    assert(death.find("repeatDeath_.Reset();\n                            FinalizeAliveEpisode")!=std::string::npos);
}
