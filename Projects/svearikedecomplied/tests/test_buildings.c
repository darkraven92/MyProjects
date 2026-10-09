#include "game.h"
#include "catalog.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
#define REJECT(call,expected) do { SrGame saved=g; CHECK((call)==(expected)); CHECK(!memcmp(&g,&saved,sizeof g)); } while(0)
int main(void) {
    CHECK(sr_game_begin(3));
    SrGame g=*sr_game_state(); SrAreaState *a=&g.areas[1];
    REJECT(sr_building_upgrade(&g,SR_FARMING),SR_BUILDING_CITY_REQUIREMENTS);
    CHECK(a->farming_level==3 && a->farming_index==2); /* Eka's original mismatch. */
    a->population=20001;
    REJECT(sr_building_upgrade(&g,SR_FARMING),SR_BUILDING_CITY_REQUIREMENTS); /* Tests city level, not population. */
    a->city_level=4;
    CHECK(sr_building_upgrade(&g,SR_FARMING)==SR_BUILDING_OK);
    CHECK(a->farming_level==4 && a->farming_index==4 && g.silver==700 && g.metal==0);
    a->special=0;
    REJECT(sr_building_upgrade(&g,SR_FARMING),SR_BUILDING_SPECIAL);
    a->special=1; a->city_level=5; g.silver=500; g.metal=200;
    CHECK(sr_building_upgrade(&g,SR_FARMING)==SR_BUILDING_OK && g.silver==0 && g.metal==0);
    CHECK(a->farming_level==5 && a->farming_index==5);
    REJECT(sr_building_upgrade(&g,SR_FARMING),SR_BUILDING_MAX_LEVEL);
    g=*sr_game_state(); a->farming_level=a->farming_index=1; a->city_level=5; g.silver=1100; g.metal=375;
    for(int level=2;level<=5;++level) {
        CHECK(sr_building_upgrade(&g,SR_FARMING)==SR_BUILDING_OK);
        CHECK(a->farming_level==level && a->farming_index==level);
    }
    CHECK(g.silver==0 && g.metal==0);

    g=*sr_game_state();
    REJECT(sr_building_upgrade(&g,SR_MINING),SR_BUILDING_SCIENCE_REQUIREMENTS);
    g.people[SR_SCIENCE].available=(SrRecordList){1,{1}};
    CHECK(sr_game_science_level(&g)==0); /* Available scientist is not employed. */
    g.people[SR_SCIENCE].owned=(SrRecordList){1,{1}};
    CHECK(sr_game_science_level(&g)==1);
    g.mining_upgrade_mod=0.75;
    CHECK(sr_building_upgrade(&g,SR_MINING)==SR_BUILDING_OK);
    CHECK(a->mining_level==2 && a->mining_index==2 && g.silver==925 && g.metal==81.25);
    REJECT(sr_building_upgrade(&g,SR_MINING),SR_BUILDING_SCIENCE_REQUIREMENTS);
    g.areas[2].special=4; CHECK(sr_game_science_level(&g)==1);
    g.areas[2].owned=1; CHECK(sr_game_science_level(&g)==6);
    g.mining_upgrade_mod=1; g.silver=1000; g.metal=350;
    CHECK(sr_building_upgrade(&g,SR_MINING)==SR_BUILDING_OK && a->mining_level==3);
    CHECK(sr_building_upgrade(&g,SR_MINING)==SR_BUILDING_OK && a->mining_level==4);
    REJECT(sr_building_upgrade(&g,SR_MINING),SR_BUILDING_SCIENCE_REQUIREMENTS);
    g.areas[3].special=4; g.areas[3].owned=1;
    CHECK(sr_game_science_level(&g)==11);
    CHECK(sr_building_upgrade(&g,SR_MINING)==SR_BUILDING_OK && a->mining_level==5 && a->mining_index==5);
    CHECK(g.silver==0 && g.metal==0 && g.year==1523 && g.turn==1 && g.random.calls==sr_game_state()->random.calls);
    REJECT(sr_building_upgrade(&g,SR_MINING),SR_BUILDING_MAX_LEVEL);
    REJECT(sr_building_upgrade(&g,2),SR_BUILDING_INVALID);
    a->owned=0; REJECT(sr_building_upgrade(&g,SR_FARMING),SR_BUILDING_INVALID);
    CHECK(sr_building_upgrade(0,SR_FARMING)==SR_BUILDING_INVALID);
    puts("Farm/city and mine/science prerequisites, costs, modifiers, level/index updates and atomic rejection passed.");
    return 0;
}
