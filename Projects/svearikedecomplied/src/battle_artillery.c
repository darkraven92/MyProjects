#include "battle_artillery.h"
static int at(const SrBattleSetup *b,int nation,int square) {
    for(int n=0;n<b->swedes+b->enemies;++n) {
        const SrBattleUnit *u=&b->units[n];
        if(u->placed && u->troop.troops>0 && u->troop.nation==nation && u->square==square) return n;
    }
    return -1;
}
static int row_target(const SrBattleSetup *b,int first) {
    int best=-1,score=-1;static const int neighbors[]={-7,-1,1,7};
    for(int square=first;square<first+7;++square) {
        int n=at(b,SR_BATTLE_SWED,square);if(n<0) continue;
        int threat=1000;
        for(int k=0;k<4;++k) {
            /* Source uses raw square +/-1, including row wrap. */
            int e=at(b,SR_BATTLE_ENEMY,square+neighbors[k]);if(e<0) continue;
            double enemy=b->units[e].troop.troops,swed=b->units[n].troop.troops;
            threat-=20;if(enemy>swed) threat-=50;if(enemy>2*swed) threat-=50;
        }
        if(threat>=score) {best=n;score=threat;}
    }
    return best;
}
int sr_battle_artillery_target(const SrBattleSetup *b) {
    if(!b || !b->ready) return -1;
    int n=row_target(b,57);if(n>=0) return n;
    for(int square=8;square<=14;++square) {
        int e=at(b,SR_BATTLE_ENEMY,square),s=at(b,SR_BATTLE_SWED,square-7);
        if(e>=0 && s>=0 && b->units[s].troop.troops>b->units[e].troop.troops) return s;
    }
    for(int square=8;square<=14;++square) {
        int e=at(b,SR_BATTLE_ENEMY,square);if(e<0) continue;
        int s=at(b,SR_BATTLE_SWED,square-6);
        if(square!=14 && s>=0) {
            if(b->units[s].troop.troops>b->units[e].troop.troops) return s;
            continue; /* Source skips the other diagonal even if this one is weaker. */
        }
        s=at(b,SR_BATTLE_SWED,square-8);
        if(s>=0 && b->units[s].troop.troops>b->units[e].troop.troops) return s;
    }
    for(int row=50;row>=36;row-=7) {n=row_target(b,row);if(n>=0) return n;}
    int strongest[2]={-1,-1};
    for(int k=0;k<b->swedes;++k) {
        const SrBattleUnit *u=&b->units[k];if(!u->placed || u->troop.troops<=0) continue;
        int type=u->troop.type;
        if(strongest[type]<0 || u->troop.troops>=b->units[strongest[type]].troop.troops) strongest[type]=k;
    }
    if(strongest[0]>=0 && b->units[strongest[0]].troop.troops>500) return strongest[0];
    if(strongest[1]>=0 && b->units[strongest[1]].troop.troops>500) return strongest[1];
    return strongest[0]>=0?strongest[0]:strongest[1];
}
int sr_battle_gun_ready(int *reload,int level) {
    if(!reload || level<1 || level>3 || *reload<1) return 0;
    if(*reload==5-level) return 1;
    ++*reload;return 0;
}
