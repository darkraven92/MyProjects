#include "battle_scene.h"
#include "battle_attack.h"
#include "battle_ai.h"
#include "battle_artillery.h"
#include "generated/battle_scene.h"
#include "generated/battle_animation.h"
#include "generated/battle_sounds.h"
#include <stddef.h>
#ifdef __wasm__
void *memcpy(void *,const void *,size_t);
int memcmp(const void *,const void *,size_t);
#else
#include <string.h>
#endif
#define BATTLE_CAPACITY (16u*1024u*1024u)
static uint8_t banks[3][BATTLE_CAPACITY];
static uint32_t image_count[3];
static int loaded_id[3],wanted[3],active,country,background,drag=-1,drag_x,drag_y;
static SrBattleSetup battle;
static int selected=-1,hover,walking=-1,walk_target,walk_frames,used,enemy_pending,max_actions;
static SrBattleAttack attack;
static SrRandom *battle_rng;
static double commander_bonus;
static int enemy_cursor,winner,round_number,retreated;
static int gun_reload[2][5],enemy_gun_cursor,shooting_gun,shooting_side,enemy_check;
/* Swedish artillery: 0 inactive, 2 officer approaching, 3 choosing target,
   4 shot in progress, 5 officer leaving. */
static int swed_art,swed_gun_cursor,officer_visible,officer_exit_needed,sight;
static int officer_x,officer_y,officer_tx,officer_ty;
static uint32_t officer_start;
static uint32_t clock_ticks,walk_start;
static int sounds[2],audio_started;
static void start_audio(void);
static void attack_sound(void) {if(attack.sound) {sounds[1]=attack.sound;attack.sound=0;}}
static int start_attack(int shooter,int target,uint32_t ticks) {
    int started=sr_battle_attack_begin(&attack,&battle,battle_rng,commander_bonus,shooter,target,ticks);
    if(started) attack_sound();
    return started;
}
static int start_gun(int side,int target,uint32_t ticks) {
    int started=sr_battle_attack_gun(&attack,&battle,battle_rng,side,target,ticks);
    if(started) attack_sound();
    return started;
}
static void swed_art_begin(uint32_t ticks);
static void swed_art_next(uint32_t ticks);
static void winner_check(void);
static uint32_t u32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int i32(const uint8_t *p) {return (int32_t)u32(p);}
uint8_t *sr_battle_asset_input(int bank) {return bank>=0 && bank<3?banks[bank]:0;}
uint32_t sr_battle_asset_capacity(void) {return BATTLE_CAPACITY;}
int sr_battle_asset_request(int bank) {
    return active && bank>=0 && bank<3 && loaded_id[bank]!=wanted[bank]?wanted[bank]:0;
}
int sr_battle_asset_load(int bank,int id,uint32_t size) {
    if(bank<0 || bank>2 || !active || id!=wanted[bank]) return 0;
    loaded_id[bank]=0;image_count[bank]=0;
    const uint8_t *p=banks[bank];
    if(size<16 || size>BATTLE_CAPACITY || memcmp(p,"SRB1",4) || u32(p+8)!=(uint32_t)id || u32(p+12)!=size) return 0;
    uint32_t count=u32(p+4),header=16+count*32;
    if(count>1024 || header>size) return 0;
    for(uint32_t n=0;n<count;++n) {
        const uint8_t *d=p+16+n*32;
        uint32_t w=u32(d+4),h=u32(d+8),offset=u32(d+20),bytes=u32(d+24);
        if(!u32(d) || !w || w>640 || !h || h>480 || bytes!=w*h*4 || offset<header || offset>size || bytes>size-offset ||
           i32(d+12)<-4096 || i32(d+12)>4096 || i32(d+16)<-4096 || i32(d+16)>4096) return 0;
    }
    image_count[bank]=count;loaded_id[bank]=id;start_audio();return 1;
}
static int ready(void) {return active && loaded_id[0]==wanted[0] && loaded_id[1]==wanted[1] && loaded_id[2]==wanted[2];}
static void start_audio(void) {
    if(!audio_started && ready()) {
        sounds[0]=background==1?SR_KRIG_FOREST:background==5?SR_KRIG_WINTER:SR_KRIG_FOREST2;
        audio_started=1;
    }
}
int sr_battle_scene_begin(const SrBattleArmy *swed,const SrBattleArmy *enemy,int opponent,SrRandom *rng,double commander) {
    if(!rng || commander<=0 || opponent<1 || opponent>4 || !sr_battle_setup(&battle,swed,enemy)) return 0;
    active=1;country=opponent;drag=-1;
    selected=walking=-1;hover=used=enemy_pending=0;max_actions=battle.swedes;
    attack=(SrBattleAttack){0};battle_rng=rng;commander_bonus=commander;
    enemy_cursor=winner=retreated=0;round_number=1;
    enemy_gun_cursor=enemy_check=0;shooting_gun=-1;shooting_side=0;
    swed_art=swed_gun_cursor=officer_visible=officer_exit_needed=sight=0;
    officer_x=officer_y=-1000;
    for(int side=0;side<2;++side) for(int n=0;n<5;++n) gun_reload[side][n]=battle.guns[side].reload;
    wanted[0]=1;wanted[1]=10+swed->level;wanted[2]=10+opponent*10+enemy->level;
    background=opponent<4?opponent:sr_random_next(rng,2)+3;
    if(sr_random_next(rng,10)==1) background=6;
    if(sr_random_next(rng,20)==1) background=7;
    sounds[0]=sounds[1]=audio_started=0;start_audio();
    return 1;
}
static const uint8_t *picture(int bank,int member) {
    if(loaded_id[bank]!=wanted[bank]) return 0;
    for(uint32_t n=0;n<image_count[bank];++n) {
        const uint8_t *d=banks[bank]+16+n*32;
        if(u32(d)==(uint32_t)member) return d;
    }
    return 0;
}
static int troop_member(int bank,int type) {
    for(unsigned n=0;n<sizeof sr_battle_casts/sizeof *sr_battle_casts;++n)
        if(sr_battle_casts[n][0]==wanted[bank]) return sr_battle_casts[n][type+1];
    return 0;
}
static void draw_ink(uint8_t *pixels,int bank,int member,int x,int y,int copy) {
    const uint8_t *d=picture(bank,member);if(!d) return;
    int w=(int)u32(d+4),h=(int)u32(d+8);x-=i32(d+12);y-=i32(d+16);
    const uint8_t *src=banks[bank]+u32(d+20);
    for(int yy=0;yy<h;++yy) for(int xx=0;xx<w;++xx) {
        int dx=x+xx,dy=y+yy;if(dx<0 || dx>=640 || dy<0 || dy>=480) continue;
        const uint8_t *p=src+(yy*w+xx)*4;
        if(p[3] || copy) {uint8_t *dest=pixels+(dy*640+dx)*4;memcpy(dest,p,4);dest[3]=255;}
    }
}
static void draw(uint8_t *pixels,int bank,int member,int x,int y) {draw_ink(pixels,bank,member,x,y,0);}
static int direction(int from,int to) {
    switch(from-to) {case 7:return 2;case 1:return 4;case -1:return 5;case -7:return 7;default:return 0;}
}
static int walk_action(int square) {
    if(selected<0) return 0;
    SrBattleRange r;
    if(!sr_battle_range(battle.contents,battle.units[selected].square,1,1,SR_BATTLE_SWED,&r)) return 0;
    for(int n=0;n<r.count;++n) if(r.squares[n]==square) return r.actions[n];
    return 0;
}
static const int *pose(int bank,int type,int motion,int dir) {
    for(unsigned n=0;n<sizeof sr_battle_poses/sizeof *sr_battle_poses;++n) {
        const int *p=sr_battle_poses[n];
        if(p[0]==wanted[bank] && p[1]==type && p[2]==motion && p[3]==dir) return p;
    }
    return 0;
}
static void film(uint8_t *pixels,int bank,int member,int frame,int x,int y) {
    for(unsigned n=0;n<sizeof sr_battle_loops/sizeof *sr_battle_loops;++n) {
        const int *loop=sr_battle_loops[n];
        if(loop[0]!=wanted[bank] || loop[1]!=member) continue;
        if(frame>=loop[8]) frame=(loop[6]&32)?loop[8]-1:frame%loop[8];
        const int *f=sr_battle_loop_frames[loop[7]+frame];
        /* All supplied troop loops contain one local bitmap per frame. Its
           anchor is relative to the film-loop's original rectangle. */
        draw_ink(pixels,bank,f[0],x-loop[4]/2+f[1]-loop[2],y-loop[5]/2+f[2]-loop[3],f[5]==0);
        return;
    }
}
static void unit(uint8_t *pixels,int n) {
    const SrBattleUnit *u=&battle.units[n];int bank=n<battle.swedes?1:2;
    int x=u->x+(u->placed?28:0),y=u->y+(u->placed?114:0);
    if(drag==n) {x=drag_x;y=drag_y;}
    int frame,shoot_dir,motion=sr_battle_attack_pose(&attack,n,clock_ticks,&frame,&shoot_dir);
    if(motion) {
        const int *p=pose(bank,u->troop.type,motion,shoot_dir);
        if(p) {film(pixels,bank,p[4],frame,x-p[5],y-p[6]);return;}
    }
    if(walking==n) {
        int dir=direction(u->square,walk_target),tx,ty;sr_battle_point(walk_target,&tx,&ty);
        const int *p=pose(bank,u->troop.type,1,dir);
        int step=(int)((clock_ticks-walk_start)/4)+1;if(step>walk_frames) step=walk_frames;
        double dx=(tx-u->x)*(double)step/walk_frames,dy=(ty-u->y)*(double)step/walk_frames;
        x+=(int)(dx+(dx<0?-0.5:0.5));y+=(int)(dy+(dy<0?-0.5:0.5));
        if(p) {film(pixels,bank,p[4],step-1,x-p[5],y-p[6]);return;}
    }
    draw(pixels,bank,troop_member(bank,u->troop.type),x,y);
}
static void gun(uint8_t *pixels,int side,int n,int x,int y) {
    int bank=side+1;
    if(attack.active && attack.artillery && shooting_side==side && shooting_gun==n && clock_ticks-attack.start<44) {
        const int *p=pose(bank,2,2,0);
        if(p) {film(pixels,bank,p[4],(int)((clock_ticks-attack.start)/4),x-p[5],y-p[6]);return;}
    }
    draw(pixels,bank,troop_member(bank,2),x,y);
}
static int officer_member(int walk) {
    for(unsigned n=0;n<sizeof sr_battle_officers/sizeof *sr_battle_officers;++n)
        if(sr_battle_officers[n][0]==wanted[1]) return sr_battle_officers[n][walk?2:1];
    return 0;
}
static void officer(uint8_t *pixels) {
    if(!officer_visible) return;
    if(swed_art==2 || swed_art==5) {
        int step=(int)((clock_ticks-officer_start)/4)+1;if(step>26) step=26;
        double dx=(officer_tx-officer_x)*(double)step/26,dy=(officer_ty-officer_y)*(double)step/26;
        int x=officer_x+5+(int)(dx+(dx<0?-0.5:0.5)),y=officer_y-20+(int)(dy+(dy<0?-0.5:0.5));
        film(pixels,1,officer_member(1),step-1,x,y);
    } else draw(pixels,1,officer_member(0),officer_x,officer_y);
}
void sr_battle_scene_draw(uint8_t pixels[640*480*4]) {
    if(!active) return;
    draw(pixels,0,131+background,320,240);
    const int (*sprites)[3]=battle.ready?sr_battle_ready_sprites:sr_battle_deploy_sprites;
    int count=battle.ready?sizeof sr_battle_ready_sprites/sizeof *sr_battle_ready_sprites:
        sizeof sr_battle_deploy_sprites/sizeof *sr_battle_deploy_sprites;
    for(int n=0;n<count;++n) draw(pixels,0,sprites[n][0],sprites[n][1],sprites[n][2]);
    static const int swed_guns[5][5][2]={{{136,385}},{{101,364},{136,385}},
        {{101,364},{136,385},{173,406}},{{64,342},{101,364},{136,385},{173,406}},
        {{64,342},{101,364},{136,385},{173,406},{209,427}}};
    static const int enemy_guns[5][5][2]={{{500,170}},{{462,151},{500,170}},
        {{462,151},{500,170},{540,190}},{{423,132},{462,151},{500,170},{540,190}},
        {{423,132},{462,151},{500,170},{540,190},{577,210}}};
    int guns=battle.ready?battle.guns[1].count:0;
    for(int n=0;n<guns;++n) gun(pixels,1,n,enemy_guns[guns-1][n][0],enemy_guns[guns-1][n][1]);
    if(selected>=0) {
        const SrBattleUnit *u=&battle.units[selected];SrBattleRange r;
        sr_battle_range(battle.contents,u->square,1,1,SR_BATTLE_SWED,&r);
        for(int n=0;n<r.count;++n) {
            int dir=direction(u->square,r.squares[n]);
            int normal=dir==2?83:dir==4?85:dir==5?86:84;
            int high=dir==2?69:dir==4?71:dir==5?72:70;
            draw(pixels,0,r.squares[n]==hover?high:normal,u->x+28,u->y+114);
        }
    }
    if(!battle.ready) for(int n=0;n<battle.swedes;++n) unit(pixels,n);
    else for(int s=62;s>=0;--s) for(int n=0;n<battle.swedes+battle.enemies;++n)
        if(battle.units[n].square==sr_battle_sort[s]) unit(pixels,n);
    guns=battle.guns[0].count;
    for(int n=0;n<guns;++n) gun(pixels,0,n,swed_guns[guns-1][n][0],swed_guns[guns-1][n][1]);
    officer(pixels); /* Original channel 41, above Swedish guns 36..40. */
    draw(pixels,0,104+country,601,40);
    if(swed_art==3 && sight) {int x,y;sr_battle_point(sight,&x,&y);draw(pixels,0,94,x+28,y+114);}
}
void sr_battle_scene_down(int x,int y) {
    if(!ready() || battle.ready || drag>=0) return;
    for(int n=battle.swedes-1;n>=0;--n) {
        const SrBattleUnit *u=&battle.units[n];const uint8_t *d=picture(1,troop_member(1,u->troop.type));
        if(!d) continue;
        int left=u->x+(u->placed?28:0)-i32(d+12),top=u->y+(u->placed?114:0)-i32(d+16);
        if(x>=left && x<left+(int)u32(d+4) && y>=top && y<top+(int)u32(d+8)) {
            if(!u->placed) {drag=n;drag_x=x;drag_y=y;}
            return;
        }
    }
}
void sr_battle_scene_move(int x,int y) {
    if(drag>=0) {drag_x=x;drag_y=y;}
    /* Score 31 only updates overSquare while over the map sprite. Leaving the
       map rectangle keeps the previous artillery sight; an empty map tile hides it. */
    if(swed_art!=3 || (x>=28 && x<612 && y>=114 && y<452))
        hover=sr_battle_square(x-28,y-114,584,338);
    if(swed_art==3) sight=hover && battle.contents[hover]==SR_BATTLE_ENEMY?hover:0;
}
static int hit_unit(int x,int y) {
    for(int s=0;s<63;++s) for(int n=0;n<battle.swedes+battle.enemies;++n) {
        const SrBattleUnit *u=&battle.units[n];if(u->square!=sr_battle_sort[s]) continue;
        int bank=n<battle.swedes?1:2;const uint8_t *d=picture(bank,troop_member(bank,u->troop.type));
        if(!d) continue;
        int left=u->x+28-i32(d+12),top=u->y+114-i32(d+16);
        if(x>=left && x<left+(int)u32(d+4) && y>=top && y<top+(int)u32(d+8)) return n;
    }
    return -1;
}
static void start_walk(int n,int square,uint32_t ticks) {
    walking=n;selected=-1;walk_target=square;walk_start=ticks;
    SrBattleUnit *u=&battle.units[n];walk_frames=u->troop.type==SR_BATTLE_INF?22:18;
    --u->actions;battle.contents[u->square]=SR_BATTLE_EMPTY;
    sounds[1]=u->troop.type==SR_BATTLE_INF?SR_KRIG_WALK:SR_KRIG_HORSE_WALK;
}
void sr_battle_scene_up(int x,int y) {
    if(battle.ready) {
        if(!ready() || winner || walking>=0 || attack.active || enemy_pending) return;
        /* ScoreScript 23 on the ready/aiming frames, channel 48's shape. */
        if((!swed_art || swed_art==3) && x>=3 && x<85 && y>=401 && y<480) {
            retreated=1;winner=SR_BATTLE_ENEMY;selected=-1;return;
        }
        if(swed_art) {
            if(swed_art!=3) return;
            sr_battle_scene_move(x,y);
            if(!sight) return;
            int sx,sy;sr_battle_point(sight,&sx,&sy);const uint8_t *d=picture(0,94);
            if(!d) return;
            int left=sx+28-i32(d+12),top=sy+114-i32(d+16);
            if(x<left || x>=left+(int)u32(d+4) || y<top || y>=top+(int)u32(d+8)) return;
            for(int n=battle.swedes;n<battle.swedes+battle.enemies;++n)
                if(battle.units[n].placed && battle.units[n].square==sight &&
                   start_gun(0,n,clock_ticks)) {
                    shooting_gun=swed_gun_cursor-1;shooting_side=0;gun_reload[0][shooting_gun]=1;
                    swed_art=4;sight=0;break;
                }
            return;
        }
        if(used>=max_actions) return;
        int square=sr_battle_square(x-28,y-114,584,338),action=walk_action(square);
        if(!square && selected>=0 && battle.units[selected].square>56 &&
           x>=369 && x<615 && y>=89 && y<257) {winner=SR_BATTLE_SWED;selected=-1;return;}
        if(action==SR_BATTLE_WALK) {
            start_walk(selected,square,clock_ticks);++used;
        } else if(action==SR_BATTLE_SHOOT) {
            for(int n=battle.swedes;n<battle.swedes+battle.enemies;++n)
                if(battle.units[n].placed && battle.units[n].square==square &&
                   start_attack(selected,n,clock_ticks)) {
                    selected=-1;++used;break;
                }
        }
        else if(x>=562 && x<640 && y>=410 && y<480) {enemy_pending=1;selected=-1;}
        else {
            int n=hit_unit(x,y);selected=-1;
            if(n>=0 && n<battle.swedes && battle.units[n].actions>0) selected=n;
        }
        return;
    }
    if(drag<0) return;
    sr_battle_deploy(&battle,drag,sr_battle_square(x-28,y-114,584,338));drag=-1;
    if(battle.placed==battle.swedes && sr_battle_setup_enemy(&battle)) swed_art_begin(clock_ticks);
}
void sr_battle_scene_cancel(void) {drag=-1;}
int sr_battle_scene_result(double losses[4],int *enemy_real,int *ran_away) {
    if(!active || !winner || !losses || !enemy_real || !ran_away) return 0;
    sr_battle_losses(&battle,losses);*enemy_real=sr_battle_loss_types(&battle);*ran_away=retreated;
    return winner;
}
int sr_battle_scene_sound(int channel) {
    if(channel<0 || channel>1) return 0;
    int sound=sounds[channel];sounds[channel]=0;return sound;
}
void sr_battle_scene_finish_audio(void) {sounds[0]=-3;}
static void winner_check(void) {
    double swed=0,enemy=0;
    for(int n=0;n<battle.swedes+battle.enemies;++n) if(battle.units[n].placed) {
        if(n<battle.swedes) swed+=battle.units[n].troop.troops;
        else enemy+=battle.units[n].troop.troops;
    }
    if((int)(swed+0.5)<=0) winner=SR_BATTLE_ENEMY;
    else if((int)(enemy+0.5)<=0) winner=SR_BATTLE_SWED;
}
static void player_start(void) {
    used=max_actions=0;swed_art=0;sight=0;
    for(int n=0;n<battle.swedes;++n) if(battle.units[n].placed) {
        battle.units[n].actions=1;battle.units[n].troop.attacked=1;++max_actions;
    }
}
static void officer_walk(int x,int y,uint32_t ticks,int leaving) {
    officer_tx=x;officer_ty=y;officer_start=ticks;swed_art=leaving?5:2;
}
static void swed_art_next(uint32_t ticks) {
    while(swed_gun_cursor<battle.guns[0].count) {
        int n=swed_gun_cursor++;
        if(!sr_battle_gun_ready(&gun_reload[0][n],battle.armies[0].level)) continue;
        const int *p=sr_battle_officer_positions[battle.guns[0].count-1][n];
        officer_walk(p[0],p[1],ticks,0);return;
    }
    sight=0;
    if(officer_exit_needed) {officer_exit_needed=0;officer_walk(237,516,ticks,1);}
    else player_start();
}
static void swed_art_begin(uint32_t ticks) {
    swed_gun_cursor=0;selected=-1;sight=0;
    officer_exit_needed=battle.guns[0].count && gun_reload[0][0]==5-battle.armies[0].level;
    if(officer_exit_needed) {officer_x=10;officer_y=336;officer_visible=1;}
    swed_art_next(ticks);
}
static void swed_art_tick(uint32_t ticks) {
    if((swed_art==2 || swed_art==5) && ticks-officer_start>=104) {
        officer_x=officer_tx;officer_y=officer_ty;
        if(swed_art==5) {officer_visible=0;officer_x=officer_y=-1000;player_start();}
        else {swed_art=3;sight=hover && battle.contents[hover]==SR_BATTLE_ENEMY?hover:0;}
    }
}
static void enemy_step(uint32_t ticks) {
    if(!enemy_cursor) {
        for(int n=battle.swedes;n<battle.swedes+battle.enemies;++n) if(battle.units[n].placed) {
            battle.units[n].actions=1;battle.units[n].troop.attacked=1;
        }
        enemy_cursor=1;enemy_gun_cursor=0;selected=-1;
    }
    /* Score 28 fires enemy artillery before resolveEnemyActions. */
    while(enemy_gun_cursor<battle.guns[1].count) {
        int n=enemy_gun_cursor++;
        if(!sr_battle_gun_ready(&gun_reload[1][n],battle.armies[1].level)) continue;
        int target=sr_battle_artillery_target(&battle);
        if(target>=0 && start_gun(1,target,ticks)) {
            gun_reload[1][n]=1;shooting_gun=n;shooting_side=1;return;
        }
    }
    while(enemy_cursor<=63) {
        int square=enemy_cursor++;if(battle.contents[square]!=SR_BATTLE_ENEMY) continue;
        SrBattleDecision d;
        if(!sr_battle_enemy_decide(&battle,battle_rng,square,&d)) continue;
        if(d.action==SR_BATTLE_ENEMY_WINS) {winner=SR_BATTLE_ENEMY;return;}
        int n=battle.swedes;
        while(n<battle.swedes+battle.enemies && battle.units[n].square!=square) ++n;
        if(n==battle.swedes+battle.enemies || battle.units[n].actions<=0) {
            winner_check();if(winner) return;continue;
        }
        if(d.action>=SR_BATTLE_ATTACK_BACK && d.action<=SR_BATTLE_ATTACK_SIDE) {
            for(int target=0;target<battle.swedes;++target)
                if(battle.units[target].placed && battle.units[target].square==d.target &&
                   start_attack(n,target,ticks)) {enemy_check=1;return;}
        } else if(d.action!=SR_BATTLE_WAIT) {start_walk(n,d.target,ticks);enemy_check=1;return;}
        winner_check();if(winner) return;
    }
    /* Score 30's Swedish artillery precedes Score 26's troop reset. */
    enemy_pending=enemy_cursor=0;++round_number;swed_art_begin(ticks);
}
void sr_battle_scene_tick(uint32_t ticks) {
    clock_ticks=ticks;
    if(!active || !ready() || !battle.ready || winner) return;
    if(attack.active) {
        sr_battle_attack_tick(&attack,ticks);
        attack_sound();
        if(!attack.active) {
            max_actions-=attack.swed_actions_lost;
            if(attack.advance_unit>=0) start_walk(attack.advance_unit,attack.advance_square,attack.end);
            if(swed_art==4) {
                winner_check();if(winner) return;
                swed_art_next(attack.end);
            }
        }
    }
    if(attack.active) return;
    if(swed_art) {swed_art_tick(ticks);return;}
    if(walking>=0) {
        if(ticks-walk_start<(uint32_t)walk_frames*4) return;
        SrBattleUnit *u=&battle.units[walking];u->square=walk_target;
        sr_battle_point(walk_target,&u->x,&u->y);battle.contents[walk_target]=u->troop.nation;
        walking=-1;
    }
    /* Enemy artillery has no winnerCheck between guns. The original performs
       it after the first enemy troop decision/action, including an exhausted
       unit's decision RNG. Preserve that ordering if artillery kills the last
       Swedish group. */
    if(!enemy_pending || enemy_check) {winner_check();enemy_check=0;}
    if(winner) {selected=-1;return;}
    if(used>=max_actions) enemy_pending=1;
    if(enemy_pending) enemy_step(ticks);
}
int sr_battle_scene_value(int field) {
    switch(field) {
    case 0:return !active?0:!battle.ready?1:winner?7:attack.active?(attack.artillery?8:6):swed_art?(swed_art==3?5:9):walking>=0?3:(enemy_pending || used>=max_actions)?4:2;
    case 1:return ready();case 2:return battle.swedes;
    case 3:return battle.placed;case 4:return battle.enemies;case 5:return drag;
    case 6:return background;case 7:return battle.formation.profile;case 8:return battle.formation.row;
    case 9:return selected;case 10:return hover;case 11:return walking;case 12:return used;case 13:return attack.active;
    case 14:return max_actions;case 15:return attack.active?attack.shooter:-1;case 16:return attack.active?attack.target:-1;
    case 17:return enemy_pending;case 18:return winner;case 19:return round_number;
    case 20:return enemy_pending && (enemy_gun_cursor<battle.guns[1].count || (attack.active && attack.artillery));
    case 21:return attack.active && attack.artillery?shooting_gun:-1;
    case 22:return attack.active && attack.artillery?shooting_side:-1;
    case 23:return battle.guns[1].count?gun_reload[1][0]:0;
    case 24:return swed_art;case 25:return officer_x;case 26:return officer_y;
    case 27:return battle.guns[0].count?gun_reload[0][0]:0;case 28:return sight;
    case 29:return swed_art?swed_gun_cursor-1:-1;
    default:return 0;
    }
}
int sr_battle_scene_unit(int n,int field) {
    if(!active || n<0 || n>=battle.swedes+battle.enemies) return -1;
    const SrBattleUnit *u=&battle.units[n];
    switch(field) {
    case 0:return u->troop.type;case 1:return (int)(u->troop.troops+0.5);case 2:return u->square;
    case 3:return u->placed;case 4:return u->x+(u->placed?28:0);case 5:return u->y+(u->placed?114:0);
    case 6:return u->actions;
    default:return -1;
    }
}
