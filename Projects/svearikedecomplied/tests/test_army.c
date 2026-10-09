#include "game.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
#define REJECT(call,code) do { SrGame before; memcpy(&before,&g,sizeof g); CHECK((call)==(code)); CHECK(!memcmp(&g,&before,sizeof g)); } while(0)
int main(void) {
    CHECK(sr_game_begin(3));
    SrGame g=*sr_game_state();
    SrAreaState *a=&g.areas[1];
    CHECK(sr_army_recruit(&g,SR_INFANTRY)==SR_ARMY_OK);
    CHECK(g.silver==975 && a->infantry==1000);
    REJECT(sr_army_recruit(&g,SR_CAVALRY),SR_ARMY_BARRACKS);
    REJECT(sr_army_recruit(&g,SR_ARTILLERY),SR_ARMY_SMITHY);
    CHECK(sr_army_upgrade(&g)==SR_ARMY_OK);
    CHECK(g.silver==875 && g.metal==75 && a->military_level==2 && g.troop_level==1);
    CHECK(sr_army_recruit(&g,SR_CAVALRY)==SR_ARMY_OK);
    CHECK(g.silver==775 && a->cavalry==1000);
    REJECT(sr_army_upgrade(&g),SR_ARMY_UPGRADE_REQUIREMENTS);
    a->mining_level=2;
    CHECK(sr_army_upgrade(&g)==SR_ARMY_OK);
    CHECK(g.silver==575 && g.metal==25 && a->military_level==3 && g.troop_level==2);
    CHECK(sr_army_recruit(&g,SR_INFANTRY)==SR_ARMY_OK && g.silver==525);
    CHECK(sr_army_recruit(&g,SR_ARTILLERY)==SR_ARMY_OK && g.silver==25 && a->artillery==1000);
    REJECT(sr_army_recruit(&g,SR_INFANTRY),SR_ARMY_SILVER);
    g.silver=1000; g.metal=400;
    REJECT(sr_army_upgrade(&g),SR_ARMY_UPGRADE_REQUIREMENTS);
    a->mining_level=3;
    CHECK(sr_army_upgrade(&g)==SR_ARMY_OK && a->military_level==4);
    CHECK(g.silver==700 && g.metal==300 && g.troop_level==2);
    a->special=0;
    REJECT(sr_army_upgrade(&g),SR_ARMY_SPECIAL_BUILDING);
    a->special=7; a->mining_level=1; /* Original final upgrade omits a mining check. */
    CHECK(sr_army_upgrade(&g)==SR_ARMY_OK && a->military_level==5);
    CHECK(g.silver==200 && g.metal==100 && g.troop_level==3);
    REJECT(sr_army_upgrade(&g),SR_ARMY_MAX_LEVEL);
    CHECK(g.year==1523 && g.turn==1 && g.crops==100);

    /* Capacity is checked before adding 1000; preserve partial-regiment quirk. */
    CHECK(sr_game_begin(3)); g=*sr_game_state();
    a->infantry=4999;
    CHECK(sr_army_recruit(&g,SR_INFANTRY)==SR_ARMY_OK && a->infantry==5999);
    REJECT(sr_army_recruit(&g,SR_INFANTRY),SR_ARMY_CAPACITY);
    a->troop_capacity_mod=1000;
    CHECK(sr_army_recruit(&g,SR_INFANTRY)==SR_ARMY_OK && a->infantry==6999);

    /* An owned smithy anywhere permits artillery, even with military level 1. */
    g=*sr_game_state(); g.areas[2].mining_level=2;
    REJECT(sr_army_recruit(&g,SR_ARTILLERY),SR_ARMY_SMITHY);
    g.areas[2].owned=1;
    CHECK(sr_army_recruit(&g,SR_ARTILLERY)==SR_ARMY_OK && a->artillery==1000);
    a->military_level=5; g.silver=1000; a->artillery=9000;
    CHECK(sr_army_recruit(&g,SR_ARTILLERY)==SR_ARMY_OK && a->artillery==10000);
    REJECT(sr_army_recruit(&g,SR_ARTILLERY),SR_ARMY_ARTILLERY_LIMIT);
    REJECT(sr_army_recruit(&g,3),SR_ARMY_INVALID);
    g.current_area=0;
    REJECT(sr_army_upgrade(&g),SR_ARMY_INVALID);
    CHECK(sr_army_recruit(0,0)==SR_ARMY_INVALID);
    CHECK(sr_game_begin(1));
    CHECK(sr_game_recruit(SR_CAVALRY)==SR_ARMY_OK && sr_game_value(10)==3000 && sr_game_value(3)==900);
    puts("Military costs, prerequisites, upgrades, capacity quirks and atomic rejection passed.");
    return 0;
}
