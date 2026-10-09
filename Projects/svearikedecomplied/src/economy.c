#include "economy.h"
#include "catalog.h"
#include "trade.h"
static int integer(double n) { return (int)(n+(n<0?-0.5:0.5)); }
static int clamp_relation(int relation) {
    return relation<-3 ? -3 : relation>10 ? 10 : relation;
}
void sr_economy_relations(SrGame *g) {
    if(!g || g->turn<1 || g->turn>60) return;
    /* MovieScript 3.checkRelation followed by MovieScript 2.checkrelationMod. */
    for(int i=0;i<4;++i) g->countries[i].relation+=sr_game_relation(g->turn,i+1);
    for(int i=0;i<4;++i) {
        SrCountry *c=&g->countries[i];
        if(g->player_relation_mod[i]) {
            double adjustment=g->player_relation_mod[i]/200.0*(c->diplomacy*0.10000000000000001+1.0)*
                (sr_random_next(&g->random,5)/5.0+0.5);
            int change=integer(adjustment);
            if(change>5) change=5;
            if(change<-5) change=-5;
            c->relation=clamp_relation(c->relation+change);
        }
        /* With no bribe the original clamps only the display variable, not
           the stored RelationLevel. Preserve that discrepancy. */
    }
}
void sr_economy_enemy_troops(SrGame *g) {
    if(!g) return;
    static const int base[4][3]={{2000,1500,1000},{2000,1000,1000},{2000,2500,1000},{3750,1500,1000}};
    for(int i=0;i<4;++i) {
        SrCountry *c=&g->countries[i];
        double multiplier=c->relation/15.0+1;
        double troops[3]={integer(c->infantry*multiplier),integer(c->cavalry*multiplier),integer(c->artillery*multiplier)};
        for(int j=0;j<3;++j) {
            int minimum=base[i][j]+(g->year-1522)*20;
            if(troops[j]<minimum)
                troops[j]=minimum*(sr_random_next(&g->random,25)/100.0+0.95000000000000007);
        }
        int level=g->year>c->troop_level3_year ? 3 : g->year>c->troop_level2_year ? 2 : 1;
        int maximum=55000+(g->troop_level-level)*10000;
        while(troops[0]+troops[1]+troops[2]>maximum)
            for(int j=0;j<3;++j) troops[j]*=0.90000000000000002;
        if(troops[2]>10000) troops[2]=10000;
        c->infantry=integer(troops[0]); c->cavalry=integer(troops[1]); c->artillery=integer(troops[2]);
        c->troops_real=0;
    }
}
unsigned sr_economy_trade(SrGame *g,unsigned wars) {
    if(!g) return 0;
    double silver=0,crops=0,metal=0;
    int harbor=0; unsigned lost=0;
    for(int n=0;n<g->player_areas.count;++n)
        if(g->areas[g->player_areas.records[n]].special==6) harbor=1;
    for(int i=0;i<4;++i) {
        int c=g->trade_crops[i],m=g->trade_metal[i];
        SrCountry *country=&g->countries[i];
        if((wars&(1u<<i)) && (c || m) && !harbor) { lost|=1u<<i; continue; }
        if(c>0) { silver+=c*1*country->crops_price_mod*g->trade_income_mod; g->silver_is_float=1; }
        else crops+=(-c)*g->trade_income_mod;
        country->crops+=c;
        if(m>0) { silver+=m*3*country->metal_price_mod*g->trade_income_mod; g->silver_is_float=1; }
        else metal+=(-m)*g->trade_income_mod;
        country->metal+=m;
    }
    g->silver+=silver; g->crops+=crops; g->metal+=metal;
    for(int i=0;i<4;++i) g->trade_crops[i]=g->trade_metal[i]=0;
    return lost;
}
static int next_price(SrRandom *rng,int index) {
    int trigger=sr_random_next(rng,4),direction=0;
    if(trigger==1) direction=sr_random_next(rng,index<4 || index>8 ? 3 : 2);
    return sr_trade_price_step(index,trigger,direction);
}
void sr_economy_prices(SrGame *g) {
    if(!g) return;
    g->trade_prices_updated=1;
    static const double prices[]={0.5,0.60000000000000009,0.70000000000000007,0.80000000000000004,
        0.90000000000000002,1.0,1.10000000000000009,1.25,1.5,1.75,2.0};
    for(int i=0;i<4;++i) {
        SrCountry *c=&g->countries[i];
        c->crops_index=next_price(&g->random,c->crops_index);
        c->crops_price_mod=prices[c->crops_index-1];
        c->metal_index=next_price(&g->random,c->metal_index);
        c->metal_price_mod=prices[c->metal_index-1];
    }
}
void sr_economy_income(SrGame *g,const int riots[SR_AREA_COUNT+1]) {
    if(!g) return;
    for(int n=0;n<g->player_areas.count;++n) {
        int area=g->player_areas.records[n]; SrProduction p;
        if(!sr_game_production(g,area,&p)) continue;
        double modifier=1;
        if(riots && (riots[area]==1 || riots[area]==3))
            modifier=1-(sr_random_next(&g->random,20)/100.0+0.10000000000000001);
        g->silver+=integer(p.silver*modifier);
        g->crops+=integer(p.crops*modifier);
        g->metal+=integer(p.metal*modifier);
    }
}
double sr_economy_troop_cost(const SrGame *g) {
    if(!g) return 0;
    double cost=0;
    for(int n=0;n<g->player_areas.count;++n) {
        const SrAreaState *a=&g->areas[g->player_areas.records[n]];
        cost+=(a->infantry+a->cavalry+a->artillery)/1000*g->troop_level*g->troop_supply_mod;
    }
    return cost;
}
int sr_economy_supply(SrGame *g) {
    if(!g) return 0;
    double cost=sr_economy_troop_cost(g);
    if(cost>g->crops) return 0;
    if(g->troop_supply_mod!=(int)g->troop_supply_mod) g->silver_is_float=1;
    g->silver-=cost; g->crops-=cost;
    return 1;
}
void sr_economy_growth(SrGame *g) {
    if(!g) return;
    int culture=0;
    const SrRecordList *owned=&g->people[SR_CULTURE].owned;
    for(int i=0;i<owned->count;++i) {
        const SrCatalogEntry *entry=sr_catalog_entry(SR_CULTURE,owned->records[i]);
        if(entry) culture+=entry->level;
    }
    for(int n=0;n<g->player_areas.count;++n)
        if(g->areas[g->player_areas.records[n]].special==3) culture+=5;
    static const int thresholds[]={1,7,15,20};
    if(g->hq_level>=1 && g->hq_level<5 && culture>=thresholds[g->hq_level-1]) ++g->hq_level;
    for(int n=0;n<g->player_areas.count;++n) {
        int area=g->player_areas.records[n]; SrAreaState *a=&g->areas[area];
        int growth=3+a->farming_level+a->mining_level+a->military_level+(a->special>0?2:0);
        growth-=sr_game_unrest(g,area,1)/10;
        a->population=integer(a->population*(1+growth/100.0));
        if(a->population>20000) a->city_level=5;
        else if(a->population>10000) a->city_level=4;
        else if(a->population>2500) a->city_level=3;
        else if(a->population>500) a->city_level=2;
        /* No else: the original retains the previous level at <=500. */
    }
}
