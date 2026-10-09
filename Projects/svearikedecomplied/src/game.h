#ifndef SVEA_GAME_H
#define SVEA_GAME_H
#include "random.h"
#define SR_AREA_COUNT 28
#define SR_FAMILY_HEAD_CAPACITY 300
typedef struct { int count, records[33]; } SrRecordList;
typedef struct { SrRecordList used, available, owned; } SrPeople;
typedef struct {
    char name[80]; /* UTF-8; convert to the field's font encoding when drawing. */
    int start_year, stop_year, start_points, points, type, title_type;
} SrFamilyHead;
typedef struct {
    const char *name, *note;
    char owner;
    int crops, metal, silver, riot, price;
} SrAreaDef;
typedef struct {
    int farming_level, farming_index, mining_level, mining_index;
    int city_level, military_level, infantry, cavalry, artillery;
    int special, troop_capacity_mod, tax_level, population, owned;
    char owner;
    int riot;
} SrAreaState;
typedef struct {
    int diplomacy, relation, trade, crops, metal, wanted_crops, wanted_metal;
    double infantry, cavalry; /* Battle losses remain fractional until fixEnemyTroops. */
    int troops_real; /* Lingo FLOAT tags: infantry bit 0, cavalry bit 1. */
    int artillery, crops_index, metal_index;
    double crops_price_mod, metal_price_mod;
    int area_priority[15], troop_level2_year, troop_level3_year;
} SrCountry;
typedef struct {
    int family, year, turn, current_area;
    double silver, crops, metal; /* Preserve fractional costs and later trade income. */
    int silver_is_float; /* Lingo integer / 2 truncates; floating division does not. */
    int hq_level, troop_level, culture_level, king, war_area, war_wins;
    int troop_capacity[5], no_riot, crushed;
    int available_count, available[SR_AREA_COUNT];
    SrRecordList player_areas; /* Acquisition order matters for random draws. */
    double silver_production_mod, crops_production_mod;
    int happiness_mod;
    double mining_upgrade_mod, diplomacy_upgrade_mod, trade_upgrade_mod;
    double relation_cost_mod, trade_income_mod, troop_supply_mod;
    int trade_crops[4], trade_metal[4], player_relation_mod[4];
    int trade_capacity[5]; /* initEconomicValues; each purchased harbor adds 50 to all levels. */
    int trade_prices_updated; /* initTrade writes literal 1.0/3.0 before float-formatted prices. */
    SrAreaState areas[SR_AREA_COUNT+1];
    SrCountry countries[4];
    SrRandom random;
    int event_b_year[6], event_b_used[6];
    SrRecordList used_events;
    SrPeople people[3]; /* Culture, science, commanders; original list order. */
    int point_mod, points, family_head_count;
    SrFamilyHead family_heads[SR_FAMILY_HEAD_CAPACITY];
    int random_initialization_pending;
} SrGame;
typedef struct { double silver, crops, metal; } SrProduction;
enum { SR_ARMY_OK, SR_ARMY_INVALID, SR_ARMY_SILVER, SR_ARMY_CAPACITY,
    SR_ARMY_BARRACKS, SR_ARMY_ARTILLERY_LIMIT, SR_ARMY_SMITHY,
    SR_ARMY_MAX_LEVEL, SR_ARMY_UPGRADE_REQUIREMENTS, SR_ARMY_SPECIAL_BUILDING };
enum { SR_INFANTRY, SR_CAVALRY, SR_ARTILLERY };
enum { SR_FARMING, SR_MINING };
enum { SR_BUILDING_OK, SR_BUILDING_INVALID, SR_BUILDING_MAX_LEVEL,
    SR_BUILDING_SPECIAL, SR_BUILDING_CITY_REQUIREMENTS, SR_BUILDING_SCIENCE_REQUIREMENTS };
int sr_game_begin(int family);
/* Explicit seeding supports replay. New games otherwise continue the stream. */
void sr_game_seed(uint32_t seed);
const char *sr_game_head_name(void);
int sr_game_points(const SrGame *state);
const SrGame *sr_game_state(void);
/* C subsystem integration; not exported as a browser mutation API. */
SrGame *sr_game_runtime(void);
const SrAreaDef *sr_game_area(int number);
const char *sr_game_area_name(int number);
int sr_game_value(int field);
int sr_game_price(int table, int level);
int sr_game_relation(int turn, int country);
/* Preview production before riots, trade and troop supply; not an end turn. */
int sr_game_production(const SrGame *state, int area, SrProduction *out);
int sr_game_unrest(const SrGame *state, int area, int population_calculation);
int sr_game_banner(double value, int maximum);
int sr_game_set_tax(int level);
int sr_game_change_tax(int amount);
int sr_army_recruit(SrGame *state,int troop);
int sr_army_upgrade(SrGame *state);
int sr_game_recruit(int troop);
int sr_game_upgrade_military(void);
int sr_building_upgrade(SrGame *state,int kind);
int sr_game_upgrade_building(int kind);
int sr_game_science_level(const SrGame *state);
#endif
