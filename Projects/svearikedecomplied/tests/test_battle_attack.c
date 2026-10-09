#include "battle_attack.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static SrBattleSetup fixture(void) {
    SrBattleSetup b={.swedes=1,.enemies=1,.placed=1,.ready=1};
    b.units[0]=(SrBattleUnit){.troop={SR_BATTLE_SWED,SR_BATTLE_INF,1,1,1000},.square=24,.placed=1,.actions=1};
    b.units[1]=(SrBattleUnit){.troop={SR_BATTLE_ENEMY,SR_BATTLE_CAV,1,1,1000},.square=31,.placed=1,.actions=1};
    b.contents[24]=SR_BATTLE_SWED;b.contents[31]=SR_BATTLE_ENEMY;
    return b;
}
int main(void) {
    SrBattleSetup b=fixture();SrRandom rng;SrBattleAttack a={0};int frame,dir;
    sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_begin(&a,&b,&rng,1,0,1,100));
    CHECK(a.damage==194 && rng.calls==1 && b.units[0].actions==0 && b.units[1].troop.attacked==2);
    CHECK(b.units[1].troop.troops==1000);
    CHECK(a.sound==176);a.sound=0;
    CHECK(!sr_battle_attack_begin(&a,&b,&rng,1,0,1,100) && rng.calls==1);
    CHECK(sr_battle_attack_pose(&a,0,100,&frame,&dir)==2 && frame==0 && dir==7);
    sr_battle_attack_tick(&a,123);CHECK(b.units[1].troop.troops==1000 && rng.calls==1);
    sr_battle_attack_tick(&a,124);CHECK(b.units[1].troop.troops==806 && rng.calls==1);
    CHECK(!a.sound); /* A hit does not invent an unused death/hit sound. */
    CHECK(sr_battle_attack_pose(&a,1,124,&frame,&dir)==4 && frame==0 && dir==0);
    sr_battle_attack_tick(&a,143);CHECK(sr_battle_attack_pose(&a,0,143,&frame,&dir)==2);
    sr_battle_attack_tick(&a,144);CHECK(!sr_battle_attack_pose(&a,0,144,&frame,&dir));
    sr_battle_attack_tick(&a,171);CHECK(a.active && !a.counter && rng.calls==1);
    sr_battle_attack_tick(&a,172);CHECK(a.active && a.counter && a.damage==209 && rng.calls==2);
    CHECK(a.sound==181);a.sound=0;
    CHECK(b.units[1].actions==0 && b.units[0].troop.troops==1000);
    CHECK(sr_battle_attack_pose(&a,1,172,&frame,&dir)==2 && frame==0 && dir==2);
    sr_battle_attack_tick(&a,195);CHECK(b.units[0].troop.troops==1000);
    sr_battle_attack_tick(&a,196);CHECK(b.units[0].troop.troops==791);
    sr_battle_attack_tick(&a,243);CHECK(a.active);
    sr_battle_attack_tick(&a,244);CHECK(!a.active && a.end==244 && a.advance_unit==-1 && !a.swed_actions_lost);
    CHECK(b.units[0].troop.attacked==1 && b.contents[24]==SR_BATTLE_SWED && b.contents[31]==SR_BATTLE_ENEMY);

    /* A skipped render interval and uint32 clock wrap must preserve source
       ordering, two RNG draws and the same final HP/actions. */
    b=fixture();a=(SrBattleAttack){0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_begin(&a,&b,&rng,1,0,1,UINT32_MAX-50));
    sr_battle_attack_tick(&a,93);
    CHECK(!a.active && a.end==93 && rng.calls==2 && b.units[0].troop.troops==791 && b.units[1].troop.troops==806);

    /* Lethal primary: no reply; board removal occurs after all twelve hit/die
       frames. The caller must then animate the attacker's extra-cost advance. */
    b=fixture();b.units[0].troop=(SrBattleTroop){SR_BATTLE_SWED,SR_BATTLE_CAV,3,1,2000};
    b.units[1].troop=(SrBattleTroop){SR_BATTLE_ENEMY,SR_BATTLE_INF,1,3,1000};
    a=(SrBattleAttack){0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_begin(&a,&b,&rng,1.5,0,1,0) && a.damage==3159);
    sr_battle_attack_tick(&a,24);CHECK(b.units[1].troop.troops==-2159 && b.contents[31]==SR_BATTLE_ENEMY);
    CHECK(sr_battle_attack_pose(&a,1,24,&frame,&dir)==3 && frame==0);
    sr_battle_attack_tick(&a,71);CHECK(b.units[1].placed && a.active);
    sr_battle_attack_tick(&a,72);CHECK(!a.active && a.advance_unit==0 && a.advance_square==31);
    CHECK(!b.units[1].placed && !b.units[1].square && !b.contents[31] && rng.calls==1);

    /* Rounded zero damage omits hit animation and starts the reply at tick 44.
       Counterattack kills the attacker; it never advances onto the empty tile. */
    b=fixture();b.units[0].troop.troops=1;
    b.units[1].troop=(SrBattleTroop){SR_BATTLE_ENEMY,SR_BATTLE_INF,3,1,1000};
    b.units[1].actions=0;a=(SrBattleAttack){0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_begin(&a,&b,&rng,1,0,1,0) && !a.damage);
    sr_battle_attack_tick(&a,24);CHECK(!sr_battle_attack_pose(&a,1,24,&frame,&dir) && b.units[1].troop.troops==1000);
    sr_battle_attack_tick(&a,43);CHECK(rng.calls==1);
    sr_battle_attack_tick(&a,44);CHECK(a.counter && a.damage==562 && b.units[1].actions==-1 && rng.calls==2);
    sr_battle_attack_tick(&a,116);CHECK(!a.active && !b.units[0].placed && a.advance_unit==-1 && !a.swed_actions_lost);

    /* Enemy attacks use the commander only for the Swedish reply. */
    b=fixture();b.units[0].troop.type=SR_BATTLE_CAV;b.units[0].actions=0;b.units[1].troop.type=SR_BATTLE_INF;
    a=(SrBattleAttack){0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_begin(&a,&b,&rng,2,1,0,0) && a.damage==194);
    sr_battle_attack_tick(&a,72);CHECK(a.damage==418 && b.units[0].actions==-1);
    sr_battle_attack_tick(&a,144);CHECK(!a.active && b.units[1].troop.troops==582);

    /* Killing a Swedish group adjusts the round budget by its remaining actions,
       including the original negative-action quirk after earlier counters. */
    for(int remaining=-1;remaining<=1;++remaining) {
        b=fixture();b.units[0].troop.troops=1;b.units[0].actions=remaining;
        a=(SrBattleAttack){0};sr_random_seed(&rng,0);
        CHECK(sr_battle_attack_begin(&a,&b,&rng,1,1,0,0));
        sr_battle_attack_tick(&a,72);CHECK(!a.active && a.swed_actions_lost==remaining && a.advance_unit==1);
    }
    b=fixture();a=(SrBattleAttack){0};sr_random_seed(&rng,0);SrBattleSetup saved=b;
    CHECK(!sr_battle_attack_begin(&a,&b,&rng,1,0,0,0));
    CHECK(!sr_battle_attack_begin(&a,&b,&rng,0,0,1,0));
    CHECK(!sr_battle_attack_begin(&a,&b,&rng,1,-1,1,0));
    CHECK(!rng.calls && !memcmp(&b,&saved,sizeof b));
    b.units[0].actions=0;CHECK(!sr_battle_attack_begin(&a,&b,&rng,1,0,1,0));
    b.units[0].actions=1;b.units[1].square=32;CHECK(!sr_battle_attack_begin(&a,&b,&rng,1,0,1,0));
    CHECK(!rng.calls);
    /* Shoot directions 2/4/5/7, for both nations and all technologies. Each
       original cavalry constructor supplies its own special direction. */
    const int offsets[]={-7,-1,1,7};
    const int sounds[2][3][4]={{{183,181,181,181},{184,184,182,184},{184,184,182,184}},
                              {{181,181,181,183},{184,182,184,184},{184,182,184,184}}};
    for(int nation=0;nation<2;++nation) for(int level=1;level<=3;++level) for(int direction=0;direction<4;++direction) {
        b=fixture();a=(SrBattleAttack){0};sr_random_seed(&rng,0);
        memset(b.contents,0,sizeof b.contents);
        b.units[nation].square=24;b.units[nation].troop.type=SR_BATTLE_CAV;b.units[nation].troop.level=level;
        b.units[1-nation].square=24+offsets[direction];
        b.contents[24]=nation+1;b.contents[24+offsets[direction]]=2-nation;
        CHECK(sr_battle_attack_begin(&a,&b,&rng,1,nation,1-nation,0));
        CHECK(a.sound==sounds[nation][level-1][direction] && rng.calls==1);
    }
    puts("Timed attacks, counters, original damage/RNG, deaths, advance requests, zero damage and clock wrap passed.");
    return 0;
}
