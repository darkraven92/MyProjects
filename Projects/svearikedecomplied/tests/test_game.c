#include "game.h"
#include "catalog.h"
#include "projector_rng_vectors.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    /* Golden values reviewed against initPlayerVar/initFAmily and DATA.CST. */
    const int homes[]={6,4,1,2,3}, silver[]={1000,1000,1000,1250,1100};
    const char *names[]={"Nyland","Östergötland","Uppland","Södermanland","Västergötland"};
    for(int family=1;family<=5;++family) {
        CHECK(sr_game_begin(family));
        const SrGame *g=sr_game_state();
        CHECK(g->family==family && g->year==1523 && g->turn==1);
        CHECK(g->current_area==homes[family-1]);
        CHECK(!strcmp(sr_game_area_name(g->current_area),names[family-1]));
        CHECK(g->silver==silver[family-1]);
        CHECK(g->crops==(family==2?150:100) && g->metal==(family==2?150:100));
        const SrAreaState *home=&g->areas[g->current_area];
        CHECK(home->special==7 && home->owned && home->population==250);
        CHECK(home->farming_level==(family==3?3:1));
        CHECK(home->farming_index==(family==3?2:1));
        CHECK(home->military_level==(family==1?2:1));
        CHECK(home->infantry==(family==1?2000:0) && home->cavalry==(family==1?2000:0));
        CHECK(g->available_count==15 && g->hq_level==1 && g->troop_level==1);
        for(int i=0;i<g->available_count;++i) CHECK(g->available[i]!=g->current_area);
        int owned=0;
        for(int i=1;i<=28;++i) {
            owned+=g->areas[i].owned;
            CHECK(g->areas[i].mining_level==1 && g->areas[i].mining_index==1);
            CHECK(g->areas[i].city_level==1 && g->areas[i].tax_level==1);
        }
        CHECK(owned==1);
        for(int i=0;i<4;++i) {
            CHECK(g->countries[i].trade==(family==2?2:1));
            CHECK(g->countries[i].diplomacy==(family==5?2:1));
            CHECK(g->countries[i].relation==0 && g->countries[i].crops_index==6);
            CHECK(g->trade_crops[i]==0 && g->trade_metal[i]==0);
        }
        CHECK(g->countries[3].infantry==3750 && g->countries[2].cavalry==2500);
        CHECK(g->random_initialization_pending==0);
        CHECK(g->points==0 && sr_game_points(g)==0 && g->family_head_count==1);
        CHECK(g->point_mod==-(13+(family==1?1:family==3?2:family==2||family==5?4:0)));
        for(int kind=0;kind<3;++kind)
            CHECK(!g->people[kind].used.count && !g->people[kind].available.count && !g->people[kind].owned.count);
    }
    CHECK(sr_game_begin(1)); /* Reset after Sture: no retained diplomacy bonus. */
    CHECK(sr_game_state()->countries[0].diplomacy==1);
    CHECK(!sr_game_begin(0) && !sr_game_begin(6));
    CHECK(sr_game_state()->family==1);
    CHECK(sr_game_area(1)->silver==50 && sr_game_area(1)->price==3700);
    CHECK(sr_game_area(6)->crops==10 && sr_game_area(6)->metal==4);
    CHECK(sr_game_area(28)->owner=='E' && !strcmp(sr_game_area(28)->note,"1564"));
    CHECK(sr_game_price(0,5)==500 && sr_game_price(5,5)==250);
    CHECK(sr_game_price(0,0)==-1 && sr_game_price(16,1)==-1);
    CHECK(sr_game_relation(1,1)==0 && sr_game_relation(60,4)==0);
    const char *heads[]={"Svante Svantesson Eka","Ture Gustafsson Eka","Nils Jöransson Eka","Svante Svantesson Eka","Petter Svantesson Eka"};
    for(unsigned n=0;n<sizeof rng_initializations/sizeof rng_initializations[0];++n) {
        sr_game_seed(rng_initializations[n].seed);
        CHECK(sr_game_begin(3));
        const SrGame *g=sr_game_state();
        const uint32_t *draws=rng_initializations[n].values;
        for(int i=0;i<6;++i) {
            CHECK(g->event_b_year[i]==sr_catalog_dated(SR_EVENTS_B,i+1)->base_year+(int)draws[i]);
            CHECK(!g->event_b_used[i]);
        }
        for(int i=0;i<4;++i) {
            CHECK(g->countries[i].troop_level2_year==1610+(int)draws[6+2*i]);
            CHECK(g->countries[i].troop_level3_year==1700+(int)draws[7+2*i]);
            CHECK(g->countries[i].crops_price_mod==1 && g->countries[i].metal_price_mod==1);
        }
        CHECK(!strcmp(sr_game_head_name(),heads[n]));
        CHECK(g->family_heads[0].type==(int)draws[16]);
        CHECK(g->random.state==rng_initializations[n].state && g->random.calls==17);
        uint32_t state=g->random.state;
        CHECK(!sr_game_begin(0) && g->random.state==state);
        CHECK(sr_game_begin(3) && g->random.calls==34 && g->random.state!=state);
    }
    sr_game_seed(1); CHECK(sr_game_begin(3));
    SrGame modified=*sr_game_state();
    modified.areas[1].farming_level++;
    modified.people[SR_CULTURE].owned=(SrRecordList){1,{1}};
    CHECK(sr_game_points(&modified)==1+sr_catalog_entry(SR_CULTURE,1)->level);
    puts("All five original starts, province tables, prices, relations and reset behavior passed.");
    return 0;
}
