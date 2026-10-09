#include "events.h"
#include "economy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    CHECK(sr_game_begin(3)); SrGame g=*sr_game_state(); SrEvent pool[SR_EVENT_POOL_CAPACITY];
    CHECK(sr_event_pool(&g,pool)==9);
    for(int i=0;i<3;++i) CHECK(pool[i].kind==SR_EVENTS && pool[i].record==1);
    CHECK(pool[3].kind==SR_CULTURE && pool[3].record==1 && pool[7].kind==SR_COMMANDERS && pool[7].record==1);
    g.areas[1].tax_level=5; CHECK(sr_event_pool(&g,pool)==12);
    for(int i=3;i<6;++i) CHECK(pool[i].kind==SR_EVENTS && pool[i].record==2);
    g.used_events=(SrRecordList){1,{1}};
    CHECK(sr_event_pool(&g,pool)==9 && pool[0].record==2);
    g.areas[1].tax_level=4; CHECK(!sr_event_eligible(&g,SR_EVENTS,2)); /* Strictly >20. */
    CHECK(sr_event_pool(&g,pool)==6);
    g.people[SR_CULTURE].used=(SrRecordList){4,{1,2,3,4}};
    g.people[SR_COMMANDERS].used=(SrRecordList){2,{1,2}};
    CHECK(sr_event_pool(&g,pool)==0);
    sr_random_seed(&g.random,1); CHECK(!sr_event_choose(&g).record && g.random.calls==1);
    for(int turn=4;turn<=5;++turn) {
        g=*sr_game_state(); g.turn=turn; sr_random_seed(&g.random,1);
        SrEvent e=sr_event_choose(&g);
        CHECK(e.kind==(turn==4?SR_SCIENCE:SR_COMMANDERS) && e.record==1);
        CHECK(g.random.calls==1 && g.random.state==UINT32_C(3357800067));
        CHECK(sr_event_apply(&g,e));
        CHECK(g.people[e.kind].used.count==1 && g.people[e.kind].available.records[0]==1);
    }
    /* Every nonempty original handler has the explicit record-number binding
       used by events.c. Unexpected source handler changes fail here. */
    for(int kind=SR_EVENTS;kind<=SR_EVENT_B;++kind) {
        int n=kind==SR_EVENTS?24:kind==SR_EVENT_A?22:6;
        for(int record=1;record<=n;++record) {
            const char *handler=kind==SR_EVENTS?sr_catalog_entry(kind,record)->result_handler:
                sr_catalog_dated(kind==SR_EVENT_A?SR_EVENTS_A:SR_EVENTS_B,record)->result_handler;
            char expected[24]; snprintf(expected,sizeof expected,"result%c%d",kind==SR_EVENTS?'C':kind==SR_EVENT_A?'A':'B',record);
            /* Lingo handler names are case-insensitive; C7 is spelled ResultC7
               in the supplied movie, unlike the other result handlers. */
            CHECK(!strcmp(handler,"-") ||
                  ((handler[0]=='r' || handler[0]=='R') && !strcmp(handler+1,expected+1)));
            g=*sr_game_state(); CHECK(sr_event_apply(&g,(SrEvent){kind,record}));
        }
    }
    g=*sr_game_state(); g.year=1800; g.hq_level=2; g.areas[1].city_level=2; g.areas[1].mining_level=3;
    CHECK(sr_event_choose_b(&g).record==1);
    g.event_b_used[0]=1; CHECK(sr_event_choose_b(&g).record==2);
    g.event_b_used[1]=1; CHECK(sr_event_choose_b(&g).record==4); /* B3 needs three owned areas. */
    g.event_b_used[3]=1; CHECK(!sr_event_choose_b(&g).record);
    g.points=10; CHECK(sr_event_choose_b(&g).record==5); /* Cached points, not live recalc. */
    g.event_b_used[4]=1; g.countries[2].trade=5; CHECK(sr_event_choose_b(&g).record==6);
    g.countries[2].trade=6; CHECK(!sr_event_choose_b(&g).record); /* ==5, not >=5. */
    g=*sr_game_state(); g.areas[2].owned=1; g.player_areas=(SrRecordList){2,{1,2}};
    CHECK(sr_event_apply(&g,(SrEvent){SR_EVENT_B,3}));
    CHECK(g.troop_capacity[0]==7000 && g.troop_capacity[4]==27000 && g.areas[1].infantry==2000 && g.areas[2].infantry==2000);
    g.areas[1].infantry=6000;
    CHECK(sr_army_recruit(&g,SR_INFANTRY)==SR_ARMY_OK && g.areas[1].infantry==7000);
    CHECK(sr_army_recruit(&g,SR_INFANTRY)==SR_ARMY_CAPACITY);
    for(int floating=0;floating<=1;++floating) {
        g=*sr_game_state(); g.silver=101; g.silver_is_float=floating;
        CHECK(sr_event_apply(&g,(SrEvent){SR_EVENT_B,4}) && g.silver==(floating?50.5:50));
        g.silver=-101; CHECK(sr_event_apply(&g,(SrEvent){SR_EVENTS,24}) && g.silver==(floating?-50.5:-50));
    }
    g=*sr_game_state(); g.trade_crops[0]=1; sr_economy_trade(&g,0);
    CHECK(g.silver==1001 && g.silver_is_float);
    CHECK(sr_event_apply(&g,(SrEvent){SR_EVENTS,24}) && g.silver==500.5);
    g.silver=99.5; CHECK(sr_event_apply(&g,(SrEvent){SR_EVENTS,12}) && g.silver==0 && !g.silver_is_float);
    g=*sr_game_state(); g.countries[1].relation=-2; g.countries[2].relation=8; g.areas[1].infantry=1500;
    CHECK(sr_event_apply(&g,(SrEvent){SR_EVENT_A,9}));
    CHECK(g.silver==1150 && g.countries[1].relation==5 && g.countries[2].relation==10);
    CHECK(sr_event_apply(&g,(SrEvent){SR_EVENT_A,3}) && g.areas[28].owner=='A' && g.available[g.available_count-1]==28);
    g=*sr_game_state(); g.player_areas=(SrRecordList){2,{17,1}}; g.areas[17].owned=1; g.current_area=17;
    g.available[g.available_count++]=17;
    CHECK(sr_event_apply(&g,(SrEvent){SR_EVENTS,14}));
    CHECK(g.current_area==1 && !g.areas[17].owned && g.areas[17].owner=='B' && g.player_areas.count==1);
    for(int i=0;i<g.available_count;++i) CHECK(g.available[i]!=17);
    const int records[]={24,20,1};
    for(int kind=0;kind<3;++kind) {
        g=*sr_game_state(); SrEvent e={kind,records[kind]};
        CHECK(sr_event_apply(&g,e) && *sr_event_minigame(e));
        sr_event_minigame_result(&g,e,5);
        CHECK(g.silver==1500 && g.point_mod==-13 && g.people[kind].owned.records[0]==records[kind] && !g.people[kind].available.count);
    }
    g=*sr_game_state(); SrGame saved=g;
    CHECK(!sr_event_apply(&g,(SrEvent){SR_EVENT_A,99}) && !memcmp(&g,&saved,sizeof g));
    CHECK(sr_event_is_king((SrEvent){SR_EVENT_A,2}) && !sr_event_is_king((SrEvent){SR_EVENT_A,3}));
    puts("Event weights, forced arrivals, eligibility, original handler bindings, effects and minigame rewards passed.");
    return 0;
}
