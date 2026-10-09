#ifndef SR_BATTLE_ARTILLERY_H
#define SR_BATTLE_ARTILLERY_H
#include "battle_setup.h"
/* ScoreScript 28 checkTarget/fixTreatList. Returns a unit index, or -1.
   Equal sorted properties currently retain insertion order, so the last tied
   entry wins. This tie behavior still needs original-runtime verification. */
int sr_battle_artillery_target(const SrBattleSetup *b);
/* Per-gun ScoreScript 28/30 reload step. Ready guns remain ready until fired;
   the caller sets their counter to one when a shot starts. */
int sr_battle_gun_ready(int *reload,int level);
#endif
