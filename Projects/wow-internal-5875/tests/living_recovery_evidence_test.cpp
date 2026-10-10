#include "../src/Bot/LivingRecoveryEvidence.h"
#include "../src/Bot/LivingAttackEvidenceScript.h"
#include "../src/Bot/LivingDeathRecoveryPolicy.h"
#include <cassert>
#include <iostream>
#include <string_view>

using E=Bot::LivingRecoveryEvidence;
using P=Bot::LivingDeathRecoveryPolicy;
using A=P::Action;
static E Complete()
{
    E e;
    e.identity=e.alive=e.known=e.healthKnown=e.lifeKnown=e.combatKnown=true;
    e.scanComplete=e.attackKnown=true; e.hp=95; e.maxHp=100;
    return e;
}
static P::Sample Sample(std::uint64_t now,const E& e)
{
    P::Sample s{};
    s.now=now; s.guid=42; s.map=1; s.identity=e.identity; s.alive=e.Living();
    s.evidenceKnown=e.Complete(); s.combat=e.Positive(); s.aggressor=e.aggressor;
    s.hp=e.healthKnown ? e.hp : 0; s.maxHp=e.healthKnown ? e.maxHp : 0;
    s.movementKnown=true; return s;
}
int main(int argc,char** argv)
{
    if (argc==2 && std::string_view(argv[1])=="--lua")
    { std::cout<<Bot::LivingAttackEvidenceScript; return 0; }
    E unknown;
    assert(!unknown.Living() && !unknown.Complete() && !unknown.QuietObservation());
    assert(unknown.Danger()==Bot::LivingDanger::Unknown);
    auto e=Complete();
    assert(e.QuietObservation() && e.Danger()==Bot::LivingDanger::Unknown); // never Safe
    for (unsigned signal=0;signal<3;++signal)
    {
        e=Complete(); e.combat=signal==0; e.aggressor=signal==1; e.attackActive=signal==2;
        assert(e.Positive() && !e.QuietObservation());
        assert(e.Danger()==Bot::LivingDanger::Observed);
        P p; p.Begin(true,42,1,{},0);
        assert(p.Update(Sample(2000,e))==A::Defense && p.Owns() && !p.Attempts());
        // An unrelated incomplete field cannot erase an independently positive signal.
        e.scanComplete=false; assert(e.Positive() && !e.Complete());
        e.combatKnown=false;
        if (signal!=0) assert(e.Positive());
    }
    e=Complete(); e.combat=true; e.healthKnown=false;
    assert(e.Positive() && !e.Living() && !e.QuietObservation()); // observe, cannot command
    e=Complete(); e.combatKnown=false; e.aggressor=true;
    assert(e.Living() && e.Positive() && !e.Complete()); // defense can still adopt exact attacker
    e=Complete(); e.attackKnown=false; e.attackActive=true;
    assert(!e.Positive() && !e.QuietObservation()); // stale default/unknown action never a false proof
    e=Complete(); e.identity=false; e.aggressor=true;
    assert(!e.Positive() && !e.Living());

    Bot::LivingRecoveryIdentity identity{42,100,200,300};
    assert(identity.Matches(identity));
    for (unsigned field=0;field<4;++field)
    {
        auto changed=identity;
        if (field==0) ++changed.guid;
        if (field==1) ++changed.manager;
        if (field==2) ++changed.player;
        if (field==3) ++changed.descriptors;
        assert(!identity.Matches(changed));
    }
    assert(!identity.Matches({}) && !Bot::LivingRecoveryIdentity{}.Valid());

    // Pressure observed only at a command gate cannot vanish with the next tick's heal.
    P p; p.Begin(true,42,1,{},0);
    e=Complete(); assert(p.Update(Sample(2000,e))==A::Plan);
    e.hp=94; assert(!p.Observe(Sample(2001,e)) && p.Attempts()==1);
    e.hp=95; assert(p.Update(Sample(2100,e))==A::Hold);
    assert(p.Update(Sample(4000,e))==A::Hold);
    assert(p.Update(Sample(4001,e))==A::Plan && p.Attempts()==2);
    e.attackActive=true; assert(!p.Observe(Sample(4002,e)));
    e.attackActive=false; assert(p.Update(Sample(4100,e))==A::Hold);
    assert(p.Update(Sample(6002,e))==A::Plan);
    e.attackKnown=false; assert(!p.Observe(Sample(6003,e)));
    e.attackKnown=true; assert(p.Update(Sample(6100,e))==A::Hold);
    assert(p.Update(Sample(8003,e))==A::Plan && p.Attempts()==4);
    assert(p.PlanReady(true,{18,0,0})); p.Arrived({18,0,0});
    auto s=Sample(9000,e); s.position={18,0,0};
    for (unsigned i=0;i<20;++i) { s.now+=250; assert(p.Observe(s)); }
    assert(!p.Complete()); // read-only command/water observations never manufacture three proofs
    for (unsigned i=0;i<2;++i) { s.now+=250; assert(p.Update(s)==A::Recover); }
    s.now+=250; assert(p.Update(s)==A::Release && p.Complete());

    p.Begin(true,42,1,{},0); e=Complete();
    assert(!p.Observe(Sample(P::DeadlineMs,e)) && p.Current()==P::State::Blocked);
    for (unsigned i=0;i<100;++i) assert(!p.Observe(Sample(P::DeadlineMs+i,e)));
    assert(!p.Attempts() && !p.Complete());
}
