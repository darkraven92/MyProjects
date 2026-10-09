#include "crossbow.h"
typedef struct { int x,y,w,h; const unsigned char *bits; } SrCrossbowMask;
#include "generated/crossbow_targets.h"

int sr_crossbow_target(int target,int x,int y) {
    if(target<0 || target>5) return 0;
    const SrCrossbowMask *m=&sr_crossbow_targets[target];
    x-=m->x; y-=m->y;
    if(x<0 || y<0 || x>=m->w || y>=m->h) return 0;
    int bit=y*m->w+x;
    return (m->bits[bit/8]>>(bit%8))&1;
}
int sr_crossbow_score(int x,int y) {
    for(int i=1;i<=5;++i) if(sr_crossbow_target(i,x,y)) return 6-i;
    return 0;
}
int sr_crossbow_result(int score) {
    /* ScoreScript 28 lists every value. Values outside 30..75 return zero. */
    static const int minimum[]={30,41,48,54,59,63,67,70,73,75};
    if(score>75) return 0;
    for(int i=9;i>=0;--i) if(score>=minimum[i]) return i+1;
    return 0;
}
void sr_crossbow_init(SrCrossbow *b) {
    *b=(SrCrossbow){0};
    b->bow_x=320; b->bow_y=580; b->bow_member=11;
    b->aim_y=251; b->wind=b->wind_picture=1;
}
static void wind(SrCrossbow *b,SrRandom *r) {
    int old=b->wind;
    b->wind=sr_random_next(r,8);
    b->wind_steps=16+b->wind-old;
    b->phase=SR_CROSSBOW_WIND;
    b->sound=31;
}
static void round_start(SrCrossbow *b,SrRandom *r) {
    ++b->round; b->bolts=5;
    for(int i=0;i<5;++i) b->stuck[i]=(SrCrossbowBolt){0};
    wind(b,r);
}
int sr_crossbow_start(SrCrossbow *b,SrRandom *r) {
    if(b->phase!=SR_CROSSBOW_INTRO) return 0;
    round_start(b,r);
    return 1;
}
static void lower(SrCrossbow *b) {
    b->phase=SR_CROSSBOW_LOWER; b->step=0;
    b->step_x=(320-b->bow_x)/10; b->step_y=(580-b->bow_y)/10;
    b->start_x=320; b->start_y=580;
}
static void score_shot(SrCrossbow *b) {
    b->last_points=sr_crossbow_score(b->hit_x,b->hit_y);
    b->score+=b->last_points; --b->bolts;
    lower(b);
}
void sr_crossbow_step(SrCrossbow *b,SrRandom *r,uint32_t ticks) {
    static const int wind_mod[8][2]={{2,0},{1,1},{0,1},{-1,1},{-2,0},{-1,-1},{0,-2},{1,-1}};
    switch(b->phase) {
    case SR_CROSSBOW_WIND:
        b->wind_picture=b->wind_picture%8+1;
        if(--b->wind_steps==0) {
            b->start_x=sr_random_next(r,100)+270;
            b->start_y=sr_random_next(r,40)+475;
            b->bow_x=320; b->bow_y=580;
            b->step_x=(b->start_x-320)/10; b->step_y=(b->start_y-580)/10;
            b->step=0; b->phase=SR_CROSSBOW_LOAD;
        }
        break;
    case SR_CROSSBOW_LOAD:
        b->bow_x+=b->step_x; b->bow_y+=b->step_y;
        if(++b->step==10) b->phase=SR_CROSSBOW_AIM;
        break;
    case SR_CROSSBOW_SHOOT:
        if(b->step<2) { b->bow_member=13+b->step++; break; }
        b->bow_member=16;
        /* Both random(5) calls occur even when one wind component is zero. */
        b->hit_x+=wind_mod[b->wind-1][0]*(sr_random_next(r,5)+5);
        b->hit_y+=wind_mod[b->wind-1][1]*(sr_random_next(r,5)+5);
        b->target_hit=sr_crossbow_target(0,b->hit_x,b->hit_y);
        b->sound=b->target_hit?32:33;
        if(b->target_hit) { b->phase=SR_CROSSBOW_HIT_WAIT; b->wait_start=ticks; }
        else score_shot(b);
        break;
    case SR_CROSSBOW_HIT_WAIT:
        if(ticks-b->wait_start>=15) {
            b->stuck[b->bolts-1]=(SrCrossbowBolt){b->hit_x,b->hit_y,sr_random_next(r,4)};
            score_shot(b);
        }
        break;
    case SR_CROSSBOW_LOWER:
        b->bow_x+=b->step_x; b->bow_y+=b->step_y;
        if(++b->step==10) {
            b->bow_member=13; b->phase=SR_CROSSBOW_RELOAD; b->wait_start=ticks;
        }
        break;
    case SR_CROSSBOW_RELOAD:
        if(ticks-b->wait_start>=45) {
            if(b->bolts) wind(b,r);
            else { b->phase=SR_CROSSBOW_ROUND_WAIT; b->wait_start=ticks; }
        }
        break;
    case SR_CROSSBOW_ROUND_WAIT:
        if(ticks-b->wait_start>=100) {
            if(b->round==3) { b->phase=SR_CROSSBOW_FINISH; b->wait_start=ticks; }
            else round_start(b,r);
        }
        break;
    case SR_CROSSBOW_FINISH:
        /* Frames 35..38 at the movie's 15 fps. */
        if(ticks-b->wait_start>=12) { b->result=sr_crossbow_result(b->score); b->phase=SR_CROSSBOW_DONE; }
        break;
    default: break;
    }
}
static int clamp(int64_t v,int low,int high) { return v<low?low:v>high?high:(int)v; }
int sr_crossbow_down(SrCrossbow *b,int x,int y) {
    if(b->phase!=SR_CROSSBOW_AIM || b->dragging) return 0;
    b->dragging=1; b->mouse_x=x; b->mouse_y=y;
    sr_crossbow_move(b,x,y);
    return 1;
}
void sr_crossbow_move(SrCrossbow *b,int x,int y) {
    if(b->phase!=SR_CROSSBOW_AIM || !b->dragging) return;
    b->bow_x=clamp((int64_t)b->start_x+x-b->mouse_x,230,410);
    b->bow_y=clamp((int64_t)b->start_y+y-b->mouse_y,465,525);
    b->aim_y=4*b->bow_y-1730;
    b->hit_x=b->bow_x; b->hit_y=b->aim_y;
}
int sr_crossbow_up(SrCrossbow *b) {
    if(b->phase!=SR_CROSSBOW_AIM || !b->dragging) return 0;
    b->dragging=0; b->phase=SR_CROSSBOW_SHOOT; b->step=0;
    return 1;
}
void sr_crossbow_cancel(SrCrossbow *b) { b->dragging=0; }
