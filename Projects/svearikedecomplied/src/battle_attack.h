#ifndef SR_BATTLE_ATTACK_H
#define SR_BATTLE_ATTACK_H
#include "battle_setup.h"
/* Asynchronous translation of troopControl.shootTrooper/attack. The caller
   retains the setup and RNG for the exchange's lifetime. Times are 60 Hz ticks.
   After a primary kill, advance_unit/advance_square request movetrooper; that
   movement consumes another unit action, but not another player command. */
typedef struct {
    SrBattleSetup *battle;
    SrRandom *rng;
    double commander;
    uint32_t start,end;
    int active,shooter,target,counter,damage,applied,artillery;
    int advance_unit,advance_square,swed_actions_lost;
    int sound; /* Latest unconsumed channel-2 cast ID, including counters. */
} SrBattleAttack;
int sr_battle_attack_begin(SrBattleAttack *a,SrBattleSetup *battle,SrRandom *rng,
    double commander,int shooter,int target,uint32_t ticks);
void sr_battle_attack_tick(SrBattleAttack *a,uint32_t ticks);
/* ArtTroop.shoot: one target, no counter/advance and no troop action cost. */
int sr_battle_attack_gun(SrBattleAttack *a,SrBattleSetup *battle,SrRandom *rng,
    int side,int target,uint32_t ticks);
/* Original pose numbers: 2 shoot, 3 die, 4 hit; zero means the standing bitmap. */
int sr_battle_attack_pose(const SrBattleAttack *a,int unit,uint32_t ticks,int *frame,int *direction);
#endif
