#include "war.h"
#include "catalog.h"
#include "events.h"
#include <float.h>
#include <limits.h>
static int integer(double n) {return (int)(n+(n<0?-0.5:0.5));}
static double divide(double n,int denominator,int real) {
    return real?n/denominator:(int)(n/denominator);
}
static char owner(int country) {return " BDFC"[country];}
static int valid(const SrGame *g,const SrWar *w) {
    return g && w && w->active && w->country>=1 && w->country<=4;
}
static int select_row(const SrRecordList *list,int position) {
    if(!list->count) return 0;
    if(position<1) position=1;
    if(position>list->count) position=list->count;
    return list->records[position-1];
}
static void defense_target(const SrGame *g,SrWar *w) {
    if(w->area || w->attacking) return;
    for(int n=0;n<15;++n) {
        int area=g->countries[w->country-1].area_priority[n];
        if(area>=1 && area<=SR_AREA_COUNT && g->areas[area].owner=='A') {w->area=area;return;}
    }
}
int sr_war_begin(const SrGame *g,SrWar *w,int country) {
    if(!g || !w || country<1 || country>4 || !g->player_areas.count) return 0;
    *w=(SrWar){.active=1,.phase=SR_WAR_NO_TROOPS,.country=country,
        .attacking=g->player_relation_mod[country-1]>0,.area=g->war_area};
    for(int n=0;n<g->player_areas.count;++n) {
        const SrAreaState *a=&g->areas[g->player_areas.records[n]];
        if(a->infantry+a->cavalry>0) {w->phase=SR_WAR_DECLARE;break;}
    }
    return 1;
}
int sr_war_negotiate(SrGame *g,SrWar *w) {
    if(!valid(g,w) || w->phase!=SR_WAR_DECLARE) return 0;
    int embassy=0;
    for(int n=0;n<g->player_areas.count;++n)
        if(g->areas[g->player_areas.records[n]].special==5) embassy=1;
    if(!embassy && g->silver<100) {w->error=SR_WAR_SILVER;return 0;}
    if(!embassy) g->silver-=100;
    w->error=0;w->offer_type=w->offer_area=0;w->offer_silver=0;
    for(int n=0;n<15;++n) {
        int a=g->countries[w->country-1].area_priority[n];
        if(sr_list_has(&g->player_areas,a)) {w->offer_area=a;break;}
    }
    double money=(g->year-1400)*2+250+sr_random_next(&g->random,3000);
    if(money>g->silver) money=g->silver;
    int money_ok=money>=1000;
    if(w->offer_area && sr_random_next(&g->random,100)<60) w->offer_type=1;
    else if(money_ok) {w->offer_type=2;w->offer_silver=money;}
    w->phase=w->offer_type?SR_WAR_OFFER:SR_WAR_NO_OFFER;
    return 1;
}
int sr_war_accept(SrGame *g,SrWar *w) {
    if(!valid(g,w) || w->phase!=SR_WAR_OFFER) return 0;
    if(w->offer_type==1) {
        int a=w->offer_area;
        sr_list_remove(&g->player_areas,a);g->areas[a].owned=0;w->territory=-a;
        /* ScoreScript 246 exits before owner/relation updates for the last province. */
        if(!g->player_areas.count) {g->crushed=1;w->phase=SR_WAR_DONE;return 1;}
        if(g->current_area==a) g->current_area=g->player_areas.records[0];
        g->areas[a].owner=owner(w->country);
    } else {g->silver-=w->offer_silver;w->fee=-w->offer_silver;}
    SrCountry *c=&g->countries[w->country-1];c->relation-=5;
    if(c->relation<-3) c->relation=-3;
    w->phase=SR_WAR_DONE;return 1;
}
int sr_war_refuse(SrWar *w) {
    if(!w || !w->active || w->phase!=SR_WAR_OFFER) return 0;
    w->phase=SR_WAR_DECLARE;return 1;
}
int sr_war_choose_battle(const SrGame *g,SrWar *w,int full) {
    if(!valid(g,w) || (w->phase!=SR_WAR_DECLARE && w->phase!=SR_WAR_NO_OFFER) || (full!=0 && full!=1)) return 0;
    w->full=full;w->error=0;
    if(w->attacking) {
        w->targets=(SrRecordList){0};
        for(int a=1;a<=SR_AREA_COUNT;++a)
            if(g->areas[a].owner==owner(w->country)) sr_list_add(&w->targets,a);
        w->area=select_row(&w->targets,1);w->phase=SR_WAR_TARGET;
    } else {defense_target(g,w);w->regiment=select_row(&g->player_areas,1);w->phase=SR_WAR_REGIMENT;}
    return 1;
}
int sr_war_select_target(const SrGame *g,SrWar *w,int position) {
    if(!valid(g,w) || w->phase!=SR_WAR_TARGET) return 0;
    w->area=select_row(&w->targets,position);return 1;
}
int sr_war_confirm_target(const SrGame *g,SrWar *w) {
    if(!valid(g,w) || w->phase!=SR_WAR_TARGET) return 0;
    w->regiment=select_row(&g->player_areas,1);w->phase=SR_WAR_REGIMENT;return 1;
}
int sr_war_select_regiment(const SrGame *g,SrWar *w,int position) {
    if(!valid(g,w) || w->phase!=SR_WAR_REGIMENT) return 0;
    w->regiment=select_row(&g->player_areas,position);w->error=0;return 1;
}
int sr_war_fight(SrGame *g,SrWar *w) {
    if(!valid(g,w) || w->phase!=SR_WAR_REGIMENT || !sr_list_has(&g->player_areas,w->regiment)) return 0;
    const SrAreaState *a=&g->areas[w->regiment];
    if(a->infantry+a->cavalry<=0) {w->error=SR_WAR_EMPTY_REGIMENT;return 0;}
    w->error=0;
    SrCountry *c=&g->countries[w->country-1];
    w->enemy_level=g->year>c->troop_level3_year?3:g->year>c->troop_level2_year?2:1;
    w->swed[0]=a->infantry;w->swed[1]=a->cavalry;w->swed[2]=a->artillery;
    w->enemy[0]=c->infantry;w->enemy[1]=c->cavalry;w->enemy[2]=c->artillery;
    w->enemy_original_real=c->troops_real;
    w->commander_bonus=1.0;
    const SrRecordList *commanders=&g->people[SR_COMMANDERS].owned;
    for(int n=0;n<commanders->count;++n) {
        const SrCatalogEntry *p=sr_catalog_entry(SR_COMMANDERS,commanders->records[n]);
        if(p) w->commander_bonus*=0.95000000000000007+p->level/10.0-(p->level-1)/20.0;
    }
    if(w->full) {w->phase=SR_WAR_FULL_PENDING;return 1;}
    double enemy=(w->enemy[0]*2+w->enemy[1]*5+w->enemy[2]*7)*w->enemy_level;
    double swed=(w->swed[0]*2+w->swed[1]*5+w->swed[2]*7)*g->troop_level*w->commander_bonus;
    swed*=sr_random_next(&g->random,40)/100.0+1;
    int swed_loss,enemy_loss;
    if(swed>2*enemy) {w->won=1;swed_loss=30;enemy_loss=80;}
    else if(enemy>2*swed) {w->won=0;swed_loss=80;enemy_loss=30;}
    else {
        swed*=sr_random_next(&g->random,50)/100.0+1;
        enemy*=sr_random_next(&g->random,50)/100.0+1;
        w->won=swed>=enemy;swed_loss=w->won?40:60;enemy_loss=w->won?60:40;
    }
    /* Preserve the original four loss draws, including zero-sized troop groups. */
    for(int n=0;n<2;++n) w->swed_dead[n]=w->swed[n]*((swed_loss-sr_random_next(&g->random,20))/100.0);
    for(int n=0;n<2;++n) w->enemy_dead[n]=w->enemy[n]*((enemy_loss-sr_random_next(&g->random,20))/100.0);
    w->enemy_dead_real=3;
    c->relation=2;w->phase=SR_WAR_QUICK;return 1;
}
static int new_level(SrGame *g,int base) {
    int level=base+sr_random_next(&g->random,3)-2;
    return level<1?1:level>5?5:level;
}
static void develop_area(SrGame *g,int area) {
    /* MovieScript 5.fixNPCArea changes levels but retains production indexes. */
    int base=g->turn>51?5:g->turn>41?4:g->turn>25?3:g->turn>15?2:1;
    int farm=new_level(g,base),mine=new_level(g,base),military=new_level(g,base),city=new_level(g,base);
    int mine_max=1,military_max=1;
    for(int n=0;n<g->player_areas.count;++n) {
        const SrAreaState *a=&g->areas[g->player_areas.records[n]];
        if(a->mining_level>mine_max) mine_max=a->mining_level;
        if(a->military_level>military_max) military_max=a->military_level;
    }
    static const int ranges[]={375,2000,7500,10000,10000},starts[]={125,500,2500,10000,20000};
    SrAreaState *a=&g->areas[area];a->population=starts[city-1]+sr_random_next(&g->random,ranges[city-1]);
    a->farming_level=farm;a->mining_level=mine>mine_max?mine_max:mine;
    a->military_level=military>military_max?military_max:military;a->city_level=city;
}
static void lose_area(SrGame *g,SrWar *w) {
    int area=w->area;if(area<1 || area>SR_AREA_COUNT) return;
    if(sr_list_has(&g->player_areas,area)) {
        /* Original selects first BEFORE removal, even if that is the lost area. */
        if(g->current_area==area) g->current_area=g->player_areas.records[0];
        sr_list_remove(&g->player_areas,area);g->areas[area].owned=0;
    } else {
        for(int n=0;n<g->available_count;++n) if(g->available[n]==area) {
            for(int k=n+1;k<g->available_count;++k) g->available[k-1]=g->available[k];
            --g->available_count;break;
        }
        if(!w->surrender && g->areas[area].special==7) g->crushed=1;
    }
    g->areas[area].owner=owner(w->country);w->territory=-area;
}
static void apply_result(SrGame *g,SrWar *w) {
    if(w->applied) return;
    w->applied=1;
    if(w->surrender) {
        if(w->attacking) {w->fee=-g->turn*50;g->silver+=w->fee;}
        else lose_area(g,w);
    } else {
        if(w->attacking && w->won && w->area>0) {
            sr_list_add(&g->player_areas,w->area);g->areas[w->area].owned=1;
            develop_area(g,w->area);g->areas[w->area].owner='A';w->territory=w->area;
        } else if(!w->attacking && !w->won) lose_area(g,w);
        else {
            double si=w->swed_dead[0],sc=w->swed_dead[1],ei=w->enemy_dead[0],ec=w->enemy_dead[1];
            double fee,min_fee;
            int swed_real=!w->full,enemy_real=w->enemy_dead_real,min_real;
            if(w->won) {
                fee=integer(divide(ei+ec,500,enemy_real)*50-divide(si+sc,500,swed_real)*25);
                min_fee=divide(si,1000,swed_real)*25+divide(sc,1000,swed_real)*100;
                min_real=swed_real;
            } else {
                int f1=integer(divide(ei+ec,500,enemy_real)*75);
                int f2=integer(divide(w->enemy[0]+w->enemy[1]-ei-ec-(w->swed[0]+w->swed[1]-si-sc),
                    500,w->enemy_original_real || enemy_real || swed_real)*50);
                fee=f1>f2?f1:f2;
                min_fee=divide(ei,1000,enemy_real&1)*25+divide(ec,1000,enemy_real&2)*100;
                min_real=enemy_real;
            }
            if(fee<500) fee=500;
            if(fee<min_fee) {fee=min_fee;if(min_real) g->silver_is_float=1;}
            w->fee=w->won?fee:-fee;g->silver+=w->fee;
        }
        if(w->won) w->enemy_dead[2]=w->enemy[2]*0.5;
        else if(!w->retreated) w->swed_dead[2]=w->swed[2]*0.5;
    }
    if(!g->player_areas.count || g->crushed) {g->crushed=1;w->phase=SR_WAR_DONE;return;}
    if(g->silver < -250) {g->silver=-250;g->silver_is_float=0;}
    if(w->regiment>=1 && w->regiment<=SR_AREA_COUNT) {
        SrAreaState *a=&g->areas[w->regiment];
        a->infantry-=integer(w->swed_dead[0]);a->cavalry-=integer(w->swed_dead[1]);a->artillery-=integer(w->swed_dead[2]);
    }
    SrCountry *c=&g->countries[w->country-1];
    static const int inf[]={3000,2000,2000,5000},cav[]={1500,1000,3000,2000};
    c->infantry-=w->enemy_dead[0];c->cavalry-=w->enemy_dead[1];c->artillery-=integer(w->enemy_dead[2]);
    if(!w->surrender) c->troops_real|=w->enemy_dead_real;
    if(c->infantry<inf[w->country-1]) {c->infantry=inf[w->country-1];c->troops_real&=~1;}
    if(c->cavalry<cav[w->country-1]) {c->cavalry=cav[w->country-1];c->troops_real&=~2;}
    if(c->artillery<1000) c->artillery=1000;
    if(w->won) {++g->war_wins;g->point_mod+=sr_random_next(&g->random,5);}
    else g->point_mod-=sr_random_next(&g->random,5);
    g->war_area=0;w->phase=SR_WAR_RESULT;
}
int sr_war_surrender(SrGame *g,SrWar *w) {
    if(!valid(g,w) || w->phase!=SR_WAR_NO_TROOPS) return 0;
    w->surrender=1;g->countries[w->country-1].relation=2;defense_target(g,w);
    apply_result(g,w);return 1;
}
int sr_war_advance_result(SrGame *g,SrWar *w) {
    if(!valid(g,w)) return 0;
    if(w->phase==SR_WAR_QUICK) {apply_result(g,w);return 1;}
    if(w->phase==SR_WAR_RESULT) {w->phase=SR_WAR_DONE;return 1;}
    return 0;
}
int sr_war_finish_manual(SrGame *g,SrWar *w,int won,int retreated,
    const double losses[4],int enemy_dead_real) {
    if(!valid(g,w) || !w->full || w->phase!=SR_WAR_FULL_PENDING || w->applied || !losses ||
       (won!=0 && won!=1) || (retreated!=0 && retreated!=1) || (won && retreated) ||
       enemy_dead_real<0 || enemy_dead_real>3) return 0;
    for(int n=0;n<4;++n) {
        double total=n<2?w->swed[n]:w->enemy[n-2];
        int real=n>=2 && (enemy_dead_real&(1<<(n-2)));
        /* FLOAT survivor summation can exceed the input by a few ulps (e.g.
           6502/7 summed seven times). Preserve ScoreScript 45's subtraction. */
        if(!(losses[n]>=(real?-16*DBL_EPSILON*total:0) && losses[n]<=total)) return 0;
        if(!real && (losses[n]>INT_MAX || losses[n]!=(int)losses[n])) return 0;
    }
    w->won=won;w->retreated=retreated;w->enemy_dead_real=enemy_dead_real;
    for(int n=0;n<2;++n) {w->swed_dead[n]=losses[n];w->enemy_dead[n]=losses[n+2];}
    g->countries[w->country-1].relation=2; /* SVEA ScoreScript 87. */
    apply_result(g,w);return 1;
}
