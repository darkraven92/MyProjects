#include "events.h"
static int same(const char *a,const char *b) {
    while(*a && *a==*b) { ++a; ++b; }
    return *a==*b;
}
int sr_list_has(const SrRecordList *list,int record) {
    for(int i=0;i<list->count;++i) if(list->records[i]==record) return 1;
    return 0;
}
int sr_list_add(SrRecordList *list,int record) {
    if(list->count>=33) return 0;
    list->records[list->count++]=record; return 1;
}
int sr_list_remove(SrRecordList *list,int record) {
    for(int i=0;i<list->count;++i) if(list->records[i]==record) {
        for(int j=i+1;j<list->count;++j) list->records[j-1]=list->records[j];
        --list->count; return 1;
    }
    return 0;
}
static const SrDatedEvent *dated(SrEvent event) {
    return event.kind==SR_EVENT_A || event.kind==SR_EVENT_B ?
        sr_catalog_dated(event.kind==SR_EVENT_A?SR_EVENTS_A:SR_EVENTS_B,event.record) : 0;
}
const char *sr_event_name(SrEvent event) {
    const SrDatedEvent *d=dated(event);
    const SrCatalogEntry *c=sr_catalog_entry(event.kind,event.record);
    return d?d->name_bytes:c?c->name_bytes:"";
}
const char *sr_event_minigame(SrEvent event) {
    const SrDatedEvent *d=dated(event);
    const SrCatalogEntry *c=sr_catalog_entry(event.kind,event.record);
    const char *mini=d?d->minigame_bytes:c?c->minigame_bytes:"-";
    return same(mini,"-") ? "" : d?d->file_name:c->file_name;
}
int sr_event_is_king(SrEvent event) {
    const SrDatedEvent *d=dated(event);
    return event.kind==SR_EVENT_A && d && same(d->type,"king");
}
int sr_event_eligible(const SrGame *g,int kind,int record) {
    if(!g) return 0;
    if(kind==SR_EVENT_B) {
        if(!sr_catalog_dated(SR_EVENTS_B,record)) return 0;
        switch(record) {
        case 1:case 2:
            for(int n=0;n<g->player_areas.count;++n) {
                const SrAreaState *a=&g->areas[g->player_areas.records[n]];
                if(record==1 ? a->mining_level>=3 : a->city_level>=2 && g->hq_level>=2) return 1;
            }
            return 0;
        case 3:return g->troop_level>=2 && g->player_areas.count>=3;
        case 4:return g->silver>=200;
        case 5:return g->points>=10;
        case 6:
            for(int i=0;i<4;++i) if(g->countries[i].trade==5 && g->silver>=1000) return 1;
            return 0;
        }
    }
    if(kind!=SR_EVENTS || !sr_catalog_entry(kind,record)) return 0;
    switch(record) {
    case 2:case 3:case 4:
        for(int n=0;n<g->player_areas.count;++n)
            if(sr_game_unrest(g,g->player_areas.records[n],0)>20) return 1;
        return 0;
    case 7:return g->event_b_used[1]!=0;
    case 8:case 14:return g->countries[0].relation>4;
    case 17:return g->countries[3].relation>4;
    case 20:case 21:return g->countries[3].relation>5;
    default:return 1;
    }
}
int sr_event_pool(const SrGame *g,SrEvent pool[SR_EVENT_POOL_CAPACITY]) {
    if(!g || !pool || g->turn<1 || g->turn>59) return 0;
    static const int kinds[]={SR_EVENTS,SR_SCIENCE,SR_CULTURE,SR_COMMANDERS};
    int count=0;
    for(int k=0;k<4;++k) {
        int kind=kinds[k],n=sr_catalog_period_count(kind,g->turn);
        const SrRecordList *used=kind==SR_EVENTS?&g->used_events:&g->people[kind].used;
        for(int p=1;p<=n;++p) {
            int record=sr_catalog_period_record(kind,g->turn,p);
            if(kind==SR_EVENTS && !sr_event_eligible(g,kind,record)) continue;
            if(sr_list_has(used,record)) continue;
            for(int copies=0;copies<(kind==SR_EVENTS?3:1);++copies)
                pool[count++]=(SrEvent){kind,record};
        }
    }
    return count;
}
SrEvent sr_event_choose(SrGame *g) {
    SrEvent pool[SR_EVENT_POOL_CAPACITY],none={0,0};
    if(!g || g->turn<1 || g->turn>59) return none;
    int count=sr_event_pool(g,pool),hit=sr_random_next(&g->random,count<=20?20:count);
    if(g->turn==4 && !g->people[SR_SCIENCE].available.count && !sr_list_has(&g->people[SR_SCIENCE].owned,1))
        return (SrEvent){SR_SCIENCE,1};
    if(g->turn==5 && !g->people[SR_COMMANDERS].available.count && !sr_list_has(&g->people[SR_COMMANDERS].owned,1))
        return (SrEvent){SR_COMMANDERS,1};
    return hit<=count?pool[hit-1]:none;
}
SrEvent sr_event_choose_b(const SrGame *g) {
    if(g) for(int i=1;i<=6;++i)
        if(sr_event_eligible(g,SR_EVENT_B,i) && g->year>=g->event_b_year[i-1] && !g->event_b_used[i-1])
            return (SrEvent){SR_EVENT_B,i};
    return (SrEvent){0,0};
}
static void halve_silver(SrGame *g) {
    g->silver=g->silver_is_float?g->silver/2:(double)((int)g->silver/2);
}
static void remove_available(SrGame *g,int area) {
    for(int i=0;i<g->available_count;++i) if(g->available[i]==area) {
        for(int j=i+1;j<g->available_count;++j) g->available[j-1]=g->available[j];
        --g->available_count; break;
    }
}
int sr_event_apply(SrGame *g,SrEvent e) {
    if(!g) return 0;
    if(e.kind>=SR_CULTURE && e.kind<=SR_COMMANDERS) {
        if(!sr_catalog_entry(e.kind,e.record)) return 0;
        SrPeople *p=&g->people[e.kind];
        if(p->used.count==33 || p->available.count==33) return 0;
        sr_list_add(&p->used,e.record); sr_list_add(&p->available,e.record); return 1;
    }
    if(e.kind==SR_EVENT_A) {
        if(!dated(e)) return 0;
        if(e.record==3 && g->available_count==SR_AREA_COUNT) return 0;
        switch(e.record) {
        case 3:g->areas[28].owner='A'; g->available[g->available_count++]=28; break;
        case 5:if(g->family==3) g->point_mod-=5; break;
        case 6:g->countries[2].relation-=3; break;
        case 9: {
            int troops=0;
            for(int i=0;i<g->player_areas.count;++i) {
                const SrAreaState *a=&g->areas[g->player_areas.records[i]];
                troops+=a->infantry+a->cavalry+a->artillery;
            }
            g->silver+=100*troops/1000;
            /* Bytecode reads an uninitialized LOCAL relation, not country data.
               D5 numeric coercion of VOID starts at zero: the results are 5,10. */
            g->countries[1].relation=5; g->countries[2].relation=10; break;
        }
        case 15:for(int i=0;i<4;++i) g->countries[i].relation-=2; break;
        case 20:g->points=sr_game_points(g); g->point_mod+=g->points>250?-5:5; break;
        }
        if(sr_event_is_king(e)) ++g->king;
        return 1;
    }
    if(e.kind==SR_EVENT_B) {
        if(!dated(e)) return 0;
        switch(e.record) {
        case 2:
            g->countries[0].relation-=2;
            for(int i=1;i<4;++i) g->countries[i].relation+=2;
            g->silver+=250; break;
        case 3:
            for(int i=0;i<5;++i) g->troop_capacity[i]+=2000;
            for(int i=0;i<g->player_areas.count;++i) g->areas[g->player_areas.records[i]].infantry+=2000;
            break;
        case 4:case 6:halve_silver(g); break;
        case 5:g->point_mod-=10; break;
        }
        g->event_b_used[e.record-1]=1; return 1;
    }
    if(e.kind!=SR_EVENTS || !sr_catalog_entry(SR_EVENTS,e.record) || g->used_events.count==33) return 0;
    sr_list_add(&g->used_events,e.record);
    switch(e.record) {
    case 1:++g->countries[0].diplomacy; break;
    case 2:case 3:case 4:g->no_riot=1; break;
    case 5:if(g->family==5) g->point_mod-=5; break;
    case 6:g->silver+=200; break;
    case 7:++g->culture_level; break;
    case 8:g->silver-=1000; break;
    case 9:if(g->family==1 || g->family==2 || g->family==4) g->point_mod-=5; break;
    case 10:g->metal+=100; break;
    case 12:if(g->silver>=100) g->silver-=100; else { g->silver=0; g->silver_is_float=0; } break;
    case 13:g->silver+=500; break;
    case 14:
        g->areas[17].owner='B'; remove_available(g,17);
        if(sr_list_remove(&g->player_areas,17)) {
            g->areas[17].owned=0;
            if(g->current_area==17) g->current_area=g->player_areas.count?g->player_areas.records[0]:0;
        }
        break;
    case 16:g->countries[3].relation=-1; break;
    case 18:g->countries[3].relation-=3; break;
    case 20:g->silver+=100; break;
    case 21:g->countries[3].relation=9; break;
    case 24:halve_silver(g); break;
    }
    return 1;
}
void sr_event_minigame_result(SrGame *g,SrEvent event,int score) {
    if(!g || score==0) return;
    const char *file=sr_event_minigame(event);
    if(!*file) return;
    g->silver+=100*score; g->point_mod+=score/2;
    int kind=-1,record=0;
    if(same(file,"ARMBORST")) { kind=SR_COMMANDERS; record=1; }
    else if(score>0 && same(file,"BELLMAN")) { kind=SR_CULTURE; record=24; }
    else if(score>0 && same(file,"LINNE")) { kind=SR_SCIENCE; record=20; }
    if(kind>=0) {
        sr_list_add(&g->people[kind].owned,record);
        sr_list_remove(&g->people[kind].available,record);
    }
}
