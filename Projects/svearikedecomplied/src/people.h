#ifndef SVEA_PEOPLE_H
#define SVEA_PEOPLE_H
#include "game.h"
#include "catalog.h"
enum { SR_HIRE_OK, SR_HIRE_SELECT, SR_HIRE_INVALID, SR_HIRE_SILVER,
       SR_HIRE_ARCH, SR_HIRE_UNIVERSITY };
/* MovieScript 3 buyPerson/buySciPerson and MovieScript 5 buyCommander.
   Records are one-based original catalog IDs, not list positions. */
int sr_people_price(int kind,int record);
int sr_people_hire(SrGame *game,int kind,int record);
#endif
