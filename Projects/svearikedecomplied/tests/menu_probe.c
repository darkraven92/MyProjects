#include "menu.h"
#include "game.h"
#include "crossbow.h"
#include "linne.h"
#include "diplomacy.h"
#include "trade_orders.h"
#include "war.h"
#include "battle_scene.h"
#include "turn.h"
#include "people.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
static int snapshot_pixels(const char *directory,const char *name,const uint8_t *pixels) {
    char path[1024];
    int n=snprintf(path,sizeof path,"%s/%s.rgba",directory,name);
    if(n<0 || (size_t)n>=sizeof path) return 0;
    FILE *f=fopen(path,"wb"); if(!f) return 0;
    size_t written=fwrite(pixels,1,640*480*4,f);
    int closed=fclose(f);
    return written==640*480*4 && !closed;
}
static int snapshot(const char *directory,const char *name) {
    return snapshot_pixels(directory,name,sr_menu_render());
}
static int battle_snapshot(const char *directory,const char *name) {
    static uint8_t pixels[640*480*4];sr_battle_scene_draw(pixels);
    return snapshot_pixels(directory,name,pixels);
}
static void click(int x,int y) { sr_menu_down(x,y); sr_menu_up(x,y); }
static int battle_assets(const char *pack_path) {
    const char *slash=strrchr(pack_path,'/');if(!slash) return 0;
    for(int bank=0;bank<3;++bank) {
        int id=sr_menu_battle_request(bank);if(!id) continue;
        char path[1024];int n=snprintf(path,sizeof path,"%.*s/battle/%d.pack",(int)(slash-pack_path),pack_path,id);
        if(n<0 || (size_t)n>=sizeof path) return 0;
        FILE *f=fopen(path,"rb");if(!f) return 0;
        if(fseek(f,0,SEEK_END)) {fclose(f);return 0;}
        long size=ftell(f);rewind(f);
        if(size<=0 || (unsigned long)size>sr_menu_battle_capacity()) {fclose(f);return 0;}
        size_t read=fread(sr_menu_battle_input(bank),1,(size_t)size,f);fclose(f);
        if(read!=(size_t)size || sr_menu_battle_load(bank,id+1,(uint32_t)size) ||
            sr_menu_battle_load(bank,id,(uint32_t)size-1) || !sr_menu_battle_load(bank,id,(uint32_t)size)) return 0;
    }
    return 1;
}
static int event_bitmap(const char *pack_path) {
    int member=sr_menu_event_request();
    if(!member) return 0;
    const char *slash=strrchr(pack_path,'/'); if(!slash) return 0;
    char path[1024];
    int n=snprintf(path,sizeof path,"%.*s/events/%d.rgba",(int)(slash-pack_path),pack_path,member);
    if(n<0 || (size_t)n>=sizeof path) return 0;
    FILE *f=fopen(path,"rb"); if(!f) return 0;
    if(fseek(f,0,SEEK_END)) { fclose(f); return 0; }
    long size=ftell(f); rewind(f);
    if(size<=0 || (unsigned long)size>sr_menu_event_capacity()) { fclose(f); return 0; }
    size_t read=fread(sr_menu_event_input(),1,(size_t)size,f); fclose(f);
    if(read!=(size_t)size || sr_menu_event_load(member+1,(uint32_t)size) ||
       sr_menu_event_load(member,(uint32_t)size-1)) return 0;
    return sr_menu_event_load(member,(uint32_t)size);
}
int main(int argc,char **argv) {
    CHECK(argc==3);
    FILE *f=fopen(argv[1],"rb"); CHECK(f);
    CHECK(!fseek(f,0,SEEK_END)); long size=ftell(f);
    CHECK(size>0 && (unsigned long)size<=sr_menu_capacity()); rewind(f);
    CHECK(fread(sr_menu_input(),1,(size_t)size,f)==(size_t)size); fclose(f);
    CHECK(!sr_menu_load((uint32_t)size-1));
    CHECK(sr_menu_load((uint32_t)size));
    CHECK(sr_menu_screen()==SR_MENU);
    CHECK(snapshot(argv[2],"menu"));
    sr_menu_down(520,25); CHECK(snapshot(argv[2],"menu-pressed"));
    sr_menu_up(100,100); CHECK(sr_menu_screen()==SR_MENU); /* Release outside cancels. */
    click(520,60); CHECK(sr_menu_action()==SR_OPEN_SAVE);
    click(520,95); CHECK(sr_menu_action()==SR_PLAY_INTRO);
    click(520,135); CHECK(sr_menu_action()==SR_QUIT);
    click(530,410); CHECK(sr_menu_screen()==SR_CREDITS);
    click(100,100); CHECK(sr_menu_screen()==SR_TEASER);
    click(100,100); CHECK(sr_menu_screen()==SR_MENU);
    click(520,25); CHECK(sr_menu_screen()==SR_SETUP && sr_menu_family()==3);
    CHECK(snapshot(argv[2],"setup"));
    const int x[]={60,170,280,400,520};
    for(int i=0;i<5;++i) { click(x[i],100); CHECK(sr_menu_family()==i+1); }
    CHECK(snapshot(argv[2],"setup-family5"));
    sr_menu_down(300,440); sr_menu_cancel(); sr_menu_up(300,440);
    CHECK(sr_menu_action()==SR_NO_ACTION);
    click(300,440); CHECK(sr_menu_action()==SR_START_GAME && sr_menu_family()==5);
    CHECK(sr_menu_screen()==SR_WELCOME);
    const int homes[]={6,4,1,2,3};
    for(int i=0;i<5;++i) {
        CHECK(sr_menu_load((uint32_t)size));
        click(520,25); click(x[i],100); click(300,440);
        CHECK(sr_menu_screen()==SR_WELCOME && sr_game_value(2)==homes[i]);
        char name[64];
        snprintf(name,sizeof name,"welcome-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_move(120,370); CHECK(sr_menu_hover_area()==0);
        sr_menu_down(390,395);
        snprintf(name,sizeof name,"welcome-pressed-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_up(300,300); CHECK(sr_menu_screen()==SR_WELCOME);
        click(390,395); CHECK(sr_menu_screen()==SR_STRATEGY);
        CHECK(!strcmp(sr_menu_field_text(130),"1523") && !strcmp(sr_menu_field_font(130),"Arial"));
        CHECK(sr_menu_field_metric(130,0)==236 && sr_menu_field_metric(130,1)==31);
        CHECK(!strcmp(sr_menu_field_font(207),"MS Sans Serif") && !strcmp(sr_menu_field_text(207)," "));
        CHECK(!*sr_menu_field_text(140) && sr_menu_field_metric(140,0)==-1);
        snprintf(name,sizeof name,"strategy-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_move(120,370); CHECK(sr_menu_hover_area()==1);
        CHECK(!strcmp(sr_menu_field_text(207),"Uppland"));
        sr_menu_move(115,277); CHECK(sr_menu_hover_area()==23); /* Overlaps area 16. */
        sr_menu_move(0,0); CHECK(sr_menu_hover_area()==0);
        click(479,317); CHECK(sr_menu_screen()==SR_HQ_SCREEN);
        snprintf(name,sizeof name,"hq-family%d",i+1);CHECK(snapshot(argv[2],name));
        click(403,280);CHECK(sr_menu_action()==SR_PENDING_CONTROL && sr_menu_screen()==SR_HQ_SCREEN);
        sr_menu_down(404,401);
        snprintf(name,sizeof name,"hq-pressed-family%d",i+1);CHECK(snapshot(argv[2],name));
        sr_menu_up(100,100);CHECK(sr_menu_screen()==SR_HQ_SCREEN);
        click(495,280);CHECK(sr_menu_screen()==SR_CULTURE_SCIENCE);
        CHECK(!strcmp(sr_menu_field_text(140)," ") && !strcmp(sr_menu_field_text(144)," "));
        CHECK(!sr_menu_person_value(SR_CULTURE,3) && !sr_menu_person_value(SR_SCIENCE,3));
        snprintf(name,sizeof name,"culture-empty-family%d",i+1);CHECK(snapshot(argv[2],name));
        click(320,120);click(510,120);
        CHECK(!sr_menu_person_value(SR_CULTURE,0) && !sr_menu_person_value(SR_SCIENCE,0));
        click(371,407);CHECK(sr_menu_error() && sr_menu_person_value(SR_CULTURE,5)==SR_HIRE_SELECT);
        click(371,436);CHECK(sr_menu_screen()==SR_CULTURE_SCIENCE);
        click(420,294);click(371,436);CHECK(sr_menu_screen()==SR_HQ_SCREEN);
        click(404,401);CHECK(sr_menu_screen()==SR_STRATEGY);
        int resources=sr_game_value(3);
        for(int kind=0;kind<2;++kind) {
            click(kind?301:452,kind?143:190);
            CHECK(sr_menu_screen()==(kind?SR_MINE:SR_FARM));
            snprintf(name,sizeof name,"%s-family%d",kind?"mine":"farm",i+1); CHECK(snapshot(argv[2],name));
            sr_menu_down(402,219);
            snprintf(name,sizeof name,"%s-pressed-family%d",kind?"mine":"farm",i+1); CHECK(snapshot(argv[2],name));
            sr_menu_move(100,100); sr_menu_up(100,100);
            CHECK(!sr_menu_error());
            click(402,219);
            CHECK(sr_menu_error() && sr_menu_result()==(kind?SR_BUILDING_SCIENCE_REQUIREMENTS:SR_BUILDING_CITY_REQUIREMENTS));
            CHECK(sr_game_value(3)==resources);
            click(404,401); CHECK(sr_menu_screen()==(kind?SR_MINE:SR_FARM));
            click(420,294); CHECK(!sr_menu_error());
            sr_menu_down(404,401); sr_menu_cancel(); sr_menu_up(404,401);
            CHECK(sr_menu_screen()==(kind?SR_MINE:SR_FARM));
            click(404,401); CHECK(sr_menu_screen()==SR_STRATEGY);
        }
        click(327,260); CHECK(sr_menu_screen()==SR_CITY && sr_game_value(14)==1);
        snprintf(name,sizeof name,"city-family%d",i+1); CHECK(snapshot(argv[2],name));
        click(515,290); CHECK(sr_game_value(14)==2);
        snprintf(name,sizeof name,"city-tax2-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_down(382,290); sr_menu_move(350,290); sr_menu_up(350,290);
        CHECK(sr_game_value(14)==2); /* Moving left must cross the previous stop. */
        sr_menu_down(382,290); sr_menu_move(700,290);
        CHECK(sr_game_value(14)==2); /* Drag commits only on release. */
        sr_menu_up(700,290); CHECK(sr_game_value(14)==5);
        CHECK(sr_game_value(15)==(i==0?26:30));
        snprintf(name,sizeof name,"city-tax5-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_down(492,290); sr_menu_move(0,290); sr_menu_cancel();
        CHECK(sr_game_value(14)==5);
        sr_menu_down(497,290); sr_menu_move(350,290); sr_menu_up(350,290);
        CHECK(sr_game_value(14)==1); /* Grab offset preserved, clamped at left. */
        click(322,290); CHECK(sr_game_value(14)==1);
        sr_menu_down(390,448);
        snprintf(name,sizeof name,"city-ok-pressed-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_up(390,448); CHECK(sr_menu_screen()==SR_STRATEGY);
        CHECK(sr_game_value(1)==1 && sr_game_value(0)==1523);
        int start_silver=sr_game_value(3),start_metal=sr_game_value(5);
        click(349,379); CHECK(sr_menu_screen()==SR_MILITARY);
        snprintf(name,sizeof name,"army-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_down(480,194);
        snprintf(name,sizeof name,"army-infantry-pressed-family%d",i+1); CHECK(snapshot(argv[2],name));
        sr_menu_move(100,100); sr_menu_up(100,100); CHECK(sr_game_value(3)==start_silver);
        click(480,194); CHECK(sr_menu_result()==SR_ARMY_OK && sr_game_value(3)==start_silver-25);
        CHECK(sr_game_value(9)==(i==0?3000:1000));
        if(i!=0) {
            click(480,253); CHECK(sr_menu_error() && sr_menu_result()==SR_ARMY_BARRACKS);
            click(480,194); CHECK(sr_game_value(3)==start_silver-25); /* Modal blocks buying. */
            click(420,294); CHECK(!sr_menu_error());
        }
        click(390,142);
        if(i==0) {
            CHECK(sr_menu_error() && sr_menu_result()==SR_ARMY_UPGRADE_REQUIREMENTS);
            click(420,294);
        } else CHECK(sr_game_value(3)==start_silver-125 && sr_game_value(5)==start_metal-25);
        CHECK(sr_game_value(8)==2);
        click(480,253); CHECK(sr_menu_result()==SR_ARMY_OK && sr_game_value(10)==(i==0?3000:1000));
        CHECK(sr_game_value(3)==start_silver-(i==0?125:225));
        click(480,314); CHECK(sr_menu_error() && sr_menu_result()==SR_ARMY_SMITHY);
        snprintf(name,sizeof name,"army-error-family%d",i+1); CHECK(snapshot(argv[2],name));
        click(420,294); CHECK(!sr_menu_error());
        snprintf(name,sizeof name,"army-upgraded-family%d",i+1); CHECK(snapshot(argv[2],name));
        int inf=sr_game_value(9),cav=sr_game_value(10),silver_now=sr_game_value(3);
        click(393,372);CHECK(sr_menu_screen()==SR_DISMISS);
        snprintf(name,sizeof name,"dismiss-family%d",i+1);CHECK(snapshot(argv[2],name));
        sr_menu_down(452,251);sr_menu_up(100,100);CHECK(sr_menu_dismiss_value(4)==0);
        click(452,251);CHECK(sr_menu_dismiss_value(4)==1000 && sr_game_value(9)==inf);
        click(355,251);CHECK(sr_menu_dismiss_value(4)==0);
        click(452,251);click(449,322);click(449,392);
        CHECK(sr_menu_dismiss_value(4)==1000 && sr_menu_dismiss_value(5)==1000 && !sr_menu_dismiss_value(6));
        snprintf(name,sizeof name,"dismiss-selected-family%d",i+1);CHECK(snapshot(argv[2],name));
        sr_menu_down(403,441);sr_menu_cancel();sr_menu_up(403,441);
        CHECK(sr_menu_screen()==SR_DISMISS && sr_game_value(9)==inf);
        click(403,441);CHECK(sr_menu_screen()==SR_MILITARY && sr_game_value(9)==inf-1000 && sr_game_value(10)==cav-1000);
        CHECK(sr_game_value(3)==silver_now);
        click(393,372);CHECK(!sr_menu_dismiss_value(4) && !sr_menu_dismiss_value(5));
        click(403,441);CHECK(sr_game_value(9)==inf-1000);
        click(470,450); CHECK(sr_menu_screen()==SR_STRATEGY);
        snprintf(name,sizeof name,"strategy-upgraded-family%d",i+1); CHECK(snapshot(argv[2],name));
    }
    CHECK(sr_menu_crossbow_preview(1) && sr_menu_screen()==SR_CROSSBOW);
    CHECK(snapshot(argv[2],"crossbow-intro"));
    sr_menu_down(320,432); sr_menu_cancel(); sr_menu_up(320,432);
    CHECK(sr_menu_crossbow_value(0)==SR_CROSSBOW_INTRO);
    click(320,432);
    uint32_t ticks=0;
    while(sr_menu_crossbow_value(0)!=SR_CROSSBOW_AIM && ticks<1000) sr_menu_tick(++ticks);
    CHECK(sr_menu_crossbow_value(0)==SR_CROSSBOW_AIM && sr_menu_crossbow_value(12)==3);
    CHECK(snapshot(argv[2],"crossbow-ready"));
    sr_menu_down(300,300); sr_menu_move(337,287);
    CHECK(sr_menu_crossbow_value(8)==320 && sr_menu_crossbow_value(9)==499);
    CHECK(snapshot(argv[2],"crossbow-aim"));
    sr_menu_up(337,287);
    while(sr_menu_crossbow_value(0)!=SR_CROSSBOW_AIM && ticks<2000) sr_menu_tick(++ticks);
    CHECK(sr_menu_crossbow_value(3)==5 && sr_menu_crossbow_value(2)==4);
    CHECK(snapshot(argv[2],"crossbow-hit"));
    /* Real menu -> end turn -> streamed event -> contest -> result -> strategy.
       Seed 8 naturally draws commander 1 in 1524; no debug state injection. */
    CHECK(sr_menu_load((uint32_t)size)); sr_game_seed(8);
    click(520,25);click(300,440);click(390,395);
    sr_menu_down(600,435);sr_menu_cancel();sr_menu_up(600,435);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1523);
    click(600,435);
    CHECK(sr_menu_screen()==SR_TURN_SCREEN && sr_menu_turn_value(0)==SR_TURN_YEAR);
    CHECK(!sr_menu_crossbow_preview(1)); /* Cannot replace an active turn. */
    sr_menu_tick(29); CHECK(sr_game_value(0)==1523);
    click(327,260); CHECK(sr_menu_screen()==SR_TURN_SCREEN);
    sr_menu_tick(30);
    CHECK(sr_game_value(0)==1524 && sr_menu_turn_value(0)==SR_TURN_EVENT && sr_menu_event_request()==1);
    click(441,430); CHECK(sr_menu_turn_value(0)==SR_TURN_EVENT); /* Wait for asset. */
    CHECK(event_bitmap(argv[1])); CHECK(snapshot(argv[2],"turn-commander"));
    sr_menu_down(441,430);sr_menu_cancel();sr_menu_up(441,430);
    CHECK(sr_menu_screen()==SR_TURN_SCREEN);
    click(441,430); CHECK(sr_menu_screen()==SR_CROSSBOW && sr_menu_turn_value(9));
    click(320,432);
    ticks=30; int shots=0;
    const int wind_mod[8][2]={{2,0},{1,1},{0,1},{-1,1},{-2,0},{-1,-1},{0,-2},{1,-1}};
    while(sr_menu_screen()==SR_CROSSBOW && ticks<10000) {
        if(sr_menu_crossbow_value(0)==SR_CROSSBOW_AIM) {
            int wind=sr_menu_crossbow_value(4)-1;
            int x=320-wind_mod[wind][0]*8, y=(264-wind_mod[wind][1]*8+1730)/4;
            int mx=300+x-sr_menu_crossbow_value(6),my=300+y-sr_menu_crossbow_value(7);
            sr_menu_down(300,300);sr_menu_move(mx,my);sr_menu_up(mx,my);++shots;
        }
        sr_menu_tick(++ticks);
    }
    CHECK(shots==15 && sr_menu_turn_value(0)==SR_TURN_MINIGAME_RESULT && sr_menu_turn_value(5)==10);
    CHECK(sr_game_value(3)==2000 && sr_game_state()->people[SR_COMMANDERS].owned.records[0]==1);
    CHECK(snapshot(argv[2],"turn-crossbow-result"));
    for(int i=0;i<100;++i) sr_menu_tick(++ticks);
    CHECK(sr_game_value(3)==2000); /* Result screen cannot reward twice. */
    click(420,294);
    while(sr_menu_screen()==SR_TURN_SCREEN && ticks<12000) {
        CHECK(sr_menu_turn_value(0)==SR_TURN_YEAR);
        sr_menu_tick(++ticks);
    }
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1528 && sr_game_value(1)==2);
    CHECK(sr_game_value(3)==2050 && sr_game_value(4)==124 && sr_game_value(5)==110);
    CHECK(snapshot(argv[2],"turn-complete"));
    CHECK(!sr_menu_event_load(0,640*480*4));
    click(349,379);click(489,372);
    CHECK(sr_menu_screen()==SR_COMMANDER_SCREEN && !sr_menu_commander_value(3) && sr_menu_commander_value(4)==1);
    CHECK(!strcmp(sr_menu_commander_name(1,1),"Jacob Bagge af Boo"));
    /* Winning the contest already employs Bagge. A second ordinary new game
       naturally draws Berent von Melen, who must actually be hired. */
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(65);
    click(520,25);click(300,440);click(390,395);click(600,435);ticks=0;
    while(sr_menu_turn_value(0)==SR_TURN_YEAR && ticks<300) sr_menu_tick(++ticks);
    CHECK(sr_menu_turn_value(0)==SR_TURN_EVENT && sr_menu_turn_value(1)==SR_COMMANDERS && sr_menu_turn_value(2)==2);
    CHECK(event_bitmap(argv[1]));click(441,430);
    while(sr_menu_screen()==SR_TURN_SCREEN && ticks<600) sr_menu_tick(++ticks);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1528 && sr_game_value(3)==1050);
    click(349,379);click(489,372);
    CHECK(sr_menu_screen()==SR_COMMANDER_SCREEN);
    CHECK(sr_menu_commander_value(3)==1);
    CHECK(!sr_menu_commander_value(0));
    CHECK(!strcmp(sr_menu_commander_name(1,0),"Berent von Melen"));
    CHECK(snapshot(argv[2],"commanders-available"));
    click(392,408);CHECK(sr_menu_error() && sr_menu_commander_value(5)==SR_HIRE_SELECT);
    click(435,100);CHECK(!sr_menu_commander_value(0)); /* Modal blocks list selection. */
    CHECK(snapshot(argv[2],"commanders-error"));
    click(420,294);click(435,100);
    CHECK(sr_menu_commander_value(0)==2 && sr_menu_commander_value(1)==1 && sr_menu_commander_value(2)==200);
    sr_menu_down(392,408);CHECK(snapshot(argv[2],"commanders-hire-pressed"));
    sr_menu_up(100,100);CHECK(sr_game_value(3)==1050 && !sr_menu_commander_value(4));
    uint32_t hire_random=sr_game_state()->random.calls;
    click(392,408);
    CHECK(!sr_menu_error() && !sr_menu_commander_value(0) && !sr_menu_commander_value(3) && sr_menu_commander_value(4)==1);
    CHECK(sr_game_value(3)==850 && sr_game_value(4)==124 && sr_game_value(5)==110 &&
          sr_game_state()->random.calls==hire_random);
    CHECK(sr_game_state()->people[SR_COMMANDERS].owned.records[0]==2);
    CHECK(snapshot(argv[2],"commanders-hired"));
    click(392,408);CHECK(sr_menu_error() && sr_game_value(3)==850);
    click(420,294);click(392,437);CHECK(sr_menu_screen()==SR_MILITARY);
    click(489,372);CHECK(!sr_menu_commander_value(3) && sr_menu_commander_value(4)==1);
    click(392,437);click(470,450);CHECK(sr_menu_screen()==SR_STRATEGY);
    /* Seed 4: normal culture arrival, hiring, estate growth, forced scientist
       arrival on turn four and a mine upgrade. No game-state injection. */
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(4);
    click(520,25);click(300,440);click(390,395);ticks=0;
    const int person_kinds[]={SR_CULTURE,SR_EVENTS,-1,SR_SCIENCE};
    for(int step=0;step<4;++step) {
        click(600,435);int event_count=0;
        while(sr_menu_screen()==SR_TURN_SCREEN && ticks<2000) {
            if(sr_menu_turn_value(0)==SR_TURN_EVENT) {
                CHECK(sr_menu_turn_value(1)==person_kinds[step] && sr_menu_turn_value(2)==1);
                CHECK(event_bitmap(argv[1]));click(441,430);++event_count;
            } else {
                CHECK(sr_menu_turn_value(0)==SR_TURN_YEAR);sr_menu_tick(++ticks);
            }
        }
        CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(1)==step+2 && event_count==(step!=2));
        if(step==0) {
            click(479,317);click(495,280);
            CHECK(sr_menu_screen()==SR_CULTURE_SCIENCE && sr_menu_person_value(SR_CULTURE,3)==1);
            CHECK(!strcmp(sr_menu_person_name(SR_CULTURE,1,0),"Johannes Magnus"));
            click(320,120);click(510,120); /* Empty science list preserves culture selection. */
            CHECK(sr_menu_person_value(SR_CULTURE,0)==1 && sr_menu_person_value(SR_CULTURE,2)==200);
            sr_menu_down(371,407);CHECK(snapshot(argv[2],"culture-hire-pressed"));
            sr_menu_cancel();sr_menu_up(371,407);CHECK(sr_game_value(3)==1050);
            uint32_t random_calls=sr_game_state()->random.calls;
            click(371,407);
            CHECK(!sr_menu_person_value(SR_CULTURE,0) && sr_menu_person_value(SR_CULTURE,4)==1 &&
                  !sr_menu_person_value(SR_CULTURE,3) && sr_game_value(3)==850);
            CHECK(sr_game_state()->random.calls==random_calls && sr_game_value(12)==1);
            click(371,436);click(404,401);
        }
        if(step==1) {
            CHECK(sr_game_value(12)==2);
            click(479,317);CHECK(snapshot(argv[2],"hq-culture-level2"));click(404,401);
        }
    }
    CHECK(sr_game_value(0)==1543 && sr_game_value(3)==1000 && sr_game_value(33)==0);
    click(479,317);click(495,280);click(510,120);
    CHECK(sr_menu_person_value(SR_SCIENCE,0)==1 && sr_menu_person_value(SR_SCIENCE,2)==200 &&
          !strcmp(sr_menu_person_name(SR_SCIENCE,0,0),"Willem Boy"));
    click(371,407);CHECK(sr_game_value(3)==800 && sr_game_value(33)==1);
    CHECK(sr_menu_person_value(SR_SCIENCE,4)==1 && !sr_menu_person_value(SR_SCIENCE,0));
    CHECK(snapshot(argv[2],"science-employed"));
    click(371,436);click(404,401);click(301,143);click(402,219);
    CHECK(!sr_menu_error() && sr_game_value(31)==2 && sr_game_value(32)==2 && sr_game_value(36)==20);
    CHECK(sr_game_value(3)==700 && sr_game_value(4)==196 && sr_game_value(5)==115 && sr_game_value(0)==1543);
    CHECK(snapshot(argv[2],"science-mine-upgraded"));click(404,401);
    /* Extend the normal seed-4 route: the newly built smithy permits an actual
       artillery purchase, then diplomacy leads to a manual battle. */
    SrGame before_artillery_route=*sr_game_runtime();
    click(349,379);click(480,194);click(480,314);
    CHECK(!sr_menu_error() && sr_game_value(9)==1000 && sr_game_value(11)==1000 && sr_game_value(3)==425);
    click(470,450);click(45,412);click(466,438);
    for(int n=0;n<4;++n) click(103,356);
    click(326,438);click(600,435);ticks=0;int artillery_route_shots=0;
    while((sr_menu_screen()==SR_TURN_SCREEN || sr_menu_screen()==SR_CROSSBOW) && ticks<10000) {
        if(sr_menu_screen()==SR_CROSSBOW) {
            if(sr_menu_crossbow_value(0)==SR_CROSSBOW_INTRO) click(320,432);
            else if(sr_menu_crossbow_value(0)==SR_CROSSBOW_AIM) {
                int wind=sr_menu_crossbow_value(4)-1;
                int x=300+320-wind_mod[wind][0]*8-sr_menu_crossbow_value(6);
                int y=300+(264-wind_mod[wind][1]*8+1730)/4-sr_menu_crossbow_value(7);
                sr_menu_down(300,300);sr_menu_move(x,y);sr_menu_up(x,y);++artillery_route_shots;
            }
            sr_menu_tick(++ticks);
        } else if(sr_menu_turn_value(0)==SR_TURN_YEAR) sr_menu_tick(++ticks);
        else if(sr_menu_turn_value(0)==SR_TURN_EVENT) {CHECK(event_bitmap(argv[1]));click(441,430);}
        else if(sr_menu_turn_value(0)==SR_TURN_MINIGAME_RESULT) {CHECK(sr_menu_turn_value(5)==10);click(420,294);}
        else CHECK(0);
    }
    CHECK(sr_game_value(0)==1548 && sr_game_state()->random.calls==206);
    CHECK(artillery_route_shots==15 && sr_menu_screen()==SR_WAR_SCREEN);click(400,400);
    if(sr_menu_war_value(0)==SR_WAR_TARGET) click(395,444);
    click(400,431);CHECK(sr_menu_screen()==SR_BATTLE_SCREEN);
    CHECK(sr_menu_battle_request(0)==1 && sr_menu_battle_request(1)==11 && sr_menu_battle_request(2)==21);
    click(65,100);CHECK(sr_menu_battle_value(5)==-1); /* Commands wait for art to load. */
    CHECK(!sr_menu_sound());
    CHECK(battle_assets(argv[1]));
    CHECK(sr_menu_sound()==2154 && !sr_menu_sound());
    CHECK(sr_menu_battle_value(2)==6);sr_menu_tick(10000);
    CHECK(snapshot(argv[2],"swedish-artillery-deploy"));
    for(int n=0;n<6;++n) {
        int x,y;CHECK(sr_battle_point(n+1,&x,&y));
        sr_menu_down(sr_menu_battle_unit(n,4),sr_menu_battle_unit(n,5)-15);sr_menu_up(x+28,y+114);
    }
    CHECK(sr_menu_battle_value(0)==9 && sr_menu_battle_value(25)==10 && sr_menu_battle_value(26)==336);
    CHECK(snapshot(argv[2],"swedish-officer-walk"));
    sr_menu_tick(10103);CHECK(sr_menu_battle_value(0)==9);
    sr_menu_tick(10104);CHECK(sr_menu_battle_value(0)==5 && sr_menu_battle_value(25)==106 && sr_menu_battle_value(26)==404);
    CHECK(snapshot(argv[2],"swedish-artillery-aim"));
    unsigned shot_rng=sr_game_state()->random.calls;
    click(600,440);click(0,0);CHECK(sr_menu_battle_value(0)==5 && sr_game_state()->random.calls==shot_rng);
    int target_x=sr_menu_battle_unit(6,4),target_y=sr_menu_battle_unit(6,5),target_hp=sr_menu_battle_unit(6,1);
    sr_menu_move(target_x,target_y);CHECK(sr_menu_battle_value(28)==sr_menu_battle_unit(6,2));
    CHECK(snapshot(argv[2],"swedish-artillery-target"));click(target_x,target_y);
    CHECK(sr_menu_battle_value(0)==8 && sr_menu_battle_value(22)==0 && sr_game_state()->random.calls==shot_rng+2);
    CHECK(sr_menu_sound2()==2172 && !sr_menu_sound2());
    CHECK(snapshot(argv[2],"swedish-artillery-shot"));
    sr_menu_tick(10127);CHECK(sr_menu_battle_unit(6,1)==target_hp);
    sr_menu_tick(10128);CHECK(sr_menu_battle_unit(6,1)<target_hp && sr_menu_battle_unit(6,6)==1);
    CHECK(sr_menu_battle_unit(6,2)==39 && target_hp==670 && sr_menu_battle_unit(6,1)==549 && sr_game_state()->random.calls==210);
    CHECK(snapshot(argv[2],"swedish-artillery-hit"));
    sr_menu_tick(10176);CHECK(sr_menu_battle_value(0)==9 && sr_menu_battle_value(24)==5);
    sr_menu_tick(10279);CHECK(sr_menu_battle_value(0)==9);
    sr_menu_tick(10280);CHECK(sr_menu_battle_value(0)==2 && sr_menu_battle_value(27)==1 && !sr_menu_battle_value(12));
    CHECK(sr_menu_battle_value(25)==-1000 && sr_menu_battle_unit(0,6)==1 && sr_menu_turn_value(7));
    CHECK(snapshot(argv[2],"swedish-artillery-complete"));
    click(40,440);
    CHECK(sr_menu_screen()==SR_WAR_SCREEN && sr_menu_war_value(0)==SR_WAR_RESULT && sr_menu_war_value(29));
    CHECK(sr_menu_war_value(22)==4 && !sr_menu_war_value(23) && !sr_menu_war_value(24));
    CHECK(sr_game_state()->areas[1].infantry==996 && sr_game_state()->areas[1].artillery==1000);
    CHECK(sr_game_state()->random.calls==211 && sr_menu_turn_value(7));
    CHECK(sr_menu_sound()==-3 && !sr_menu_sound() && !sr_menu_sound2());
    CHECK(snapshot(argv[2],"manual-retreat-result"));
    SrGame retreat_result=*sr_game_state();sr_menu_tick(10281);
    CHECK(!memcmp(&retreat_result,sr_game_state(),sizeof retreat_result));
    click(400,394);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1548 && sr_game_value(1)==6 && !sr_menu_turn_value(7));
    CHECK(sr_game_value(3)==574 && sr_game_value(4)==219 && sr_game_value(5)==135 && sr_game_state()->random.calls==226);
    CHECK(sr_game_state()->areas[1].infantry==996 && sr_game_state()->areas[1].artillery==1000);
    CHECK(snapshot(argv[2],"manual-retreat-settled"));
    /* Restore only for the following explicitly synthetic person-error fixture. */
    CHECK(sr_menu_load((uint32_t)size));click(520,25);click(300,440);click(390,395);
    *sr_game_runtime()=before_artillery_route;
    /* Native UI fixture: mutually exclusive selections and rank-five errors. */
    SrGame *people=sr_game_runtime();people->silver=1000;
    people->people[SR_CULTURE].available=(SrRecordList){1,{10}};
    people->people[SR_SCIENCE].available=(SrRecordList){1,{20}};
    click(479,317);click(495,280);click(320,120);
    CHECK(sr_menu_person_value(SR_CULTURE,0)==10 && !sr_menu_person_value(SR_SCIENCE,0));
    click(510,120);
    CHECK(sr_menu_person_value(SR_SCIENCE,0)==20 && !sr_menu_person_value(SR_CULTURE,0));
    CHECK(!strcmp(sr_menu_field_text(183),"Carl von Linn\xe9") && !strcmp(sr_menu_field_text(184),"Rang: 5"));
    CHECK(!strcmp(sr_menu_field_text(185),"Pris: 800") && sr_menu_field_metric(183,10)==1);
    click(371,407);CHECK(sr_menu_error() && sr_menu_person_value(SR_SCIENCE,5)==SR_HIRE_UNIVERSITY);
    click(320,120);CHECK(sr_menu_person_value(SR_SCIENCE,0)==20 && !sr_menu_person_value(SR_CULTURE,0));
    click(420,294);click(320,120);click(371,407);
    CHECK(sr_menu_error() && sr_menu_person_value(SR_CULTURE,5)==SR_HIRE_ARCH && sr_game_value(3)==1000);
    click(420,294);click(371,436);
    CHECK(!sr_menu_person_value(SR_CULTURE,0) && !sr_menu_person_value(SR_SCIENCE,0));
    /* A historical state fixture verifies the 1560 accession image/king change.
       The normal browser test above starts from an unmodified new game. */
    CHECK(sr_menu_load((uint32_t)size)); sr_game_seed(1);
    click(520,25);click(300,440);click(390,395);
    sr_game_runtime()->year=1558; sr_game_runtime()->turn=8;
    click(600,435);ticks=0;
    while(sr_menu_turn_value(0)==SR_TURN_YEAR && ticks<300) sr_menu_tick(++ticks);
    CHECK(sr_menu_turn_value(0)==SR_TURN_EVENT && sr_menu_event_request()==194 && sr_game_state()->king==2);
    CHECK(event_bitmap(argv[1]) && snapshot(argv[2],"turn-new-king"));
    /* Guaranteed riot fixture: original choices and modal rejection stay live. */
    CHECK(sr_menu_load((uint32_t)size)); sr_game_seed(1);
    click(520,25);click(300,440);click(390,395);
    SrGame *riot=sr_game_runtime();
    riot->areas[1].tax_level=5;riot->areas[1].riot=70;
    riot->used_events=(SrRecordList){2,{1,2}};
    click(600,435);ticks=0;
    while(sr_menu_turn_value(0)==SR_TURN_YEAR && ticks<300) sr_menu_tick(++ticks);
    CHECK(sr_menu_turn_value(0)==SR_TURN_RIOT && snapshot(argv[2],"turn-riot"));
    click(403,343);CHECK(sr_menu_turn_value(6)==SR_RIOT_NO_TROOPS);
    CHECK(snapshot(argv[2],"turn-riot-error"));
    click(403,401);CHECK(riot->areas[1].tax_level==5 && sr_menu_turn_value(6)==SR_RIOT_NO_TROOPS);
    click(420,294);CHECK(!sr_menu_turn_value(6));
    click(403,401);CHECK(riot->areas[1].tax_level==1 && riot->areas[1].riot==50);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(1)==2);
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(1);
    click(520,25);click(300,440);click(390,395);
    SrGame *starving=sr_game_runtime();starving->crops=0;starving->areas[1].infantry=25000;
    starving->areas[1].military_level=5;
    click(600,435);ticks=0;
    while(sr_menu_screen()==SR_TURN_SCREEN && sr_menu_turn_value(0)==SR_TURN_YEAR && ticks<300)
        sr_menu_tick(++ticks);
    CHECK(sr_menu_screen()==SR_DISMISS && sr_menu_dismiss_value(7) && sr_menu_dismiss_value(8));
    CHECK(sr_menu_dismiss_value(10)==1000 && sr_game_value(3)==1050 && sr_game_value(4)==24);
    CHECK(snapshot(argv[2],"starving-warning"));
    click(452,251);CHECK(!sr_menu_dismiss_value(4)); /* Warning blocks the arrows. */
    click(420,294);CHECK(!sr_menu_dismiss_value(7));
    CHECK(snapshot(argv[2],"starving-dismiss"));
    click(403,441);CHECK(sr_menu_dismiss_value(7) && sr_game_value(3)==1050 && sr_game_value(4)==24);
    click(420,294);click(452,251);CHECK(sr_menu_dismiss_value(4)==1000 && sr_game_value(9)==25000);
    click(403,441);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(1)==2 && sr_game_value(9)==24000);
    CHECK(sr_game_value(3)==1026 && sr_game_value(4)==0); /* Income once, upkeep once. */
    /* LINNE: exercise original card positions and real pointer dispatch. */
    CHECK(sr_menu_load((uint32_t)size) && sr_menu_linne_preview());
    CHECK(snapshot(argv[2],"linne-intro"));
    sr_menu_down(320,432);sr_menu_cancel();sr_menu_up(320,432);
    CHECK(sr_menu_linne_value(0)==SR_LINNE_INTRO);
    sr_menu_tick(0);click(320,432);
    CHECK(sr_menu_linne_value(0)==SR_LINNE_WATCH && sr_menu_sound2()==1030 && !sr_menu_sound2());
    CHECK(snapshot(argv[2],"linne-watch"));
    click(108,87);CHECK(!sr_menu_linne_value(1));
    sr_menu_tick(299);CHECK(sr_menu_linne_value(0)==SR_LINNE_WATCH);
    sr_menu_tick(300);CHECK(sr_menu_linne_value(0)==SR_LINNE_PLAY);
    CHECK(snapshot(argv[2],"linne-play"));
    sr_menu_down(108,87);sr_menu_up(0,0);CHECK(!sr_menu_linne_value(1));
    click(108,87);CHECK(sr_menu_linne_value(1)==1 && sr_menu_sound()==1029 && !sr_menu_sound());
    CHECK(snapshot(argv[2],"linne-selected"));
    click(391,433);CHECK(sr_menu_linne_value(4)==1 && !sr_menu_linne_value(3) && !sr_menu_linne_value(1));
    CHECK(snapshot(argv[2],"linne-mismatch"));
    const int flowers[6][2]={{108,87},{218,215},{113,206},{102,330},{211,345},{208,89}};
    const int names[6][2]={{113,417},{391,433},{206,435},{484,431},{297,435},{575,426}};
    for(int pair=0;pair<6;++pair) {
        click(names[pair][0],names[pair][1]);CHECK(sr_menu_linne_value(2)==pair+1);
        click(flowers[pair][0],flowers[pair][1]);
        CHECK(sr_menu_linne_value(3)==pair+1 && sr_menu_linne_value(4)==pair+2 && !sr_menu_linne_value(2));
        if(!pair) CHECK(snapshot(argv[2],"linne-first-pair"));
        click(flowers[pair][0],flowers[pair][1]);CHECK(!sr_menu_linne_value(1));
    }
    CHECK(sr_menu_linne_value(5)==63 && sr_menu_linne_value(0)==SR_LINNE_PLAY);
    sr_menu_tick(301);CHECK(sr_menu_linne_value(0)==SR_LINNE_RESULT && sr_menu_linne_value(6));
    CHECK(sr_menu_sound()==1031 && sr_menu_sound2()==-2);
    CHECK(snapshot(argv[2],"linne-success"));
    sr_menu_down(320,286);sr_menu_up(0,0);CHECK(sr_menu_linne_value(0)==SR_LINNE_RESULT);
    click(320,286);CHECK(sr_menu_linne_value(0)==SR_LINNE_DONE && sr_menu_linne_value(7)==8);
    CHECK(snapshot(argv[2],"linne-done"));
    CHECK(sr_menu_linne_preview());click(320,432);sr_menu_tick(601);sr_menu_tick(2402);
    CHECK(sr_menu_linne_value(0)==SR_LINNE_RESULT && !sr_menu_linne_value(6) && sr_menu_sound()==1032);
    CHECK(snapshot(argv[2],"linne-timeout"));
    click(320,286);CHECK(sr_menu_linne_value(7)==0);
    /* Historical native fixture only: this does not claim a playable route from
       1523 to 1737. Seed 5's real event draw selects scientist 20 in period 43. */
    for(int timeout=0;timeout<2;++timeout) {
        CHECK(sr_menu_load((uint32_t)size));sr_game_seed(5);
        click(520,25);click(300,440);click(390,395);
        SrGame *historical=sr_game_runtime();historical->year=1733;historical->turn=43;
        click(600,435);ticks=0;
        while(ticks<1000) {
            if(sr_menu_turn_value(0)==SR_TURN_YEAR) sr_menu_tick(++ticks);
            else if(sr_menu_turn_value(0)==SR_TURN_EVENT) {
                CHECK(event_bitmap(argv[1]));
                int target=sr_menu_turn_value(1)==SR_SCIENCE && sr_menu_turn_value(2)==20;
                if(target) CHECK(snapshot(argv[2],"turn-linne-event"));
                click(441,430);
                if(target) break;
            } else CHECK(0);
        }
        CHECK(sr_menu_screen()==SR_LINNE_SCREEN && sr_menu_turn_value(10) && historical->year==1737);
        CHECK(sr_list_has(&historical->people[SR_SCIENCE].available,20) &&
              !sr_list_has(&historical->people[SR_SCIENCE].owned,20));
        CHECK(!sr_menu_linne_preview());
        double original_silver=historical->silver;int original_points=historical->point_mod;
        uint32_t original_random=historical->random.calls;
        click(320,432);sr_menu_tick(ticks+=300);
        for(int pair=0;pair<6;++pair) {
            click(flowers[timeout?0:pair][0],flowers[timeout?0:pair][1]);
            click(names[timeout?1:pair][0],names[timeout?1:pair][1]);
        }
        sr_menu_tick(ticks+=timeout?1801:1);
        CHECK(sr_menu_linne_value(0)==SR_LINNE_RESULT && sr_menu_linne_value(6)==!timeout);
        click(320,286);
        CHECK(sr_menu_screen()==SR_TURN_SCREEN && sr_menu_turn_value(0)==SR_TURN_MINIGAME_RESULT &&
              sr_menu_turn_value(5)==10 && !sr_menu_turn_value(10));
        CHECK(historical->silver==original_silver+1000 && historical->point_mod==original_points+5);
        CHECK(sr_list_has(&historical->people[SR_SCIENCE].owned,20) &&
              !sr_list_has(&historical->people[SR_SCIENCE].available,20));
        CHECK(historical->random.calls==original_random);
        CHECK(snapshot(argv[2],"turn-linne-reward"));
        sr_menu_tick(++ticks);CHECK(historical->silver==original_silver+1000);
        click(420,294);CHECK(sr_menu_turn_value(0)==SR_TURN_YEAR && historical->silver==original_silver+1000);
    }
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(1);
    click(520,25);click(300,440);click(390,395);
    const int foreign_map[4][2]={{45,412},{116,438},{164,433},{182,321}};
    for(int country=0;country<4;++country) {
        sr_menu_down(foreign_map[country][0],foreign_map[country][1]);sr_menu_up(0,0);
        CHECK(sr_menu_screen()==SR_STRATEGY);
        click(foreign_map[country][0],foreign_map[country][1]);CHECK(sr_menu_screen()==SR_NEIGHBORS);
        CHECK(!strcmp(sr_menu_field_text(319),"2000") && !strcmp(sr_menu_field_text(328),"3750"));
        if(!country) CHECK(snapshot(argv[2],"neighbors"));
        click(323,438);CHECK(sr_menu_screen()==SR_STRATEGY);
    }
    click(45,412);sr_menu_down(466,438);CHECK(snapshot(argv[2],"neighbors-diplomacy-pressed"));
    sr_menu_up(0,0);CHECK(sr_menu_screen()==SR_NEIGHBORS);
    click(466,438);CHECK(sr_menu_screen()==SR_DIPLOMACY);
    CHECK(snapshot(argv[2],"diplomacy"));
    uint32_t before_diplomacy=sr_game_state()->random.calls;
    sr_menu_down(103,204);CHECK(snapshot(argv[2],"diplomacy-upgrade-pressed"));sr_menu_cancel();sr_menu_up(103,204);
    CHECK(sr_menu_diplomacy_value(1,0)==1 && sr_game_value(3)==1000);
    click(103,204);CHECK(sr_menu_diplomacy_value(1,0)==2 && sr_game_value(3)==900);
    CHECK(snapshot(argv[2],"diplomacy-upgraded"));
    click(103,204);CHECK(sr_menu_error() && sr_menu_diplomacy_value(1,4)==SR_DIPLOMACY_REQUIREMENTS);
    CHECK(snapshot(argv[2],"diplomacy-error"));
    click(248,204);click(326,438);CHECK(sr_menu_error() && sr_menu_screen()==SR_DIPLOMACY && sr_game_value(3)==900);
    click(420,294);CHECK(!sr_menu_error());
    sr_menu_down(103,328);CHECK(snapshot(argv[2],"diplomacy-improve-pressed"));sr_menu_up(0,0);
    CHECK(sr_menu_diplomacy_value(1,2)==0);
    click(103,328);click(103,328);
    CHECK(sr_menu_diplomacy_value(1,2)==-200 && sr_game_value(3)==700 && sr_menu_diplomacy_value(1,1)==0);
    CHECK(!strcmp(sr_menu_field_text(304),"200") && !strcmp(sr_menu_field_text(305),"0"));
    CHECK(snapshot(argv[2],"diplomacy-reserved"));
    click(103,356);CHECK(sr_menu_diplomacy_value(1,2)==-100 && sr_game_value(3)==800);
    click(103,328);CHECK(sr_game_value(3)==700 && sr_game_state()->random.calls==before_diplomacy);
    /* Four columns are ordered 1,4,3,2, but arrow channels map by country ID. */
    const int foreign_x[]={103,536,392,248};
    for(int country=2;country<=4;++country) {
        click(foreign_x[country-1],356);CHECK(sr_menu_diplomacy_value(country,2)==100 && sr_game_value(3)==600);
        click(foreign_x[country-1],328);CHECK(sr_menu_diplomacy_value(country,2)==0 && sr_game_value(3)==700);
    }
    click(177,438);CHECK(sr_menu_screen()==SR_NEIGHBORS);click(466,438);
    CHECK(sr_menu_diplomacy_value(1,2)==-200 && sr_game_value(3)==700);
    click(326,438);CHECK(sr_menu_screen()==SR_STRATEGY);click(600,435);ticks=0;
    while(sr_menu_screen()==SR_TURN_SCREEN && sr_menu_turn_value(0)==SR_TURN_YEAR && ticks<400) sr_menu_tick(++ticks);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1528 && sr_game_value(3)==750);
    CHECK(sr_menu_diplomacy_value(1,2)==0 && sr_menu_diplomacy_value(1,1)>=-2 && sr_menu_diplomacy_value(1,1)<=-1 && sr_game_value(4)==124 && sr_game_value(5)==110);
    click(45,412);CHECK(snapshot(argv[2],"neighbors-after-diplomacy"));click(466,438);
    CHECK(snapshot(argv[2],"diplomacy-after-turn"));
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(1);
    click(520,25);click(300,440);click(390,395);click(45,412);click(175,438);
    CHECK(sr_menu_screen()==SR_TRADE_SCREEN && sr_menu_trade_value(1,1)==10);
    CHECK(snapshot(argv[2],"trade"));
    CHECK(!strcmp(sr_menu_field_text(170),"1.0") && !strcmp(sr_menu_field_text(174),"3.0"));
    CHECK(!strcmp(sr_menu_field_text(159),"10") && !strcmp(sr_menu_field_text(255),"0"));
    CHECK(sr_menu_field_metric(255,0)==90 && sr_menu_field_metric(255,13)==1);
    uint32_t trade_calls=sr_game_state()->random.calls;
    const int trade_centers[]={104,536,392,248};
    for(int country=1;country<=4;++country) for(int kind=0;kind<2;++kind) {
        int cx=trade_centers[country-1],y=kind?344:268;
        click(cx-58,y);CHECK(sr_menu_trade_value(country,kind+2)==-1 && sr_game_value(3)==1000-(kind?3:1));
        click(cx+58,y);CHECK(sr_menu_trade_value(country,kind+2)==0 && sr_game_value(3)==1000);
    }
    /* Its original mouseDown handler waits for any release, unlike diplomacy. */
    sr_menu_down(103,195);CHECK(snapshot(argv[2],"trade-upgrade-pressed"));sr_menu_up(0,0);
    CHECK(sr_menu_trade_value(1,0)==2 && sr_game_value(3)==900 && sr_game_value(5)==75);
    CHECK(snapshot(argv[2],"trade-upgraded"));
    click(103,195);CHECK(sr_menu_error() && sr_menu_trade_value(1,4)==SR_TRADE_REQUIREMENTS);
    CHECK(snapshot(argv[2],"trade-error"));
    click(162,268);click(326,438);CHECK(sr_menu_error() && sr_menu_screen()==SR_TRADE_SCREEN && !sr_game_state()->trade_crops[0]);
    click(420,294);
    sr_menu_down(107,268);sr_menu_move(142,268);
    CHECK(sr_menu_trade_value(1,2)==25 && sr_menu_trade_value(1,5)==139 && sr_game_value(4)==100);
    CHECK(!strcmp(sr_menu_field_text(132),"75") && !strcmp(sr_menu_field_text(255),"25"));
    CHECK(snapshot(argv[2],"trade-drag-preview"));sr_menu_cancel();sr_menu_up(142,268);
    CHECK(!sr_game_state()->trade_crops[0] && sr_game_value(4)==100 && sr_menu_trade_value(1,5)==104);
    for(int i=0;i<10;++i) click(162,268);
    for(int i=0;i<5;++i) click(46,344);
    CHECK(sr_menu_trade_value(1,2)==10 && sr_menu_trade_value(1,3)==-5 && sr_game_value(3)==885 && sr_game_value(4)==90 && sr_game_value(5)==75);
    CHECK(snapshot(argv[2],"trade-reserved"));
    click(469,438);CHECK(sr_menu_screen()==SR_DIPLOMACY);click(469,438);
    CHECK(sr_menu_screen()==SR_TRADE_SCREEN && sr_menu_trade_value(1,2)==10 && sr_menu_trade_value(1,3)==-5);
    click(176,438);CHECK(sr_menu_screen()==SR_NEIGHBORS);click(175,438);
    CHECK(sr_menu_screen()==SR_TRADE_SCREEN && sr_game_state()->random.calls==trade_calls);
    click(326,438);click(600,435);ticks=0;
    while(sr_menu_screen()==SR_TURN_SCREEN && sr_menu_turn_value(0)==SR_TURN_YEAR && ticks<400) sr_menu_tick(++ticks);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1528 && sr_game_value(3)==945 && sr_game_value(4)==114 && sr_game_value(5)==90);
    CHECK(!sr_game_state()->trade_crops[0] && !sr_game_state()->trade_metal[0]);
    click(45,412);click(175,438);CHECK(snapshot(argv[2],"trade-after-turn"));
    CHECK(sr_game_state()->trade_prices_updated && strlen(sr_menu_field_text(170))==4);
    /* Repeat starts at tick 20, then ticks 30,40,... even outside the arrow. */
    sr_menu_tick(1000);sr_menu_down(162,268);CHECK(sr_menu_trade_value(1,2)==1);
    sr_menu_move(0,0);sr_menu_tick(1019);CHECK(sr_menu_trade_value(1,2)==1);
    sr_menu_tick(1020);CHECK(sr_menu_trade_value(1,2)==2);
    sr_menu_tick(1040);CHECK(sr_menu_trade_value(1,2)==4);
    sr_menu_up(0,0);sr_menu_tick(1050);CHECK(sr_menu_trade_value(1,2)==4);
    /* Commit a real drag, including grab offset and release outside its row. */
    int marker_x=sr_menu_trade_value(1,5);
    sr_menu_down(marker_x+3,268);sr_menu_move(107,0);sr_menu_up(107,0);
    CHECK(!sr_game_state()->trade_crops[0] && sr_game_value(4)==114);
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(3);
    click(520,25);click(60,100);click(300,440);click(390,395);
    click(45,412);click(466,438);
    for(int n=0;n<4;++n) click(103,356);
    click(326,438);click(600,435);ticks=0;
    while(sr_menu_screen()==SR_TURN_SCREEN && ticks<1000) {
        if(sr_menu_turn_value(0)==SR_TURN_YEAR) sr_menu_tick(++ticks);
        else if(sr_menu_turn_value(0)==SR_TURN_EVENT) {CHECK(event_bitmap(argv[1]));click(441,430);}
        else CHECK(0);
    }
    CHECK(sr_menu_screen()==SR_WAR_SCREEN && sr_menu_war_value(0)==SR_WAR_DECLARE && sr_menu_war_value(1)==1 && sr_menu_war_value(2)==1);
    CHECK(sr_menu_turn_value(0)==SR_TURN_WAR && sr_menu_turn_value(7));
    CHECK(snapshot(argv[2],"war-declare"));
    sr_menu_down(400,372);CHECK(snapshot(argv[2],"war-quick-pressed"));sr_menu_up(0,0);
    CHECK(sr_menu_war_value(0)==SR_WAR_DECLARE);
    click(400,372);CHECK(sr_menu_war_value(0)==SR_WAR_TARGET && sr_menu_war_value(3)==17);
    CHECK(snapshot(argv[2],"war-target"));
    uint32_t war_calls=sr_game_state()->random.calls;
    click(340,122);CHECK(sr_menu_war_value(3)==18);click(340,108);CHECK(sr_menu_war_value(3)==17);
    click(395,444);CHECK(sr_menu_war_value(0)==SR_WAR_REGIMENT && sr_menu_war_value(4)==6);
    CHECK(!strcmp(sr_menu_war_area_name(0,0),"Skåne") && !strcmp(sr_menu_war_area_name(0,1),"Nyland"));
    CHECK(snapshot(argv[2],"war-regiment"));
    sr_menu_down(400,431);sr_menu_cancel();sr_menu_up(400,431);
    CHECK(sr_menu_war_value(0)==SR_WAR_REGIMENT && sr_game_state()->random.calls==war_calls);
    click(400,431);CHECK(sr_menu_war_value(0)==SR_WAR_QUICK && sr_menu_war_value(5) && sr_game_state()->random.calls==29);
    CHECK(!sr_menu_war_value(12) && sr_game_state()->player_areas.count==1);
    CHECK(snapshot(argv[2],"war-quick"));
    click(400,394);CHECK(sr_menu_war_value(0)==SR_WAR_RESULT && sr_menu_war_value(12) && sr_menu_war_value(13)==17);
    CHECK(sr_game_state()->player_areas.count==2 && sr_game_state()->areas[17].owned && sr_game_state()->areas[17].owner=='A');
    CHECK(sr_menu_war_value(22)==520 && sr_menu_war_value(23)==780 && sr_menu_war_value(27)==500);
    CHECK(sr_game_value(9)==1480 && sr_game_value(10)==1220 && sr_game_state()->random.calls==35);
    CHECK(snapshot(argv[2],"war-victory"));
    sr_menu_tick(++ticks);CHECK(sr_game_state()->random.calls==35 && sr_menu_war_value(28)==1);
    click(400,394);
    CHECK(sr_menu_screen()==SR_STRATEGY && sr_game_value(0)==1528 && sr_game_value(1)==2);
    CHECK(sr_game_value(3)==688 && sr_game_value(4)==123 && sr_game_value(5)==111 && sr_game_state()->random.calls==57);
    CHECK(snapshot(argv[2],"war-settled"));
    /* Same normal new-game route, now choosing STRID and real deployment. */
    CHECK(sr_menu_load((uint32_t)size));sr_game_seed(3);
    click(520,25);click(60,100);click(300,440);click(390,395);
    click(45,412);click(466,438);for(int n=0;n<4;++n) click(103,356);
    click(326,438);click(600,435);ticks=0;
    while(sr_menu_screen()==SR_TURN_SCREEN && ticks<1000) {
        if(sr_menu_turn_value(0)==SR_TURN_YEAR) sr_menu_tick(++ticks);
        else if(sr_menu_turn_value(0)==SR_TURN_EVENT) {CHECK(event_bitmap(argv[1]));click(441,430);}
        else CHECK(0);
    }
    CHECK(sr_menu_screen()==SR_WAR_SCREEN);click(400,400);click(395,444);click(400,431);
    CHECK(sr_menu_screen()==SR_BATTLE_SCREEN && sr_menu_battle_value(0)==1);
    CHECK(!sr_menu_battle_request(0) && !sr_menu_battle_request(1) && !sr_menu_battle_request(2)); /* Cached from the first battle. */
    CHECK(sr_menu_battle_value(2)==6 && sr_game_state()->random.calls==24);
    CHECK(battle_assets(argv[1]) && sr_menu_battle_value(1));
    CHECK(sr_menu_sound()==2154 && !sr_menu_sound());
    CHECK(snapshot(argv[2],"battle-deploy"));
    sr_menu_down(65,100);CHECK(sr_menu_battle_value(5)==0);sr_menu_move(200,200);
    CHECK(snapshot(argv[2],"battle-drag"));sr_menu_cancel();sr_menu_up(139,345);
    CHECK(sr_menu_battle_value(3)==0 && sr_menu_battle_unit(0,4)==65 && sr_menu_battle_value(5)==-1);
    sr_menu_down(65,100);sr_menu_up(0,0);CHECK(sr_menu_battle_value(3)==0);
    sr_menu_down(65,100);sr_menu_up(139,345);CHECK(sr_menu_battle_value(3)==1 && sr_menu_battle_unit(0,2)==3);
    sr_menu_down(100,100);sr_menu_up(139,345);CHECK(sr_menu_battle_value(3)==1 && !sr_menu_battle_unit(1,3));
    const int targets[5][2]={{175,366},{212,387},{67,303},{103,324},{247,408}};
    for(int n=1;n<6;++n) {sr_menu_down(65+n*35,100);CHECK(sr_menu_battle_value(5)==n);sr_menu_up(targets[n-1][0],targets[n-1][1]);}
    CHECK(sr_menu_battle_value(0)==2 && sr_menu_battle_value(3)==6 && sr_menu_battle_value(7)==4);
    CHECK(sr_menu_battle_value(8)==7 && sr_menu_battle_unit(6,2)==53);
    CHECK(sr_game_state()->random.calls==24 && sr_game_value(9)==2000 && sr_game_value(10)==2000 && sr_menu_turn_value(7));
    CHECK(snapshot(argv[2],"battle-ready"));
    sr_menu_tick(5000);click(139,330);CHECK(sr_menu_battle_value(9)==0);
    sr_menu_move(175,324);CHECK(sr_menu_battle_value(10)==10);
    CHECK(snapshot(argv[2],"battle-selected"));click(175,324);
    CHECK(sr_menu_battle_value(0)==3 && sr_menu_battle_value(11)==0 && sr_menu_battle_unit(0,6)==0);
    CHECK(sr_menu_battle_value(12)==1 && sr_menu_battle_unit(0,2)==3);
    CHECK(sr_menu_sound2()==2179 && !sr_menu_sound2());
    CHECK(snapshot(argv[2],"battle-cavalry-walk-start"));
    sr_menu_tick(5036);CHECK(snapshot(argv[2],"battle-cavalry-walk"));
    click(212,370);CHECK(sr_menu_battle_value(9)==-1); /* Original blocking animation. */
    sr_menu_tick(5071);CHECK(sr_menu_battle_value(0)==3 && sr_menu_battle_unit(0,2)==3);
    sr_menu_tick(5072);CHECK(sr_menu_battle_value(0)==2 && sr_menu_battle_unit(0,2)==10);
    CHECK(sr_menu_battle_unit(0,4)==175 && sr_menu_battle_unit(0,5)==324);
    CHECK(snapshot(argv[2],"battle-cavalry-moved"));
    click(175,285);CHECK(sr_menu_battle_value(9)==-1); /* Exhausted group; above overlapping cavalry. */
    click(67,288);CHECK(sr_menu_battle_value(9)==3);click(103,282);
    CHECK(sr_menu_battle_value(0)==3 && sr_menu_battle_value(11)==3);
    CHECK(sr_menu_sound2()==2171 && !sr_menu_sound2());
    CHECK(snapshot(argv[2],"battle-infantry-walk-start"));
    sr_menu_tick(5116);CHECK(snapshot(argv[2],"battle-infantry-walk"));
    sr_menu_tick(5159);CHECK(sr_menu_battle_unit(3,2)==1);
    sr_menu_tick(5160);CHECK(sr_menu_battle_unit(3,2)==8 && sr_menu_battle_value(12)==2);
    CHECK(snapshot(argv[2],"battle-infantry-moved"));
    click(600,440);CHECK(sr_menu_battle_value(0)==4 && sr_menu_battle_value(12)==2);
    CHECK(sr_game_state()->random.calls==24 && sr_menu_turn_value(7));
    sr_menu_tick(5161);CHECK(sr_menu_battle_value(0)==8 && sr_menu_battle_value(20));
    CHECK(sr_game_state()->random.calls==26 && sr_menu_battle_value(16)==5);
    CHECK(sr_menu_battle_value(21)==0 && sr_menu_battle_value(22)==1 && sr_menu_battle_value(23)==1);
    CHECK(sr_menu_sound2()==2172 && !sr_menu_sound2());
    CHECK(snapshot(argv[2],"battle-enemy-artillery"));
    sr_menu_tick(5177);CHECK(snapshot(argv[2],"battle-enemy-artillery-fire"));
    sr_menu_tick(5184);CHECK(sr_menu_battle_unit(5,1)==666);
    sr_menu_tick(5185);CHECK(sr_menu_battle_unit(5,1)<666 && sr_menu_battle_unit(5,6)==1);
    CHECK(snapshot(argv[2],"battle-artillery-hit"));
    sr_menu_tick(5205);CHECK(snapshot(argv[2],"battle-artillery-recover"));
    ticks=5232;CHECK(sr_menu_battle_value(0)==8);
    do {sr_menu_tick(++ticks);} while(sr_menu_battle_value(0)!=2 && ticks<10000);
    CHECK(sr_menu_battle_value(0)==2 && sr_menu_battle_value(19)==2 && !sr_menu_battle_value(12));
    CHECK(sr_menu_battle_unit(6,2)==46 && sr_menu_battle_value(23)==1);
    CHECK(sr_menu_battle_unit(5,1)==456 && sr_game_state()->random.calls==27);
    CHECK(snapshot(argv[2],"battle-round-two"));
    for(int next=3;next<=4;++next) {
        click(600,440);int gun_fired=0;
        do {sr_menu_tick(++ticks);if(sr_menu_battle_value(0)==8) gun_fired=1;}
        while(sr_menu_battle_value(0)!=2 && ticks<10000);
        CHECK(sr_menu_battle_value(0)==2 && sr_menu_battle_value(19)==next && !gun_fired);
        CHECK(sr_menu_battle_value(23)==next-1 && sr_menu_battle_unit(5,1)==456);
        CHECK(sr_game_state()->random.calls==(unsigned)next+25);
    }
    /* Let the actual seed-3 opponent finish the war through normal turns. */
    while(sr_menu_screen()==SR_BATTLE_SCREEN && ticks<30000) {
        if(sr_menu_battle_value(0)==2) click(600,440);
        sr_menu_tick(++ticks);
    }
    CHECK(sr_menu_screen()==SR_WAR_SCREEN && sr_menu_war_value(0)==SR_WAR_RESULT && !sr_menu_war_value(5) && !sr_menu_war_value(29));
    CHECK(sr_menu_sound()==-3 && !sr_menu_sound());
    CHECK(snapshot(argv[2],"manual-defeat-result"));
    CHECK(sr_menu_war_value(22)==1280 && sr_menu_war_value(23)==335 && !sr_menu_war_value(24));
    CHECK(sr_menu_war_value(25)==491 && !sr_menu_war_value(26) && !sr_menu_war_value(27));
    CHECK(sr_menu_war_value(11)==-500 && sr_game_state()->random.calls==40);
    click(400,394);CHECK(sr_menu_screen()==SR_STRATEGY && !sr_menu_turn_value(7));
    CHECK(snapshot(argv[2],"manual-defeat-settled"));
    CHECK(sr_game_value(0)==1528 && sr_game_value(1)==2 && sr_game_value(3)==138 && sr_game_value(4)==108 && sr_game_value(5)==104);
    CHECK(sr_game_state()->random.calls==64 && sr_game_value(9)==720 && sr_game_value(10)==1665);
    SrGame after_manual_loss=*sr_game_state();
    unsigned campaign_random=sr_game_state()->random.calls;
    /* Explicit renderer fixture: same troop banks, no artillery. */
    SrBattleArmy fixture_sw={2000,2000,0,1,0},fixture_en={2000,1500,0,1,0};SrRandom fixture_rng;
    sr_random_seed(&fixture_rng,3);
    CHECK(sr_battle_scene_begin(&fixture_sw,&fixture_en,1,&fixture_rng,1));
    const int fixture_squares[6]={3,4,5,1,2,6};
    for(int n=0;n<6;++n) {
        int x,y;CHECK(sr_battle_point(fixture_squares[n],&x,&y));
        sr_battle_scene_down(65+n*35,100);sr_battle_scene_up(x+28,y+114);
    }
    CHECK(sr_battle_scene_value(0)==2);sr_battle_scene_up(600,440);
    ticks=5160;
    for(int round=2;round<=5;++round) {
        do {sr_battle_scene_tick(++ticks);} while(sr_battle_scene_value(0)!=2 && sr_battle_scene_value(0)!=7 && ticks<10000);
        CHECK(sr_battle_scene_value(0)==2 && sr_battle_scene_value(19)==round);
        CHECK(!sr_battle_scene_value(12) && sr_battle_scene_unit(3,6)==1);
        CHECK(fixture_rng.calls==(unsigned)round+1); /* Center-column side decision is drawn even when moving forward. */
        if(round<5) sr_battle_scene_up(600,440);
    }
    CHECK(sr_battle_scene_unit(11,2)==8 && sr_battle_scene_unit(11,1)==666);
    CHECK(battle_snapshot(argv[2],"battle-fixture-round-five"));
    sr_battle_scene_up(67,288);CHECK(sr_battle_scene_value(9)==3);sr_battle_scene_up(103,282);
    CHECK(sr_battle_scene_value(0)==6 && sr_battle_scene_value(15)==3 && sr_battle_scene_value(16)==11);
    CHECK(sr_battle_scene_value(12)==1 && sr_battle_scene_unit(3,6)==0 && fixture_rng.calls==7);
    CHECK(battle_snapshot(argv[2],"battle-fixture-attack"));
    sr_battle_scene_up(600,440);CHECK(!sr_battle_scene_value(17)); /* Commands remain blocked during the exchange. */
    sr_battle_scene_tick(ticks+23);CHECK(sr_battle_scene_unit(11,1)==666);
    sr_battle_scene_tick(ticks+24);CHECK(sr_battle_scene_unit(11,1)<666);
    CHECK(battle_snapshot(argv[2],"battle-fixture-hit"));
    sr_battle_scene_tick(ticks+72);CHECK(sr_battle_scene_value(15)==11 && fixture_rng.calls==8);
    CHECK(sr_battle_scene_unit(11,6)==-1 && sr_battle_scene_unit(3,1)==666);
    CHECK(battle_snapshot(argv[2],"battle-fixture-counterattack"));
    sr_battle_scene_tick(ticks+144);CHECK(sr_battle_scene_value(0)==2 && sr_battle_scene_unit(3,1)<666);
    ticks+=144;sr_battle_scene_up(600,440);int enemy_attacked=0,negative_actions=0;
    while(sr_battle_scene_value(0)!=7 && ticks<20000) {
        sr_battle_scene_tick(++ticks);
        if(sr_battle_scene_value(0)==6 && sr_battle_scene_value(17)) enemy_attacked=1;
        for(int n=0;n<6;++n) if(sr_battle_scene_unit(n,6)<0) negative_actions=1;
        if(sr_battle_scene_value(0)==2) sr_battle_scene_up(600,440);
    }
    CHECK(enemy_attacked && negative_actions && sr_battle_scene_value(0)==7);
    CHECK(sr_battle_scene_value(18)==SR_BATTLE_ENEMY);
    CHECK(sr_game_state()->random.calls==campaign_random && !memcmp(&after_manual_loss,sr_game_state(),sizeof after_manual_loss));
    /* A second explicit fixture forces a primary kill to verify that scene
       animation consumes the controller's advance request exactly once. */
    sr_random_seed(&fixture_rng,3);
    CHECK(sr_battle_scene_begin(&fixture_sw,&fixture_en,1,&fixture_rng,10));
    for(int n=0;n<6;++n) {
        int x,y;CHECK(sr_battle_point(fixture_squares[n],&x,&y));
        sr_battle_scene_down(65+n*35,100);sr_battle_scene_up(x+28,y+114);
    }
    for(int round=2;round<=5;++round) {
        sr_battle_scene_up(600,440);
        do {sr_battle_scene_tick(++ticks);} while(sr_battle_scene_value(0)!=2 && ticks<30000);
        CHECK(sr_battle_scene_value(0)==2 && sr_battle_scene_value(19)==round);
    }
    sr_battle_scene_up(67,288);CHECK(sr_battle_scene_value(9)==3);sr_battle_scene_up(103,282);
    sr_battle_scene_tick(ticks+24);CHECK(sr_battle_scene_unit(11,1)<0 && sr_battle_scene_unit(11,3));
    CHECK(battle_snapshot(argv[2],"battle-fixture-death"));
    sr_battle_scene_tick(ticks+72);CHECK(sr_battle_scene_value(0)==3 && sr_battle_scene_value(11)==3);
    CHECK(!sr_battle_scene_unit(11,3) && sr_battle_scene_unit(3,6)==-1 && sr_battle_scene_value(12)==1);
    CHECK(fixture_rng.calls==7 && sr_battle_scene_unit(3,2)==1);
    sr_battle_scene_tick(ticks+159);CHECK(sr_battle_scene_value(0)==3);
    sr_battle_scene_tick(ticks+160);CHECK(sr_battle_scene_value(0)==2 && sr_battle_scene_unit(3,2)==8);
    CHECK(sr_battle_scene_unit(3,6)==-1 && sr_battle_scene_value(12)==1);
    CHECK(battle_snapshot(argv[2],"battle-fixture-advance"));
    puts("Menu, province controls, events, minigames, diplomacy, trade, quick war and settlement tests passed.");
    return 0;
}
