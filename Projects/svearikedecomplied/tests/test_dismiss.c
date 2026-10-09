#include "dismiss.h"
#include "economy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    CHECK(sr_game_begin(3)); SrGame g=*sr_game_state(),before;
    g.player_areas=(SrRecordList){3,{1,28,2}};
    g.areas[28].owned=g.areas[2].owned=1;
    g.areas[1].infantry=2500; g.areas[1].cavalry=1000; g.areas[1].artillery=999;
    g.areas[28].infantry=4000; g.areas[28].cavalry=2000;g.areas[28].artillery=3000;
    g.areas[2].infantry=1000;
    SrDismiss d;
    CHECK(sr_dismiss_begin(&g,&d) && d.area==1 && d.areas.records[1]==28);
    before=g;
    CHECK(sr_dismiss_change(&g,&d,SR_INFANTRY,1));
    CHECK(sr_dismiss_remaining(&g,&d,SR_INFANTRY)==1500 && d.home[1][0]==1000);
    CHECK(sr_dismiss_change(&g,&d,SR_INFANTRY,1));
    CHECK(!sr_dismiss_change(&g,&d,SR_INFANTRY,1) && sr_dismiss_remaining(&g,&d,SR_INFANTRY)==500);
    CHECK(!sr_dismiss_change(&g,&d,SR_ARTILLERY,1)); /* Preserve sub-1000 survivors. */
    CHECK(sr_dismiss_change(&g,&d,SR_CAVALRY,1));
    CHECK(sr_dismiss_change(&g,&d,SR_CAVALRY,0) && !sr_dismiss_change(&g,&d,SR_CAVALRY,0));
    CHECK(!memcmp(&g,&before,sizeof g)); /* Selection consumes no troops, money or RNG. */
    CHECK(sr_dismiss_select(&d,2) && d.area==28);
    for(int kind=0;kind<3;++kind) CHECK(sr_dismiss_change(&g,&d,kind,1));
    CHECK(sr_dismiss_select(&d,99) && d.area==2 && sr_dismiss_change(&g,&d,SR_INFANTRY,1));
    CHECK(sr_dismiss_select(&d,-5) && d.area==1 && d.home[1][0]==2000);
    CHECK(sr_dismiss_change(&g,&d,SR_INFANTRY,0));
    CHECK(sr_dismiss_commit(&g,&d) && !d.active && !sr_dismiss_commit(&g,&d));
    CHECK(g.areas[1].infantry==1500 && g.areas[1].cavalry==1000 && g.areas[1].artillery==999);
    CHECK(g.areas[28].infantry==3000 && g.areas[28].cavalry==1000 && g.areas[28].artillery==2000);
    CHECK(g.areas[2].infantry==0 && g.current_area==1);
    CHECK(g.silver==before.silver && g.crops==before.crops && g.metal==before.metal &&
          !memcmp(&g.random,&before.random,sizeof g.random));
    CHECK(!sr_dismiss_change(&g,&d,SR_INFANTRY,1));
    CHECK(sr_dismiss_begin(&g,&d) && sr_dismiss_select(&d,2));
    CHECK(sr_dismiss_change(&g,&d,SR_ARTILLERY,1));
    g.areas[28].artillery=500; before=g;
    CHECK(!sr_dismiss_commit(&g,&d) && !memcmp(&g,&before,sizeof g));
    CHECK(sr_dismiss_begin(&g,&d)); g.player_areas.records[1]=2; before=g;
    CHECK(!sr_dismiss_commit(&g,&d) && !memcmp(&g,&before,sizeof g));
    CHECK(!sr_dismiss_change(&g,&d,-1,1) && !sr_dismiss_change(&g,&d,3,1));
    puts("Dismissal groups, undo, acquisition-order selection, provisional state and atomic multi-province commit passed.");
    return 0;
}
