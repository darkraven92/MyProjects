#include "battle_ai.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static void put(SrBattleSetup *b,int index,int square,int nation,int type,double strength) {
    b->units[index]=(SrBattleUnit){.troop={nation,type,1,1,strength},.square=square,.placed=1,.actions=1};
    b->contents[square]=nation;
}
static SrBattleSetup board(int square) {
    SrBattleSetup b={.swedes=4,.enemies=1,.ready=1};
    put(&b,4,square,SR_BATTLE_ENEMY,SR_BATTLE_INF,1000);
    return b;
}
int main(void) {
    SrBattleSetup b=board(32);SrRandom rng;SrBattleDecision d;
    sr_random_seed(&rng,0);
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_MOVE_FORWARD && d.target==25);
    CHECK(rng.calls==1); /* Lower-priority sideways decision still draws. */
    b.contents[25]=SR_BATTLE_BLOCKED;
    for(int seed=0;seed<8;++seed) {
        sr_random_seed(&rng,seed);
        CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_MOVE_SIDE && d.target==33 && rng.calls==1);
    }
    /* Off-center movement favors the center and consumes no random number. */
    b=board(33);b.contents[26]=SR_BATTLE_BLOCKED;sr_random_seed(&rng,0);
    CHECK(sr_battle_enemy_decide(&b,&rng,33,&d) && d.target==32 && d.action==SR_BATTLE_MOVE_SIDE && !rng.calls);
    b=board(31);b.contents[24]=SR_BATTLE_BLOCKED;
    CHECK(sr_battle_enemy_decide(&b,&rng,31,&d) && d.target==32 && !rng.calls);
    b=board(35);b.contents[28]=SR_BATTLE_BLOCKED;
    CHECK(sr_battle_enemy_decide(&b,&rng,35,&d) && d.target==34 && !rng.calls);

    /* Side attacks outrank forward movement; strict infantry/cavalry thresholds. */
    b=board(32);put(&b,0,33,SR_BATTLE_SWED,SR_BATTLE_INF,1249);
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_ATTACK_SIDE && d.target==33);
    b.units[0].troop.troops=1250;
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_MOVE_FORWARD);
    b.units[0].troop.type=SR_BATTLE_CAV;b.units[0].troop.troops=999;
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_ATTACK_SIDE);
    b.units[0].troop.troops=1000;
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_MOVE_FORWARD);
    b.units[0].troop.troops=999;put(&b,1,31,SR_BATTLE_SWED,SR_BATTLE_INF,1000);
    sr_random_seed(&rng,0);CHECK(sr_battle_enemy_decide(&b,&rng,32,&d));
    CHECK(d.action==SR_BATTLE_ATTACK_SIDE && d.target==33 && rng.calls==1);
    put(&b,2,25,SR_BATTLE_SWED,SR_BATTLE_INF,1499);
    sr_random_seed(&rng,0);CHECK(sr_battle_enemy_decide(&b,&rng,32,&d));
    CHECK(d.action==SR_BATTLE_ATTACK_FORWARD && d.target==25 && rng.calls==1);
    b.units[2].troop.troops=1500;
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_ATTACK_SIDE);
    b.units[2].troop.type=SR_BATTLE_CAV;b.units[2].troop.troops=1249;
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_ATTACK_FORWARD);
    b.units[2].troop.troops=1250;
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_ATTACK_SIDE);
    put(&b,3,39,SR_BATTLE_SWED,SR_BATTLE_CAV,50000);
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && d.action==SR_BATTLE_ATTACK_BACK && d.target==39);
    /* The source chooses even an overwhelming rear target, but omits tile 63. */
    b=board(56);put(&b,0,63,SR_BATTLE_SWED,SR_BATTLE_INF,1);
    CHECK(sr_battle_enemy_decide(&b,&rng,56,&d) && d.action==SR_BATTLE_MOVE_FORWARD && d.target==49);

    /* Failed retreat still consumes randomness: source checks the forward
       Swedish tile for EMPTY, rather than the empty rear destination. */
    b=board(32);b.contents[31]=b.contents[33]=SR_BATTLE_BLOCKED;
    put(&b,0,25,SR_BATTLE_SWED,SR_BATTLE_INF,1500);
    sr_random_seed(&rng,0);CHECK(sr_battle_enemy_decide(&b,&rng,32,&d));
    CHECK(d.action==SR_BATTLE_WAIT && d.target==32 && rng.calls==1);
    b=board(32);b.units[4].actions=0;sr_random_seed(&rng,0);
    CHECK(sr_battle_enemy_decide(&b,&rng,32,&d) && rng.calls==1);
    /* Exiting at the Swedish edge precedes action-count checks or RNG. */
    b=board(7);b.units[4].actions=0;sr_random_seed(&rng,0);
    CHECK(sr_battle_enemy_decide(&b,&rng,7,&d) && d.action==SR_BATTLE_ENEMY_WINS && !rng.calls);
    CHECK(!sr_battle_enemy_decide(&b,&rng,8,&d) && !rng.calls);
    puts("Enemy movement, attack priority, strict thresholds, edge victory and original RNG quirks passed.");
    return 0;
}
