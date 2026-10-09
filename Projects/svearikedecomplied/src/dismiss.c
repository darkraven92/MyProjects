#include "dismiss.h"
static int has_area(const SrDismiss *d,int area) {
    for(int i=0;i<d->areas.count;++i) if(d->areas.records[i]==area) return 1;
    return 0;
}
static int troops(const SrAreaState *a,int troop) {
    return troop==SR_INFANTRY?a->infantry:troop==SR_CAVALRY?a->cavalry:a->artillery;
}
int sr_dismiss_begin(const SrGame *g,SrDismiss *d) {
    if(!g || !d || g->player_areas.count<1 || g->player_areas.count>SR_AREA_COUNT ||
       g->current_area<1 || g->current_area>SR_AREA_COUNT) return 0;
    for(int i=0;i<g->player_areas.count;++i)
        if(g->player_areas.records[i]<1 || g->player_areas.records[i]>SR_AREA_COUNT) return 0;
    *d=(SrDismiss){0}; d->active=1; d->area=g->current_area; d->areas=g->player_areas;
    return 1;
}
int sr_dismiss_select(SrDismiss *d,int line) {
    if(!d || !d->active || !d->areas.count) return 0;
    if(line<1) line=1;
    if(line>d->areas.count) line=d->areas.count;
    d->area=d->areas.records[line-1]; return 1;
}
int sr_dismiss_remaining(const SrGame *g,const SrDismiss *d,int troop) {
    if(!g || !d || !d->active || troop<0 || troop>2 || d->area<1 || d->area>SR_AREA_COUNT) return 0;
    return troops(&g->areas[d->area],troop)-d->home[d->area][troop];
}
int sr_dismiss_change(const SrGame *g,SrDismiss *d,int troop,int home) {
    if(!g || !d || !d->active || troop<0 || troop>2 || !has_area(d,d->area)) return 0;
    int *amount=&d->home[d->area][troop];
    if(home) {
        if(sr_dismiss_remaining(g,d,troop)<1000) return 0;
        *amount+=1000;
    } else {
        if(*amount<1000) return 0;
        *amount-=1000;
    }
    return 1;
}
int sr_dismiss_commit(SrGame *g,SrDismiss *d) {
    if(!g || !d || !d->active || d->areas.count<1 || d->areas.count>SR_AREA_COUNT ||
       d->areas.count!=g->player_areas.count) return 0;
    /* Modal game state normally cannot change. Reject a stale/corrupt request
       atomically instead of dismissing from different/newly lost provinces. */
    for(int i=0;i<d->areas.count;++i) {
        int n=d->areas.records[i];
        if(n<1 || n>SR_AREA_COUNT || n!=g->player_areas.records[i]) return 0;
        for(int kind=0;kind<3;++kind)
            if(d->home[n][kind]<0 || d->home[n][kind]>troops(&g->areas[n],kind)) return 0;
    }
    for(int i=0;i<d->areas.count;++i) {
        int n=d->areas.records[i]; SrAreaState *a=&g->areas[n];
        a->infantry-=d->home[n][SR_INFANTRY];
        a->cavalry-=d->home[n][SR_CAVALRY];
        a->artillery-=d->home[n][SR_ARTILLERY];
    }
    d->active=0; return 1;
}
