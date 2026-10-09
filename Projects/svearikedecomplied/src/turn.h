#ifndef SVEA_TURN_H
#define SVEA_TURN_H
#include "events.h"
enum {
    SR_TURN_NONE, SR_TURN_YEAR, SR_TURN_EVENT, SR_TURN_MINIGAME,
    SR_TURN_MINIGAME_RESULT, SR_TURN_WAR, SR_TURN_RIOT, SR_TURN_UNREST,
    SR_TURN_LOST_AREA, SR_TURN_TRADE_LOST, SR_TURN_STARVING,
    SR_TURN_FINISHED, SR_TURN_GAME_OVER, SR_TURN_FAULT
};
enum { SR_RIOT_STRIKE=2, SR_RIOT_TALK=3, SR_RIOT_CONCEDE=4 };
enum { SR_RIOT_NO_ERROR, SR_RIOT_NO_TROOPS, SR_RIOT_NO_SILVER };
typedef struct {
    int type, area, country, score;
    SrEvent event;
} SrTurnRequest;
typedef struct {
    int active, phase, year_index, event_b_slot, event_slot, scan, error;
    unsigned war_countries, trade_losses;
    int riots[SR_AREA_COUNT+1];
    SrTurnRequest pending;
} SrTurn;
int sr_turn_begin(SrGame *game,SrTurn *turn);
/* Idempotent while waiting: inspecting a request cannot consume random draws. */
const SrTurnRequest *sr_turn_advance(SrGame *game,SrTurn *turn);
/* YEAR acknowledgments are issued after the original 30-tick delay by the host.
   Other ordinary requests are acknowledged by the player's original controls.
   RIOT takes one of the original three decisions. Wars/minigames have separate
   completion calls and cannot be skipped through this method. */
int sr_turn_respond(SrGame *game,SrTurn *turn,int choice);
int sr_turn_minigame_finished(SrGame *game,SrTurn *turn,int score);
/* Battle subsystem must apply its complete outcome before calling this. */
int sr_turn_war_finished(SrGame *game,SrTurn *turn);
#endif
