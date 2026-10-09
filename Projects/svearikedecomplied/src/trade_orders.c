#include "trade_orders.h"
static int integer(double n) { return (int)(n+(n<0?-0.5:0.5)); }
static int absolute(int n) { return n<0?-n:n; }
static int valid(const SrGame *g,int country,int commodity) {
    return g && country>=1 && country<=4 && commodity>=0 && commodity<=1;
}
static int center(int country) {static const int x[]={104,536,392,248};return x[country-1];}
int sr_trade_capacity(const SrGame *g,int country) {
    if(!valid(g,country,0)) return 0;
    int level=g->countries[country-1].trade;
    return level>=1 && level<=5?g->trade_capacity[level-1]:0;
}
int sr_trade_upgrade(SrGame *g,int country) {
    if(!valid(g,country,0) || g->countries[country-1].trade<1) return SR_TRADE_INVALID;
    SrCountry *c=&g->countries[country-1];
    if(c->trade>=5) return SR_TRADE_MAX_LEVEL;
    int next=c->trade+1,hq=next==3?2:next==4?3:0;
    if(next==5) {
        int harbor=0;
        for(int n=0;n<g->player_areas.count;++n) {
            int area=g->player_areas.records[n];
            if(area>=1 && area<=SR_AREA_COUNT && g->areas[area].special==6) harbor=1;
        }
        if(!harbor) return SR_TRADE_HARBOR;
    }
    double silver=sr_game_price(4,next)*g->trade_upgrade_mod;
    double metal=sr_game_price(5,next)*g->trade_upgrade_mod;
    if(g->hq_level<hq || g->silver<silver || g->metal<metal) return SR_TRADE_REQUIREMENTS;
    g->silver-=silver;g->metal-=metal;c->trade=next;
    if(g->trade_upgrade_mod!=(int)g->trade_upgrade_mod) g->silver_is_float=1;
    return SR_TRADE_OK;
}
int sr_trade_arrow(SrGame *g,int country,int commodity,int amount) {
    if(!valid(g,country,commodity) || (amount!=-1 && amount!=1)) return SR_TRADE_INVALID;
    int capacity=sr_trade_capacity(g,country),i=country-1;
    if(capacity<=0) return SR_TRADE_INVALID;
    const SrCountry *c=&g->countries[i];
    double price=commodity?c->metal_price_mod*3:c->crops_price_mod;
    if(price<=0) return SR_TRADE_INVALID;
    int *load=commodity?&g->trade_metal[i]:&g->trade_crops[i];
    int other=commodity?g->trade_crops[i]:g->trade_metal[i];
    double *goods=commodity?&g->metal:&g->crops;
    /* Original checks the button's resource even when reversing an order
       would refund it. Do not replace this with a net-affordability check. */
    if(amount<0?g->silver<price:*goods<1) return SR_TRADE_RESOURCES;
    if(absolute(*load+amount)+absolute(other)>capacity) return SR_TRADE_CAPACITY;
    if(amount<0) {
        if(*load<=0) {g->silver-=price;g->silver_is_float=1;}
        else ++*goods;
    } else {
        if(*load>=0) --*goods;
        else {g->silver+=price;g->silver_is_float=1;}
    }
    *load+=amount;
    return SR_TRADE_OK;
}
int sr_trade_marker(const SrGame *g,int country,int commodity,int setup) {
    if(!valid(g,country,commodity)) return 0;
    int cap=sr_trade_capacity(g,country);
    if(!cap) return center(country);
    int load=commodity?g->trade_metal[country-1]:g->trade_crops[country-1];
    double shift=35.0/cap*load;
    /* setUpTradeMenu rounds center+shift; clickBarArrow rounds abs(shift).
       Their half-pixel positions differ for negative orders. */
    return setup?integer(center(country)+shift):center(country)+(load<0?-integer(-shift):integer(shift));
}
int sr_trade_drag_begin(const SrGame *g,SrTradeDrag *d,int country,int commodity,int mouse_x,int marker_x) {
    if(!d || !valid(g,country,commodity)) return 0;
    int cap=sr_trade_capacity(g,country),i=country-1;
    double price=commodity?g->countries[i].metal_price_mod*3:g->countries[i].crops_price_mod;
    if(cap<=0 || price<=0) return 0;
    int load=commodity?g->trade_metal[i]:g->trade_crops[i];
    int other=commodity?g->trade_crops[i]:g->trade_metal[i];
    *d=(SrTradeDrag){.active=1,.country=country,.commodity=commodity,.capacity=cap,
        .load=load,.x=marker_x,.offset=mouse_x-marker_x,.price=price,
        .silver=g->silver,.crops=g->crops,.metal=g->metal};
    if(load<0) d->silver+=-load*price;
    else if(commodity) d->metal+=load;
    else d->crops+=load;
    /* integer(...)/100 uses Lingo integer division, not /100.0. */
    d->min=integer(d->silver/price*100)/100;
    d->max=commodity?d->metal:d->crops;
    int available=cap-absolute(other);
    if(d->min>available) d->min=available;
    if(d->max>available) d->max=available;
    if(d->min<0) d->min=0;
    sr_trade_drag_move(d,mouse_x);
    return 1;
}
void sr_trade_drag_move(SrTradeDrag *d,int mouse_x) {
    if(!d || !d->active) return;
    int cx=center(d->country);double h=mouse_x-d->offset,step=d->capacity/35.0;
    if(h>cx+35) h=cx+35;
    if(h>cx+d->max/step) h=cx+d->max/step;
    if(h<cx-35) h=cx-35;
    if(h<cx-d->min/step) h=cx-d->min/step;
    double load=h==cx?0:h<cx?-1*d->capacity/35.0*(cx-h):d->capacity/35.0*(h-cx);
    if(h<cx && load < -d->min) load=-d->min;
    if(h>cx && load > d->max) load=d->max;
    d->load=integer(load);d->x=integer(h);
}
void sr_trade_drag_resources(const SrTradeDrag *d,double *silver,double *crops,double *metal) {
    *silver=d->silver;*crops=d->crops;*metal=d->metal;
    if(d->load>0) {
        if(d->commodity) *metal=integer(d->metal-d->load);
        else *crops=integer(d->crops-d->load);
    } else *silver=integer(d->silver)+integer(d->load*d->price);
}
int sr_trade_drag_commit(SrGame *g,SrTradeDrag *d) {
    if(!g || !d || !d->active) return 0;
    sr_trade_drag_resources(d,&g->silver,&g->crops,&g->metal);
    if(d->load<=0) g->silver_is_float=0;
    else if((d->commodity?g->trade_metal[d->country-1]:g->trade_crops[d->country-1])<0) g->silver_is_float=1;
    if(d->commodity) g->trade_metal[d->country-1]=d->load;
    else g->trade_crops[d->country-1]=d->load;
    d->active=0;return 1;
}
