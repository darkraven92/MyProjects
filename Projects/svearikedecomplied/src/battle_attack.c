#include "battle_attack.h"
#include "generated/battle_sounds.h"
static void begin_shot(SrBattleAttack *a,uint32_t ticks) {
    SrBattleUnit *u=&a->battle->units[a->shooter],*d=&a->battle->units[a->target];
    a->damage=sr_battle_damage(a->rng,&u->troop,&d->troop,a->counter,a->commander);
    --u->actions; /* A surviving defender counters even with no actions left. */
    a->start=ticks;a->applied=0;a->active=1;
    /* Troop.shoot uses direction-specific cavalry sounds for either nation. */
    if(u->troop.type==SR_BATTLE_INF) a->sound=u->troop.level==1?SR_KRIG_HALBERD:SR_KRIG_GUNSHOT;
    else {
        int delta=u->square-d->square,enemy=u->troop.nation==SR_BATTLE_ENEMY;
        a->sound=u->troop.level==1?SR_KRIG_CROSSBOW:SR_KRIG_GUNSHOT;
        if(u->troop.level==1 && delta==(enemy?-7:7)) a->sound=SR_KRIG_HORSE_KICK;
        if(u->troop.level>1 && delta==(enemy?1:-1)) a->sound=SR_KRIG_SABRE;
    }
}
int sr_battle_attack_begin(SrBattleAttack *a,SrBattleSetup *b,SrRandom *rng,
        double commander,int shooter,int target,uint32_t ticks) {
    if(!a || a->active || !b || !b->ready || !rng || commander<=0 ||
       shooter<0 || target<0 || shooter>=b->swedes+b->enemies || target>=b->swedes+b->enemies) return 0;
    SrBattleUnit *u=&b->units[shooter],*d=&b->units[target];
    if(!u->placed || !d->placed || u->actions<=0 || u->troop.troops<=0 || d->troop.troops<=0 ||
       u->troop.nation==d->troop.nation || d->troop.attacked<1 || d->troop.attacked>7) return 0;
    SrBattleRange r;int allowed=0;
    if(!sr_battle_range(b->contents,u->square,1,1,u->troop.nation,&r)) return 0;
    for(int n=0;n<r.count;++n) if(r.squares[n]==d->square && r.actions[n]==SR_BATTLE_SHOOT) allowed=1;
    if(!allowed) return 0;
    /* Reject malformed troop records before mutating either unit or the RNG. */
    if(u->troop.type<0 || u->troop.type>1 || d->troop.type<0 || d->troop.type>1 ||
       u->troop.level<1 || u->troop.level>3 || d->troop.level<1 || d->troop.level>3) return 0;
    *a=(SrBattleAttack){.battle=b,.rng=rng,.commander=commander,.shooter=shooter,
        .target=target,.advance_unit=-1};
    begin_shot(a,ticks);return 1;
}
int sr_battle_attack_gun(SrBattleAttack *a,SrBattleSetup *b,SrRandom *rng,int side,int target,uint32_t ticks) {
    if(!a || a->active || !b || !b->ready || !rng || side<0 || side>1 ||
       !b->guns[side].count || target<0 || target>=b->swedes+b->enemies) return 0;
    SrBattleUnit *u=&b->units[target];
    if(!u->placed || u->troop.troops<=0 || u->troop.nation==side+1) return 0;
    int damage=sr_battle_artillery(rng,b->armies[side].level,b->guns[side].per_icon,u->troop.type);
    if(damage<0) return 0;
    *a=(SrBattleAttack){.battle=b,.rng=rng,.start=ticks,.active=1,.shooter=-1,
        .target=target,.damage=damage,.advance_unit=-1,.artillery=1,
        .sound=b->armies[side].level==3?SR_KRIG_CANNON3:SR_KRIG_CANNON12};
    return 1;
}
void sr_battle_attack_tick(SrBattleAttack *a,uint32_t ticks) {
    if(!a) return;
    while(a->active) {
        uint32_t elapsed=ticks-a->start,duration=a->damage?72:a->artillery?24:44;
        SrBattleUnit *d=&a->battle->units[a->target];
        if(elapsed>=24 && !a->applied) {
            if(a->damage) d->troop.troops-=a->damage;
            a->applied=1;
        }
        if(elapsed<duration) return;
        a->end=a->start+duration;
        if(d->troop.troops<=0) {
            int square=d->square;
            if(d->troop.nation==SR_BATTLE_SWED) a->swed_actions_lost+=d->actions;
            a->battle->contents[square]=SR_BATTLE_EMPTY;
            d->placed=0;d->square=0;d->x=d->y=1000;
            if(!a->counter && !a->artillery) {a->advance_unit=a->shooter;a->advance_square=square;}
            a->active=0;
        } else if(!a->counter && !a->artillery) {
            int first=a->shooter;a->shooter=a->target;a->target=first;a->counter=1;
            begin_shot(a,a->end);
        } else a->active=0;
    }
}
int sr_battle_attack_pose(const SrBattleAttack *a,int unit,uint32_t ticks,int *frame,int *dir) {
    if(!a || !a->active || !frame || !dir) return 0;
    uint32_t elapsed=ticks-a->start;*dir=0;
    if(!a->artillery && unit==a->shooter && elapsed<44) {
        int from=a->battle->units[a->shooter].square,to=a->battle->units[a->target].square;
        switch(from-to) {case 7:*dir=2;break;case 1:*dir=4;break;case -1:*dir=5;break;case -7:*dir=7;break;}
        *frame=(int)(elapsed/4);return 2;
    }
    if(unit==a->target && a->damage && elapsed>=24) {
        *frame=(int)((elapsed-24)/4);return a->battle->units[unit].troop.troops<=0?3:4;
    }
    return 0;
}
