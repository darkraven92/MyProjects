#ifndef SVEA_DIPLOMACY_H
#define SVEA_DIPLOMACY_H
#include "game.h"
enum { SR_DIPLOMACY_OK,SR_DIPLOMACY_INVALID,SR_DIPLOMACY_MAX_LEVEL,
    SR_DIPLOMACY_EMBASSY,SR_DIPLOMACY_REQUIREMENTS,SR_DIPLOMACY_SILVER,SR_DIPLOMACY_LIMIT };
int sr_diplomacy_upgrade(SrGame *game,int country);
int sr_diplomacy_adjust(SrGame *game,int country,int amount);
int sr_diplomacy_relation_icon(int relation);
const char *sr_diplomacy_country_name(int country);
#endif
