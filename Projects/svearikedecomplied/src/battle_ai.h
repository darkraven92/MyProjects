#ifndef SR_BATTLE_AI_H
#define SR_BATTLE_AI_H
#include "battle_setup.h"
enum { SR_BATTLE_ATTACK_BACK=1, SR_BATTLE_ATTACK_FORWARD, SR_BATTLE_ATTACK_SIDE,
    SR_BATTLE_MOVE_FORWARD, SR_BATTLE_MOVE_SIDE, SR_BATTLE_WAIT,
    SR_BATTLE_MOVE_BACK, SR_BATTLE_ENEMY_WINS };
typedef struct {int action,target;} SrBattleDecision;
/* One iteration of troopControl.resolveEnemyActions. The controller must scan
   the live board in ascending square order, including units whose actions are
   exhausted: the original still consumes decision RNG before checking actions.
   This function selects an action; it does not animate or execute that action. */
int sr_battle_enemy_decide(const SrBattleSetup *b,SrRandom *rng,int square,SrBattleDecision *out);
#endif
