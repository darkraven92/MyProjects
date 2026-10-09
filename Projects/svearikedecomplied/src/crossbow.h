#ifndef SVEA_CROSSBOW_H
#define SVEA_CROSSBOW_H
#include "random.h"
enum {
    SR_CROSSBOW_INTRO, SR_CROSSBOW_WIND, SR_CROSSBOW_LOAD,
    SR_CROSSBOW_AIM, SR_CROSSBOW_SHOOT, SR_CROSSBOW_HIT_WAIT,
    SR_CROSSBOW_LOWER, SR_CROSSBOW_RELOAD, SR_CROSSBOW_ROUND_WAIT,
    SR_CROSSBOW_FINISH, SR_CROSSBOW_DONE
};
typedef struct { int x,y,variant; } SrCrossbowBolt;
typedef struct {
    int phase, round, bolts, score, result, wind, wind_picture, wind_steps;
    int bow_x,bow_y,bow_member,aim_y,start_x,start_y,step_x,step_y,step;
    int dragging,mouse_x,mouse_y,hit_x,hit_y,target_hit,last_points,sound;
    uint32_t wait_start;
    /* Original channels 14..18 (shadows), 20..24 (bolts), indexed low first. */
    SrCrossbowBolt stuck[5];
} SrCrossbow;
void sr_crossbow_init(SrCrossbow *bow);
int sr_crossbow_start(SrCrossbow *bow,SrRandom *random);
/* One updateStage per call; elapsed ticks are original 1/60-second units.
   Browser presentation cadence is provisional, independent of random draws. */
void sr_crossbow_step(SrCrossbow *bow,SrRandom *random,uint32_t ticks);
int sr_crossbow_down(SrCrossbow *bow,int x,int y);
void sr_crossbow_move(SrCrossbow *bow,int x,int y);
int sr_crossbow_up(SrCrossbow *bow);
void sr_crossbow_cancel(SrCrossbow *bow);
int sr_crossbow_target(int target,int x,int y); /* 0: board, 1..5: inner to outer. */
int sr_crossbow_score(int x,int y);
int sr_crossbow_result(int score);
#endif
