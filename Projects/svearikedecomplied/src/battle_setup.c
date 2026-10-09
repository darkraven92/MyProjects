#include "battle_setup.h"
#include <limits.h>
static int valid(const SrBattleArmy *a) {
    if(!a || !(a->infantry>=0 && a->cavalry>=0) ||
       !(a->infantry+a->cavalry>0 && a->infantry+a->cavalry<=INT_MAX) ||
       a->artillery<0 || a->level<1 || a->level>3 || a->real<0 || a->real>3) return 0;
    return ((a->real&SR_BATTLE_REAL_INF) || a->infantry==(int)a->infantry) &&
           ((a->real&SR_BATTLE_REAL_CAV) || a->cavalry==(int)a->cavalry);
}
static int groups(SrBattleUnit *out,const SrBattleArmy *a,int nation) {
    double total=a->infantry+a->cavalry,icons;
    int icons_real=0;
    if(total>10000) icons=10;
    else if(total>6000) {
        icons=total/1000;
        icons_real=a->real!=0;
        if(!icons_real) icons=(int)icons;
    } else icons=6;
    int infantry=(int)((a->infantry/total)*icons+0.5);
    double cavalry=icons-infantry;
    int count=0;
    if(cavalry) {
        double men=a->cavalry/cavalry;
        if(!(a->real&SR_BATTLE_REAL_CAV) && !icons_real) men=(int)men;
        /* The source repeats X=0 to cavIcons-1, which can be a FLOAT. */
        for(int n=0;n<=cavalry-1;++n)
            out[count++]=(SrBattleUnit){.troop={nation,SR_BATTLE_CAV,a->level,1,men},.actions=1,.real=!!(a->real&SR_BATTLE_REAL_CAV) || icons_real};
    }
    if(infantry) {
        double men=a->infantry/infantry;
        if(!(a->real&SR_BATTLE_REAL_INF)) men=(int)men;
        for(int n=0;n<infantry;++n)
            out[count++]=(SrBattleUnit){.troop={nation,SR_BATTLE_INF,a->level,1,men},.actions=1,.real=!!(a->real&SR_BATTLE_REAL_INF)};
    }
    return count;
}
int sr_battle_setup(SrBattleSetup *b,const SrBattleArmy *swed,const SrBattleArmy *enemy) {
    if(!b || !valid(swed) || !valid(enemy) || swed->real) return 0;
    SrBattleSetup next={0};next.armies[0]=*swed;next.armies[1]=*enemy;
    next.swedes=groups(next.units,swed,SR_BATTLE_SWED);
    next.enemies=groups(next.units+next.swedes,enemy,SR_BATTLE_ENEMY);
    for(int n=0;n<next.swedes;++n) {
        int index=n+1,row=index/7+1;
        next.units[n].x=30+(row==1?index:index-6)*35;
        next.units[n].y=row==1?119:185;
    }
    for(int n=0;n<2;++n) {
        int art=next.armies[n].artillery,count=art>=5000?5:art/1000;
        next.guns[n]=(SrBattleGuns){count,count?art/count:0,5-next.armies[n].level};
    }
    *b=next;return 1;
}
int sr_battle_deploy(SrBattleSetup *b,int unit,int square) {
    if(!b || b->ready || unit<0 || unit>=b->swedes || b->units[unit].placed || square<0 || square>63) return 0;
    SrBattleUnit *u=&b->units[unit];u->square=square;
    if(!square || square>=22 || b->contents[square]!=SR_BATTLE_EMPTY) return 0;
    u->placed=1;++b->placed;b->contents[square]=SR_BATTLE_SWED;
    /* Board-local position. Renderer adds the map origin for placed troops;
       unplaced staging positions above are already stage coordinates. */
    sr_battle_point(square,&u->x,&u->y);return 1;
}
int sr_battle_setup_enemy(SrBattleSetup *b) {
    if(!b || b->ready || b->swedes<=0 || b->placed!=b->swedes) return 0;
    int squares[10],count=0;
    for(int n=0;n<b->swedes;++n) if(b->units[n].troop.type==SR_BATTLE_CAV) squares[count++]=b->units[n].square;
    const SrBattleArmy *s=&b->armies[0],*e=&b->armies[1];
    if(!sr_battle_enemy_formation((int)s->infantry,(int)s->cavalry,s->artillery,s->level,
        e->cavalry,e->real&SR_BATTLE_REAL_CAV,e->artillery,e->level,squares,count,&b->formation)) return 0;
    for(int n=0;n<b->enemies;++n) {
        SrBattleUnit *u=&b->units[b->swedes+n];
        u->square=b->formation.squares[n];u->placed=1;
        b->contents[u->square]=SR_BATTLE_ENEMY;
        sr_battle_point(u->square,&u->x,&u->y);
    }
    b->ready=1;return 1;
}
void sr_battle_losses(const SrBattleSetup *b,double losses[4]) {
    if(!b || !losses) return;
    losses[0]=b->armies[0].infantry;losses[1]=b->armies[0].cavalry;
    losses[2]=b->armies[1].infantry;losses[3]=b->armies[1].cavalry;
    double survivors[4]={0};
    for(int n=0;n<b->swedes+b->enemies;++n) {
        const SrBattleTroop *t=&b->units[n].troop;
        if(t->troops>0) survivors[(n<b->swedes?0:2)+t->type]+=t->troops;
    }
    for(int n=0;n<4;++n) losses[n]-=survivors[n];
}
int sr_battle_loss_types(const SrBattleSetup *b) {
    if(!b) return 0;
    int real=b->armies[1].real;
    /* ScoreScript 45 replaces negative HP with INTEGER zero. Exactly zero
       retains its original type. Dead enemy objects remain in gEnemyList. */
    for(int n=b->swedes;n<b->swedes+b->enemies;++n)
        if(b->units[n].real && b->units[n].troop.troops>=0)
            real|=1<<b->units[n].troop.type;
    return real;
}
