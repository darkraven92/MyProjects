#include "../src/Bot/CombatDefensiveContainmentPolicy.h"
#include <cassert>
#include <limits>

using namespace Bot;

static DefensiveContainmentSample Sample(std::uint64_t now=0)
{
    DefensiveContainmentSample s{};
    s.nowMs=now; s.guid=7; s.playerAlive=true; s.targetValid=true;
    s.targetHp=80; s.evidenceKnown=true; s.hostileEngaged=true;
    return s;
}

int main()
{
    std::array<DefensiveThreatPosition,CombatDefensiveEscapePolicy::MaximumThreats> threats{};
    DefensiveEscapePoint escape{};
    assert(!CombatDefensiveEscapePolicy::AwayPoint({0,0,0},threats,0,escape));
    threats[0]={-2,0};
    assert(CombatDefensiveEscapePolicy::AwayPoint({0,0,5},threats,1,escape));
    assert(escape.x==CombatDefensiveEscapePolicy::LocalEscapeDistance);
    assert(escape.y==0 && escape.z==5);
    threats[1]={2,0};
    assert(!CombatDefensiveEscapePolicy::AwayPoint({0,0,5},threats,2,escape));
    threats[1]={-2,-2};
    assert(CombatDefensiveEscapePolicy::AwayPoint({0,0,5},threats,2,escape));
    assert(escape.x>0 && escape.y>0);
    threats[0]={std::numeric_limits<float>::quiet_NaN(),0};
    assert(!CombatDefensiveEscapePolicy::AwayPoint({0,0,5},threats,1,escape));

    CombatDefensiveContainmentPolicy p;
    assert(!p.Active());
    p.Begin(7,80,100);
    assert(p.Active());
    assert(!p.Expired(100));
    assert(!p.Expired(30099));
    assert(p.Expired(30100));
    auto s=Sample(100);
    assert(p.Observe(s).action==DefensiveContainmentAction::Observe);
    s.nowMs=300; s.routeArrived=true;
    assert(p.Observe(s).action==DefensiveContainmentAction::Observe);
    s.nowMs=30100;
    assert(p.Observe(s).action==DefensiveContainmentAction::Fail);

    p.Begin(7,80,100);
    s=Sample(100); s.hostileEngaged=false;
    assert(p.Observe(s).action==DefensiveContainmentAction::Observe);
    s.nowMs=1099;
    assert(p.Observe(s).action==DefensiveContainmentAction::Observe);
    s.nowMs=1100;
    assert(p.Observe(s).action==DefensiveContainmentAction::VerifiedDisengagement);

    p.Begin(7,80,100);
    s=Sample(100); s.hostileEngaged=false; p.Observe(s);
    s=Sample(500); p.Observe(s); // renewed hostile resets proof
    s.hostileEngaged=false; s.nowMs=600; p.Observe(s);
    s.nowMs=1200;
    assert(p.Observe(s).action==DefensiveContainmentAction::Observe);

    p.Begin(7,80,100);
    s=Sample(200); s.targetHp=79;
    assert(p.Observe(s).action==DefensiveContainmentAction::Observe);
    s.reengageReady=true;
    assert(p.Observe(s).action==DefensiveContainmentAction::Reengage);
    p.Begin(7,80,100);
    s=Sample(200); s.routeFailed=true;
    assert(p.Observe(s).action==DefensiveContainmentAction::Fail);
    p.Begin(7,80,100);
    s=Sample(200); s.waterBlocked=true;
    assert(p.Observe(s).action==DefensiveContainmentAction::Fail);
    p.Begin(7,80,100);
    s=Sample(30100);
    assert(p.Observe(s).action==DefensiveContainmentAction::Fail);
    p.Begin(7,80,100);
    s=Sample(200); s.playerAlive=false;
    assert(p.Observe(s).action==DefensiveContainmentAction::DeathHandoff);
    p.Begin(7,80,100);
    s=Sample(200); s.guid=8;
    assert(p.Observe(s).action==DefensiveContainmentAction::Fail);
}
