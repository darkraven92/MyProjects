#include "../src/Bot/LivingDeathRecoveryPolicy.h"
#include "../src/Bot/DeathRecoveryRepeatDeathPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

using P=Bot::LivingDeathRecoveryPolicy;
using A=P::Action;
static P::Sample Sample(std::uint64_t now)
{
    P::Sample s{};
    s.now=now; s.guid=42; s.map=1; s.identity=s.alive=s.evidenceKnown=s.movementKnown=true;
    s.hp=95; s.maxHp=100; s.position={0,0,0}; return s;
}
static P Begin()
{
    P p;
    assert(p.Begin(true,42,1,{0,0,0},0));
    assert(p.Owns() && p.Current()==P::State::Waiting);
    return p;
}
static void Reach(P& p)
{
    assert(p.Update(Sample(2000))==A::Plan);
    assert(p.PlanReady(true,{18,0,0}));
    assert(p.Update(Sample(2001))==A::Navigate);
    p.Arrived({18,0,0});
    assert(p.Current()==P::State::Settling);
}
static std::string Read(const char* name)
{
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream f(root/name); assert(f);
    return {std::istreambuf_iterator<char>(f),{}};
}
int main()
{
    // Automatic completion holds normal ownership, never grants an immediate pull.
    auto p=Begin();
    assert(p.Update(Sample(1))==A::Hold);
    Reach(p);
    auto s=Sample(2002); s.position={18,0,0}; s.hp=50;
    assert(p.Update(s)==A::Hold); // damage first requires a renewed quiet interval
    s.now=4002; assert(p.Update(s)==A::Recover);
    s.hp=95; s.now=4252;
    assert(p.Update(s)==A::Recover);
    assert(p.Update(s)==A::Recover); // duplicate read is not another proof
    s.now+=250; assert(p.Update(s)==A::Recover);
    s.now+=250; assert(p.Update(s)==A::Release);
    assert(p.Owns()); // release is not committed before the final command guard
    assert(p.Complete());
    assert(!p.Owns() && p.Update(s)==A::None); // exactly one release

    p=Begin(); Reach(p); s=Sample(3000); s.position={18,0,0};
    p.Update(s); s.now+=250; p.Update(s); s.now+=250;
    assert(p.Update(s)==A::Release);
    p.WorldGap(); assert(!p.Complete() && p.Owns());

    // Positive aggression/native combat/active defense always preempt egress.
    for (unsigned signal=0; signal<3; ++signal)
    {
        p=Begin(); s=Sample(2000);
        s.aggressor=signal==0; s.combat=signal==1; s.defense=signal==2;
        assert(p.Update(s)==A::Defense && p.Attempts()==0 && p.Owns());
        s=Sample(2100); assert(p.Update(s)==A::Hold);
    }
    // Water wins over defense and never changes the automatic budget or permits release.
    p=Begin(); s=Sample(2000); s.water=s.aggressor=true;
    assert(p.Update(s)==A::Water && p.Attempts()==0);
    s.now=P::DeadlineMs; assert(p.Update(s)==A::Water);
    assert(p.Current()==P::State::Blocked);
    s.water=false; assert(p.Update(s)==A::Defense); // terminal interlock still permits defense
    s.aggressor=false; assert(p.Update(s)==A::Block);

    // Each failed candidate consumes a finite slot, including partial/projection failures.
    p=Begin();
    for (unsigned i=0;i<P::MaximumCandidates;++i)
    {
        assert(p.Update(Sample(2000+i))==A::Plan);
        assert(!p.PlanReady(false,{18,0,0}));
    }
    assert(p.Current()==P::State::Blocked && p.Owns());
    for (unsigned i=0;i<100;++i) assert(p.Update(Sample(3000+i))==A::Block);
    assert(p.Attempts()==P::MaximumCandidates);
    p=Begin(); assert(p.Update(Sample(2000))==A::Plan);
    assert(!p.PlanReady(true,{2,0,0})); // projected near corpse is not egress

    // Replan failure, repeated preemption and timeout cannot reset the episode.
    p=Begin(); assert(p.Update(Sample(2000))==A::Plan);
    assert(p.PlanReady(true,{18,0,0})); p.RouteFailed();
    assert(p.Attempts()==1 && p.Update(Sample(3000))==A::Plan);
    p.Interrupt(); assert(p.Attempts()==2);
    assert(p.Update(Sample(P::DeadlineMs))==A::Block);
    assert(p.Current()==P::State::Blocked);
    p=Begin();
    for (unsigned i=0;i<P::MaximumCandidates;++i)
    {
        assert(p.Update(Sample(2000+i))==A::Plan);
        assert(p.PlanReady(true,{18,0,0}));
        p.RouteFailed(); // execution failure/replan cap consumes the same finite budget
    }
    assert(p.Update(Sample(3000))==A::Block && p.Attempts()==P::MaximumCandidates);
    p=Begin(); s=Sample(P::DeadlineMs); s.alive=false;
    assert(p.Update(s)==A::Block); // missing life evidence cannot hide expiration

    // HP loss is pressure without attributing its cause; a quiet interval is required again.
    p=Begin(); assert(p.Update(Sample(100))==A::Hold);
    s=Sample(2100); s.hp=94;
    assert(p.Update(s)==A::Hold && p.Attempts()==0);
    s.now=4099; assert(p.Update(s)==A::Hold);
    s.now=4100; assert(p.Update(s)==A::Plan);
    // Unknown/partial observations cannot prove disengagement or release.
    p=Begin(); s=Sample(2000); s.evidenceKnown=false;
    assert(p.Update(s)==A::Hold);
    s=Sample(2100); assert(p.Update(s)==A::Hold);
    s=Sample(P::DeadlineMs); s.movementKnown=false; assert(p.Update(s)==A::Block);

    // World loss, foreign identity and clock rollback all fail closed.
    p=Begin(); Reach(p); p.WorldGap();
    s=Sample(3000); s.position={18,0,0};
    assert(p.Update(s)==A::Block);
    p=Begin(); s=Sample(2000); s.guid=43; assert(p.Update(s)==A::Block);
    p=Begin(); s=Sample(2000); s.map=2; assert(p.Update(s)==A::Block);
    p=Begin(); p.Update(Sample(1000)); assert(p.Update(Sample(999))==A::Block);
    p=Begin(); Reach(p); assert(p.Update(Sample(3000))==A::Block); // returned beside corpse
    p=Begin(); s=Sample(2000); s.position.x=std::numeric_limits<float>::quiet_NaN();
    assert(p.Update(s)==A::Hold);

    // Manual-alive completion cannot inherit automatic living ownership/history.
    p=Begin(); assert(!p.Begin(false,42,1,{0,0,0},4000));
    assert(!p.Owns() && p.Update(Sample(5000))==A::None);
    Bot::DeathRecoveryRepeatDeathPolicy repeat;
    repeat.RecordAutomaticAlive(42,1,{0,0,0},1000);
    p=Begin(); p.WorldGap(); // separate ownership object cannot mutate the repeat policy
    repeat.BeginDeath(42,1,9567);
    assert(repeat.ObserveCorpse({0,0,0}) && repeat.AgeAtDeathMs()==8567);
    assert(!repeat.ShouldBlock(false) && repeat.ShouldBlock(true));
    repeat.Reset(); repeat.BeginDeath(42,1,10000); assert(!repeat.ObserveCorpse({0,0,0}));

    // Production wiring sentinels supplement (do not replace) the action-policy tests.
    const auto monitor=Read("src/Bot/WorldMonitor.h");
    const auto branch=monitor.find("if (livingDeathRecovery.Owns())\n");
    const auto end=monitor.find("continue; // even completion",branch);
    assert(branch!=std::string::npos && end!=std::string::npos);
    const auto body=monitor.substr(branch,end-branch);
    assert(body.find("grindMode.Update(")==std::string::npos);
    assert(body.find("combat.Update(")==std::string::npos);
    assert(branch<monitor.find("questPlannerRuntime.Update("));
    assert(monitor.find("if (waterEmergency.Blocked())")<branch);
    assert(monitor.find("livingDeathRecovery.InvalidateWorld();")!=std::string::npos);
    assert(monitor.find("deathRecovery.AutomaticCompletion()")!=std::string::npos);
    assert(monitor.find("if (automatic)\n")<monitor.find("livingDeathRecovery.Begin("));
    const auto combat=Read("src/Bot/CombatController.h");
    const auto wrapper=combat.substr(combat.find("void UpdateLivingDefense"),1800);
    assert(wrapper.find("AdoptExactTargetForDefense")!=std::string::npos);
    assert(wrapper.find("AdoptSelectedTargetForDefense")==std::string::npos);
    assert(combat.find("if (livingDefenseOnly_)\n            {\n                if (killed")!=std::string::npos);
    const auto controller=Read("src/Bot/LivingDeathRecoveryController.h");
    assert(controller.find("GhostDeathRecovery")==std::string::npos);
    assert(controller.find("WaterTraversalMode::AvoidUntilQualified")!=std::string::npos);
    assert(controller.find("PlanningOnlyReachedDestination()")!=std::string::npos);
    assert(controller.find("route_->LifetimeReplans()>=int(P::MaximumReplans)")!=std::string::npos);
    const auto follower=Read("src/Navigation/GenericNavMeshPathFollower.h");
    assert(follower.find("int LifetimeReplans() const { return totalReplans_; }")!=std::string::npos);
    assert(controller.find("bool CommandWorld")!=std::string::npos);
    assert(controller.find("{ Stop(world.player); policy_.Interrupt();")==std::string::npos);
    assert(controller.find("if (same && e.Living()) Stop(world.player);")!=std::string::npos);
}
