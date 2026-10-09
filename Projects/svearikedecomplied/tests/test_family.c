#include "family.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    sr_game_seed(1); CHECK(sr_game_begin(3)); SrGame g=*sr_game_state();
    CHECK(!strcmp(g.family_heads[0].name,"Ture Gustafsson Eka"));
    CHECK(!strcmp(sr_family_title(&g.family_heads[0]),"Hovgunstling"));
    sr_random_seed(&g.random,1); g.year=1528;
    CHECK(sr_family_update(&g) && g.family_head_count==1 && g.random.calls==1);
    g=*sr_game_state(); sr_random_seed(&g.random,1); g.point_mod--;
    CHECK(sr_family_update(&g) && g.family_head_count==2 && g.random.calls==4);
    CHECK(g.family_heads[0].points==-1 && g.family_heads[0].stop_year==1523);
    CHECK(strstr(g.family_heads[1].name," Turesson Eka") && g.family_heads[1].start_points==-1);
    CHECK(!strcmp(sr_family_title(&g.family_heads[0]),"Fängslad"));
    sr_family_rebuild_titles(&g); CHECK(g.random.calls==5);
    const int years[]={1610,1611,1718,1719};
    for(int i=0;i<4;++i) {
        g=*sr_game_state(); sr_random_seed(&g.random,1); g.year=years[i];
        CHECK(sr_family_update(&g) && g.family_head_count==2 && g.random.calls==3);
        CHECK(g.family_heads[1].start_year==years[i] && strstr(g.family_heads[1].name," Turesson Eka"));
        /* Death roll 56; age forces succession. Next raw draw is 207579012. */
        const char *expected=i==0?"Magnus":i==3?"Fredrik":"Carl";
        CHECK(!strncmp(g.family_heads[1].name,expected,strlen(expected)));
    }
    g=*sr_game_state(); g.year=1573; g.point_mod+=100; sr_random_seed(&g.random,1);
    strcpy(g.family_heads[0].name,"Nils Nilsson Tre Rosor"); g.family=1;
    CHECK(sr_family_update(&g));
    CHECK(!strcmp(g.family_heads[0].name,"Nils Nilsson Tre Palpatine"));
    CHECK(strstr(g.family_heads[1].name," Nilsson Tre Rosor"));
    CHECK(!strcmp(sr_family_title(&g.family_heads[0]),"Rymdimperiets kejsare"));
    SrFamilyHead h={0}; h.type=h.title_type=1;
    h.points=14; CHECK(!strcmp(sr_family_title(&h),"Riksråd"));
    h.points=15; CHECK(!strcmp(sr_family_title(&h),"Riksföreståndare"));
    h.title_type=2; CHECK(!strcmp(sr_family_title(&h),"Kanslipresident"));
    puts("Succession draw order, name-era boundaries, patronymics, titles and the original Palpatine rule passed.");
    return 0;
}
