#include "trade_orders.h"
#include "economy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
static int close_to(double a,double b) {return a-b<0.000001 && b-a<0.000001;}
int main(void) {
    for(int family=1;family<=5;++family) for(int country=1;country<=4;++country) {
        sr_game_seed(1);CHECK(sr_game_begin(family));SrGame g=*sr_game_state(),saved;
        CHECK(g.trade_capacity[0]==10 && g.trade_capacity[4]==250);
        SrCountry *c=&g.countries[country-1];c->trade=1;
        CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_OK && c->trade==2 && sr_trade_capacity(&g,country)==25);
        CHECK(g.silver==sr_game_state()->silver-100 && g.metal==sr_game_state()->metal-25);saved=g;
        CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_REQUIREMENTS && !memcmp(&g,&saved,sizeof g));
        g.hq_level=2;g.silver=2000;g.metal=1000;
        CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_OK && c->trade==3 && sr_trade_capacity(&g,country)==50);
        g.hq_level=3;CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_OK && c->trade==4 && sr_trade_capacity(&g,country)==100);
        g.silver=0;saved=g;
        CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_HARBOR && !memcmp(&g,&saved,sizeof g));
        int other=g.current_area==1?2:1;g.areas[other].special=6;
        CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_HARBOR);
        g.areas[g.current_area].special=6;CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_REQUIREMENTS);
        g.silver=1000;g.hq_level=1;
        CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_OK && c->trade==5 && g.silver==500 && g.metal==600);
        saved=g;CHECK(sr_trade_upgrade(&g,country)==SR_TRADE_MAX_LEVEL && !memcmp(&g,&saved,sizeof g));
        CHECK(g.random.calls==sr_game_state()->random.calls);
    }
    sr_game_seed(1);CHECK(sr_game_begin(3));SrGame base=*sr_game_state();
    /* Reserve/refund every commodity/country/direction; both loads share a cap. */
    for(int c=1;c<=4;++c) for(int kind=0;kind<=1;++kind) for(int sign=-1;sign<=1;sign+=2) {
        SrGame g=base;g.countries[c-1].trade=1;
        for(int n=0;n<10;++n) CHECK(sr_trade_arrow(&g,c,kind,sign)==SR_TRADE_OK);
        SrGame saved=g;
        CHECK(sr_trade_arrow(&g,c,kind,sign)==SR_TRADE_CAPACITY && !memcmp(&g,&saved,sizeof g));
        CHECK(sr_trade_arrow(&g,c,1-kind,-sign)==SR_TRADE_CAPACITY && !memcmp(&g,&saved,sizeof g));
        for(int n=0;n<10;++n) CHECK(sr_trade_arrow(&g,c,kind,-sign)==SR_TRADE_OK);
        CHECK(close_to(g.silver,base.silver) && g.crops==base.crops && g.metal==base.metal);
        CHECK(!g.trade_crops[c-1] && !g.trade_metal[c-1] && g.random.calls==base.random.calls);
    }
    SrGame g=base,saved;g.countries[0].crops_price_mod=1.25;g.countries[0].metal_price_mod=0.5;
    CHECK(sr_trade_arrow(&g,1,0,-1)==SR_TRADE_OK && g.silver==998.75 && g.silver_is_float);
    CHECK(sr_trade_arrow(&g,1,1,-1)==SR_TRADE_OK && g.silver==997.25);
    /* Original arrow checks can prevent undo despite an available refund. */
    g.crops=0;saved=g;CHECK(sr_trade_arrow(&g,1,0,1)==SR_TRADE_RESOURCES && !memcmp(&g,&saved,sizeof g));
    g=base;CHECK(sr_trade_arrow(&g,1,0,1)==SR_TRADE_OK);g.silver=0;saved=g;
    CHECK(sr_trade_arrow(&g,1,0,-1)==SR_TRADE_RESOURCES && !memcmp(&g,&saved,sizeof g));
    /* A pointer grab doesn't apply the arrow-specific restriction. Drag back
       to zero returns reserved goods even when silver is zero. */
    SrTradeDrag d={0};int marker=sr_trade_marker(&g,1,0,0);
    CHECK(sr_trade_drag_begin(&g,&d,1,0,marker+3,marker));
    sr_trade_drag_move(&d,107);CHECK(d.load==0 && d.x==104);
    CHECK(sr_trade_drag_commit(&g,&d) && !g.trade_crops[0] && g.crops==100 && !g.silver_is_float);
    /* Drag pricing rounds operands separately, and uses an integer-divided
       affordability limit. At 3.75 silver / 1.25, three crops cost four silver. */
    g=base;g.silver=3.75;g.countries[0].crops_price_mod=1.25;
    CHECK(sr_trade_drag_begin(&g,&d,1,0,104,104));sr_trade_drag_move(&d,0);
    CHECK(d.load==-3 && d.min==3);CHECK(sr_trade_drag_commit(&g,&d));
    CHECK(g.silver==0 && g.trade_crops[0]==-3 && g.crops==100);
    CHECK(sr_trade_drag_begin(&g,&d,1,0,d.x,d.x));sr_trade_drag_move(&d,104);
    CHECK(sr_trade_drag_commit(&g,&d) && g.silver==4 && !g.trade_crops[0]);
    /* Negative half-pixel marker rounding differs after returning to the panel. */
    g=base;g.trade_crops[0]=-1;
    CHECK(sr_trade_marker(&g,1,0,0)==100 && sr_trade_marker(&g,1,0,1)==101);
    g=base;g.silver=4.4;g.countries[0].crops_price_mod=1.5;
    CHECK(sr_trade_drag_begin(&g,&d,1,0,104,104) && d.min==2);
    sr_trade_drag_move(&d,0);CHECK(d.load==-2);
    /* Other resource reservations constrain either drag direction. */
    g=base;g.trade_metal[0]=8;g.metal-=8;
    CHECK(sr_trade_drag_begin(&g,&d,1,0,104,104));sr_trade_drag_move(&d,640);CHECK(d.load==2);
    sr_trade_drag_move(&d,0);CHECK(d.load==-2);
    /* Trade upgrades preserve fractional modifiers and reservation quantities. */
    g=base;g.trade_upgrade_mod=0.5625;g.trade_crops[0]=7;
    CHECK(sr_trade_upgrade(&g,1)==SR_TRADE_OK && g.silver==943.75 && g.metal==85.9375 && g.trade_crops[0]==7);
    /* Actual queued arrows integrate with existing settlement once. */
    g=base;g.countries[0].crops_price_mod=1.25;g.countries[1].metal_price_mod=0.5;
    for(int n=0;n<5;++n) CHECK(sr_trade_arrow(&g,1,0,1)==SR_TRADE_OK);
    for(int n=0;n<2;++n) CHECK(sr_trade_arrow(&g,2,1,-1)==SR_TRADE_OK);
    CHECK(g.silver==997 && g.crops==95 && g.metal==100);
    CHECK(!sr_economy_trade(&g,0) && g.silver==1003.25 && g.crops==95 && g.metal==102);
    saved=g;CHECK(!sr_economy_trade(&g,0) && !memcmp(&g,&saved,sizeof g));
    /* A war loses the reservation; an owned harbor preserves delivery. */
    for(int harbor=0;harbor<2;++harbor) {
        g=base;CHECK(sr_trade_arrow(&g,1,0,1)==SR_TRADE_OK);
        g.areas[g.current_area].special=harbor?6:7;
        CHECK(sr_economy_trade(&g,1)==(unsigned)!harbor && g.crops==99 && !g.trade_crops[0]);
        CHECK(g.silver==1000+harbor*g.countries[0].crops_price_mod);
    }
    CHECK(sr_trade_arrow(0,1,0,1)==SR_TRADE_INVALID && !sr_trade_capacity(&g,0));
    CHECK(sr_trade_arrow(&g,5,0,1)==SR_TRADE_INVALID && sr_trade_arrow(&g,1,2,1)==SR_TRADE_INVALID);
    CHECK(sr_trade_arrow(&g,1,0,2)==SR_TRADE_INVALID && sr_trade_upgrade(&g,0)==SR_TRADE_INVALID);
    puts("Trade reservations, original arrow/drag quirks, upgrade prerequisites and settlement passed.");
    return 0;
}
