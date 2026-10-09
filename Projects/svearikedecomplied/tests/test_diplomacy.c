#include "diplomacy.h"
#include "economy.h"
#include "fields.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void) {
    for(int family=1;family<=5;++family) for(int country=1;country<=4;++country) {
        sr_game_seed(1);CHECK(sr_game_begin(family));SrGame g=*sr_game_state(),saved;
        SrCountry *c=&g.countries[country-1];c->diplomacy=1;
        double money=g.silver;uint32_t calls=g.random.calls;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_OK && c->diplomacy==2);
        CHECK(g.silver==money-100*g.diplomacy_upgrade_mod && g.random.calls==calls);
        saved=g;CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_REQUIREMENTS && !memcmp(&g,&saved,sizeof g));
        g.hq_level=2;g.silver=1000;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_OK && c->diplomacy==3);
        g.hq_level=3;CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_OK && c->diplomacy==4);
        g.silver=0;saved=g;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_EMBASSY && !memcmp(&g,&saved,sizeof g));
        /* An unowned embassy does not qualify. */
        int other=g.current_area==1?2:1;g.areas[other].special=5;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_EMBASSY);
        g.areas[g.current_area].special=5;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_REQUIREMENTS);
        g.silver=1000;g.hq_level=1;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_OK && c->diplomacy==5);
        CHECK(g.silver==1000-500*g.diplomacy_upgrade_mod);saved=g;
        CHECK(sr_diplomacy_upgrade(&g,country)==SR_DIPLOMACY_MAX_LEVEL && !memcmp(&g,&saved,sizeof g));
    }
    sr_game_seed(1);CHECK(sr_game_begin(3));SrGame base=*sr_game_state();
    for(int country=1;country<=4;++country) for(int direction=-1;direction<=1;direction+=2) {
        SrGame g=base,saved;g.silver=1000;g.relation_cost_mod=0.01;
        for(int step=1;step<=10;++step) {
            CHECK(sr_diplomacy_adjust(&g,country,direction*100)==SR_DIPLOMACY_OK);
            CHECK(g.player_relation_mod[country-1]==direction*step*100 && g.silver==1000-step*100);
            CHECK(g.countries[country-1].relation==base.countries[country-1].relation && g.random.calls==base.random.calls);
        }
        saved=g;CHECK(sr_diplomacy_adjust(&g,country,direction*100)==SR_DIPLOMACY_LIMIT && !memcmp(&g,&saved,sizeof g));
        for(int step=9;step>=0;--step) {
            CHECK(sr_diplomacy_adjust(&g,country,-direction*100)==SR_DIPLOMACY_OK);
            CHECK(g.player_relation_mod[country-1]==direction*step*100 && g.silver==1000-step*100);
        }
        g.silver=99.5;saved=g;
        CHECK(sr_diplomacy_adjust(&g,country,direction*100)==SR_DIPLOMACY_SILVER && !memcmp(&g,&saved,sizeof g));
    }
    SrGame g=base,saved;g.silver=1000;g.diplomacy_upgrade_mod=0.5625;
    CHECK(sr_diplomacy_upgrade(&g,1)==SR_DIPLOMACY_OK && g.silver==943.75);
    saved=g;CHECK(sr_diplomacy_adjust(&g,1,200)==SR_DIPLOMACY_INVALID && !memcmp(&g,&saved,sizeof g));
    CHECK(sr_diplomacy_upgrade(&g,0)==SR_DIPLOMACY_INVALID && sr_diplomacy_adjust(&g,5,100)==SR_DIPLOMACY_INVALID);
    CHECK(sr_diplomacy_upgrade(0,1)==SR_DIPLOMACY_INVALID && sr_diplomacy_adjust(0,1,100)==SR_DIPLOMACY_INVALID);
    CHECK(!strcmp(sr_diplomacy_country_name(2),"Kejsarstaterna") && !strcmp(sr_diplomacy_country_name(4),"Ryssland"));
    CHECK(sr_diplomacy_relation_icon(-99)==1 && sr_diplomacy_relation_icon(-2)==2 && sr_diplomacy_relation_icon(3)==3 &&
          sr_diplomacy_relation_icon(4)==4 && sr_diplomacy_relation_icon(7)==5 && sr_diplomacy_relation_icon(99)==5);
    g=base;CHECK(sr_diplomacy_adjust(&g,1,-100)==SR_DIPLOMACY_OK);
    SrFieldContext context={0};char text[32];
    CHECK(sr_field_text(&g,&context,304,text,sizeof text)==3 && !strcmp(text,"100"));
    CHECK(sr_field_text(&g,&context,305,text,sizeof text)==1 && !strcmp(text,"0"));
    CHECK(sr_field_text(&g,&context,319,text,sizeof text)==4 && !strcmp(text,"2000"));
    puts("Diplomacy costs, prerequisites, embassy ownership, reservation limits/refunds and country mapping passed.");
    return 0;
}
