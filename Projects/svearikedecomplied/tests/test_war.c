#include "war.h"
#include "turn.h"
#include "diplomacy.h"
#include "economy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static int near(double a,double b) {return a-b<0.000001 && b-a<0.000001;}
static int reach(SrGame *g,SrTurn *t,int target) {
    for(int n=0;n<60;++n) {
        int type=sr_turn_advance(g,t)->type;
        if(type==target) return 1;
        if(type!=SR_TURN_YEAR && type!=SR_TURN_EVENT && type!=SR_TURN_TRADE_LOST && type!=SR_TURN_UNREST) return 0;
        if(!sr_turn_respond(g,t,0)) return 0;
    }
    return 0;
}
int main(void) {
    sr_game_seed(1);CHECK(sr_game_begin(1));SrGame base=*sr_game_state(),g,saved;SrWar w;
    /* Seven-draw evenly matched branches, with original Windows random vectors.
       Strengths: Sweden 14000, Denmark 18500. Seeds 0/4 choose opposite winners. */
    for(int win=0;win<=1;++win) {
        g=base;sr_random_seed(&g.random,win?4:0);CHECK(sr_war_begin(&g,&w,1));
        CHECK(w.phase==SR_WAR_DECLARE && !w.attacking && !g.random.calls);
        CHECK(sr_war_choose_battle(&g,&w,0) && w.phase==SR_WAR_REGIMENT && w.area==7 && w.regiment==6);
        CHECK(sr_war_fight(&g,&w) && w.phase==SR_WAR_QUICK && w.won==win && g.random.calls==7);
        CHECK(g.areas[6].infantry==2000 && g.countries[0].relation==2);
        CHECK(near(w.swed_dead[0],win?460:1080) && near(w.swed_dead[1],win?780:940));
        CHECK(near(w.enemy_dead[0],win?1080:700) && near(w.enemy_dead[1],win?795:330));
        CHECK(sr_war_advance_result(&g,&w) && w.phase==SR_WAR_RESULT && w.applied);
        CHECK(g.random.calls==8 && g.war_wins==win);
        CHECK(g.point_mod==base.point_mod+(win?4:-3));
        CHECK(g.areas[6].infantry==(win?1540:920) && g.areas[6].cavalry==(win?1220:1060));
        CHECK(g.countries[0].infantry==3000 && g.countries[0].cavalry==1500 && g.countries[0].artillery==1000);
        CHECK(!g.countries[0].troops_real);
        CHECK(win?(w.fee==500 && g.silver==1500):(w.territory==-7 && g.areas[7].owner=='B'));
        saved=g;CHECK(sr_war_advance_result(&g,&w) && w.phase==SR_WAR_DONE && !memcmp(&g,&saved,sizeof g));
        CHECK(!sr_war_advance_result(&g,&w) && !memcmp(&g,&saved,sizeof g));
    }
    /* Fractional enemy survivors are not rounded at battle resolution. */
    g=base;g.areas[6].infantry=1;g.areas[6].cavalry=0;
    g.countries[0].infantry=10001;g.countries[0].cavalry=5001;
    sr_random_seed(&g.random,0);CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,0) && sr_war_fight(&g,&w));
    CHECK(!w.won && g.random.calls==5 && sr_war_advance_result(&g,&w));
    CHECK(g.countries[0].infantry!=(int)g.countries[0].infantry && g.countries[0].cavalry!=(int)g.countries[0].cavalry);
    CHECK(g.countries[0].troops_real==3);
    sr_economy_enemy_troops(&g);CHECK(!g.countries[0].troops_real);
    /* Overwhelming Swedish strength takes five draws; half the enemy guns
       are lost, but the original enemy minimum can restore them immediately. */
    g=base;g.areas[6].infantry=50000;g.areas[6].artillery=3001;sr_random_seed(&g.random,0);
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,0) && sr_war_fight(&g,&w));
    CHECK(w.won && g.random.calls==5 && sr_war_advance_result(&g,&w));
    CHECK(w.swed_dead[2]==0 && w.enemy_dead[2]==500 && g.areas[6].artillery==3001 && g.countries[0].artillery==1000);
    /* Losing an offensive quick battle charges at least 500, not the
       turn*50 surrender tariff. Own provinces are preserved. */
    g=base;g.areas[6].infantry=1;g.areas[6].cavalry=0;g.areas[6].artillery=1;
    g.player_relation_mod[0]=100;g.silver=10;sr_random_seed(&g.random,0);
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,0) && sr_war_confirm_target(&g,&w) && sr_war_fight(&g,&w));
    CHECK(!w.won && sr_war_advance_result(&g,&w) && w.fee<=-500 && g.silver==-250);
    CHECK(g.player_areas.count==1 && g.areas[6].owned && g.areas[6].artillery==0 && w.swed_dead[2]==0.5);
    /* No enemy province: attacking victory gives monetary compensation. */
    g=base;g.player_relation_mod[0]=100;g.areas[6].infantry=50000;
    for(int a=1;a<=SR_AREA_COUNT;++a) if(g.areas[a].owner=='B') g.areas[a].owner='E';
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,0) && !w.targets.count && !w.area);
    CHECK(sr_war_confirm_target(&g,&w) && sr_war_fight(&g,&w) && w.won && sr_war_advance_result(&g,&w));
    CHECK(w.fee>=500 && !w.territory && g.player_areas.count==1);
    /* Strict country technology thresholds and multiplicative commander ranks. */
    for(int after=0;after<2;++after) {
        g=base;g.year=g.countries[0].troop_level2_year+after;
        sr_list_add(&g.people[SR_COMMANDERS].owned,1);sr_list_add(&g.people[SR_COMMANDERS].owned,2);
        CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,0) && sr_war_fight(&g,&w));
        CHECK(w.enemy_level==1+after && near(w.commander_bonus,1.05*1.05));
    }
    /* The selected regiment must have infantry/cavalry, not artillery alone. */
    g=base;sr_list_add(&g.player_areas,1);g.areas[1].owned=1;g.areas[1].artillery=1000;
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,0));
    CHECK(sr_war_select_regiment(&g,&w,99) && w.regiment==1);saved=g;
    CHECK(!sr_war_fight(&g,&w) && w.error==SR_WAR_EMPTY_REGIMENT && !memcmp(&g,&saved,sizeof g));
    CHECK(sr_war_select_regiment(&g,&w,0) && w.regiment==6 && !w.error);
    /* Explicit manual-battle choice remains a wait, never silently quick-resolves. */
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1));saved=g;
    CHECK(sr_war_fight(&g,&w) && w.phase==SR_WAR_FULL_PENDING && !memcmp(&g,&saved,sizeof g));
    /* Manual return is immediate and single-use. Retreat retains artillery;
       ordinary defeat loses half, rounded only when updating the regiment. */
    for(int retreat=0;retreat<2;++retreat) {
        g=base;g.player_relation_mod[0]=100;g.areas[6].artillery=3001;
        CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1) &&
            sr_war_confirm_target(&g,&w) && sr_war_fight(&g,&w));
        unsigned calls=g.random.calls;double losses[]={2,102,2,20};
        saved=g;SrWar before=w;
        CHECK(!sr_war_finish_manual(&g,&w,1,1,losses,0) && !memcmp(&saved,&g,sizeof g) && !memcmp(&before,&w,sizeof w));
        CHECK(sr_war_finish_manual(&g,&w,0,retreat,losses,0));
        CHECK(w.phase==SR_WAR_RESULT && w.applied && w.retreated==retreat && !w.surrender);
        CHECK(w.fee==-500 && g.silver==500 && g.random.calls==calls+1 && g.countries[0].relation==2);
        CHECK(g.areas[6].infantry==1998 && g.areas[6].cavalry==1898 && g.areas[6].artillery==(retreat?3001:1500));
        saved=g;CHECK(!sr_war_finish_manual(&g,&w,0,retreat,losses,0) && !memcmp(&saved,&g,sizeof g));
        CHECK(sr_war_advance_result(&g,&w) && w.phase==SR_WAR_DONE && !memcmp(&saved,&g,sizeof g));
    }
    /* INTEGER casualty division truncates before multiplication. Whole-valued
       FLOAT losses produce different fees; preserve their tags, not just values. */
    for(int real=0;real<2;++real) {
        g=base;g.countries[0].infantry=20000;g.countries[0].cavalry=5000;
        CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1) && sr_war_fight(&g,&w));
        double losses[]={2,2,5999,0};
        CHECK(sr_war_finish_manual(&g,&w,1,0,losses,real));
        CHECK(w.fee==(real?600:550) && g.silver==(real?1600:1550));
        CHECK(g.countries[0].infantry==14001 && g.countries[0].troops_real==real);
    }
    /* Swedish replacement minimum remains INTEGER (5999 cavalry => 500).
       Winning a province still develops it with five draws plus prestige. */
    g=base;g.areas[6].cavalry=10000;
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1) && sr_war_fight(&g,&w));
    double manual_losses[]={0,5999,0,0};
    CHECK(sr_war_finish_manual(&g,&w,1,0,manual_losses,0) && w.fee==500 && !g.silver_is_float);
    g=base;g.player_relation_mod[0]=100;
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1) && sr_war_confirm_target(&g,&w) && sr_war_fight(&g,&w));
    unsigned manual_calls=g.random.calls;int acquired=w.area;double no_losses[]={2,2,2,0};
    CHECK(sr_war_finish_manual(&g,&w,1,0,no_losses,0) && w.territory==acquired && g.areas[acquired].owner=='A');
    CHECK(g.random.calls==manual_calls+6 && w.enemy_dead[2]==500 && g.war_wins==1);
    /* Losing the last province exits before casualties, debt clamp and prestige. */
    g=base;g.war_area=6;g.silver=-300;
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1) && sr_war_fight(&g,&w));
    manual_calls=g.random.calls;
    CHECK(sr_war_finish_manual(&g,&w,0,0,no_losses,0) && w.phase==SR_WAR_DONE && g.crushed);
    CHECK(!g.player_areas.count && g.areas[6].infantry==2000 && g.random.calls==manual_calls && g.silver==-300);
    /* A legitimate FLOAT roundoff loss must not strand the campaign. */
    g=base;g.player_relation_mod[0]=100;g.countries[0].infantry=6502;g.countries[0].troops_real=1;
    CHECK(sr_war_begin(&g,&w,1) && sr_war_choose_battle(&g,&w,1) && sr_war_confirm_target(&g,&w) && sr_war_fight(&g,&w));
    double roundoff_losses[]={2,2,6502-(6502.0/7+6502.0/7+6502.0/7+6502.0/7+6502.0/7+6502.0/7+6502.0/7),0};
    CHECK(roundoff_losses[2]<0 && sr_war_finish_manual(&g,&w,0,1,roundoff_losses,1));
    CHECK(w.phase==SR_WAR_RESULT && g.countries[0].infantry>6502 && g.countries[0].troops_real==1);
    /* Negotiation fee, embassy exemption, strict <60 offer roll, and refusal. */
    for(int embassy=0;embassy<2;++embassy) {
        g=base;g.silver=99;g.areas[6].special=embassy?5:7;sr_random_seed(&g.random,1);
        CHECK(sr_war_begin(&g,&w,1));saved=g;
        if(!embassy) CHECK(!sr_war_negotiate(&g,&w) && w.error==SR_WAR_SILVER && !memcmp(&g,&saved,sizeof g));
        else CHECK(sr_war_negotiate(&g,&w) && w.phase==SR_WAR_NO_OFFER && g.silver==99 && g.random.calls==1);
    }
    g=base;g.silver=5000;sr_random_seed(&g.random,0);
    CHECK(sr_war_begin(&g,&w,1) && sr_war_negotiate(&g,&w));
    CHECK(w.phase==SR_WAR_OFFER && w.offer_type==2 && g.silver==4900 && g.random.calls==1);
    double offer=w.offer_silver;CHECK(offer>=1000 && offer<=4900);
    CHECK(sr_war_refuse(&w) && w.phase==SR_WAR_DECLARE && g.silver==4900);
    CHECK(sr_war_negotiate(&g,&w));
    if(w.phase==SR_WAR_OFFER) {
        offer=w.offer_silver;double money=g.silver;g.countries[0].relation=1;
        CHECK(sr_war_accept(&g,&w) && w.phase==SR_WAR_DONE && g.silver==money-offer && g.countries[0].relation==-3);
        saved=g;CHECK(!sr_war_accept(&g,&w) && !memcmp(&g,&saved,sizeof g));
    }
    /* Denmark desires Uppland: negotiation can transfer the player's last
       province, exiting before changing the original country/owner records. */
    sr_game_seed(1);CHECK(sr_game_begin(3));g=*sr_game_state();g.areas[1].infantry=1000;
    int got_area=0;
    for(int seed=0;seed<20 && !got_area;++seed) {
        SrGame h=g;sr_random_seed(&h.random,seed);CHECK(sr_war_begin(&h,&w,1) && sr_war_negotiate(&h,&w));
        if(w.offer_type==1) {
            got_area=1;CHECK(w.offer_area==1 && sr_war_accept(&h,&w));
            CHECK(h.crushed && !h.player_areas.count && h.areas[1].owner=='A' && h.countries[0].relation==0);
        }
    }
    CHECK(got_area);
    /* No troops: an attacking surrender pays turn*50; debt bottoms at -250.
       A defensive surrender loses the first Swedish priority area. */
    g=*sr_game_state();g.player_relation_mod[0]=100;g.turn=10;g.silver=0;
    CHECK(sr_war_begin(&g,&w,1) && w.phase==SR_WAR_NO_TROOPS && sr_war_surrender(&g,&w));
    CHECK(w.fee==-500 && g.silver==-250 && w.phase==SR_WAR_RESULT && g.countries[0].relation==2);
    g=*sr_game_state();CHECK(sr_war_begin(&g,&w,1) && sr_war_surrender(&g,&w));
    CHECK(w.territory==-7 && !g.crushed && g.areas[7].owner=='B');
    /* Normal Tre Rosor seed 3, original 400-silver declaration and real turn.
       No state injection: victory adds Skåne and advances settlement. */
    sr_game_seed(3);CHECK(sr_game_begin(1));g=*sr_game_state();SrTurn t={0};
    for(int n=0;n<4;++n) CHECK(sr_diplomacy_adjust(&g,1,100)==SR_DIPLOMACY_OK);
    CHECK(sr_turn_begin(&g,&t) && reach(&g,&t,SR_TURN_WAR));
    CHECK(t.pending.country==1 && sr_war_begin(&g,&w,1) && w.attacking);
    CHECK(sr_war_choose_battle(&g,&w,0) && w.phase==SR_WAR_TARGET && w.area==17);
    CHECK(sr_war_confirm_target(&g,&w) && sr_war_fight(&g,&w) && w.won && g.random.calls==29);
    CHECK(near(w.swed_dead[0],520) && near(w.swed_dead[1],780) && near(w.enemy_dead[0],800) && near(w.enemy_dead[1],630));
    CHECK(sr_war_advance_result(&g,&w) && w.territory==17 && g.random.calls==35);
    CHECK(g.areas[17].owned && g.areas[17].owner=='A' && g.player_areas.records[1]==17);
    CHECK(g.areas[17].farming_index==1 && g.areas[17].mining_index==1);
    CHECK(sr_war_advance_result(&g,&w) && sr_turn_war_finished(&g,&t) && reach(&g,&t,SR_TURN_FINISHED));
    CHECK(g.year==1528 && g.turn==2 && g.war_wins==1 && !g.player_relation_mod[0]);
    printf("Normal war route: %.17g/%.17g/%.17g resources, troops %d/%d, calls %u\n",g.silver,g.crops,g.metal,g.areas[6].infantry,g.areas[6].cavalry,g.random.calls);
    puts("Quick battles, negotiation, surrender, territory, casualty precision and real turn integration passed.");
    return 0;
}
