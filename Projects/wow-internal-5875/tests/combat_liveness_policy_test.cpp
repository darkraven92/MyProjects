#include "../src/Bot/CombatLivenessPolicy.h"
#include "../src/Bot/CombatActionEvidenceScript.h"
#include <cassert>
#include <iostream>
#include <string_view>

using Bot::CombatLivenessPolicy;
using Bot::CombatLivenessSample;
using Bot::CombatRecoveryAction;
using Bot::CombatStallClass;

static CombatLivenessSample Sample(std::uint64_t ms = 1)
{
    CombatLivenessSample s{};
    s.nowMs = ms; s.sampleTick = ms; s.targetGuid = 7; s.targetObject = 19;
    s.targetHp = 100; s.playerHp = 100; s.attackPeriodMs = 2000;
    s.fresh = true; s.alive = true; s.targetValid = true;
    s.selectionKnown = true; s.selectedGuid = 7;
    s.melee = true; s.facing = true; s.inputSafe = true;
    s.actionKnown = true; s.attackActive = true;
    return s;
}
int main(int argc, char** argv)
{
    if (argc==2 && std::string_view(argv[1])=="--lua")
    { std::cout<<Bot::CombatActionEvidenceScript; return 0; }
    auto s = Sample(); CombatLivenessPolicy p;
    assert(p.Observe(s).action == CombatRecoveryAction::None);
    s = Sample(8001); s.targetHp = 90;
    assert(p.Observe(s).action == CombatRecoveryAction::None);
    assert(p.NoDamageMs(s.nowMs) == 0);
    s = Sample(9001); s.targetHp = 90; s.selectedGuid = 0;
    auto d = p.Observe(s);
    assert(d.classification == CombatStallClass::TargetSelectionDesync);
    assert(d.action == CombatRecoveryAction::RestoreTarget);
    p.Dispatched(d.action, s.nowMs);
    s.nowMs = s.sampleTick = 9251;
    assert(p.Observe(s).action == CombatRecoveryAction::None); // dispatch != restore
    s.selectedGuid = 7; s.nowMs = s.sampleTick = 9501;
    assert(p.Observe(s).verified == CombatRecoveryAction::RestoreTarget);
    assert(p.NoDamageMs(s.nowMs) == 1500); // target restore isn't damage
    s.targetGuid = 8; s.nowMs = s.sampleTick = 10001;
    assert(p.Observe(s).action == CombatRecoveryAction::RestoreTarget);
    s = Sample(); p.Reset(); p.Observe(s);
    s = Sample(8001); s.fresh = false;
    assert(p.Observe(s).classification == CombatStallClass::UnknownOrStale);
    s = Sample(8002);
    assert(p.Observe(s).action == CombatRecoveryAction::None); // stale gap rebaselines
    for (int kind = 0; kind < 4; ++kind)
    {
        p.Reset(); p.Observe(Sample()); s = Sample(9001);
        if (kind == 0) s.facing = false;
        if (kind == 1) s.melee = false;
        if (kind == 2) s.actionWait = true;
        if (kind == 3) s.inputSafe = false;
        assert(p.Observe(s).action == CombatRecoveryAction::None);
    }
    p.Reset(); p.Observe(Sample()); s = Sample(4001); s.playerHp = 90;
    assert(p.Observe(s).action == CombatRecoveryAction::RefreshAttack);
    p.Reset(); p.Observe(Sample()); s = Sample(4001);
    assert(p.Observe(s).action == CombatRecoveryAction::None);
    s = Sample(8001); auto refresh = p.Observe(s);
    assert(refresh.action == CombatRecoveryAction::RefreshAttack);
    p.Dispatched(refresh.action, s.nowMs);
    s = Sample(9001);
    assert(p.Observe(s).verified == CombatRecoveryAction::None); // latch isn't damage
    s.targetHp = 80; s.nowMs = s.sampleTick = 9501;
    assert(p.Observe(s).verified == CombatRecoveryAction::RefreshAttack);
    p.Reset(); p.Observe(Sample()); s = Sample(8001);
    refresh = p.Observe(s); p.Dispatched(refresh.action, s.nowMs);
    s = Sample(16001);
    assert(p.Observe(s).action == CombatRecoveryAction::Fail);
    p.Reset(); s = Sample(); s.attackActive = false;
    auto reengage = p.Observe(s);
    assert(reengage.action == CombatRecoveryAction::ReengageAttack);
    p.Dispatched(reengage.action, s.nowMs);
    s = Sample(251); s.attackActive = false;
    assert(p.Observe(s).action == CombatRecoveryAction::None);
    s = Sample(501);
    assert(p.Observe(s).verified == CombatRecoveryAction::ReengageAttack);
    p.Reset(); s = Sample(); s.selectionKnown = false;
    assert(p.Observe(s).action == CombatRecoveryAction::None);
    p.Reset(); p.Observe(Sample()); s = Sample(10001); s.targetHp = 0;
    assert(p.Observe(s).classification == CombatStallClass::TargetEnded);
    // Repeated failed same-GUID restoration is bounded, not every tick.
    p.Reset(); s = Sample(); s.selectedGuid = 0;
    for (unsigned attempt = 0; attempt < CombatLivenessPolicy::MaximumRepairs; ++attempt)
    {
        s.nowMs = s.sampleTick = 1 + attempt * 1000;
        d = p.Observe(s); assert(d.action == CombatRecoveryAction::RestoreTarget);
        p.Dispatched(d.action, s.nowMs);
    }
    s.nowMs = s.sampleTick = 4001;
    assert(p.Observe(s).action == CombatRecoveryAction::Fail);
    // HP samples must be keyed by object identity as well as locked GUID.
    p.Reset(); p.Observe(Sample()); s = Sample(9001); s.targetObject=20;
    assert(p.Observe(s).action==CombatRecoveryAction::None);
    assert(p.NoDamageMs(s.nowMs)==0);
    // A repeated snapshot tick cannot manufacture a fresh no-damage window.
    s.nowMs=20001;
    assert(p.Observe(s).classification==CombatStallClass::UnknownOrStale);
    // Cast/channel and short-cooldown waits do not re-arm spent repairs.
    p.Reset(); s=Sample(); s.attackActive=false;
    d=p.Observe(s); p.Dispatched(d.action,s.nowMs);
    s=Sample(8001); s.actionWait=true;
    assert(p.Observe(s).action==CombatRecoveryAction::None);
    assert(p.Repairs()==1);
    s=Sample(8002);
    assert(p.Observe(s).action==CombatRecoveryAction::None);
    s=Sample(16002);
    assert(p.Observe(s).action==CombatRecoveryAction::RefreshAttack);
    // Replay the deep-stall boundary: refresh turns the latch off. Off time
    // cannot consume the post-reengage offensive observation window.
    p.Reset(); p.Observe(Sample());
    s=Sample(4001); s.playerHp=90;
    d=p.Observe(s); assert(d.action==CombatRecoveryAction::RefreshAttack);
    p.Dispatched(d.action,s.nowMs);
    s=Sample(4251); s.playerHp=90; s.attackActive=false;
    d=p.Observe(s);
    assert(d.action==CombatRecoveryAction::ReengageAttack);
    assert(d.verified==CombatRecoveryAction::None); // refresh dispatch != damage
    p.Dispatched(d.action,s.nowMs);
    s=Sample(4501); s.playerHp=80;
    d=p.Observe(s);
    assert(d.verified==CombatRecoveryAction::ReengageAttack);
    assert(d.action==CombatRecoveryAction::None);
    s=Sample(4857); s.playerHp=80;
    assert(p.Observe(s).action==CombatRecoveryAction::None); // 356 ms is not a swing
    assert(p.Repairs()==2 && p.NoDamageMs(s.nowMs)==4856);
    s=Sample(8501); s.playerHp=70;
    assert(p.Observe(s).action==CombatRecoveryAction::Fail); // still bounded
    // Active-window verification does not refund repairs; actual damage does.
    s=Sample(8751); s.playerHp=70; s.targetHp=99;
    d=p.Observe(s);
    assert(d.verified==CombatRecoveryAction::RefreshAttack && p.Repairs()==0);
    // Slow weapons are not diagnosed before two swing periods + probe slack.
    p.Reset(); s=Sample(); s.attackPeriodMs=5000; p.Observe(s);
    s=Sample(8001); s.attackPeriodMs=5000;
    assert(p.Observe(s).action==CombatRecoveryAction::None);
    s.nowMs=s.sampleTick=11001;
    assert(p.Observe(s).action==CombatRecoveryAction::RefreshAttack);
    // Death, invalid target and unknown selection never authorize input.
    for (int guard=0; guard<3; ++guard)
    {
        p.Reset(); s=Sample(); s.attackActive=false;
        if (guard==0) s.alive=false;
        if (guard==1) s.targetValid=false;
        if (guard==2) s.selectionKnown=false;
        assert(p.Observe(s).action==CombatRecoveryAction::None);
    }
    // Movement/input/transaction ownership is a gate, not earned progress.
    p.Reset(); s=Sample(); s.attackActive=false;
    d=p.Observe(s); p.Dispatched(d.action,s.nowMs);
    s=Sample(2001); s.inputSafe=false;
    assert(p.Observe(s).action==CombatRecoveryAction::None);
    assert(p.Repairs()==1 && p.NoDamageMs(s.nowMs)==2000);
    // Server victim == 0 is deliberately NOT part of UI selection evidence.
    p.Reset(); s=Sample(); s.selectedGuid=7;
    assert(p.Observe(s).classification==CombatStallClass::Healthy);
    s.selectedGuid=42; s.nowMs=s.sampleTick=251;
    d=p.Observe(s);
    assert(d.action==CombatRecoveryAction::RestoreTarget && s.targetGuid==7);
    p.Dispatched(d.action,s.nowMs);
    s.targetHp=90; s.nowMs=s.sampleTick=501;
    assert(p.Observe(s).verified!=CombatRecoveryAction::RestoreTarget);
    // Missing health evidence must not look like target death / refund repairs.
    p.Reset(); s=Sample(); s.attackActive=false;
    d=p.Observe(s); p.Dispatched(d.action,s.nowMs);
    s=Sample(251); s.fresh=false; s.targetHp=0;
    assert(p.Observe(s).classification==CombatStallClass::UnknownOrStale);
    assert(p.Repairs()==1);
}
