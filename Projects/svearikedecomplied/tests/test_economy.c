#include "game.h"
#include <limits.h>
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
static int near(double a,double b) { double d=a-b; return d>-0.000001 && d<0.000001; }
int main(void) {
    CHECK(!sr_game_set_tax(2));
    /* Golden production/unrest at all five tax levels, before riots and upkeep.
       Source: MovieScript 2.fixIncome, 5.calcHappiness and 6.fixCityBanners. */
    const double silver[5][5]={{40,60,80,100,120},{40,60,80,100,120},
        {50,75,100,125,150},{50,75,100,125,150},{50,75,100,125,150}};
    const double crops[5][5]={{10,9,8,7,6},{10,9,8,7,6},{24,21.6,19.2,16.8,14.4},
        {12,10.8,9.6,8.4,7.2},{12,10.8,9.6,8.4,7.2}};
    const double metal[5][5]={{4,3.6,3.2,2.8,2.4},{7,6.3,5.6,4.9,4.2},
        {10,9,8,7,6},{5,4.5,4,3.5,3},{7,6.3,5.6,4.9,4.2}};
    const int unrest[5]={0,5,10,20,30}, tre_rosor[5]={0,5,10,16,26};
    for(int f=1;f<=5;++f) {
        CHECK(sr_game_begin(f));
        int cash=sr_game_value(3),goods=sr_game_value(4);
        for(int tax=1;tax<=5;++tax) {
            CHECK(sr_game_set_tax(tax));
            SrProduction p;
            CHECK(sr_game_production(sr_game_state(),sr_game_value(2),&p));
            CHECK(near(p.silver,silver[f-1][tax-1]));
            CHECK(near(p.crops,crops[f-1][tax-1]));
            CHECK(near(p.metal,metal[f-1][tax-1]));
            CHECK(sr_game_value(15)==(f==1?tre_rosor[tax-1]:unrest[tax-1]));
        }
        CHECK(sr_game_value(3)==cash && sr_game_value(4)==goods && sr_game_value(1)==1);
        CHECK(sr_game_change_tax(INT_MAX) && sr_game_value(14)==5);
        CHECK(sr_game_change_tax(INT_MIN) && sr_game_value(14)==1);
        CHECK(!sr_game_set_tax(0) && !sr_game_set_tax(6) && sr_game_value(14)==1);
    }
    CHECK(sr_game_begin(3) && sr_game_value(14)==1);
    SrGame fixture=*sr_game_state();
    fixture.areas[1].special=1; /* Mill doubles grain; HQ does not. */
    fixture.areas[1].city_level=2;
    fixture.silver_production_mod=1.5; fixture.crops_production_mod=2;
    SrProduction p;
    CHECK(sr_game_production(&fixture,1,&p));
    CHECK(near(p.silver,112.5) && near(p.crops,96) && near(p.metal,10));
    fixture.areas[7].tax_level=3;
    CHECK(sr_game_unrest(&fixture,7,0)==30); /* Småland's base riot +10. */
    CHECK(sr_game_unrest(&fixture,7,1)==20); /* Population omits province modifier. */
    fixture.areas[7].infantry=1999;
    CHECK(sr_game_unrest(&fixture,7,0)==29); /* Integer division, not nearest. */
    CHECK(sr_game_banner(74.999,500)==7 && sr_game_banner(75,500)==8);
    CHECK(sr_game_banner(0,500)==1 && sr_game_banner(-5,500)==1);
    CHECK(sr_game_banner(1000,500)==50 && sr_game_banner(10,0)==1);
    CHECK(!sr_game_production(&fixture,0,&p) && !sr_game_production(0,1,&p));
    CHECK(sr_game_unrest(&fixture,29,0)==-1);
    puts("Five-family tax production, unrest, modifiers, rounding and non-advancement checks passed.");
    return 0;
}
