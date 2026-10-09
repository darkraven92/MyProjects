#ifndef SVEA_ECONOMY_H
#define SVEA_ECONOMY_H
#include "game.h"
/* Individual original endTurn stages. The caller must orchestrate events,
   wars, riots and modal outcomes in script order; these do not advance time. */
void sr_economy_relations(SrGame *game);
void sr_economy_enemy_troops(SrGame *game);
/* Returns a bit per country whose trade was lost to war without a harbor. */
unsigned sr_economy_trade(SrGame *game,unsigned war_countries);
void sr_economy_prices(SrGame *game);
void sr_economy_income(SrGame *game,const int riot_types[SR_AREA_COUNT+1]);
double sr_economy_troop_cost(const SrGame *game);
/* No mutation on food shortage. Resume after dismissal without repeating income. */
int sr_economy_supply(SrGame *game);
void sr_economy_growth(SrGame *game);
#endif
