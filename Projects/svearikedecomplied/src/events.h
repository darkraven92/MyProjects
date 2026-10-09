#ifndef SVEA_EVENTS_H
#define SVEA_EVENTS_H
#include "game.h"
#include "catalog.h"
/* First four kinds use catalog.h's culture/science/commander/period-event IDs. */
enum { SR_EVENT_A=4, SR_EVENT_B=5 };
typedef struct { int kind, record; } SrEvent;
#define SR_EVENT_POOL_CAPACITY 164
int sr_list_has(const SrRecordList *list,int record);
int sr_list_add(SrRecordList *list,int record);
int sr_list_remove(SrRecordList *list,int record);
int sr_event_eligible(const SrGame *game,int kind,int record);
int sr_event_pool(const SrGame *game,SrEvent pool[SR_EVENT_POOL_CAPACITY]);
/* Consumes the selection roll even for forced turn-4/5 person arrivals. */
SrEvent sr_event_choose(SrGame *game);
SrEvent sr_event_choose_b(const SrGame *game);
/* Applies explicit C event handlers and records used/available lists. */
int sr_event_apply(SrGame *game,SrEvent event);
const char *sr_event_name(SrEvent event);
const char *sr_event_minigame(SrEvent event);
int sr_event_is_king(SrEvent event);
void sr_event_minigame_result(SrGame *game,SrEvent event,int score);
#endif
