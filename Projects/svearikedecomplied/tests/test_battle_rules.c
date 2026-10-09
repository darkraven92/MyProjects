#include "battle_rules.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static int action(const SrBattleRange *r,int square) {
    for(int n=0;n<r->count;++n) if(r->squares[n]==square) return r->actions[n];
    return 0;
}
int main(void) {
    /* Twelve source profiles: front row, center/left/right concentrations,
       pure armies and strict 5:1 thresholds. Cavalry occupies rows one to three. */
    const int placements[12][6]={{15,17,20},{0},{17,18,19},{3,4,5},
        {6,13,20,21,17},{6,7,13},{1,8,15,16,17},{1,2,8},{0},{0},{0},{0}};
    const int counts[]={3,0,3,3,5,3,5,3,0,0,0,0};
    const int first[]={54,47,48,53,56,54,50,52,46,53,40,61};
    const int second[]={52,45,44,54,44,55,48,51,47,55,38,59};
    SrBattleFormation f;
    for(int p=1;p<=12;++p) {
        int inf=p==10?0:p==11?10001:2000;
        int cav=p==9?0:p==12?10001:2000;
        CHECK(sr_battle_enemy_formation(inf,cav,1000,1,cav,0,1000,1,placements[p-1],counts[p-1],&f));
        CHECK(f.profile==p && f.row==(p>=11?6:7));
        CHECK(f.squares[0]==first[p-1] && f.squares[1]==second[p-1]);
        int occupied[64]={0};
        for(int n=0;n<10;++n) {
            CHECK(f.squares[n]>=36 && f.squares[n]<=63 && !occupied[f.squares[n]]);
            occupied[f.squares[n]]=1;
        }
    }
    CHECK(sr_battle_enemy_formation(10000,2000,1000,1,2000,0,1000,1,0,0,&f) && f.profile==2);
    CHECK(sr_battle_enemy_formation(2000,10000,1000,1,10000,0,1000,1,0,0,&f) && f.profile==2);
    CHECK(sr_battle_enemy_formation(2000,2000,9000,1,100,0,1000,3,0,0,&f) && f.row==8);
    CHECK(sr_battle_enemy_formation(2000,2000,0,3,9000,0,9000,1,0,0,&f) && f.row==6);
    CHECK(sr_battle_enemy_formation(2000,2000,9000,1,100,0,0,3,0,0,&f) && f.row==6);
    /* Lingo INTEGER 3/2 is 1; FLOAT 3.0/2 is 1.5, changing deployment. */
    CHECK(sr_battle_enemy_formation(1,1,1000,1,3,0,1000,1,0,0,&f) && f.row==7);
    CHECK(sr_battle_enemy_formation(1,1,1000,1,3,1,1000,1,0,0,&f) && f.row==6);
    const int duplicate[]={1,1},outside[]={22};SrBattleFormation saved_f=f;
    CHECK(!sr_battle_enemy_formation(1,1,0,1,1,0,0,1,duplicate,2,&f));
    CHECK(!sr_battle_enemy_formation(1,1,0,1,1,0,0,1,outside,1,&f));
    CHECK(!memcmp(&saved_f,&f,sizeof f));
    /* Original KRIG positions are slightly irregular. Every supplied position
       must be selectable, including the edge rows and the one-pixel offsets. */
    int x,y;
    for(int square=1;square<=63;++square) {
        CHECK(sr_battle_point(square,&x,&y));
        CHECK(sr_battle_square(x,y,584,338)==square);
    }
    CHECK(sr_battle_point(5,&x,&y) && x==184 && y==273);
    CHECK(sr_battle_point(24,&x,&y) && x==220 && y==170);
    CHECK(sr_battle_point(63,&x,&y) && x==544 && y==148);
    CHECK(!sr_battle_point(0,&x,&y) && !sr_battle_point(64,&x,&y));
    CHECK(!sr_battle_square(-1,189,584,338) && !sr_battle_square(584,189,584,338));
    CHECK(!sr_battle_square(39,-1,584,338) && !sr_battle_square(39,338,584,338));
    CHECK(!sr_battle_square(0,0,584,338));
    int board[64]={0};SrBattleRange r;
    CHECK(sr_battle_range(board,32,1,1,SR_BATTLE_SWED,&r) && r.count==4);
    const int around[]={25,31,33,39};
    for(int n=0;n<4;++n) CHECK(r.squares[n]==around[n] && r.actions[n]==SR_BATTLE_WALK);
    CHECK(sr_battle_range(board,7,1,1,SR_BATTLE_SWED,&r) && r.count==2);
    CHECK(r.squares[0]==6 && r.squares[1]==14 && !action(&r,8));
    CHECK(sr_battle_range(board,1,2,0,SR_BATTLE_SWED,&r) && r.count==8);
    const int corner[]={2,3,8,9,10,15,16,17};
    for(int n=0;n<8;++n) CHECK(r.squares[n]==corner[n]);
    board[25]=SR_BATTLE_SWED;board[31]=SR_BATTLE_ROCK;board[33]=SR_BATTLE_ENEMY;
    CHECK(sr_battle_range(board,32,1,1,SR_BATTLE_SWED,&r) && r.count==2);
    CHECK(r.squares[0]==39 && r.actions[0]==SR_BATTLE_WALK);
    CHECK(r.squares[1]==33 && r.actions[1]==SR_BATTLE_SHOOT);
    /* Destination-only movement; even occupied intermediate tiles do not
       block movement range two. Attack range two has the source's LOS toggle. */
    memset(board,0,sizeof board);board[24]=SR_BATTLE_SWED;board[25]=SR_BATTLE_SWED;
    CHECK(sr_battle_range(board,32,2,0,SR_BATTLE_SWED,&r) && action(&r,17)==SR_BATTLE_WALK);
    board[17]=SR_BATTLE_ENEMY;
    CHECK(sr_battle_range(board,32,0,2,SR_BATTLE_SWED,&r) && action(&r,17)==SR_BATTLE_SHOOT);
    board[24]=SR_BATTLE_EMPTY;
    CHECK(sr_battle_range(board,32,0,2,SR_BATTLE_SWED,&r) && !action(&r,17));
    board[25]=SR_BATTLE_RIVER;board[24]=SR_BATTLE_ROCK;
    CHECK(sr_battle_range(board,32,0,2,SR_BATTLE_SWED,&r) && action(&r,17)==SR_BATTLE_SHOOT);
    board[25]=SR_BATTLE_BLOCKED;
    CHECK(sr_battle_range(board,32,0,3,SR_BATTLE_SWED,&r) && action(&r,17)==SR_BATTLE_SHOOT);
    /* The original range-five cone uses positive rows for both nations. */
    memset(board,0,sizeof board);board[24]=board[38]=board[39]=board[40]=SR_BATTLE_SWED;
    CHECK(sr_battle_range(board,32,0,5,SR_BATTLE_ENEMY,&r) && r.count==3);
    CHECK(r.squares[0]==38 && r.squares[1]==39 && r.squares[2]==40 && !action(&r,24));
    CHECK(!sr_battle_range(board,0,1,1,SR_BATTLE_SWED,&r));
    CHECK(!sr_battle_range(board,1,3,1,SR_BATTLE_SWED,&r));

    /* Golden calculations from KRIG troopControl.attack and the separately
       verified Windows RNG, not recordings of original Director execution. */
    const unsigned seeds[]={0,1,4,65};
    const int damage[]={194,382,230,317},reply[]={209,140,413,135},art[]={280,201,954,131};
    SrRandom rng;SrBattleExchange e;
    SrBattleTroop a,d;
    for(int n=0;n<4;++n) {
        a=(SrBattleTroop){SR_BATTLE_SWED,SR_BATTLE_INF,1,1,1000};
        d=(SrBattleTroop){SR_BATTLE_ENEMY,SR_BATTLE_CAV,1,1,1000};
        sr_random_seed(&rng,seeds[n]);CHECK(sr_battle_melee(&rng,&a,&d,1,&e));
        CHECK(e.damage==damage[n] && e.reply==reply[n] && rng.calls==2);
        CHECK(a.troops==1000-reply[n] && d.troops==1000-damage[n]);
        CHECK(a.attacked==1 && d.attacked==2 && !e.attacker_dead && !e.defender_dead);
        sr_random_seed(&rng,seeds[n]);CHECK(sr_battle_artillery(&rng,1,1000,SR_BATTLE_INF)==art[n]);
        CHECK(rng.calls==2); /* Includes overwritten random(myDamage). */
    }
    a=(SrBattleTroop){SR_BATTLE_SWED,SR_BATTLE_CAV,3,1,2000};
    d=(SrBattleTroop){SR_BATTLE_ENEMY,SR_BATTLE_INF,1,3,1000};
    sr_random_seed(&rng,0);CHECK(sr_battle_melee(&rng,&a,&d,1.5,&e));
    CHECK(e.damage==3159 && e.defender_dead && !e.reply && rng.calls==1);
    CHECK(a.troops==2000 && d.troops==-2159 && d.attacked==4);
    a=(SrBattleTroop){SR_BATTLE_ENEMY,SR_BATTLE_INF,1,1,1000};
    d=(SrBattleTroop){SR_BATTLE_SWED,SR_BATTLE_CAV,1,1,1000};
    sr_random_seed(&rng,0);CHECK(sr_battle_melee(&rng,&a,&d,2,&e));
    CHECK(e.damage==194 && e.reply==418); /* Commander applies to Swedish reply. */
    a=(SrBattleTroop){SR_BATTLE_SWED,SR_BATTLE_INF,1,1,1};
    d=(SrBattleTroop){SR_BATTLE_ENEMY,SR_BATTLE_INF,3,1,1000};
    sr_random_seed(&rng,0);CHECK(sr_battle_melee(&rng,&a,&d,1,&e));
    CHECK(!e.damage && e.reply==562 && e.attacker_dead && rng.calls==2);
    SrBattleTroop saved=a;unsigned calls=rng.calls;
    CHECK(!sr_battle_melee(&rng,&a,&d,1,&e) && rng.calls==calls && !memcmp(&saved,&a,sizeof a));
    CHECK(sr_battle_artillery(&rng,0,1000,SR_BATTLE_INF)==-1 && rng.calls==calls);
    sr_random_seed(&rng,0);CHECK(sr_battle_artillery(&rng,3,2000,SR_BATTLE_CAV)==1008 && rng.calls==2);
    puts("Original enemy formations, board positions, ordered ranges, LOS quirks, melee replies and artillery RNG passed.");
    return 0;
}
