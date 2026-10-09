#include "menu.h"
#include "game.h"
#include "crossbow.h"
#include "linne.h"
#include "turn.h"
#include "dismiss.h"
#include "economy.h"
#include "people.h"
#include "fields.h"
#include "diplomacy.h"
#include "trade_orders.h"
#include "war.h"
#include "battle_scene.h"
#include "generated/map_stamps.h"
#include "generated/turn_pictures.h"
#include "generated/event_images.h"
#include "generated/linne_layout.h"
#ifdef __wasm__
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
#else
#include <string.h>
#endif

#define CAPACITY (72u*1024u*1024u)
static uint8_t pack[CAPACITY], pixels[640*480*4];
static uint32_t images, regions;
static int loaded, screen, family, pressed, action, hover_area;
static int army_result, army_error, pressed_over;
static int tax_drag, tax_drag_offset, tax_drag_level, tax_drag_x;
static SrCrossbow crossbow;
static SrLinne linne;
static int linne_in_turn;
static SrRandom crossbow_random;
static SrTurn turn;
static int crossbow_in_turn,event_loaded;
static uint32_t clock_ticks,year_wait_start;
static uint8_t event_pixels[641*481*4];
static SrDismiss dismissal;
static int dismiss_warning,dismiss_in_turn,military_picture;
static int selected_people[3],hire_result;
static int diplomacy_result;
static int trade_result,trade_x[8],trade_arrow_held;
static uint32_t trade_repeat_tick;
static SrTradeDrag trade_drag;
static SrWar war;
static int battle_audio;
/* ScoreScript 271's rectangles, in original country/commodity order. */
static const int trade_rects[23][4]={
    {40,259,53,277},{40,335,53,353},{472,259,485,277},{472,335,485,353},
    {328,259,341,277},{328,335,341,353},{184,259,197,277},{184,335,197,353},
    {156,259,169,277},{156,335,169,353},{588,259,601,277},{588,335,601,353},
    {444,259,457,277},{444,335,457,353},{300,259,313,277},{300,335,313,353},
    {48,184,159,206},{481,184,592,206},{337,184,448,206},{193,184,304,206},
    {120,427,231,449},{279,427,372,449},{414,427,525,449}};
static void setup_trade(void) {
    for(int i=0;i<8;++i) trade_x[i]=sr_trade_marker(sr_game_state(),i/2+1,i%2,1);
}
static void enter_trade(void) {
    screen=SR_TRADE_SCREEN;trade_result=SR_TRADE_OK;trade_arrow_held=0;trade_drag.active=0;setup_trade();
}
static void trade_arrow_click(int button) {
    int bar=(button-1)%8;
    trade_result=sr_trade_arrow(sr_game_runtime(),bar/2+1,bar%2,button<=8?-1:1);
    if(trade_result==SR_TRADE_OK) trade_x[bar]=sr_trade_marker(sr_game_state(),bar/2+1,bar%2,0);
}
static void present_turn(void);
static int field_frame(void) {
    switch(screen) {
    case SR_STRATEGY:case SR_TURN_SCREEN:case SR_WAR_SCREEN:return 12;
    case SR_CITY:return 48;case SR_HQ_SCREEN:return 53;case SR_CULTURE_SCIENCE:return 69;
    case SR_FARM:return 78;case SR_MINE:return 83;case SR_MILITARY:return 88;
    case SR_COMMANDER_SCREEN:return 98;case SR_DISMISS:return 103;
    case SR_NEIGHBORS:return 112;case SR_DIPLOMACY:return 118;
    case SR_TRADE_SCREEN:return 132;
    default:return 0;
    }
}
const char *sr_menu_field_text(int member) {
    static char text[4096];
    if(!loaded || !sr_field_layout(field_frame(),member)) return "";
    SrFieldContext context={{selected_people[0],selected_people[1],selected_people[2]},
        screen==SR_COMMANDER_SCREEN,hover_area,&dismissal,trade_drag.active?&trade_drag:0};
    return sr_field_text(sr_game_state(),&context,member,text,sizeof text)>=0?text:"";
}
const char *sr_menu_field_font(int member) {
    const SrFieldLayout *p=loaded?sr_field_layout(field_frame(),member):0;
    return p?p->font_name:"";
}
int sr_menu_field_metric(int member,int field) {
    const SrFieldLayout *p=loaded?sr_field_layout(field_frame(),member):0;
    if(!p) return -1;
    if(screen==SR_TRADE_SCREEN && field==0 && member>=255 && member<=262) {
        static const int bars[]={0,1,6,7,4,5,2,3};int bar=bars[member-255];
        return (trade_drag.active && bar==(trade_drag.country-1)*2+trade_drag.commodity?trade_drag.x:trade_x[bar])-14;
    }
    switch(field) {
    case 0:return p->x;case 1:return p->y;case 2:return p->width;case 3:return p->height;
    case 4:return p->font_id;case 5:return p->size;case 6:return p->line_height;case 7:return p->ascent;
    case 8:return (int)p->color;case 9:return p->alignment;case 10:return sr_field_encoding(member);
    case 11:return p->channel;case 12:return p->ink;
    case 13:return p->face;
    default:return -1;
    }
}
static int same_text(const char *a,const char *b) {
    while(*a && *a==*b) { ++a; ++b; }
    return *a==*b;
}
int sr_menu_person_value(int kind,int field) {
    if(kind<SR_CULTURE || kind>SR_COMMANDERS) return 0;
    const SrPeople *p=&sr_game_state()->people[kind];
    const SrCatalogEntry *selected=sr_catalog_entry(kind,selected_people[kind]);
    switch(field) {
    case 0:return selected_people[kind];
    case 1:return selected?selected->level:0;
    case 2:return sr_people_price(kind,selected_people[kind]);
    case 3:return p->available.count;
    case 4:return p->owned.count;
    case 5:return hire_result;
    default:return 0;
    }
}
const char *sr_menu_person_name(int kind,int position,int owned) {
    if(kind<SR_CULTURE || kind>SR_COMMANDERS) return "";
    const SrPeople *p=&sr_game_state()->people[kind];
    const SrRecordList *list=owned?&p->owned:&p->available;
    int record=position==0?selected_people[kind]:
        position>0 && position<=list->count?list->records[position-1]:0;
    const SrCatalogEntry *entry=sr_catalog_entry(kind,record);
    return entry?entry->name_bytes:"";
}
int sr_menu_commander_value(int field) { return sr_menu_person_value(SR_COMMANDERS,field); }
const char *sr_menu_commander_name(int position,int owned) {
    return sr_menu_person_name(SR_COMMANDERS,position,owned);
}
static void select_person(int kind,int line) {
    const SrRecordList *list=&sr_game_state()->people[kind].available;
    if(!list->count) return;
    if(line<0) line=0;
    if(line>=list->count) line=list->count-1;
    selected_people[kind]=list->records[line];
    if(kind==SR_CULTURE || kind==SR_SCIENCE) selected_people[1-kind]=0;
}
static const int tax_positions[5]={346,382,417,453,492};
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int i32(const uint8_t *p) { return (int32_t)u32(p); }
uint8_t *sr_menu_input(void) { return pack; }
uint32_t sr_menu_capacity(void) { return CAPACITY; }
int sr_menu_load(uint32_t size) {
    loaded=0;
    if(size<16 || size>CAPACITY || memcmp(pack,"SRM1",4) || u32(pack+12)!=size) return 0;
    images=u32(pack+4); regions=u32(pack+8);
    /* Each frame has at most 48 channels; regions span multiple screens. */
    if(images>1024 || regions>1024) return 0;
    uint32_t header=16+images*36+regions*32;
    if(header>size) return 0;
    for(uint32_t i=0;i<images;++i) {
        const uint8_t *d=pack+16+i*36;
        uint32_t w=u32(d+8),h=u32(d+12),o=u32(d+24),n=u32(d+28);
        if(u32(d)>4 || !w || w>640 || !h || h>480 || n!=w*h*4 || o<header || o>size || n>size-o) return 0;
        if(i32(d+16)<-4096 || i32(d+16)>4096 || i32(d+20)<-4096 || i32(d+20)>4096) return 0;
    }
    for(uint32_t i=0;i<regions;++i) {
        const uint8_t *r=pack+16+images*36+i*32;
        if((u32(r)>1 && u32(r)!=SR_STRATEGY && (u32(r)<SR_CITY || u32(r)>SR_MINE) && (u32(r)<SR_DISMISS || u32(r)>SR_CULTURE_SCIENCE) &&
            (u32(r)<SR_NEIGHBORS || u32(r)>SR_DIPLOMACY) && (u32(r)<100 || u32(r)>107)) || u32(r+4)>48 || i32(r+16)<0 || i32(r+20)<0 ||
           i32(r+24)<=0 || i32(r+28)<=0 || (int64_t)i32(r+16)+i32(r+24)>640 ||
           (int64_t)i32(r+20)+i32(r+28)>480) return 0;
    }
    screen=SR_MENU; family=3; pressed=0; action=0; hover_area=0; loaded=1;
    tax_drag=0;
    army_result=army_error=pressed_over=0;
    turn=(SrTurn){0}; crossbow_in_turn=event_loaded=0;
    sr_linne_init(&linne);linne_in_turn=0;
    clock_ticks=year_wait_start=0;
    dismissal=(SrDismiss){0};dismiss_warning=dismiss_in_turn=0;military_picture=1;
    memset(selected_people,0,sizeof selected_people);hire_result=SR_HIRE_OK;
    diplomacy_result=SR_DIPLOMACY_OK;
    trade_result=SR_TRADE_OK;trade_arrow_held=0;trade_drag=(SrTradeDrag){0};
    war=(SrWar){0};battle_audio=0;
    return 1;
}
static const uint8_t *image(int scene, int member) {
    for(uint32_t i=0;i<images;++i) {
        const uint8_t *d=pack+16+i*36;
        if(u32(d)==(uint32_t)scene && u32(d+4)==(uint32_t)member) return d;
    }
    return NULL;
}
static void draw(int scene, int member, int x, int y, int registered) {
    const uint8_t *d=image(scene,member); if(!d) return;
    int w=(int)u32(d+8),h=(int)u32(d+12);
    if(registered) { x-=i32(d+16); y-=i32(d+20); }
    const uint8_t *src=pack+u32(d+24);
    for(int sy=0;sy<h;++sy) for(int sx=0;sx<w;++sx) {
        int dx=x+sx,dy=y+sy;
        if(dx<0 || dx>=640 || dy<0 || dy>=480) continue;
        const uint8_t *p=src+(sy*w+sx)*4;
        if(p[3]) memcpy(pixels+(dy*640+dx)*4,p,4);
    }
}
static const uint8_t *region(int channel) {
    for(uint32_t i=0;i<regions;++i) {
        const uint8_t *r=pack+16+images*36+i*32;
        if(u32(r)==(uint32_t)(screen==SR_WAR_SCREEN?100+war.phase:screen) && u32(r+4)==(uint32_t)channel) return r;
    }
    return NULL;
}
static void draw_scaled(int scene,int member,int x,int y,int width,int height) {
    const uint8_t *d=image(scene,member); if(!d) return;
    int w=(int)u32(d+8),h=(int)u32(d+12);
    x-=i32(d+16)*width/w; y-=i32(d+20)*height/h;
    const uint8_t *src=pack+u32(d+24);
    for(int dy=0;dy<height;++dy) for(int dx=0;dx<width;++dx) {
        int xx=x+dx,yy=y+dy;
        if(xx<0 || xx>=640 || yy<0 || yy>=480) continue;
        const uint8_t *p=src+((dy*h/height)*w+dx*w/width)*4;
        if(p[3]) memcpy(pixels+(yy*640+xx)*4,p,4);
    }
}
static void draw_crossbow(void) {
    const SrCrossbow *b=&crossbow;
    if(b->phase==SR_CROSSBOW_INTRO) { draw(3,100,320,240,1); return; }
    if(b->phase==SR_CROSSBOW_DONE) { draw(3,37,320,240,1); return; }
    draw(3,22,320,241,1);
    if(b->phase==SR_CROSSBOW_SHOOT || b->phase==SR_CROSSBOW_HIT_WAIT) {
        draw(3,40,320,241,1);
        for(int member=46;member>=42;--member) draw(3,member,320,264,1);
    }
    for(int i=0;i<5;++i) if(b->stuck[i].variant)
        draw(3,72+b->stuck[i].variant*2,b->stuck[i].x,b->stuck[i].y,1);
    draw(3,38,320,241,1);
    for(int i=0;i<5;++i) if(b->stuck[i].variant)
        draw(3,71+b->stuck[i].variant*2,b->stuck[i].x,b->stuck[i].y,1);
    draw(3,b->bow_member,b->bow_x,b->bow_y,1);
    draw_scaled(3,12,b->aim_y==251 && !b->dragging?617:618,b->aim_y,7,5);
    draw(3,90+b->wind_picture,437,162,1);
    draw(3,23,320,240,1);
}
static void draw_linne(void) {
    if(linne.phase==SR_LINNE_INTRO) { draw(4,13,320,240,1);draw(4,43,327,240,1);return; }
    if(linne.phase==SR_LINNE_WATCH) { draw(4,1,320,240,1);return; }
    if(linne.phase==SR_LINNE_DONE) { draw(4,33,320,240,1);return; }
    draw(4,2,320,240,1);
    if(linne.phase==SR_LINNE_RESULT) {draw(4,linne.success?35:36,320,240,1);return;}
    for(int i=0;i<6;++i) if(linne.matched&(1u<<i)) draw(4,sr_linne_complete[i],320,240,1);
    for(int i=0;i<6;++i) if(!(linne.matched&(1u<<i))) draw(4,sr_linne_flowers[i],320,240,1);
    for(int i=0;i<6;++i) if(!(linne.matched&(1u<<i))) draw(4,sr_linne_names[i],320,240,1);
    if(linne.flower) draw(4,sr_linne_flower_high[linne.flower-1],320,240,1);
    if(linne.name) draw(4,sr_linne_name_high[linne.name-1],320,240,1);
}
static void draw_foreign(void) {
    const SrGame *g=sr_game_state();int dip=screen==SR_DIPLOMACY;
    draw(2,dip?295:281,320,240,1);
    /* Original display order differs from country IDs: Danmark, Ryssland,
       Polen, Kejsarstaterna. Icon pictures replace the original 48x48 members. */
    static const int countries[]={1,4,3,2},relation_members[]={0,323,325,324,326,327};
    static const int nx[]={103,246,392,538},dx[]={107,252,394,538},rx[]={107,253,394,538};
    for(int col=0;col<4;++col) {
        const SrCountry *c=&g->countries[countries[col]-1];
        if(c->diplomacy>=1 && c->diplomacy<=5) draw(2,345+c->diplomacy,dip?dx[col]:nx[col],dip?149:(col==3?216:215),1);
        draw(2,relation_members[sr_diplomacy_relation_icon(c->relation)],dip?rx[col]:(col==2?391:nx[col]),dip?270:149,1);
        if(!dip && c->trade>=1 && c->trade<=5) draw(2,335+c->trade,nx[col],283,1);
        if(!dip) {
            int level=g->year>c->troop_level3_year?3:g->year>c->troop_level2_year?2:1;
            draw(2,835+col+(level-1)*4,320,240,1);
        }
    }
    if(!army_error && pressed && pressed_over) {
        const uint8_t *r=region(pressed);int member=0;
        if(r) {
            int script=(int)u32(r+8);
            if(!dip) member=script==28?284:script==15?283:script==35?282:0;
            else member=script>=29 && script<=32?300:script==62?301:script==61?302:
                script==272?297:script==117?298:script==273?299:0;
            if(member) draw(2,member,i32(r+16),i32(r+20),0);
        }
    }
    if(army_error) {
        draw(2,17,425,247,1);
        if(pressed==104 && pressed_over) draw(2,18,377,283,0);
    }
}
static void draw_trade(void) {
    static const int countries[]={1,4,3,2},icon_x[]={107,253,394,538};
    draw(2,287,320,240,1);
    for(int col=0;col<4;++col) draw(2,335+sr_game_state()->countries[countries[col]-1].trade,icon_x[col],153,1);
    /* Pressed art is puppet channel 29; marker channels 30–37 draw above it. */
    if(!army_error && pressed>=417 && pressed<=423) {
        static const int member[]={293,293,293,293,290,291,292};
        static const int x[]={103,536,392,248,176,326,469};
        draw(2,member[pressed-417],x[pressed-417],pressed<=420?195:438,1);
    }
    for(int bar=0;bar<8;++bar) {
        int x=trade_drag.active && bar==(trade_drag.country-1)*2+trade_drag.commodity?trade_drag.x:trade_x[bar];
        draw(2,288,x,bar%2?344:268,1);
    }
    if(army_error) {
        draw(2,17,425,247,1);
        if(pressed==104 && pressed_over) draw(2,18,377,283,0);
    }
}
static int linne_hit(int member,int x,int y) {
    const uint8_t *d=image(4,member);if(!d) return 0;
    int sx=x-320+i32(d+16),sy=y-240+i32(d+20);
    int w=(int)u32(d+8),h=(int)u32(d+12);
    return sx>=0 && sy>=0 && sx<w && sy<h && pack[u32(d+24)+(sy*w+sx)*4+3];
}
static int hit_image(int member,int cx,int cy,int x,int y) {
    const uint8_t *d=image(2,member); if(!d) return 0;
    int left=cx-i32(d+16),top=cy-i32(d+20);
    return x>=left && y>=top && x<left+(int)u32(d+8) && y<top+(int)u32(d+12);
}
static int hit(int x, int y) {
    if(!loaded) return 0;
    if(screen==SR_LINNE_SCREEN) {
        if(linne.phase==SR_LINNE_INTRO) return x>=280 && x<373 && y>=421 && y<446?300:0;
        if(linne.phase==SR_LINNE_RESULT) return x>=269 && x<366 && y>=274 && y<299?320:0;
        if(linne.phase!=SR_LINNE_PLAY) return 0;
        for(int i=5;i>=0;--i) if(!(linne.matched&(1u<<i)) && linne_hit(sr_linne_names[i],x,y)) return 311+i;
        for(int i=5;i>=0;--i) if(!(linne.matched&(1u<<i)) && linne_hit(sr_linne_flowers[i],x,y)) return 301+i;
        return 0;
    }
    if(screen==SR_DISMISS && dismiss_warning)
        return x>=377 && x<468 && y>=283 && y<305?104:0;
    if(screen==SR_TURN_SCREEN) {
        if(turn.error) return x>=377 && x<468 && y>=283 && y<305?208:0;
        switch(turn.pending.type) {
        case SR_TURN_EVENT:
            return event_loaded==sr_menu_event_request() && event_loaded &&
                x>=386 && x<496 && y>=412 && y<448?201:0;
        case SR_TURN_MINIGAME_RESULT:case SR_TURN_LOST_AREA:case SR_TURN_TRADE_LOST:
            return x>=377 && x<468 && y>=283 && y<305?202:0;
        case SR_TURN_UNREST:return x>=338 && x<434 && y>=274 && y<299?203:0;
        case SR_TURN_RIOT:
            if(x>=296 && x<510 && y>=332 && y<354) return 204;
            if(x>=326 && x<480 && y>=361 && y<383) return 205;
            if(x>=296 && x<510 && y>=390 && y<412) return 206;
            return 0;
        default:return 0;
        }
    }
    if(screen==SR_CROSSBOW) return crossbow.phase==SR_CROSSBOW_INTRO &&
        x>=272 && x<367 && y>=420 && y<445 ? 107 : 0;
    if(army_error) return x>=377 && x<468 && y>=283 && y<305 ? 104 : 0;
    if(screen==SR_TRADE_SCREEN) {
        for(int bar=7;bar>=0;--bar) if(hit_image(288,trade_x[bar],bar%2?344:268,x,y)) return 430+bar;
        for(int n=0;n<23;++n) {
            const int *r=trade_rects[n];
            if(x>=r[0] && x<r[2] && y>=r[1] && y<r[3]) return 401+n;
        }
        return 0;
    }
    if(screen==SR_WELCOME) return x>=346 && x<438 && y>=384 && y<406 ? 100 : 0;
    if(screen==SR_STRATEGY && x>=573 && x<631 && y>=407 && y<466) return 200;
    if(screen==SR_STRATEGY && hit_image(64+sr_game_state()->areas[sr_game_value(2)].city_level,327,260,x,y)) return 101;
    if(screen==SR_STRATEGY && hit_image(48+sr_game_value(8),349,379,x,y)) return 103;
    if(screen==SR_STRATEGY && hit_image(32+sr_game_value(6),452,190,x,y)) return 105;
    if(screen==SR_STRATEGY && hit_image(40+sr_game_value(31),301,143,x,y)) return 106;
    if(screen==SR_STRATEGY && hit_image(56+sr_game_value(12),479,317,x,y)) return 108;
    if(screen==SR_CITY && hit_image(288,tax_positions[sr_game_value(14)-1],290,x,y)) return 102;
    for(int ch=48;ch>0;--ch) {
        const uint8_t *r=region(ch); if(!r) continue;
        if(x>=i32(r+16) && y>=i32(r+20) && x<i32(r+16)+i32(r+24) && y<i32(r+20)+i32(r+28)) return ch;
    }
    return 0;
}
static void draw_strategy(void) {
    const SrGame *g=sr_game_state();
    const SrAreaState *a=&g->areas[g->current_area];
    draw(2,1,320,240,1);
    draw(2,400+g->current_area,400,224,1);
    /* Puppet positions established by SVEA ScoreScript 67 on frame 4. */
    draw(2,32+a->farming_level,452,190,1);
    draw(2,40+a->mining_level,301,143,1);
    draw(2,48+a->military_level,349,379,1);
    draw(2,56+g->hq_level,479,317,1);
    draw(2,64+a->city_level,327,260,1);
    if(g->king>=1 && g->king<=16) draw(2,sr_king_pictures[g->king],428,46,1);
    draw(2,499,108,372,1);
    /* fixMiniMap's 28 stamp operations, including the player's G/6 color. */
    for(int i=1;i<=28;++i) {
        int color=0;
        switch(g->areas[i].owner) {
        case 'A':color=5;break;case 'B':color=1;break;case 'C':color=4;break;
        case 'D':color=2;break;case 'F':color=3;break;
        }
        if(g->areas[i].owned) color=6;
        if(color) draw(2,sr_map_stamps[i-1][color],108,372,1);
    }
    draw(2,354+g->family,44,52,1);
    if(g->turn>=1 && g->turn<=60) draw(2,sr_time_pictures[g->turn],381,453,1);
    if(screen==SR_WELCOME) {
        draw(2,365,393,253,1);
        if(pressed==100) draw(2,366,346,384,0);
    }
    if(screen==SR_CITY) {
        /* Frame 48, with picture replacements from fixAreaInfo/fixCityBanners. */
        draw(2,165,392,239,1);
        draw(2,172+a->city_level,391,75,1);
        draw(2,759+sr_game_value(16),418,168,1);
        draw(2,759+sr_game_value(17),418,229,1);
        draw(2,288,tax_drag?tax_drag_x:tax_positions[a->tax_level-1],290,1);
        if(pressed==29) draw(2,166,345,437,0);
    }
    if(screen==SR_MILITARY) {
        draw(2,193,394,241,1);
        draw(2,199+a->military_level,393,71,1);
        /* Frame 88 starts with troopNiv1; returning from dismissal explicitly
           replaces it with the current technology, per ScoreScript 123. */
        if(pressed!=31) draw(2,216+military_picture,394,241,1);
        if(!army_error && pressed_over) {
            int member=0;
            if(pressed==27) member=199;
            else if(pressed==28 || pressed==30) member=197;
            else if(pressed==29) member=196;
            else if(pressed==31) member=195;
            const uint8_t *r=region(pressed);
            if(member && r) draw(2,member,i32(r+16),i32(r+20),0);
        }
    }
    if(screen==SR_FARM || screen==SR_MINE) {
        int mining=screen==SR_MINE;
        draw(2,mining?111:90,mining?403:404,256,1);
        draw(2,mining?116+a->mining_level:95+a->farming_level,mining?402:404,156,1);
        draw(2,759+sr_game_value(mining?36:34),mining?429:430,283,1);
        draw(2,759+sr_game_value(mining?37:35),mining?429:430,353,1);
        if(!army_error && pressed_over && (pressed==25 || pressed==26)) {
            const uint8_t *r=region(pressed);
            int member=(mining?112:91)+(pressed==25);
            if(r) draw(2,member,i32(r+16),i32(r+20),0);
        }
    }
    if(screen==SR_DISMISS) {
        draw(2,382,404,247,1);
        draw(2,227+g->troop_level,404,247,1);
    }
    if(screen==SR_COMMANDER_SCREEN) {
        draw(2,231,390,245,1);
        if(!army_error && pressed_over && (pressed==35 || pressed==36)) {
            const uint8_t *r=region(pressed);
            if(r) draw(2,pressed==35?234:232,i32(r+16),i32(r+20),0);
        }
    }
    if(screen==SR_HQ_SCREEN) {
        /* Frame 53: HQLevel/HerreFam pictures are replaced by game state. */
        draw(2,133,404,256,1);
        draw(2,135+g->hq_level,403,154,1);
        draw(2,359+g->family,312,281,1);
        if(pressed==26 && pressed_over) draw(2,134,358,390,0);
    }
    if(screen==SR_CULTURE_SCIENCE) {
        draw(2,151,371,243,1);
        if(!army_error && pressed_over && (pressed==36 || pressed==37)) {
            const uint8_t *r=region(pressed);
            if(r) draw(2,pressed==36?152:153,i32(r+16),i32(r+20),0);
        }
    }
    if(army_error || (screen==SR_DISMISS && dismiss_warning)) {
        draw(2,17,425,247,1);
        if(pressed==104 && pressed_over) draw(2,18,377,283,0);
    }
}
static const SrEventImage *event_image(void) {
    SrEvent e=turn.pending.event;
    if(e.kind<0 || e.kind>=6 || e.record<1 || e.record>=34) return NULL;
    const SrEventImage *image=&sr_event_images[e.kind][e.record];
    return image->member?image:NULL;
}
static void draw_war(void) {
    draw_strategy();
    switch(war.phase) {
    case SR_WAR_DECLARE:draw(2,241,401,256,1);draw(2,241+war.country,400,184,1);break;
    case SR_WAR_OFFER:draw(2,491,401,256,1);break;
    case SR_WAR_NO_OFFER:draw(2,240,401,256,1);break;
    case SR_WAR_NO_TROOPS:draw(2,266,401,256,1);draw(2,241+war.country,400,184,1);break;
    case SR_WAR_TARGET:draw(2,262,398,253,1);break;
    case SR_WAR_REGIMENT:case SR_WAR_FULL_PENDING:draw(2,264,401,239,1);break;
    case SR_WAR_QUICK:draw(2,252,401,252,1);draw(2,253+war.country,472,151,1);break;
    case SR_WAR_RESULT:
        draw(2,258,401,252,1);draw(2,253+war.country,472,151,1);
        if(war.won) draw(2,259,401,252,1);
        break;
    }
    if(!army_error && pressed && pressed_over) {
        const uint8_t *r=region(pressed);
        if(r) {
            int script=(int)u32(r+8),member=script==100?247:script==49?248:script==84?246:
                script==74?263:script==73?265:script==77?253:script==101?260:0;
            if(member) draw(2,member,i32(r+16),i32(r+20),0);
        }
    }
    if(army_error) {
        draw(2,17,425,247,1);
        if(pressed==104 && pressed_over) draw(2,18,377,283,0);
    }
}
static void draw_turn(void) {
    draw_strategy();
    if(turn.pending.type==SR_TURN_EVENT) {
        const SrEventImage *e=event_image();
        if(e && event_loaded==e->member) {
            int x=320-e->reg_x,y=240-e->reg_y;
            for(int sy=0;sy<e->height;++sy) for(int sx=0;sx<e->width;++sx) {
                int dx=x+sx,dy=y+sy;
                if(dx>=0 && dx<640 && dy>=0 && dy<480)
                    memcpy(pixels+(dy*640+dx)*4,event_pixels+(sy*e->width+sx)*4,4);
            }
        }
    } else if(turn.pending.type==SR_TURN_RIOT) {
        draw(2,273,404,256,1);
        if(pressed>=204 && pressed<=206 && pressed_over) {
            const int x[]={296,326,296},y[]={332,361,390};
            draw(2,274+pressed-204,x[pressed-204],y[pressed-204],0);
        }
    } else if(turn.pending.type==SR_TURN_UNREST) draw(2,277,390,240,1);
    else if(turn.pending.type==SR_TURN_MINIGAME_RESULT || turn.pending.type==SR_TURN_LOST_AREA ||
            turn.pending.type==SR_TURN_TRADE_LOST) draw(2,17,425,247,1);
    if(turn.error) {
        draw(2,17,425,247,1);
        if(pressed==208 && pressed_over) draw(2,18,377,283,0);
    }
}
const uint8_t *sr_menu_render(void) {
    memset(pixels,0,sizeof pixels);
    for(unsigned i=3;i<sizeof pixels;i+=4) pixels[i]=255;
    if(!loaded) return pixels;
    if(screen==SR_MENU) {
        draw(0,11,320,240,1);
        if(pressed>=4 && pressed<=7) {
            const uint8_t *r=region(pressed);
            if(r) draw(0,pressed+8,i32(r+16),i32(r+20),0);
        }
    } else if(screen==SR_SETUP) {
        draw(1,23,320,240,1);
        draw(1,32+family,320,240,1);
        if(pressed==16) {
            const uint8_t *r=region(16);
            if(r) draw(1,4,i32(r+16),i32(r+20),0);
        }
    } else if(screen==SR_CREDITS) draw(0,20,320,240,1);
    else if(screen==SR_TEASER) draw(0,30,320,240,1);
    else if((screen>=SR_STRATEGY && screen<=SR_MINE) || (screen>=SR_DISMISS && screen<=SR_CULTURE_SCIENCE)) draw_strategy();
    else if(screen==SR_CROSSBOW) draw_crossbow();
    else if(screen==SR_LINNE_SCREEN) draw_linne();
    else if(screen==SR_NEIGHBORS || screen==SR_DIPLOMACY) draw_foreign();
    else if(screen==SR_TRADE_SCREEN) draw_trade();
    else if(screen==SR_WAR_SCREEN) draw_war();
    else if(screen==SR_BATTLE_SCREEN) sr_battle_scene_draw(pixels);
    else if(screen==SR_TURN_SCREEN) draw_turn();
    return pixels;
}
void sr_menu_down(int x,int y) {
    action=SR_NO_ACTION;
    if(screen==SR_BATTLE_SCREEN) {sr_battle_scene_down(x,y);return;}
    pressed=hit(x,y);
    pressed_over=pressed!=0;
    if(screen==SR_LINNE_SCREEN) return;
    if(screen==SR_DISMISS) { action=SR_DISMISS_ACTION; return; }
    if(screen==SR_TURN_SCREEN) { action=SR_TURN_ACTION; return; }
    if(screen==SR_CROSSBOW) { sr_crossbow_down(&crossbow,x,y); return; }
    if(army_error) { action=screen==SR_FARM || screen==SR_MINE ? SR_BUILDING_RESULT : SR_ARMY_RESULT; return; }
    if(screen==SR_TRADE_SCREEN) {
        if(pressed>=401 && pressed<=416) {
            trade_arrow_held=pressed-400;trade_arrow_click(trade_arrow_held);
            trade_repeat_tick=clock_ticks+20;
        } else if(pressed>=430 && pressed<=437) {
            int bar=pressed-430;
            sr_trade_drag_begin(sr_game_state(),&trade_drag,bar/2+1,bar%2,x,trade_x[bar]);
        }
        return;
    }
    if(screen==SR_STRATEGY && pressed!=101 && pressed!=103 && pressed!=105 && pressed!=106 && pressed!=108 && pressed!=200) action=SR_PENDING_CONTROL;
    if(screen==SR_CITY && pressed==102) {
        tax_drag=1; tax_drag_level=sr_game_value(14);
        tax_drag_x=tax_positions[tax_drag_level-1]; tax_drag_offset=x-tax_drag_x;
    }
}
static void finish_manual_battle(void) {
    double losses[4];int real,retreated;
    int winner=sr_battle_scene_result(losses,&real,&retreated);
    if(winner && sr_war_finish_manual(sr_game_runtime(),&war,winner==SR_BATTLE_SWED,retreated,losses,real)) {
        sr_battle_scene_finish_audio();
        screen=SR_WAR_SCREEN;pressed=0;army_error=0;
        if(war.phase==SR_WAR_DONE && sr_turn_war_finished(sr_game_runtime(),&turn)) {
            war.active=0;present_turn();
        }
    }
}
void sr_menu_up(int x,int y) {
    if(!loaded) return;
    if(screen==SR_BATTLE_SCREEN) {sr_battle_scene_up(x,y);finish_manual_battle();return;}
    if(screen==SR_TRADE_SCREEN && !army_error) {
        if(trade_drag.active) {
            sr_trade_drag_move(&trade_drag,x);
            trade_x[(trade_drag.country-1)*2+trade_drag.commodity]=trade_drag.x;
            sr_trade_drag_commit(sr_game_runtime(),&trade_drag);
        } else if(pressed>=417 && pressed<=420) {
            trade_result=sr_trade_upgrade(sr_game_runtime(),pressed-416);setup_trade();
            army_error=trade_result==SR_TRADE_HARBOR || trade_result==SR_TRADE_REQUIREMENTS;
        } else if(pressed==421) screen=SR_NEIGHBORS;
        else if(pressed==422) screen=SR_STRATEGY;
        else if(pressed==423) {screen=SR_DIPLOMACY;diplomacy_result=SR_DIPLOMACY_OK;}
        /* ScoreScript 271 waits for release without a release-over test. */
        pressed=trade_arrow_held=0;return;
    }
    if(screen==SR_CROSSBOW) {
        if(crossbow.phase==SR_CROSSBOW_INTRO) {
            if(pressed==107 && hit(x,y)==107) sr_crossbow_start(&crossbow,
                crossbow_in_turn?&sr_game_runtime()->random:&crossbow_random);
        } else {
            sr_crossbow_move(&crossbow,x,y);
            sr_crossbow_up(&crossbow);
        }
        pressed=0; return;
    }
    if(tax_drag) {
        sr_menu_move(x,y);
        sr_game_set_tax(tax_drag_level);
        tax_drag=0; pressed=0; return;
    }
    if(screen==SR_CREDITS) { screen=SR_TEASER; pressed=0; return; }
    if(screen==SR_TEASER) { screen=SR_MENU; pressed=0; return; }
    int ch=pressed, release=hit(x,y); pressed=0;
    if(!ch || release!=ch) return;
    if(screen==SR_LINNE_SCREEN) {
        if(ch==300) sr_linne_start(&linne,clock_ticks);
        else if(ch>=301 && ch<=306) sr_linne_choose(&linne,0,ch-300);
        else if(ch>=311 && ch<=316) sr_linne_choose(&linne,1,ch-310);
        else if(ch==320 && sr_linne_finish(&linne) && linne_in_turn) {
            if(sr_turn_minigame_finished(sr_game_runtime(),&turn,linne.result)) present_turn();
        }
        return;
    }
    if(screen==SR_DISMISS) {
        action=SR_DISMISS_ACTION;
        if(dismiss_warning) { dismiss_warning=0; return; }
        if(ch==24) sr_dismiss_select(&dismissal,(y-82)/14+1);
        else if(ch>=35 && ch<=40) sr_dismiss_change(sr_game_state(),&dismissal,(ch-35)%3,ch>=38);
        else if(ch==43 && sr_dismiss_commit(sr_game_runtime(),&dismissal)) {
            if(dismiss_in_turn) {
                if(sr_turn_respond(sr_game_runtime(),&turn,0)) present_turn();
                else { sr_dismiss_begin(sr_game_state(),&dismissal); dismiss_warning=1; }
            } else {
                military_picture=sr_game_state()->troop_level; screen=SR_MILITARY;
                army_result=SR_ARMY_OK; action=SR_ARMY_RESULT;
            }
        }
        return;
    }
    if(screen==SR_TURN_SCREEN) {
        if(ch==208) { turn.error=0; return; }
        int choice=ch>=204 && ch<=206?ch-202:0;
        if(sr_turn_respond(sr_game_runtime(),&turn,choice)) present_turn();
        return;
    }
    if(army_error) { army_error=army_result=0; hire_result=SR_HIRE_OK;war.error=0;action=SR_NO_ACTION; return; }
    if(screen==SR_WELCOME) { screen=SR_STRATEGY; return; }
    if(screen==SR_STRATEGY && ch==200) {
        if(sr_turn_begin(sr_game_runtime(),&turn)) present_turn();
        return;
    }
    if(screen==SR_STRATEGY && ch==101) { screen=SR_CITY; hover_area=0; return; }
    if(screen==SR_STRATEGY && ch==103) {
        screen=SR_MILITARY; military_picture=1; hover_area=0;
        selected_people[SR_COMMANDERS]=0;hire_result=SR_HIRE_OK; return;
    }
    if(screen==SR_STRATEGY && ch==105) { screen=SR_FARM; hover_area=0; return; }
    if(screen==SR_STRATEGY && ch==106) { screen=SR_MINE; hover_area=0; return; }
    if(screen==SR_STRATEGY && ch==108) {
        const SrGame *g=sr_game_state();
        if(g->areas[g->current_area].special==7) { screen=SR_HQ_SCREEN; hover_area=0; }
        else action=SR_PENDING_CONTROL;
        return;
    }
    const uint8_t *r=region(ch);
    if(!r) return;
    int script=(int)u32(r+8);
    if(screen==SR_WAR_SCREEN) {
        SrGame *g=sr_game_runtime();
        if(script==100) sr_war_negotiate(g,&war);
        else if(script==49 || script==84) sr_war_choose_battle(g,&war,script==84);
        else if(script==246) sr_war_accept(g,&war);
        else if(script==243) sr_war_refuse(&war);
        else if(script==105) sr_war_surrender(g,&war);
        else if(script==76) sr_war_select_target(g,&war,(y-101)/14+1);
        else if(script==74) sr_war_confirm_target(g,&war);
        else if(script==79) sr_war_select_regiment(g,&war,(y-90)/14+1);
        else if(script==73) {
            if(sr_war_fight(g,&war) && war.phase==SR_WAR_FULL_PENDING) {
                SrBattleArmy swed={war.swed[0],war.swed[1],(int)war.swed[2],g->troop_level,0};
                SrBattleArmy enemy={war.enemy[0],war.enemy[1],(int)war.enemy[2],war.enemy_level,g->countries[war.country-1].troops_real};
                if(sr_battle_scene_begin(&swed,&enemy,war.country,&g->random,war.commander_bonus)) {
                    battle_audio=1;
                    screen=SR_BATTLE_SCREEN;sr_battle_scene_tick(clock_ticks);
                }
            }
        }
        else if(script==77 || script==101) sr_war_advance_result(g,&war);
        army_error=war.error!=0;
        if(war.phase==SR_WAR_DONE && sr_turn_war_finished(g,&turn)) {war.active=0;present_turn();}
        return;
    }
    if(screen==SR_STRATEGY && script>=110 && script<=113) {
        screen=SR_NEIGHBORS;hover_area=0;action=SR_NO_ACTION;diplomacy_result=SR_DIPLOMACY_OK;return;
    }
    if(screen==SR_NEIGHBORS) {
        if(script==28) {screen=SR_DIPLOMACY;diplomacy_result=SR_DIPLOMACY_OK;}
        else if(script==35) enter_trade();
        else if(script==15) screen=SR_STRATEGY;
        else action=SR_PENDING_CONTROL;
        return;
    }
    if(screen==SR_DIPLOMACY) {
        if(script==273) {enter_trade();return;}
        if(script==117) {screen=SR_STRATEGY;return;}
        if(script==272) {screen=SR_NEIGHBORS;return;}
        if(script>=29 && script<=32) diplomacy_result=sr_diplomacy_upgrade(sr_game_runtime(),script-28);
        else if(script==61 || script==62) diplomacy_result=sr_diplomacy_adjust(sr_game_runtime(),ch-(script==61?26:22),script==61?100:-100);
        else {action=SR_PENDING_CONTROL;return;}
        army_error=diplomacy_result==SR_DIPLOMACY_EMBASSY || diplomacy_result==SR_DIPLOMACY_REQUIREMENTS || diplomacy_result==SR_DIPLOMACY_SILVER;
        return;
    }
    if(screen==SR_COMMANDER_SCREEN) {
        if(script==42) {
            select_person(SR_COMMANDERS,(y-94)/14);
        } else if(script==43) {
            hire_result=sr_people_hire(sr_game_runtime(),SR_COMMANDERS,selected_people[SR_COMMANDERS]);
            army_error=hire_result!=SR_HIRE_OK;
            if(!army_error) selected_people[SR_COMMANDERS]=0;
        } else if(script==93) {
            military_picture=sr_game_state()->troop_level; screen=SR_MILITARY;
            action=SR_ARMY_RESULT;
        }
        return;
    }
    if(screen==SR_HQ_SCREEN) {
        if(script==18) {
            selected_people[SR_CULTURE]=selected_people[SR_SCIENCE]=0;
            hire_result=SR_HIRE_OK;screen=SR_CULTURE_SCIENCE;
        } else if(script==342) screen=SR_STRATEGY;
        else action=SR_PENDING_CONTROL;
        return;
    }
    if(screen==SR_CULTURE_SCIENCE) {
        if(script==20 || script==21) select_person(script-20,(y-111)/13);
        else if(script==22) {
            int kind=selected_people[SR_CULTURE]?SR_CULTURE:SR_SCIENCE;
            hire_result=sr_people_hire(sr_game_runtime(),kind,selected_people[kind]);
            army_error=hire_result!=SR_HIRE_OK;
            if(!army_error) selected_people[SR_CULTURE]=selected_people[SR_SCIENCE]=0;
        } else if(script==17) {
            selected_people[SR_CULTURE]=selected_people[SR_SCIENCE]=0;
            screen=SR_HQ_SCREEN;
        }
        return;
    }
    if(screen==SR_FARM || screen==SR_MINE) {
        if(script==91) { screen=SR_STRATEGY; return; }
        if(script!=10 && script!=11) { action=SR_PENDING_CONTROL; return; }
        army_result=sr_game_upgrade_building(script==10?SR_FARMING:SR_MINING);
        army_error=army_result!=SR_BUILDING_OK && army_result!=SR_BUILDING_MAX_LEVEL;
        action=SR_BUILDING_RESULT;
        return;
    }
    if(screen==SR_MILITARY) {
        if(script==90) { screen=SR_COMMANDER_SCREEN; return; }
        if(script==124 && sr_dismiss_begin(sr_game_state(),&dismissal)) {
            screen=SR_DISMISS; dismiss_in_turn=dismiss_warning=0; action=SR_DISMISS_ACTION; return;
        }
        if(script==91) { screen=SR_STRATEGY; return; }
        if(script==26) army_result=sr_game_upgrade_military();
        else if(script==27) army_result=sr_game_recruit(SR_INFANTRY);
        else if(script==33) army_result=sr_game_recruit(SR_CAVALRY);
        else if(script==34) army_result=sr_game_recruit(SR_ARTILLERY);
        else { action=SR_PENDING_CONTROL; return; }
        army_error=army_result!=SR_ARMY_OK && army_result!=SR_ARMY_MAX_LEVEL;
        action=SR_ARMY_RESULT;
        return;
    }
    if(screen==SR_CITY) {
        if(script==65) sr_game_change_tax(1);
        else if(script==66) sr_game_change_tax(-1);
        else if(script==303) screen=SR_STRATEGY;
        else action=SR_PENDING_CONTROL;
        return;
    }
    if(screen==SR_MENU) {
        /* HMENY ScoreScripts 2–5 and 23, release-over check after mouseDown. */
        if(script==3) { screen=SR_SETUP; family=3; }
        else if(script==4) action=SR_OPEN_SAVE;
        else if(script==5) action=SR_PLAY_INTRO;
        else if(script==2) action=SR_QUIT;
        else if(script==23) screen=SR_CREDITS;
    } else if(screen==SR_SETUP) {
        /* SETUP ScoreScript 8: gFamily = clickOn - 5. */
        if(script==8 && ch>=6 && ch<=10) family=ch-5;
        /* SETUP 9 -> DATABASE 1 -> SVEA 68/67 -> welcome frame 6. */
        else if(script==9 && sr_game_begin(family)) { screen=SR_WELCOME; action=SR_START_GAME; }
    }
}
void sr_menu_cancel(void) { pressed=0; tax_drag=0;trade_arrow_held=0;trade_drag.active=0;sr_crossbow_cancel(&crossbow);sr_battle_scene_cancel(); }
int sr_menu_screen(void) { return screen; }
int sr_menu_family(void) { return family; }
int sr_menu_action(void) { return action; }
int sr_menu_result(void) { return army_result; }
int sr_menu_error(void) { return army_error; }
const char *sr_menu_country_name(int country) {return sr_diplomacy_country_name(country);}
int sr_menu_trade_value(int country,int field) {
    if(country<1 || country>4) return 0;
    const SrGame *g=sr_game_state();int i=country-1;
    switch(field) {
    case 0:return g->countries[i].trade;case 1:return sr_trade_capacity(g,country);
    case 2:case 3:return trade_drag.active && trade_drag.country==country && trade_drag.commodity==field-2?
        trade_drag.load:field==2?g->trade_crops[i]:g->trade_metal[i];
    case 4:return trade_result;
    case 5:case 6:return trade_drag.active && trade_drag.country==country && trade_drag.commodity==field-5?
        trade_drag.x:trade_x[i*2+field-5];
    case 7:return trade_drag.active;
    default:return 0;
    }
}
int sr_menu_war_value(int field) {
    switch(field) {
    case 0:return war.phase;case 1:return war.country;case 2:return war.attacking;
    case 3:return war.area;case 4:return war.regiment;case 5:return war.won;
    case 6:return war.enemy_level;case 7:return war.error;case 8:return war.offer_type;
    case 9:return war.offer_area;case 10:return (int)(war.offer_silver+0.5);
    case 11:return (int)(war.fee+(war.fee<0?-0.5:0.5));case 12:return war.applied;
    case 13:return war.territory;case 14:return war.targets.count;
    case 15:return sr_game_state()->player_areas.count;
    case 16:case 17:case 18:return (int)(war.swed[field-16]+0.5);
    case 19:case 20:case 21:return (int)(war.enemy[field-19]+0.5);
    case 22:case 23:case 24:return (int)(war.swed_dead[field-22]+0.5);
    case 25:case 26:case 27:return (int)(war.enemy_dead[field-25]+0.5);
    case 28:return sr_game_state()->war_wins;
    case 29:return war.retreated;
    default:return 0;
    }
}
const char *sr_menu_war_area_name(int position,int regiment) {
    const SrRecordList *list=regiment?&sr_game_state()->player_areas:&war.targets;
    int area=position==0?(regiment?war.regiment:war.area):position>0 && position<=list->count?list->records[position-1]:0;
    return area?sr_game_area_name(area):"";
}
int sr_menu_diplomacy_value(int country,int field) {
    if(country<1 || country>4) return 0;
    const SrGame *g=sr_game_state();const SrCountry *c=&g->countries[country-1];
    switch(field) {
    case 0:return c->diplomacy;case 1:return c->relation;
    case 2:return g->player_relation_mod[country-1];case 3:return sr_diplomacy_relation_icon(c->relation);
    case 4:return diplomacy_result;case 5:return c->trade;
    case 6:return (int)(c->infantry+0.5);case 7:return (int)(c->cavalry+0.5);case 8:return c->artillery;
    default:return 0;
    }
}
/* Original MovieScript 247 hit rectangles, first match wins (overlaps matter). */
void sr_menu_move(int x,int y) {
    if(screen==SR_BATTLE_SCREEN) {sr_battle_scene_move(x,y);return;}
    if(screen==SR_CROSSBOW) { sr_crossbow_move(&crossbow,x,y); return; }
    if(trade_drag.active) {sr_trade_drag_move(&trade_drag,x);return;}
    if(pressed) pressed_over=hit(x,y)==pressed;
    if(tax_drag) {
        /* ScoreScript 297's polling loop reaches the same stable level while
           the pointer is stationary. Retain its directional threshold behavior,
           grab offset, held position and release-only state update. */
        int h=x-tax_drag_offset;
        if(h<346) h=346;
        if(h>492) h=492;
        for(int n=0;n<5;++n) {
            int hi=tax_drag_level<5?tax_drag_level+1:5;
            int lo=tax_drag_level>1?tax_drag_level-1:1;
            int before=tax_drag_level;
            if(h>=tax_positions[hi-1]) { tax_drag_x=h; tax_drag_level=hi; }
            else if(h<=tax_positions[lo-1]) { tax_drag_x=h; tax_drag_level=lo; }
            if(tax_drag_level==before) break;
        }
        return;
    }
    static const int boxes[35][5]={
        {170,384,185,400,28},{70,441,96,454,27},{186,379,204,393,25},{187,349,203,372,26},
        {172,400,188,423,24},{120,271,136,281,23},{104,272,120,298,23},{89,289,105,315,23},
        {36,349,52,396,22},{50,349,63,369,22},{87,314,100,334,21},{53,319,87,349,21},
        {85,330,107,360,20},{57,357,83,389,19},{124,401,140,421,18},{79,420,107,436,17},
        {102,305,130,332,16},{113,275,149,306,16},{155,313,181,344,15},{155,283,181,314,15},
        {140,301,157,321,14},{126,320,146,344,14},{79,372,94,396,13},{176,322,196,347,12},
        {164,340,184,365,11},{110,341,130,366,10},{147,350,167,378,9},{93,361,114,378,8},
        {85,407,113,423,7},{166,362,189,378,6},{96,377,118,392,5},{100,393,121,408,4},
        {75,395,96,409,3},{110,386,131,396,2},{117,365,138,387,1}
    };
    hover_area=0;
    if(screen!=SR_STRATEGY) return;
    for(int i=0;i<35;++i) if(x>=boxes[i][0] && x<boxes[i][2] && y>=boxes[i][1] && y<boxes[i][3]) {
        hover_area=boxes[i][4]; break;
    }
}
int sr_menu_hover_area(void) { return hover_area; }
uint8_t *sr_menu_battle_input(int bank) {return sr_battle_asset_input(bank);}
uint32_t sr_menu_battle_capacity(void) {return sr_battle_asset_capacity();}
int sr_menu_battle_request(int bank) {return screen==SR_BATTLE_SCREEN?sr_battle_asset_request(bank):0;}
int sr_menu_battle_load(int bank,int id,uint32_t size) {return screen==SR_BATTLE_SCREEN?sr_battle_asset_load(bank,id,size):0;}
int sr_menu_battle_value(int field) {return screen==SR_BATTLE_SCREEN?sr_battle_scene_value(field):0;}
int sr_menu_battle_unit(int unit,int field) {return screen==SR_BATTLE_SCREEN?sr_battle_scene_unit(unit,field):-1;}
int sr_menu_crossbow_preview(uint32_t seed) {
    if(!loaded || turn.active || !image(3,100)) return 0;
    sr_menu_cancel(); hover_area=0; action=SR_NO_ACTION;
    sr_crossbow_init(&crossbow); sr_random_seed(&crossbow_random,seed);
    crossbow_in_turn=0;
    screen=SR_CROSSBOW;
    return 1;
}
void sr_menu_tick(uint32_t ticks) {
    clock_ticks=ticks;
    if(!loaded) return;
    if(screen==SR_BATTLE_SCREEN) {sr_battle_scene_tick(ticks);finish_manual_battle();}
    if(screen==SR_TRADE_SCREEN && trade_arrow_held) {
        while((int32_t)(ticks-trade_repeat_tick)>=0) {
            trade_arrow_click(trade_arrow_held);trade_repeat_tick+=10;
        }
    }
    if(screen==SR_LINNE_SCREEN) sr_linne_step(&linne,ticks);
    else if(screen==SR_CROSSBOW) {
        sr_crossbow_step(&crossbow,crossbow_in_turn?&sr_game_runtime()->random:&crossbow_random,ticks);
        if(crossbow_in_turn && crossbow.phase==SR_CROSSBOW_DONE) {
            if(sr_turn_minigame_finished(sr_game_runtime(),&turn,crossbow.result)) present_turn();
        }
    } else if(screen==SR_TURN_SCREEN && turn.pending.type==SR_TURN_YEAR && ticks-year_wait_start>=30) {
        if(sr_turn_respond(sr_game_runtime(),&turn,0)) present_turn();
    }
}
int sr_menu_crossbow_value(int field) {
    switch(field) {
    case 0:return crossbow.phase; case 1:return crossbow.round;
    case 2:return crossbow.bolts; case 3:return crossbow.score;
    case 4:return crossbow.wind; case 5:return crossbow.result;
    case 6:return crossbow.start_x; case 7:return crossbow.start_y;
    case 8:return crossbow.bow_x; case 9:return crossbow.bow_y;
    case 10:return crossbow.hit_x; case 11:return crossbow.hit_y;
    case 12:return (int)(crossbow_in_turn?sr_game_state()->random.calls:crossbow_random.calls); case 13:return crossbow.dragging;
    default:return 0;
    }
}
int sr_menu_sound(void) {
    if(battle_audio) {
        int sound=sr_battle_scene_sound(0);if(sound) return sound>0?2000+sound:sound;
    }
    if(screen==SR_LINNE_SCREEN) {
        int sound=linne.sound1;linne.sound1=0;return sound>0?1000+sound:sound;
    }
    if(screen!=SR_CROSSBOW) return 0;
    int sound=crossbow.sound; crossbow.sound=0;
    return sound;
}
int sr_menu_sound2(void) {
    if(battle_audio) {
        int sound=sr_battle_scene_sound(1);if(sound) return sound>0?2000+sound:sound;
    }
    if(screen!=SR_LINNE_SCREEN) return 0;
    int sound=linne.sound2;linne.sound2=0;return sound>0?1000+sound:sound;
}
int sr_menu_linne_preview(void) {
    if(!loaded || turn.active || !image(4,43)) return 0;
    sr_menu_cancel();hover_area=0;action=SR_NO_ACTION;
    sr_linne_init(&linne);linne_in_turn=0;screen=SR_LINNE_SCREEN;return 1;
}
int sr_menu_linne_value(int field) {
    switch(field) {
    case 0:return linne.phase;case 1:return linne.flower;case 2:return linne.name;
    case 3:return linne.score;case 4:return linne.attempts;case 5:return (int)linne.matched;
    case 6:return linne.success;case 7:return linne.result;
    case 8: {
        uint32_t elapsed=clock_ticks-linne.start;
        return linne.phase==SR_LINNE_PLAY && elapsed<1800?(int)(1800-elapsed):0;
    }
    default:return 0;
    }
}
static void present_turn(void) {
    const SrTurnRequest *r=sr_turn_advance(sr_game_runtime(),&turn);
    crossbow_in_turn=linne_in_turn=0; action=SR_TURN_ACTION; hover_area=0; pressed=0;
    if(r->type==SR_TURN_FINISHED) { screen=SR_STRATEGY; return; }
    screen=SR_TURN_SCREEN;
    if(r->type==SR_TURN_YEAR) year_wait_start=clock_ticks;
    if(r->type==SR_TURN_EVENT) event_loaded=0;
    if(r->type==SR_TURN_WAR && sr_war_begin(sr_game_state(),&war,r->country)) screen=SR_WAR_SCREEN;
    if(r->type==SR_TURN_STARVING && sr_dismiss_begin(sr_game_state(),&dismissal)) {
        screen=SR_DISMISS;dismiss_in_turn=dismiss_warning=1;action=SR_DISMISS_ACTION;
    }
    if(r->type==SR_TURN_MINIGAME && same_text(sr_event_minigame(r->event),"ARMBORST")) {
        crossbow_in_turn=1; sr_crossbow_init(&crossbow); screen=SR_CROSSBOW;
    }
    if(r->type==SR_TURN_MINIGAME && same_text(sr_event_minigame(r->event),"LINNE")) {
        linne_in_turn=1;sr_linne_init(&linne);screen=SR_LINNE_SCREEN;
    }
}
uint8_t *sr_menu_event_input(void) { return event_pixels; }
uint32_t sr_menu_event_capacity(void) { return sizeof event_pixels; }
int sr_menu_event_request(void) {
    const SrEventImage *e=event_image();
    return screen==SR_TURN_SCREEN && turn.pending.type==SR_TURN_EVENT && e?e->member:0;
}
int sr_menu_event_load(int member,uint32_t size) {
    const SrEventImage *e=event_image();
    if(!e || member<=0 || member!=sr_menu_event_request() || size!=(uint32_t)e->width*e->height*4) return 0;
    event_loaded=member; return 1;
}
int sr_menu_turn_value(int field) {
    switch(field) {
    case 0:return turn.pending.type; case 1:return turn.pending.event.kind;
    case 2:return turn.pending.event.record; case 3:return turn.pending.country;
    case 4:return turn.pending.area; case 5:return turn.pending.score;
    case 6:return turn.error; case 7:return turn.active;
    case 8:return event_loaded; case 9:return crossbow_in_turn;
    case 10:return linne_in_turn;
    default:return 0;
    }
}
const char *sr_menu_turn_name(void) { return sr_event_name(turn.pending.event); }
const char *sr_menu_turn_minigame(void) { return sr_event_minigame(turn.pending.event); }
int sr_menu_dismiss_value(int field) {
    if(field>=1 && field<=3) return sr_dismiss_remaining(sr_game_state(),&dismissal,field-1);
    if(field>=4 && field<=6 && dismissal.area>=1 && dismissal.area<=SR_AREA_COUNT)
        return dismissal.home[dismissal.area][field-4];
    switch(field) {
    case 0:return dismissal.area;
    case 7:return dismiss_warning;
    case 8:return dismiss_in_turn;
    case 9:return dismissal.areas.count;
    case 10: {
        const SrGame *g=sr_game_state();
        double shortage=sr_economy_troop_cost(g)-g->crops;
        /* Original integer() rounds this positive display value. Dismissal
           itself still moves only complete 1000-man groups. */
        return shortage>0 && g->troop_level>0?(int)(shortage/g->troop_level*1000+0.5):0;
    }
    default:return 0;
    }
}
