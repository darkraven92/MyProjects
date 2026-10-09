#ifndef SR_BATTLE_SCENE_H
#define SR_BATTLE_SCENE_H
#include "battle_setup.h"
#include <stdint.h>
int sr_battle_scene_begin(const SrBattleArmy *swed,const SrBattleArmy *enemy,int country,SrRandom *rng,double commander);
uint8_t *sr_battle_asset_input(int bank);
uint32_t sr_battle_asset_capacity(void);
int sr_battle_asset_request(int bank);
int sr_battle_asset_load(int bank,int id,uint32_t size);
void sr_battle_scene_draw(uint8_t pixels[640*480*4]);
void sr_battle_scene_down(int x,int y);
void sr_battle_scene_move(int x,int y);
void sr_battle_scene_up(int x,int y);
void sr_battle_scene_cancel(void);
void sr_battle_scene_tick(uint32_t ticks);
int sr_battle_scene_value(int field);
int sr_battle_scene_unit(int unit,int field);
/* Available only after an outcome; leaves outputs untouched while playing. */
int sr_battle_scene_result(double losses[4],int *enemy_real,int *retreated);
/* One-shot sound commands: positive original KRIG member; -3 fades channel 1
   over 15 ticks. Channel indices are 0/1. Finish keeps the last effect playing. */
int sr_battle_scene_sound(int channel);
void sr_battle_scene_finish_audio(void);
#endif
