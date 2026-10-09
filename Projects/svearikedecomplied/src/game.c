#include "game.h"
#include "catalog.h"
#include "family.h"
#include "generated/world_data.h"
static SrGame game={.random={1,0}};
void sr_game_seed(uint32_t seed) { sr_random_seed(&game.random,seed); }
const char *sr_game_head_name(void) {
    return game.family_head_count ? game.family_heads[game.family_head_count-1].name : "";
}
int sr_game_points(const SrGame *state) {
    if(!state) return 0;
    int points=state->point_mod+state->hq_level;
    for(int kind=SR_CULTURE;kind<=SR_SCIENCE;++kind) {
        const SrRecordList *list=&state->people[kind].owned;
        for(int i=0;i<list->count;++i) {
            const SrCatalogEntry *entry=sr_catalog_entry(kind,list->records[i]);
            if(entry) points+=entry->level;
        }
    }
    for(int i=1;i<=SR_AREA_COUNT;++i) {
        const SrAreaState *a=&state->areas[i];
        if(a->owned) points+=a->farming_level+a->mining_level+a->city_level+a->military_level;
    }
    for(int i=0;i<4;++i) points+=state->countries[i].diplomacy+state->countries[i].trade;
    return points;
}
const SrGame *sr_game_state(void) { return &game; }
SrGame *sr_game_runtime(void) { return &game; }
const SrAreaDef *sr_game_area(int number) {
    return number>=1 && number<=SR_AREA_COUNT ? &sr_area_defs[number-1] : 0;
}
const char *sr_game_area_name(int number) {
    const SrAreaDef *a=sr_game_area(number); return a ? a->name : "";
}
int sr_game_begin(int family) {
    if(family<1 || family>5) return 0;
    SrRandom random=game.random;
    game=(SrGame){0};
    game.random=random;
    /* SVEA MovieScript 1: initStartVariabels, initMilitary, initPlayerVar. */
    game.family=family; game.year=1523; game.turn=1; game.king=1;
    game.silver=1000; game.crops=100; game.metal=100;
    game.hq_level=1; game.troop_level=1;
    for(int i=0;i<5;++i) game.troop_capacity[i]=(i+1)*5000;
    static const int trade_capacity[]={10,25,50,100,250};
    for(int i=0;i<5;++i) game.trade_capacity[i]=trade_capacity[i];
    game.silver_production_mod=game.crops_production_mod=1;
    game.mining_upgrade_mod=game.diplomacy_upgrade_mod=game.trade_upgrade_mod=1;
    game.relation_cost_mod=game.trade_income_mod=game.troop_supply_mod=1;
    /* initEventsB precedes every other random draw in start(). */
    for(int i=0;i<6;++i)
        game.event_b_year[i]=sr_catalog_dated(SR_EVENTS_B,i+1)->base_year+sr_random_next(&game.random,15);
    const int starts[5]={6,4,1,2,3};
    game.current_area=starts[family-1];
    game.player_areas=(SrRecordList){1,{game.current_area}};
    /* initAreas: do not confuse member numbers (area+1) with area numbers. */
    for(int i=1;i<=SR_AREA_COUNT;++i) {
        SrAreaState *a=&game.areas[i];
        a->farming_level=a->farming_index=a->mining_level=a->mining_index=1;
        a->city_level=a->military_level=a->tax_level=1; a->population=250;
        a->owner=sr_area_defs[i-1].owner; a->riot=sr_area_defs[i-1].riot;
        if(sr_area_defs[i-1].owner=='A' && i!=game.current_area)
            game.available[game.available_count++]=i;
    }
    SrAreaState *home=&game.areas[game.current_area];
    home->special=7; home->owned=1;
    /* initCountries, including the original conquest priority order. */
    const int commodities[4]={25,25,40,50}, inf[4]={2000,2000,2000,3750}, cav[4]={1500,1000,2500,1500};
    static const int priorities[4][15]={
        {17,19,21,22,23,20,18,27,7,3,2,1,5,13,8},
        {27,18,17,24,28,25,9,6,7,3,2,1,5,13,8},
        {24,28,25,26,6,18,17,27,7,3,2,1,5,13,8},
        {25,26,24,28,12,6,11,15,9,14,2,1,5,13,8}
    };
    for(int i=0;i<4;++i) {
        SrCountry *c=&game.countries[i];
        c->diplomacy=c->trade=1;
        c->crops=c->metal=c->wanted_crops=c->wanted_metal=commodities[i];
        c->infantry=inf[i]; c->cavalry=cav[i]; c->artillery=1000;
        c->crops_index=c->metal_index=6;
        c->crops_price_mod=c->metal_price_mod=1;
        for(int j=0;j<15;++j) c->area_priority[j]=priorities[i][j];
        c->troop_level2_year=1610+sr_random_next(&game.random,40);
        c->troop_level3_year=1700+sr_random_next(&game.random,40);
    }
    /* initFAmily. Preserve the original Eka level=3, index=2 mismatch. */
    switch(family) {
    case 1: home->military_level=2; home->infantry=2000; home->cavalry=2000; break;
    case 2:
        game.crops+=50; game.metal+=50;
        for(int i=0;i<4;++i) game.countries[i].trade=2;
        break;
    case 3: home->farming_level=3; home->farming_index=2; break;
    case 4: game.silver+=250; break;
    case 5:
        game.silver+=100;
        for(int i=0;i<4;++i) game.countries[i].diplomacy=2;
        break;
    }
    sr_family_init(&game);
    return 1;
}
int sr_game_value(int field) {
    const SrAreaState *a=&game.areas[game.current_area];
    switch(field) {
    case 0:return game.year; case 1:return game.turn; case 2:return game.current_area;
    case 3:return (int)(game.silver+(game.silver<0?-0.5:0.5));
    case 4:return (int)(game.crops+(game.crops<0?-0.5:0.5));
    case 5:return (int)(game.metal+(game.metal<0?-0.5:0.5));
    case 6:return a->farming_level; case 7:return a->farming_index; case 8:return a->military_level;
    case 9:return a->infantry; case 10:return a->cavalry; case 11:return a->artillery;
    case 12:return game.hq_level; case 13:return game.random_initialization_pending;
    case 14:return a->tax_level;
    case 15:return sr_game_unrest(&game,game.current_area,0);
    case 16: {
        SrProduction p;
        return sr_game_production(&game,game.current_area,&p) ? sr_game_banner(p.silver,500) : 0;
    }
    case 17:return sr_game_banner(sr_game_unrest(&game,game.current_area,0),100);
    case 18:return a->population;
    case 19:return game.troop_level;
    case 28:return (int)game.random.calls;
    case 29:return sr_game_points(&game);
    case 30:return game.family_heads[0].type;
    case 31:return a->mining_level;
    case 32:return a->mining_index;
    case 33:return sr_game_science_level(&game);
    case 34:case 35:case 36:case 37: {
        double total=0;
        int all=(field==35 || field==37),mining=field>=36;
        for(int n=0;n<(all?game.player_areas.count:1);++n) {
            int i=all?game.player_areas.records[n]:game.current_area;
            SrProduction p;
            if(sr_game_production(&game,i,&p)) total+=mining?p.metal:p.crops;
        }
        return sr_game_banner(total,all?250:50);
    }
    default:
        if(field>=20 && field<24) return game.countries[field-20].trade;
        if(field>=24 && field<28) return game.countries[field-24].diplomacy;
        if(field>=40 && field<46) return game.event_b_year[field-40];
        if(field>=50 && field<54) return game.countries[field-50].troop_level2_year;
        if(field>=54 && field<58) return game.countries[field-54].troop_level3_year;
        return -1;
    }
}
int sr_game_price(int table,int level) {
    return table>=0 && table<16 && level>=1 && level<=5 ? sr_price_tables[table][level-1] : -1;
}
int sr_game_relation(int turn,int country) {
    return turn>=1 && turn<=60 && country>=1 && country<=4 ? sr_relation_table[turn-1][country-1] : -999;
}
int sr_game_production(const SrGame *state,int area,SrProduction *out) {
    if(!state || !out || area<1 || area>SR_AREA_COUNT) return 0;
    const SrAreaState *a=&state->areas[area];
    const SrAreaDef *d=&sr_area_defs[area-1];
    if(a->tax_level<1 || a->tax_level>5 || a->city_level<1 || a->city_level>5) return 0;
    /* MovieScript 2.fixIncome / MovieScript 6 banner scripts, before riots.
       Keep the original list literals and multiplication order. */
    static const double silver_mod[]={1,1.5,2,2.5,3};
    static const double goods_mod[]={1,0.90000000000000002,0.80000000000000004,0.70000000000000007,0.60000000000000009};
    out->silver=d->silver*silver_mod[a->city_level-1]*silver_mod[a->tax_level-1]*state->silver_production_mod;
    out->metal=d->metal*a->mining_index*goods_mod[a->tax_level-1];
    out->crops=d->crops*a->farming_index*(a->special==1?2:1)*goods_mod[a->tax_level-1]*state->crops_production_mod;
    return 1;
}
int sr_game_unrest(const SrGame *state,int area,int population_calculation) {
    if(!state || area<1 || area>SR_AREA_COUNT) return -1;
    const SrAreaState *a=&state->areas[area];
    if(a->tax_level<1 || a->tax_level>5) return -1;
    /* MovieScript 5.calcHappiness: integer division occurs before integer(). */
    int troops=(a->infantry+a->cavalry+a->artillery)/1000;
    int unrest=(a->tax_level-1)*10-troops-(a->special?10:0)+state->happiness_mod;
    if(!population_calculation) unrest+=a->riot;
    int minimum=(a->tax_level-1)*5;
    return unrest<minimum ? minimum : unrest;
}
int sr_game_banner(double value,int maximum) {
    if(maximum<=0 || !(value>0)) return 1;
    double scaled=value/(double)maximum*50;
    if(scaled>=50) return 50;
    /* D5 integer(float) rounds to nearest; positive half ties round up. */
    int index=(int)(scaled+0.5);
    return index<1 ? 1 : index;
}
int sr_game_set_tax(int level) {
    if(!game.current_area || level<1 || level>5) return 0;
    game.areas[game.current_area].tax_level=level;
    return 1;
}
int sr_game_change_tax(int amount) {
    if(!game.current_area) return 0;
    int current=game.areas[game.current_area].tax_level;
    /* Clamp before adding, so even out-of-range API inputs cannot overflow. */
    if(amount>5-current) return sr_game_set_tax(5);
    if(amount<1-current) return sr_game_set_tax(1);
    return sr_game_set_tax(current+amount);
}
static SrAreaState *army_area(SrGame *state) {
    if(!state || state->current_area<1 || state->current_area>SR_AREA_COUNT ||
       state->troop_level<1 || state->troop_level>3) return 0;
    SrAreaState *a=&state->areas[state->current_area];
    return a->owned && a->military_level>=1 && a->military_level<=5 ? a : 0;
}
int sr_army_recruit(SrGame *state,int troop) {
    SrAreaState *a=army_area(state);
    if(!a || troop<SR_INFANTRY || troop>SR_ARTILLERY) return SR_ARMY_INVALID;
    /* MovieScript 5.buyInf/buyCav/buyArt: preserve validation order and the
       pre-purchase capacity check (a partial regiment may exceed it by <1000). */
    if(troop==SR_CAVALRY && a->military_level<2) return SR_ARMY_BARRACKS;
    if(troop==SR_ARTILLERY) {
        if(a->artillery>9000) return SR_ARMY_ARTILLERY_LIMIT;
        int smithy=0;
        for(int i=1;i<=SR_AREA_COUNT;++i)
            if(state->areas[i].owned && state->areas[i].mining_level>1) smithy=1;
        if(!smithy) return SR_ARMY_SMITHY;
    }
    static const int costs[]={25,100,250};
    int cost=costs[troop]*state->troop_level;
    if(state->silver<cost) return SR_ARMY_SILVER;
    int total=a->infantry+a->cavalry+a->artillery;
    int capacity=state->troop_capacity[a->military_level-1]+a->troop_capacity_mod;
    if(total>=capacity) return SR_ARMY_CAPACITY;
    state->silver-=cost;
    if(troop==SR_INFANTRY) a->infantry+=1000;
    else if(troop==SR_CAVALRY) a->cavalry+=1000;
    else a->artillery+=1000;
    return SR_ARMY_OK;
}
int sr_army_upgrade(SrGame *state) {
    SrAreaState *a=army_area(state);
    if(!a) return SR_ARMY_INVALID;
    int level=a->military_level+1;
    if(level>5) return SR_ARMY_MAX_LEVEL;
    int silver=sr_game_price(8,level),metal=sr_game_price(9,level);
    int mining=level==3 ? 2 : (level==4 ? 3 : 0);
    if(level==5 && a->special!=2 && a->special!=7) return SR_ARMY_SPECIAL_BUILDING;
    if(a->mining_level<mining || state->silver<silver || state->metal<metal)
        return SR_ARMY_UPGRADE_REQUIREMENTS;
    /* Original level-5 branch has no mining test. Do not add one. */
    if(level==3 && state->troop_level==1) state->troop_level=2;
    if(level==5 && state->troop_level==2) state->troop_level=3;
    state->silver-=silver; state->metal-=metal; a->military_level=level;
    return SR_ARMY_OK;
}
int sr_game_recruit(int troop) { return sr_army_recruit(&game,troop); }
int sr_game_upgrade_military(void) { return sr_army_upgrade(&game); }
int sr_game_science_level(const SrGame *state) {
    if(!state) return 0;
    int level=0;
    const SrRecordList *list=&state->people[SR_SCIENCE].owned;
    for(int i=0;i<list->count;++i) {
        const SrCatalogEntry *entry=sr_catalog_entry(SR_SCIENCE,list->records[i]);
        if(entry) level+=entry->level;
    }
    for(int i=1;i<=SR_AREA_COUNT;++i)
        if(state->areas[i].owned && state->areas[i].special==4) level+=5;
    return level;
}
int sr_building_upgrade(SrGame *state,int kind) {
    if(!state || state->current_area<1 || state->current_area>SR_AREA_COUNT ||
       (kind!=SR_FARMING && kind!=SR_MINING)) return SR_BUILDING_INVALID;
    SrAreaState *a=&state->areas[state->current_area];
    int level=kind==SR_FARMING ? a->farming_level : a->mining_level;
    if(!a->owned || level<1 || level>5) return SR_BUILDING_INVALID;
    if(level==5) return SR_BUILDING_MAX_LEVEL;
    ++level;
    int table=kind==SR_FARMING ? 0 : 2;
    double modifier=kind==SR_FARMING ? 1 : state->mining_upgrade_mod;
    double silver=sr_game_price(table,level)*modifier,metal=sr_game_price(table+1,level)*modifier;
    if(kind==SR_FARMING) {
        /* Original validation order: level-five special building before city/resources. */
        if(level==5 && a->special!=1 && a->special!=7) return SR_BUILDING_SPECIAL;
        if(a->city_level<level || state->silver<silver || state->metal<metal) return SR_BUILDING_CITY_REQUIREMENTS;
    } else {
        static const int science[]={0,1,3,6,10};
        if(sr_game_science_level(state)<science[level-1] || state->silver<silver || state->metal<metal)
            return SR_BUILDING_SCIENCE_REQUIREMENTS;
    }
    state->silver-=silver; state->metal-=metal;
    if(modifier!=(int)modifier) state->silver_is_float=1;
    if(kind==SR_FARMING) a->farming_level=a->farming_index=level;
    else a->mining_level=a->mining_index=level;
    return SR_BUILDING_OK;
}
int sr_game_upgrade_building(int kind) { return sr_building_upgrade(&game,kind); }
