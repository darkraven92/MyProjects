#include "battle_ai.h"
static const SrBattleTroop *troop(const SrBattleSetup *b,int square) {
    for(int n=0;n<b->swedes+b->enemies;++n)
        if(b->units[n].placed && b->units[n].square==square) return &b->units[n].troop;
    return 0;
}
static int content(const SrBattleSetup *b,int square) {
    return square?b->contents[square]:SR_BATTLE_BLOCKED;
}
static int attack(const SrBattleSetup *b,int square,double strength,int forward) {
    if(content(b,square)!=SR_BATTLE_SWED) return 0;
    const SrBattleTroop *t=troop(b,square);
    if(!t) return 0;
    double factor=t->type==SR_BATTLE_INF?(forward?1.5:1.25):(forward?1.25:1);
    return t->troops<(int)(strength*factor+0.5);
}
static int side(SrRandom *rng,int col,int left,int right) {
    if(col==4) {
        (void)sr_random_next(rng,2);
        return left; /* Both original random branches select leftSquare. */
    }
    return col>4?right:left;
}
int sr_battle_enemy_decide(const SrBattleSetup *b,SrRandom *rng,int square,SrBattleDecision *out) {
    if(!b || !rng || !out || square<1 || square>63 || b->contents[square]!=SR_BATTLE_ENEMY) return 0;
    const SrBattleTroop *t=troop(b,square);
    if(!t || t->nation!=SR_BATTLE_ENEMY) return 0;
    if(square<=7) {*out=(SrBattleDecision){SR_BATTLE_ENEMY_WINS,square};return 1;}
    int col=(square-1)%7+1,left=col<7?square+1:0,right=col>1?square-1:0;
    int forward=square-7,back=square<=56?square+7:0;
    SrBattleDecision d={SR_BATTLE_WAIT,square};
    int l=content(b,left),r=content(b,right),f=content(b,forward);
    if(l==SR_BATTLE_EMPTY && r==SR_BATTLE_EMPTY) d=(SrBattleDecision){SR_BATTLE_MOVE_SIDE,side(rng,col,left,right)};
    else if(l==SR_BATTLE_EMPTY) d=(SrBattleDecision){SR_BATTLE_MOVE_SIDE,left};
    else if(r==SR_BATTLE_EMPTY) d=(SrBattleDecision){SR_BATTLE_MOVE_SIDE,right};
    if(f==SR_BATTLE_EMPTY) d=(SrBattleDecision){SR_BATTLE_MOVE_FORWARD,forward};
    int attack_left=attack(b,left,t->troops,0),attack_right=attack(b,right,t->troops,0);
    if(attack_left && attack_right) d=(SrBattleDecision){SR_BATTLE_ATTACK_SIDE,side(rng,col,left,right)};
    else if(attack_left) d=(SrBattleDecision){SR_BATTLE_ATTACK_SIDE,left};
    else if(attack_right) d=(SrBattleDecision){SR_BATTLE_ATTACK_SIDE,right};
    if(attack(b,forward,t->troops,1)) d=(SrBattleDecision){SR_BATTLE_ATTACK_FORWARD,forward};
    else if(f==SR_BATTLE_SWED && d.action==SR_BATTLE_WAIT) {
        int roll=sr_random_next(rng,2);
        /* Source tests `square` (the forward square), not backSquare. Since
           forwardCont is Swedish here, this retreat branch cannot succeed. */
        if(roll==1 && back && content(b,forward)==SR_BATTLE_EMPTY)
            d=(SrBattleDecision){SR_BATTLE_MOVE_BACK,back};
    }
    /* Strict <63 in source deliberately excludes a Swedish unit at tile 63. */
    if(back && back<63 && content(b,back)==SR_BATTLE_SWED)
        d=(SrBattleDecision){SR_BATTLE_ATTACK_BACK,back};
    *out=d;return 1;
}
