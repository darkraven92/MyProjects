#include "battle_rules.h"
#include "generated/battle_rules.h"
static int integer(double x) {return (int)(x+(x<0?-0.5:0.5));}
int sr_battle_enemy_formation(int infantry,int cavalry,int artillery,int level,
    double enemy_cavalry,int enemy_cavalry_real,int enemy_artillery,int enemy_level,
    const int *cavalry_squares,int count,SrBattleFormation *out) {
    if(!out || infantry<0 || cavalry<0 || artillery<0 || enemy_cavalry<0 || enemy_artillery<0 ||
       level<1 || level>3 || enemy_level<1 || enemy_level>3 || count<0 || count>10 ||
       (count && !cavalry_squares)) return 0;
    int left=0,right=0,center=0,front=0,seen[22]={0};
    for(int n=0;n<count;++n) {
        int s=cavalry_squares[n];
        if(s<1 || s>21 || seen[s]) return 0;
        seen[s]=1;
        int col=(s-1)%7+1;
        if(col>=6) ++left;else if(col<=2) ++right;else ++center;
        if(s>=15) ++front;
    }
    int profile;
    if(!cavalry) profile=9;
    else if(!infantry) profile=10;
    else if((double)infantry>cavalry*5.0) profile=11;
    else if((double)cavalry>infantry*5.0) profile=12;
    else {
        profile=front>2?1:2;
        if(center>2 || left>2 || right>2) {
            if(left>right) profile+=left>center?4:2;
            else profile+=right>center?6:2;
        }
    }
    int row=6;
    if(profile!=11 && profile!=12 && enemy_artillery) {
        row=7;
        if(enemy_cavalry*2<cavalry) ++row;
        double half=enemy_cavalry/2;
        if(!enemy_cavalry_real) half=(int)half;
        if(half>cavalry) --row;
        if(enemy_artillery+2000.0<artillery) ++row;
        if(enemy_artillery-2000.0>artillery) --row;
        if(enemy_level>level) ++row;
        if(enemy_level<level) --row;
    }
    if(row>8) row=8;
    if(row<6) row=6;
    *out=(SrBattleFormation){.profile=profile,.row=row};
    for(int n=0;n<10;++n) {
        const unsigned char *p=sr_battle_formations[profile-1][n];
        out->squares[n]=(row+p[1]-1)*7+p[0];
    }
    return 1;
}
int sr_battle_point(int square,int *x,int *y) {
    if(square<1 || square>63 || !x || !y) return 0;
    *x=sr_battle_points[square-1][0];*y=sr_battle_points[square-1][1];return 1;
}
int sr_battle_square(int x,int y,int width,int height) {
    if(x<0 || y<0 || x>=width || y>=height) return 0;
    int col=x/36+1,row=y/21+1;
    if(col>16) col=16;
    if(row>16) row=16;
    /* checkSquare deliberately indexes gMapList[X][Y], gSquareMap[Y][X]. */
    int type=sr_battle_halves[col-1][row-1];
    const unsigned char *where=sr_battle_cells[row-1][col-1];
    int mx=x-(col-1)*36,my=y-row*21;if(my<0) my=-my;
    if(type==1) return where[mx*0.58800000000000008<my?0:1];
    if(type==2) return where[21-mx*0.58800000000000008<my?1:0];
    return 0;
}
static int offset(int square,int dx,int dy) {
    int x=(square-1)%7+dx,y=(square-1)/7+dy;
    return x>=0 && x<7 && y>=0 && y<9?y*7+x+1:0;
}
static const signed char (*range(int n,int *count))[2] {
    switch(n) {
    case 1:*count=4;return sr_battle_range1;
    case 2:*count=24;return sr_battle_range2;
    case 3:*count=48;return sr_battle_range3;
    case 5:*count=35;return sr_battle_range5;
    default:*count=0;return 0;
    }
}
static void add(SrBattleRange *r,int square,int action) {
    for(int n=0;n<r->count;++n) if(r->squares[n]==square) {r->actions[n]=action;return;}
    r->squares[r->count]=square;r->actions[r->count++]=action;
}
int sr_battle_range(const int contents[64],int square,int movement,int attack,int nation,SrBattleRange *out) {
    if(!contents || !out || square<1 || square>63 || movement<0 || movement>2 ||
       (attack!=0 && attack!=1 && attack!=2 && attack!=3 && attack!=5) ||
       (nation!=SR_BATTLE_SWED && nation!=SR_BATTLE_ENEMY)) return 0;
    *out=(SrBattleRange){0};int count;
    const signed char (*mods)[2]=range(movement,&count);
    for(int n=0;n<count;++n) {
        int target=offset(square,mods[n][0],mods[n][1]);
        if(target && contents[target]==SR_BATTLE_EMPTY) add(out,target,SR_BATTLE_WALK);
    }
    mods=range(attack,&count);int enemy=nation==SR_BATTLE_SWED?SR_BATTLE_ENEMY:SR_BATTLE_SWED;
    for(int n=0;n<count;++n) {
        int target=offset(square,mods[n][0],mods[n][1]);
        if(!target || contents[target]!=enemy) continue;
        int clear=1;
        if(attack==2) for(int k=0;k<sr_battle_los[n][0];++k) {
            int between=offset(square,sr_battle_los[n][1+k*2],sr_battle_los[n][2+k*2]);
            if(between && contents[between]!=SR_BATTLE_EMPTY && contents[between]!=SR_BATTLE_RIVER && contents[between]!=SR_BATTLE_ROCK)
                clear=!clear; /* Original toggles; two blockers re-enable a shot. */
        }
        if(clear) add(out,target,SR_BATTLE_SHOOT);
    }
    return 1;
}
static int valid(const SrBattleTroop *t) {
    return t && (t->nation==SR_BATTLE_SWED || t->nation==SR_BATTLE_ENEMY) &&
        (t->type==SR_BATTLE_INF || t->type==SR_BATTLE_CAV) && t->level>=1 && t->level<=3 && t->troops>0;
}
int sr_battle_damage(SrRandom *rng,SrBattleTroop *a,SrBattleTroop *d,int counter,double commander) {
    if(!rng || !valid(a) || !valid(d) || a==d || a->nation==d->nation ||
       (counter!=0 && counter!=1) || (!counter && (d->attacked<1 || d->attacked>7)) || commander<=0) return -1;
    static const double levels[3][3]={{1.80000000000000004,1.40000000000000013,1.0},
        {2.20000000000000018,1.80000000000000004,1.40000000000000013},
        {2.60000000000000009,2.20000000000000018,1.80000000000000004}};
    static const double repeated[]={1,1.25,1.5,1.75,2,2.25,2.5};
    int type=counter?(a->type==SR_BATTLE_INF?3:2):(a->type==SR_BATTLE_INF?2:5);
    int attack=counter?1:d->attacked++;
    double people=a->troops/1000.0,level=levels[a->level-1][d->level-1];
    double leader=a->nation==SR_BATTLE_SWED?commander:1;
    double random=0.05+sr_random_next(rng,100)/1000.0;
    return integer(type*people*level*leader*repeated[attack-1]*random*1000);
}
int sr_battle_melee(SrRandom *rng,SrBattleTroop *a,SrBattleTroop *d,double commander,SrBattleExchange *out) {
    if(!rng || !out || !valid(a) || !valid(d) || a==d || a->nation==d->nation ||
       d->attacked<1 || d->attacked>7 || commander<=0) return 0;
    *out=(SrBattleExchange){0};out->damage=sr_battle_damage(rng,a,d,0,commander);d->troops-=out->damage;
    out->defender_dead=d->troops<=0;
    if(!out->defender_dead) {
        out->reply=sr_battle_damage(rng,d,a,1,commander);a->troops-=out->reply;out->attacker_dead=a->troops<=0;
    }
    return 1;
}
int sr_battle_artillery(SrRandom *rng,int level,double troops,int target_type) {
    if(!rng || level<1 || level>3 || troops<=0 || (target_type!=SR_BATTLE_INF && target_type!=SR_BATTLE_CAV)) return -1;
    (void)sr_random_next(rng,3); /* ArtTroop.shoot overwrites this damage roll. */
    double random=0.01+sr_random_next(rng,100)/1000.0;
    int type=target_type==SR_BATTLE_INF?5:3;
    return integer(1.75*random*level*type*(troops/1000.0)*1000);
}
