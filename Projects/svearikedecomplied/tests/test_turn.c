#include "turn.h"
#include "crossbow.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
static int run_to(SrGame *g,SrTurn *t,int target) {
    for(int i=0;i<100;++i) {
        const SrTurnRequest *r=sr_turn_advance(g,t);
        if(r->type==target) return 1;
        if(r->type==SR_TURN_YEAR || r->type==SR_TURN_EVENT || r->type==SR_TURN_UNREST ||
           r->type==SR_TURN_MINIGAME_RESULT || r->type==SR_TURN_TRADE_LOST || r->type==SR_TURN_LOST_AREA) {
            if(!sr_turn_respond(g,t,0)) return 0;
        } else return 0;
    }
    return 0;
}
int main(void) {
    /* Full first-turn transactions for every family and multiple deterministic
       streams. The isolated protocol test supplies a completed zero-score
       minigame result; the browser must wait for actual minigame code. */
    const int money[]={1036,1040,1050,1300,1150},food[]={106,160,124,112,112},metal[]={104,157,110,105,107};
    for(int family=1;family<=5;++family) for(int seed=0;seed<20;++seed) {
        sr_game_seed((uint32_t)seed); CHECK(sr_game_begin(family));
        SrGame g=*sr_game_state(); SrTurn t={0}; CHECK(sr_turn_begin(&g,&t));
        CHECK(t.event_slot!=t.event_b_slot && t.event_slot>=1 && t.event_slot<=5);
        SrGame saved=g; CHECK(!sr_turn_begin(&g,&t) && !memcmp(&saved,&g,sizeof g));
        int years=0;
        for(int step=0;step<30;++step) {
            const SrTurnRequest *r=sr_turn_advance(&g,&t);
            saved=g; SrTurn snapshot=t;
            CHECK(sr_turn_advance(&g,&t)->type==r->type && !memcmp(&saved,&g,sizeof g) && !memcmp(&snapshot,&t,sizeof t));
            if(r->type==SR_TURN_FINISHED) break;
            if(r->type==SR_TURN_YEAR) { CHECK(g.year==1523+years); ++years; }
            if(r->type==SR_TURN_MINIGAME) {
                CHECK(!sr_turn_respond(&g,&t,0)); CHECK(sr_turn_minigame_finished(&g,&t,0));
                CHECK(!sr_turn_minigame_finished(&g,&t,10));
            } else CHECK(sr_turn_respond(&g,&t,0));
        }
        CHECK(!t.active && t.pending.type==SR_TURN_FINISHED && years==5 && g.year==1528 && g.turn==2);
        CHECK(g.silver==money[family-1] && g.crops==food[family-1] && g.metal==metal[family-1]);
        CHECK(g.family_head_count>=1 && g.family_head_count<=2);
    }
    sr_game_seed(1); CHECK(sr_game_begin(3)); SrGame g=*sr_game_state(); SrTurn t={0};
    g.countries[0].relation=10; g.trade_crops[0]=10; g.no_riot=1;
    CHECK(sr_turn_begin(&g,&t) && run_to(&g,&t,SR_TURN_WAR));
    CHECK(t.pending.country==1 && t.war_countries==1 && g.turn==2 && g.year==1528 && g.silver==1000);
    SrGame saved=g; CHECK(!sr_turn_respond(&g,&t,0) && !memcmp(&saved,&g,sizeof g));
    CHECK(!sr_turn_minigame_finished(&g,&t,10));
    CHECK(sr_turn_war_finished(&g,&t) && run_to(&g,&t,SR_TURN_TRADE_LOST));
    CHECK(t.pending.country==1 && !g.trade_crops[0]);
    CHECK(sr_turn_respond(&g,&t,0) && run_to(&g,&t,SR_TURN_FINISHED));
    CHECK(g.silver==1050 && !g.no_riot);

    g=*sr_game_state(); t=(SrTurn){0}; g.turn=5; g.year=1543;
    CHECK(sr_turn_begin(&g,&t) && run_to(&g,&t,SR_TURN_MINIGAME));
    CHECK(t.pending.event.kind==SR_COMMANDERS && t.pending.event.record==1);
    CHECK(!strcmp(sr_event_minigame(t.pending.event),"ARMBORST"));
    /* Play the actual crossbow C implementation on the same game RNG. Aim
       against the midpoint wind displacement; all residual error stays inside
       the innermost original bitmap. No injected minigame score here. */
    const int wind_mod[8][2]={{2,0},{1,1},{0,1},{-1,1},{-2,0},{-1,-1},{0,-2},{1,-1}};
    SrCrossbow bow; sr_crossbow_init(&bow); CHECK(sr_crossbow_start(&bow,&g.random));
    int shots=0;
    for(uint32_t ticks=1;ticks<10000 && bow.phase!=SR_CROSSBOW_DONE;++ticks) {
        if(bow.phase==SR_CROSSBOW_AIM) {
            int x=320-wind_mod[bow.wind-1][0]*8;
            int y=(264-wind_mod[bow.wind-1][1]*8+1730)/4;
            CHECK(sr_crossbow_down(&bow,300,300));
            sr_crossbow_move(&bow,300+x-bow.start_x,300+y-bow.start_y);
            CHECK(sr_crossbow_up(&bow)); ++shots;
        }
        sr_crossbow_step(&bow,&g.random,ticks);
    }
    CHECK(shots==15 && bow.phase==SR_CROSSBOW_DONE && bow.score==75 && bow.result==10);
    CHECK(sr_turn_minigame_finished(&g,&t,bow.result) && t.pending.type==SR_TURN_MINIGAME_RESULT);
    CHECK(g.silver==2000 && g.people[SR_COMMANDERS].owned.records[0]==1 && !g.people[SR_COMMANDERS].available.count);
    CHECK(!sr_turn_minigame_finished(&g,&t,bow.result));
    CHECK(sr_turn_respond(&g,&t,0) && run_to(&g,&t,SR_TURN_FINISHED) && g.silver==2050);

    /* A riot is guaranteed at unrest 100. Refused suppression consumes nothing. */
    g=*sr_game_state(); t=(SrTurn){0}; g.areas[1].tax_level=5; g.areas[1].riot=70;
    g.used_events=(SrRecordList){2,{1,2}}; /* No revolt-suppressing event this turn. */
    CHECK(sr_turn_begin(&g,&t) && run_to(&g,&t,SR_TURN_RIOT));
    saved=g; CHECK(!sr_turn_respond(&g,&t,SR_RIOT_STRIKE) && t.error==SR_RIOT_NO_TROOPS && !memcmp(&saved,&g,sizeof g));
    g.areas[1].infantry=1999; g.troop_level=3; g.silver=4; saved=g;
    CHECK(!sr_turn_respond(&g,&t,SR_RIOT_STRIKE) && t.error==SR_RIOT_NO_SILVER && !memcmp(&saved,&g,sizeof g));
    g.silver=5; CHECK(sr_turn_respond(&g,&t,SR_RIOT_STRIKE));
    CHECK(g.silver==0 && g.areas[1].riot==80 && t.riots[1]==2);
    CHECK(run_to(&g,&t,SR_TURN_GAME_OVER) && g.crushed && !g.player_areas.count);
    CHECK(!sr_turn_begin(&g,&t));
    g=*sr_game_state(); t=(SrTurn){0}; g.areas[1].tax_level=5; g.areas[1].riot=70; g.used_events=(SrRecordList){2,{1,2}};
    CHECK(sr_turn_begin(&g,&t) && run_to(&g,&t,SR_TURN_RIOT));
    CHECK(sr_turn_respond(&g,&t,SR_RIOT_CONCEDE));
    CHECK(g.areas[1].tax_level==1 && g.areas[1].riot==50 && t.riots[1]==4);
    CHECK(run_to(&g,&t,SR_TURN_FINISHED) && g.silver==1050 && g.crops==124);

    g=*sr_game_state(); t=(SrTurn){0}; g.crops=0; g.areas[1].infantry=25000; g.areas[1].military_level=5;
    CHECK(sr_turn_begin(&g,&t) && run_to(&g,&t,SR_TURN_STARVING));
    CHECK(g.silver==1050 && g.crops==24);
    saved=g; CHECK(!sr_turn_respond(&g,&t,0) && !memcmp(&saved,&g,sizeof g));
    CHECK(sr_turn_advance(&g,&t)->type==SR_TURN_STARVING && !memcmp(&saved,&g,sizeof g));
    g.areas[1].infantry=24000;
    CHECK(sr_turn_respond(&g,&t,0) && run_to(&g,&t,SR_TURN_FINISHED));
    CHECK(g.silver==1026 && g.crops==0); /* Income awarded once; upkeep deducted once. */
    puts("Five-year turn transactions, idempotent waits, event/minigame/war boundaries, riots and starvation resume passed.");
    return 0;
}
