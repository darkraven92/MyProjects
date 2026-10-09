#include "crossbow.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
static int until(SrCrossbow *b,SrRandom *r,uint32_t *ticks,int phase) {
    for(int i=0;i<1000 && b->phase!=phase;++i) sr_crossbow_step(b,r,++*ticks);
    return b->phase==phase;
}
static void aim(SrCrossbow *b,int x,int y) {
    sr_crossbow_down(b,300,300);
    sr_crossbow_move(b,300+x-b->start_x,300+y-b->start_y);
    sr_crossbow_up(b);
}
int main(void) {
    /* The center and corners distinguish the original silhouette from a box. */
    CHECK(sr_crossbow_score(320,264)==5);
    CHECK(sr_crossbow_score(309,253)<5);
    CHECK(sr_crossbow_score(320,285)==3);
    CHECK(sr_crossbow_score(320,315)==1);
    CHECK(sr_crossbow_score(230,130)==0 && !sr_crossbow_target(0,230,130));
    CHECK(sr_crossbow_target(0,320,264));
    CHECK(!sr_crossbow_target(-1,320,264) && !sr_crossbow_target(6,320,264));
    const int low[]={0,30,41,48,54,59,63,67,70,73,75};
    for(int i=1;i<=10;++i) {
        CHECK(sr_crossbow_result(low[i])==i);
        CHECK(sr_crossbow_result(low[i]-1)==i-1);
    }
    CHECK(!sr_crossbow_result(-1) && !sr_crossbow_result(76));
    SrCrossbow b; SrRandom r; uint32_t ticks=0;
    sr_crossbow_init(&b); sr_random_seed(&r,1);
    CHECK(!sr_crossbow_down(&b,320,300) && !sr_crossbow_up(&b) && !r.calls);
    CHECK(sr_crossbow_start(&b,&r) && !sr_crossbow_start(&b,&r));
    CHECK(b.round==1 && b.bolts==5 && b.wind==4 && r.calls==1 && b.sound==31);
    CHECK(until(&b,&r,&ticks,SR_CROSSBOW_AIM));
    CHECK(r.calls==3 && b.start_x==283 && b.start_y==512);
    /* Integer animation steps do not reach the exact logical start point. */
    CHECK(b.bow_x==290 && b.bow_y==520);
    CHECK(sr_crossbow_down(&b,320,300));
    CHECK(b.bow_x==283 && b.bow_y==512 && b.aim_y==318);
    sr_crossbow_move(&b,INT_MAX,INT_MIN);
    CHECK(b.bow_x==410 && b.bow_y==465 && b.hit_y==130);
    sr_crossbow_move(&b,INT_MIN,INT_MAX);
    CHECK(b.bow_x==230 && b.bow_y==525 && b.hit_y==370);
    sr_crossbow_cancel(&b);
    CHECK(!sr_crossbow_up(&b) && b.bolts==5 && r.calls==3);
    aim(&b,320,499); /* Aim at (320,266), then apply original wind. */
    CHECK(!sr_crossbow_up(&b));
    CHECK(until(&b,&r,&ticks,SR_CROSSBOW_HIT_WAIT));
    CHECK(r.calls==5 && b.hit_x==312 && b.hit_y==272 && b.sound==32);
    uint32_t wait=b.wait_start;
    SrCrossbow saved=b; sr_crossbow_step(&b,&r,wait+14);
    CHECK(!memcmp(&b,&saved,sizeof b) && r.calls==5);
    sr_crossbow_step(&b,&r,wait+15); ticks=wait+15;
    CHECK(b.bolts==4 && b.score==5 && r.calls==6 && b.stuck[4].variant==2);
    CHECK(b.stuck[4].x==312 && b.stuck[4].y==272);
    CHECK(until(&b,&r,&ticks,SR_CROSSBOW_RELOAD));
    wait=b.wait_start;
    sr_crossbow_step(&b,&r,wait+44); CHECK(r.calls==6);
    sr_crossbow_step(&b,&r,wait+45); ticks=wait+45; CHECK(r.calls==7);
    CHECK(until(&b,&r,&ticks,SR_CROSSBOW_AIM));
    CHECK(b.bow_member==13 && r.calls==9);
    aim(&b,230,465); /* Certain miss, no cosmetic variant draw. */
    CHECK(until(&b,&r,&ticks,SR_CROSSBOW_RELOAD));
    CHECK(r.calls==11 && b.bolts==3 && b.score==5 && !b.target_hit && b.sound==33);
    /* Finish all fifteen shots with edge misses: three rounds, zero new hits,
       two unconditional wind-offset draws per shot, no draws after completion. */
    int shots=2;
    while(b.phase!=SR_CROSSBOW_DONE && ticks<10000) {
        if(b.phase==SR_CROSSBOW_AIM) { aim(&b,230,465); ++shots; }
        sr_crossbow_step(&b,&r,++ticks);
    }
    CHECK(shots==15 && b.round==3 && b.bolts==0 && b.score==5 && b.result==0);
    CHECK(r.calls==76); /* 15*(wind + startH + startV + dx + dy), plus one hit. */
    saved=b; uint32_t calls=r.calls;
    for(int i=0;i<100;++i) sr_crossbow_step(&b,&r,++ticks);
    CHECK(!memcmp(&b,&saved,sizeof b) && r.calls==calls && !sr_crossbow_start(&b,&r));
    puts("Crossbow silhouettes, 15-shot lifecycle, wind/draw order, drag limits, delays and result tiers passed.");
    return 0;
}
