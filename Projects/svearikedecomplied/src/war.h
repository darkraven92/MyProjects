#ifndef SR_WAR_H
#define SR_WAR_H
#include "game.h"
enum { SR_WAR_DECLARE, SR_WAR_OFFER, SR_WAR_NO_OFFER, SR_WAR_NO_TROOPS,
    SR_WAR_TARGET, SR_WAR_REGIMENT, SR_WAR_QUICK, SR_WAR_RESULT,
    SR_WAR_DONE, SR_WAR_FULL_PENDING };
enum { SR_WAR_OK, SR_WAR_INVALID, SR_WAR_SILVER, SR_WAR_EMPTY_REGIMENT };
typedef struct {
    int active,phase,country,attacking,full,area,regiment,error;
    int offer_type,offer_area,won,surrender,applied,enemy_level;
    int retreated,enemy_original_real,enemy_dead_real;
    double offer_silver,fee;
    SrRecordList targets;
    double swed[3],enemy[3],swed_dead[3],enemy_dead[3],commander_bonus;
    int territory; /* Positive acquisition, negative loss, zero money/no transfer. */
} SrWar;
int sr_war_begin(const SrGame *g,SrWar *w,int country);
int sr_war_negotiate(SrGame *g,SrWar *w);
int sr_war_accept(SrGame *g,SrWar *w);
int sr_war_refuse(SrWar *w);
int sr_war_choose_battle(const SrGame *g,SrWar *w,int full);
int sr_war_select_target(const SrGame *g,SrWar *w,int position);
int sr_war_confirm_target(const SrGame *g,SrWar *w);
int sr_war_select_regiment(const SrGame *g,SrWar *w,int position);
int sr_war_fight(SrGame *g,SrWar *w);
int sr_war_surrender(SrGame *g,SrWar *w);
/* Quick-result OK applies outcome once; result OK then completes the war. */
int sr_war_advance_result(SrGame *g,SrWar *w);
/* KRIG returns immediately to the campaign result screen. Loss order is
   Swedish infantry/cavalry, enemy infantry/cavalry; real bits are 1/2. */
int sr_war_finish_manual(SrGame *g,SrWar *w,int won,int retreated,
    const double losses[4],int enemy_dead_real);
#endif
