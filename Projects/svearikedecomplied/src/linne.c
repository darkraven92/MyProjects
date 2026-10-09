#include "linne.h"
void sr_linne_init(SrLinne *g) { *g=(SrLinne){0}; }
int sr_linne_start(SrLinne *g,uint32_t ticks) {
    if(!g || g->phase!=SR_LINNE_INTRO) return 0;
    *g=(SrLinne){.phase=SR_LINNE_WATCH,.start=ticks,.sound2=30};
    return 1;
}
int sr_linne_result(int attempts) {
    /* ScoreScript 26 tests attempts only, not the score/success predicate in
       ScoreScript 16. Preserve that difference even on timeout. */
    static const int tiers[]={10,8,7,6,5,2,1};
    return attempts>=6 && attempts<=12?tiers[attempts-6]:0;
}
void sr_linne_step(SrLinne *g,uint32_t ticks) {
    if(!g) return;
    if(g->phase==SR_LINNE_WATCH && ticks-g->start>=300) {
        g->phase=SR_LINNE_PLAY;g->start=ticks;
    } else if(g->phase==SR_LINNE_PLAY && (ticks-g->start>1800 || g->score==6)) {
        g->phase=SR_LINNE_RESULT;
        g->success=g->attempts<=12 && g->score==6;
        g->sound1=g->success?31:32;g->sound2=-2;
    }
}
int sr_linne_choose(SrLinne *g,int name,int pair) {
    if(!g || g->phase!=SR_LINNE_PLAY || (name!=0 && name!=1) || pair<1 || pair>6 ||
       (g->matched&(1u<<(pair-1)))) return 0;
    g->sound1=29;
    if(name) g->name=pair; else g->flower=pair;
    if(g->flower && g->name) {
        if(g->flower==g->name) {g->matched|=1u<<(pair-1);++g->score;}
        g->flower=g->name=0;++g->attempts;
    }
    return 1;
}
int sr_linne_finish(SrLinne *g) {
    if(!g || g->phase!=SR_LINNE_RESULT) return 0;
    g->result=sr_linne_result(g->attempts);g->phase=SR_LINNE_DONE;
    return 1;
}
