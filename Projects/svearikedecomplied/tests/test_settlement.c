#include "economy.h"
#include "catalog.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
static int near(double a,double b) { double d=a-b; return d>-0.000001 && d<0.000001; }
int main(void) {
    const int silver[]={1036,1040,1050,1300,1150},crops[]={106,160,124,112,112},metal[]={104,157,110,105,107};
    const int population[]={273,270,275,270,270};
    for(int family=1;family<=5;++family) {
        CHECK(sr_game_begin(family)); SrGame g=*sr_game_state();
        sr_economy_income(&g,0); CHECK(sr_economy_supply(&g));
        CHECK(g.silver==silver[family-1] && g.crops==crops[family-1] && g.metal==metal[family-1]);
        sr_economy_growth(&g);
        CHECK(g.areas[g.current_area].population==population[family-1] && g.hq_level==1);
        CHECK(g.random.calls==sr_game_state()->random.calls && g.turn==1 && g.year==1523);
    }
    CHECK(sr_game_begin(3)); SrGame g=*sr_game_state();
    sr_random_seed(&g.random,1);
    int riots[29]={0}; riots[1]=1;
    sr_economy_income(&g,riots);
    CHECK(g.silver==1037 && g.crops==118 && g.metal==107);
    CHECK(g.random.calls==1 && g.random.state==UINT32_C(3357800067)); /* Original RNG golden state. */
    for(int kind=2;kind<=4;kind+=2) {
        g=*sr_game_state(); sr_random_seed(&g.random,1); riots[1]=kind;
        sr_economy_income(&g,riots);
        CHECK(g.silver==1050 && g.crops==124 && g.metal==110 && g.random.calls==0);
    }
    /* Acquisition order controls which province receives each riot draw. */
    for(int reversed=0;reversed<=1;++reversed) {
        g=*sr_game_state(); sr_random_seed(&g.random,1); g.areas[2].owned=1;
        g.player_areas=(SrRecordList){2,{reversed?2:1,reversed?1:2}};
        riots[1]=riots[2]=3; sr_economy_income(&g,riots);
        CHECK(g.crops==127 && g.metal==(reversed?112:111) && g.random.calls==2);
    }
    g=*sr_game_state(); g.areas[1].infantry=1999; g.areas[2].owned=1;
    g.areas[2].infantry=999; g.player_areas=(SrRecordList){2,{1,2}};
    g.troop_supply_mod=0.75; g.troop_level=2;
    CHECK(sr_economy_troop_cost(&g)==1.5); /* Truncates each province, before summing. */
    g.crops=1; SrGame saved=g;
    CHECK(!sr_economy_supply(&g) && !memcmp(&saved,&g,sizeof g));
    g.areas[1].infantry=999; CHECK(sr_economy_supply(&g) && g.crops==1);
    g.areas[1].infantry=1999; g.crops=1.5; g.silver=0;
    CHECK(sr_economy_supply(&g) && g.crops==0 && g.silver==-1.5); /* Silver may go negative. */

    g=*sr_game_state(); sr_random_seed(&g.random,1); sr_economy_prices(&g);
    const int prices[]={6,5,6,7,6,6,6,5};
    for(int i=0;i<4;++i) CHECK(g.countries[i].crops_index==prices[2*i] && g.countries[i].metal_index==prices[2*i+1]);
    CHECK(g.random.calls==11 && g.random.state==UINT32_C(3104592263));
    CHECK(near(g.countries[0].metal_price_mod,0.9) && near(g.countries[1].metal_price_mod,1.1));

    for(int harbor=0;harbor<=1;++harbor) {
        g=*sr_game_state(); g.trade_income_mod=1.25;
        g.trade_crops[0]=10; g.trade_metal[1]=3; g.trade_crops[2]=-4; g.trade_metal[3]=-5;
        g.countries[0].crops_price_mod=1.25; g.countries[1].metal_price_mod=0.6;
        if(harbor) g.areas[1].special=6;
        CHECK(sr_economy_trade(&g,1)==(harbor?0u:1u));
        CHECK(near(g.silver,harbor?1022.375:1006.75) && g.crops==105 && g.metal==106.25);
        CHECK(g.countries[0].crops==(harbor?35:25) && g.countries[1].metal==28 && g.countries[2].crops==36 && g.countries[3].metal==45);
        for(int i=0;i<4;++i) CHECK(!g.trade_crops[i] && !g.trade_metal[i]);
    }
    g=*sr_game_state(); sr_random_seed(&g.random,1); g.turn=7;
    g.countries[0].relation=9; sr_economy_relations(&g);
    CHECK(g.countries[0].relation==11 && !g.random.calls); /* Display clamp doesn't change stored relation. */
    g.turn=1; g.player_relation_mod[0]=-1000;
    sr_economy_relations(&g); CHECK(g.countries[0].relation==7 && g.random.calls==1);

    g=*sr_game_state(); sr_random_seed(&g.random,1); g.year=1528;
    sr_economy_enemy_troops(&g);
    const int enemy[4][3]={{2141,1750,1254},{2290,1131,1187},{2247,2882,1098},{4567,1831,1232}};
    for(int i=0;i<4;++i) CHECK(g.countries[i].infantry==enemy[i][0] && g.countries[i].cavalry==enemy[i][1] && g.countries[i].artillery==enemy[i][2]);
    CHECK(g.random.calls==12 && g.random.state==UINT32_C(10044473));
    for(int after=0;after<=1;++after) {
        g=*sr_game_state(); sr_random_seed(&g.random,1); g.year=1611+after;
        for(int i=0;i<4;++i) {
            g.countries[i].infantry=g.countries[i].cavalry=g.countries[i].artillery=20000;
            g.countries[i].troop_level2_year=1611; g.countries[i].troop_level3_year=1800;
        }
        sr_economy_enemy_troops(&g);
        CHECK(!g.random.calls && g.countries[0].infantry==(after?14580:18000) && g.countries[0].artillery==10000);
    }
    /* Freeze population growth with integer unrest/10, then check strict cutoffs. */
    const int populations[]={500,501,2500,2501,10000,10001,20000,20001};
    const int levels[]={1,2,2,3,3,4,4,5};
    for(int i=0;i<8;++i) {
        g=*sr_game_state(); g.happiness_mod=110; g.areas[1].population=populations[i];
        sr_economy_growth(&g); CHECK(g.areas[1].population==populations[i] && g.areas[1].city_level==levels[i]);
    }
    g=*sr_game_state(); g.happiness_mod=110; g.areas[1].population=400; g.areas[1].city_level=5;
    sr_economy_growth(&g); CHECK(g.areas[1].city_level==5); /* Original missing low-population else. */
    g=*sr_game_state(); g.areas[2].owned=1; g.player_areas=(SrRecordList){2,{1,2}};
    g.areas[2].special=3; g.culture_level=99;
    sr_economy_growth(&g); CHECK(g.hq_level==2);
    sr_economy_growth(&g); CHECK(g.hq_level==2); /* 5 theater ranks <7; culture_level is not consulted. */
    puts("Turn-stage income, riots, upkeep pauses, trade, price RNG, relations, enemy armies and growth passed.");
    return 0;
}
