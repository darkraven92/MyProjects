#ifndef SVEA_MENU_H
#define SVEA_MENU_H
#include <stddef.h>
#include <stdint.h>
/* C browser presentation and input; no original bytecode execution. */
enum { SR_MENU, SR_SETUP, SR_CREDITS, SR_TEASER, SR_STRATEGY, SR_WELCOME, SR_CITY, SR_MILITARY, SR_FARM, SR_MINE, SR_CROSSBOW, SR_TURN_SCREEN, SR_DISMISS, SR_COMMANDER_SCREEN, SR_HQ_SCREEN, SR_CULTURE_SCIENCE, SR_LINNE_SCREEN, SR_NEIGHBORS, SR_DIPLOMACY, SR_TRADE_SCREEN, SR_WAR_SCREEN, SR_BATTLE_SCREEN };
enum { SR_NO_ACTION, SR_OPEN_SAVE, SR_PLAY_INTRO, SR_START_GAME, SR_QUIT, SR_PENDING_CONTROL, SR_ARMY_RESULT, SR_BUILDING_RESULT, SR_TURN_ACTION, SR_DISMISS_ACTION };
uint8_t *sr_menu_input(void);
uint32_t sr_menu_capacity(void);
int sr_menu_load(uint32_t size);
const uint8_t *sr_menu_render(void);
void sr_menu_down(int x, int y);
void sr_menu_up(int x, int y);
void sr_menu_cancel(void);
void sr_menu_move(int x,int y);
int sr_menu_hover_area(void);
int sr_menu_screen(void);
int sr_menu_family(void);
int sr_menu_action(void);
int sr_menu_result(void);
int sr_menu_error(void);
/* Standalone contest entry; strategy-triggered contests use the turn controller. */
int sr_menu_crossbow_preview(uint32_t seed);
void sr_menu_tick(uint32_t ticks);
int sr_menu_crossbow_value(int field);
int sr_menu_sound(void); /* Consume the next original channel-1 sample, or zero. */
int sr_menu_sound2(void); /* Channel 2; -1 stops, -2 ends looping; LINNE members add 1000. */
int sr_menu_linne_preview(void);
int sr_menu_linne_value(int field);
uint8_t *sr_menu_event_input(void);
uint32_t sr_menu_event_capacity(void);
int sr_menu_event_request(void); /* Original EVENT cast member to stream, or 0. */
int sr_menu_event_load(int member,uint32_t size);
int sr_menu_turn_value(int field);
const char *sr_menu_turn_name(void); /* Original legacy-encoded event name. */
const char *sr_menu_turn_minigame(void);
int sr_menu_dismiss_value(int field);
int sr_menu_commander_value(int field);
const char *sr_menu_commander_name(int position,int owned);
int sr_menu_person_value(int kind,int field);
const char *sr_menu_person_name(int kind,int position,int owned);
/* Original field content and metadata; glyph rendering is not implemented yet.
   Returned text uses a shared buffer; consume/copy it before another call. */
const char *sr_menu_field_text(int member);
const char *sr_menu_field_font(int member);
int sr_menu_field_metric(int member,int field);
/* Field metric 13 is the source STXT face; bit 0 denotes bold. */
int sr_menu_diplomacy_value(int country,int field);
const char *sr_menu_country_name(int country);
/* 0 level, 1 capacity, 2/3 signed crop/metal order, 4 action result,
   5/6 marker positions, 7 drag active. Orders include active drag previews. */
int sr_menu_trade_value(int country,int field);
int sr_menu_war_value(int field);
const char *sr_menu_war_area_name(int position,int regiment);
uint8_t *sr_menu_battle_input(int bank);
uint32_t sr_menu_battle_capacity(void);
int sr_menu_battle_request(int bank);
int sr_menu_battle_load(int bank,int id,uint32_t size);
int sr_menu_battle_value(int field);
int sr_menu_battle_unit(int unit,int field);
#endif
