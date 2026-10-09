#ifndef SR_BATTLE_SETUP_H
#define SR_BATTLE_SETUP_H
#include "battle_rules.h"
/* Bits preserve original numeric types, not merely fractional values. */
enum { SR_BATTLE_REAL_INF=1, SR_BATTLE_REAL_CAV=2 };
typedef struct {double infantry,cavalry;int artillery,level,real;} SrBattleArmy;
typedef struct {
    SrBattleTroop troop;
    int square,placed,x,y,actions,real;
} SrBattleUnit;
typedef struct {int count,per_icon,reload;} SrBattleGuns;
typedef struct {
    SrBattleArmy armies[2];
    SrBattleUnit units[20]; /* Swedish creation order, then enemy creation order. */
    SrBattleGuns guns[2];
    int swedes,enemies,placed,ready,contents[64];
    SrBattleFormation formation;
} SrBattleSetup;
/* Creates the original six-to-ten groups, including integer division losses.
   Swedish troop inputs are INTEGER in the strategy game. */
int sr_battle_setup(SrBattleSetup *b,const SrBattleArmy *swed,const SrBattleArmy *enemy);
/* Index is zero-based creation order; a deployed group cannot be moved again.
   Invalid drop squares leave the original visual position and occupancy intact,
   but retain the original's changed logical square until the next valid drop. */
int sr_battle_deploy(SrBattleSetup *b,int unit,int square);
int sr_battle_setup_enemy(SrBattleSetup *b);
/* Original end-of-battle survivor subtraction; includes initial division loss.
   Array order: Swedish infantry/cavalry, enemy infantry/cavalry. */
void sr_battle_losses(const SrBattleSetup *b,double losses[4]);
/* Numeric types of enemy casualties, including whole-valued FLOAT survivors. */
int sr_battle_loss_types(const SrBattleSetup *b);
#endif
