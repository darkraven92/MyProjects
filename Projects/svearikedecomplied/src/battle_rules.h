#ifndef SR_BATTLE_RULES_H
#define SR_BATTLE_RULES_H
#include "random.h"
enum { SR_BATTLE_EMPTY, SR_BATTLE_SWED, SR_BATTLE_ENEMY, SR_BATTLE_RIVER,
    SR_BATTLE_ROCK, SR_BATTLE_BLOCKED };
enum { SR_BATTLE_INF, SR_BATTLE_CAV };
enum { SR_BATTLE_WALK=1, SR_BATTLE_SHOOT=2 };
typedef struct {int count,squares[63],actions[63];} SrBattleRange;
typedef struct {int nation,type,level,attacked;double troops;} SrBattleTroop;
typedef struct {int damage,reply,attacker_dead,defender_dead;} SrBattleExchange;
typedef struct {int profile,row,squares[10];} SrBattleFormation;
/* setUpEnemy uses Swedish cavalry placement and the original troop totals.
   The numeric type of enemy cavalry matters for its division by two: pass
   enemy_cavalry_real for a Lingo FLOAT, even when its value is a whole number. */
int sr_battle_enemy_formation(int infantry,int cavalry,int artillery,int level,
    double enemy_cavalry,int enemy_cavalry_real,int enemy_artillery,int enemy_level,
    const int *cavalry_squares,int count,SrBattleFormation *out);
/* Board-local original coordinates; callers add the current map sprite origin. */
int sr_battle_point(int square,int *x,int *y);
int sr_battle_square(int x,int y,int width,int height);
int sr_battle_range(const int contents[64],int square,int movement,int attack,int nation,SrBattleRange *out);
/* Rule calculations only: animation, action selection and board mutation belong
   to the manual battle controller. Failed arguments consume no randomness. */
int sr_battle_melee(SrRandom *rng,SrBattleTroop *attacker,SrBattleTroop *defender,double commander,SrBattleExchange *out);
/* One damage roll, without applying HP loss. Primary attacks increment the
   target's attacked count; counterattacks use multiplier one. -1 is invalid. */
int sr_battle_damage(SrRandom *rng,SrBattleTroop *attacker,SrBattleTroop *defender,int counter,double commander);
int sr_battle_artillery(SrRandom *rng,int level,double troops,int target_type);
#endif
