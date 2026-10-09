#include "people.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    CHECK(sr_game_begin(3));
    const SrGame original=*sr_game_state();
    const int prices[]={0,200,350,500,650,800};
    for(int kind=SR_CULTURE;kind<=SR_COMMANDERS;++kind) {
        for(int record=1;record<=sr_catalog_count(kind);++record) {
            SrGame g=original,before;
            const SrCatalogEntry *p=sr_catalog_entry(kind,record);
            CHECK(sr_people_price(kind,record)==prices[p->level]);
            g.people[kind].available=(SrRecordList){1,{record}};
            g.silver=0;before=g;
            int missing=p->level==5 && kind!=SR_COMMANDERS;
            CHECK(sr_people_hire(&g,kind,record)==(missing?(kind==SR_CULTURE?SR_HIRE_ARCH:SR_HIRE_UNIVERSITY):SR_HIRE_SILVER));
            CHECK(!memcmp(&before,&g,sizeof g));
            if(missing) {
                /* An unowned building does not meet the prerequisite. */
                g.areas[28].special=3+kind;before=g;
                CHECK(sr_people_hire(&g,kind,record)==(kind==SR_CULTURE?SR_HIRE_ARCH:SR_HIRE_UNIVERSITY));
                CHECK(!memcmp(&before,&g,sizeof g));
                g.player_areas.records[g.player_areas.count++]=28;
                g.areas[28].owned=1;
            }
            g.silver=prices[p->level]-0.5; before=g;
            CHECK(sr_people_hire(&g,kind,record)==SR_HIRE_SILVER && !memcmp(&before,&g,sizeof g));
            g.silver=prices[p->level]+0.5;g.silver_is_float=1;
            CHECK(sr_people_hire(&g,kind,record)==SR_HIRE_OK);
            CHECK(g.silver==0.5 && g.silver_is_float && !g.people[kind].available.count &&
                  g.people[kind].owned.count==1 && g.people[kind].owned.records[0]==record);
            CHECK(g.crops==original.crops && g.metal==original.metal && g.year==original.year &&
                  !memcmp(&g.random,&original.random,sizeof g.random));
            if(kind==SR_SCIENCE) CHECK(sr_game_science_level(&g)==p->level+(missing?5:0));
            before=g;
            CHECK(sr_people_hire(&g,kind,record)==SR_HIRE_INVALID && !memcmp(&before,&g,sizeof g));
        }
    }
    SrGame g=original;
    g.silver=10000;
    g.people[SR_COMMANDERS].available=(SrRecordList){3,{3,1,2}};
    CHECK(sr_people_hire(&g,SR_COMMANDERS,1)==SR_HIRE_OK);
    CHECK(g.people[SR_COMMANDERS].available.count==2 && g.people[SR_COMMANDERS].available.records[0]==3 &&
          g.people[SR_COMMANDERS].available.records[1]==2);
    CHECK(sr_people_hire(&g,SR_COMMANDERS,3)==SR_HIRE_OK && g.people[SR_COMMANDERS].owned.records[1]==3);
    SrGame before=g;
    CHECK(sr_people_hire(&g,SR_COMMANDERS,0)==SR_HIRE_SELECT);
    CHECK(sr_people_hire(&g,SR_EVENTS,1)==SR_HIRE_INVALID);
    CHECK(sr_people_hire(&g,SR_COMMANDERS,99)==SR_HIRE_INVALID);
    CHECK(!memcmp(&g,&before,sizeof g));
    puts("Original person prices, rank-five prerequisites, atomic hiring, list order and science effects passed.");
    return 0;
}
