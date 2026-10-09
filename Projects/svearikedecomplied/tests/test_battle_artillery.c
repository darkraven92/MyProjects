#include "battle_artillery.h"
#include "battle_attack.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static SrBattleSetup fixture(void) {
    SrBattleSetup b={.swedes=3,.enemies=2,.ready=1};
    for(int n=0;n<5;++n) b.units[n]=(SrBattleUnit){.troop={n<3?SR_BATTLE_SWED:SR_BATTLE_ENEMY,SR_BATTLE_INF,1,1,600},
        .square=n<3?n+1:n+21,.placed=1,.actions=1};
    b.armies[1].level=1;b.guns[1]=(SrBattleGuns){2,1000,4};return b;
}
int main(void) {
    SrBattleSetup b=fixture();
    CHECK(sr_battle_artillery_target(&b)==2); /* Equal strength: last insertion (provisional runtime tie). */
    b.units[0].troop.troops=700;CHECK(sr_battle_artillery_target(&b)==0);
    b.units[2].troop.type=SR_BATTLE_CAV;b.units[2].troop.troops=2000;
    CHECK(sr_battle_artillery_target(&b)==0); /* Infantry >500 precedes stronger cavalry. */
    b.units[0].troop.troops=b.units[1].troop.troops=500;
    CHECK(sr_battle_artillery_target(&b)==2);
    b.units[2].troop.troops=500;CHECK(sr_battle_artillery_target(&b)==1);
    b.units[0].placed=b.units[1].placed=0;CHECK(sr_battle_artillery_target(&b)==2);
    b.units[2].troop.troops=0;CHECK(sr_battle_artillery_target(&b)==-1);

    b=fixture();b.units[0].square=36;b.units[1].square=43;b.units[2].square=50;
    CHECK(sr_battle_artillery_target(&b)==2);
    b.units[0].square=57;CHECK(sr_battle_artillery_target(&b)==0);
    b.units[1].square=60;CHECK(sr_battle_artillery_target(&b)==1);
    b.units[3].square=56; /* Raw -1 wraps to preceding row and penalizes square 57. */
    b.units[4].square=59;b.units[4].troop.troops=601;
    CHECK(sr_battle_artillery_target(&b)==0); /* 980 beats 930. */
    b.units[3].troop.troops=1300;CHECK(sr_battle_artillery_target(&b)==1); /* 880 loses to 930. */

    b=fixture();b.units[3].square=10;b.units[3].troop.troops=599;
    b.units[0].troop.troops=2000;CHECK(sr_battle_artillery_target(&b)==2); /* Direct blocker at 3. */
    b.units[2].square=36;b.units[1].troop.troops=599;
    CHECK(sr_battle_artillery_target(&b)==2); /* Equal diagonal strength does not block; use row priority. */
    b.units[1].square=4;b.units[1].troop.troops=600;
    CHECK(sr_battle_artillery_target(&b)==1); /* Diagonal blocker precedes row 36. */
    b.units[1].troop.troops=100;b.units[0].square=2;
    CHECK(sr_battle_artillery_target(&b)==2); /* Weak +diagonal skips strong -diagonal. */
    b.units[1].placed=0;CHECK(sr_battle_artillery_target(&b)==0);
    b.units[3].square=14;b.units[0].square=8;b.units[2].square=6;
    CHECK(sr_battle_artillery_target(&b)==2); /* Source excludes +diagonal at tile14. */

    for(int level=1;level<=3;++level) {
        int reload=5-level;CHECK(sr_battle_gun_ready(&reload,level) && reload==5-level);
        reload=1;
        for(int turn=1;turn<5-level;++turn) CHECK(!sr_battle_gun_ready(&reload,level) && reload==turn+1);
        CHECK(sr_battle_gun_ready(&reload,level));
        CHECK(sr_battle_gun_ready(&reload,level)); /* No target leaves the gun ready. */
    }
    b=fixture();b.units[0].troop.troops=1000;SrRandom rng;SrBattleAttack a={0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_gun(&a,&b,&rng,1,0,100) && a.damage==280 && rng.calls==2);
    CHECK(b.units[0].actions==1 && b.units[0].troop.attacked==1);
    CHECK(!sr_battle_attack_gun(&a,&b,&rng,1,0,100) && rng.calls==2);
    sr_battle_attack_tick(&a,123);CHECK(b.units[0].troop.troops==1000);
    sr_battle_attack_tick(&a,124);CHECK(b.units[0].troop.troops==720);
    sr_battle_attack_tick(&a,171);CHECK(a.active);
    sr_battle_attack_tick(&a,172);CHECK(!a.active && rng.calls==2 && a.advance_unit==-1 && !a.counter);
    CHECK(b.units[0].actions==1 && b.units[0].troop.attacked==1);
    b=fixture();b.units[0].troop.troops=100;a=(SrBattleAttack){0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_gun(&a,&b,&rng,1,0,0));sr_battle_attack_tick(&a,24);
    CHECK(b.units[0].troop.troops==-180 && b.units[0].placed);
    sr_battle_attack_tick(&a,72);CHECK(!b.units[0].placed && a.swed_actions_lost==1 && a.advance_unit==-1 && rng.calls==2);
    /* Synthetic sub-icon strength exercises ArtTroop's distinct zero-damage
       duration; real initialized guns contain at least 1000 men per icon. */
    b=fixture();b.guns[1].per_icon=1;a=(SrBattleAttack){0};sr_random_seed(&rng,0);
    CHECK(sr_battle_attack_gun(&a,&b,&rng,1,0,UINT32_MAX-10) && !a.damage);
    sr_battle_attack_tick(&a,12);CHECK(a.active);
    sr_battle_attack_tick(&a,13);CHECK(!a.active && b.units[0].troop.troops==600 && rng.calls==2);
    a=(SrBattleAttack){0};CHECK(!sr_battle_attack_gun(&a,&b,&rng,1,3,0) && rng.calls==2);
    puts("Artillery target priorities, threat penalties, reload intervals, shot timing, two RNG draws and deaths passed.");
    return 0;
}
