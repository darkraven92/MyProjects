#include "../src/Bot/CombatTerminalPolicy.h"
#include <cassert>
using namespace Bot;
static CombatTerminalSample Sample(std::uint64_t now=1)
{
    CombatTerminalSample s{};
    s.nowMs=now; s.targetGuid=s.selectedGuid=s.serverVictimGuid=7;
    s.playerHp=100; s.optionalGrind=s.known=s.inputSafe=s.attackKnown=true;
    s.hostileEngaged=false; s.attackActive=true;
    return s;
}
int main()
{
    CombatTerminalPolicy p;
    assert(p.Observe(Sample()).action==CombatTerminalAction::Observe);
    auto s=Sample(1001);
    assert(p.Observe(s).action==CombatTerminalAction::StopAndClearOwnTarget);
    p.Dispatched(s.nowMs);
    s=Sample(1251);
    assert(p.Observe(s).action==CombatTerminalAction::Observe); // dispatch != release
    s.selectedGuid=s.serverVictimGuid=0; s.attackActive=false;
    assert(p.Observe(s).action==CombatTerminalAction::Abandoned);
    for (int guard=0; guard<7; ++guard)
    {
        p.Reset(); s=Sample();
        if (guard==0) s.optionalGrind=false; // mandatory quest
        if (guard==1) s.known=false; // world/death evidence gap
        if (guard==2) s.hostileEngaged=true; // target or another aggressor
        if (guard==3) s.inputSafe=false; // dialog/vendor/loading/keyboard owner
        if (guard==4) s.playerHp=0;
        if (guard==5) s.selectedGuid=42; // never clear another owner's selection
        if (guard==6) s.attackKnown=false;
        assert(p.Observe(s).action==CombatTerminalAction::SystemFail);
    }
    p.Reset(); p.Observe(Sample()); s=Sample(1001); s.playerHp=99;
    assert(p.Observe(s).action==CombatTerminalAction::SystemFail);
    p.Reset(); p.Observe(Sample()); s=Sample(251); s.playerHp=110;
    assert(p.Observe(s).action==CombatTerminalAction::Observe);
    s=Sample(501); s.playerHp=105;
    assert(p.Observe(s).action==CombatTerminalAction::SystemFail); // healed then hit
    p.Reset(); p.Observe(Sample()); s=Sample(1001); s.targetGuid=8;
    assert(p.Observe(s).action==CombatTerminalAction::SystemFail);
    p.Reset(); p.Observe(Sample()); p.Dispatched(1001); s=Sample(2001);
    assert(p.Observe(s).action==CombatTerminalAction::SystemFail); // bounded, no spam
    // Each release postcondition is independently required.
    for (int field=0; field<3; ++field)
    {
        p.Reset(); p.Observe(Sample()); p.Dispatched(1001); s=Sample(1251);
        s.selectedGuid=s.serverVictimGuid=0; s.attackActive=false;
        if (field==0) s.selectedGuid=7;
        if (field==1) s.serverVictimGuid=7;
        if (field==2) s.attackActive=true;
        assert(p.Observe(s).action==CombatTerminalAction::Observe);
    }
    p.Reset(); // re-arm is an explicit controller boundary, not synthetic success
    assert(p.Observe(Sample()).action==CombatTerminalAction::Observe);

    // A mandatory objective receives a typed owner failure only after the
    // same safe disengagement and post-command proof; it is never blacklisted
    // as an optional Grind target or credited as a kill.
    p.Reset(); s=Sample(); s.optionalGrind=false; s.mandatoryObjective=true;
    assert(p.Observe(s).action==CombatTerminalAction::Observe);
    s.nowMs=1001;
    assert(p.Observe(s).action==CombatTerminalAction::StopAndClearOwnTarget);
    p.Dispatched(s.nowMs);
    s.nowMs=1251; s.selectedGuid=s.serverVictimGuid=0; s.attackActive=false;
    assert(p.Observe(s).action==CombatTerminalAction::OwnerFailure);
    p.Reset(); s=Sample(); s.optionalGrind=false; s.mandatoryObjective=true;
    s.hostileEngaged=true;
    assert(p.Observe(s).action==CombatTerminalAction::SystemFail);

    // An already stopped own attack after verified defensive disengagement
    // needs no duplicate clear command and never credits a kill.
    p.Reset(); s=Sample(); s.selectedGuid=s.serverVictimGuid=0;
    s.attackActive=false;
    assert(p.Observe(s).action==CombatTerminalAction::Observe);
    s.nowMs=1001;
    assert(p.Observe(s).action==CombatTerminalAction::Abandoned);
    p.Reset(); s=Sample(); s.optionalGrind=false; s.mandatoryObjective=true;
    s.selectedGuid=s.serverVictimGuid=0; s.attackActive=false;
    p.Observe(s); s.nowMs=1001;
    assert(p.Observe(s).action==CombatTerminalAction::OwnerFailure);
}
