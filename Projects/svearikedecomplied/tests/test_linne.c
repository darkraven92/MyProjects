#include "linne.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    SrLinne g,before;
    sr_linne_init(&g);
    CHECK(g.phase==SR_LINNE_INTRO && !sr_linne_choose(&g,0,1) && !sr_linne_finish(&g));
    CHECK(sr_linne_start(&g,100) && g.sound2==30 && !sr_linne_start(&g,101));
    sr_linne_step(&g,399);CHECK(g.phase==SR_LINNE_WATCH && !sr_linne_choose(&g,0,1));
    sr_linne_step(&g,400);CHECK(g.phase==SR_LINNE_PLAY && g.start==400);
    CHECK(sr_linne_choose(&g,0,1) && g.flower==1 && !g.attempts && g.sound1==29);
    CHECK(sr_linne_choose(&g,0,2) && g.flower==2 && !g.attempts);
    CHECK(sr_linne_choose(&g,1,1) && !g.score && g.attempts==1 && !g.flower && !g.name);
    CHECK(sr_linne_choose(&g,1,2) && sr_linne_choose(&g,0,2));
    CHECK(g.score==1 && g.attempts==2 && g.matched==2 && !g.flower && !g.name);
    before=g;
    CHECK(!sr_linne_choose(&g,0,2) && !sr_linne_choose(&g,1,2) && !sr_linne_choose(&g,2,1) &&
          !sr_linne_choose(&g,0,0) && !sr_linne_choose(&g,0,7) && !memcmp(&g,&before,sizeof g));
    sr_linne_step(&g,2200);CHECK(g.phase==SR_LINNE_PLAY);
    sr_linne_step(&g,2201);CHECK(g.phase==SR_LINNE_RESULT && !g.success && g.sound1==32 && g.sound2==-2);
    CHECK(sr_linne_finish(&g) && g.phase==SR_LINNE_DONE && !g.result && !sr_linne_finish(&g));
    const int tiers[]={10,8,7,6,5,2,1,0};
    for(int wrong=0;wrong<8;++wrong) {
        sr_linne_init(&g);CHECK(sr_linne_start(&g,0));sr_linne_step(&g,300);
        for(int n=0;n<wrong;++n) CHECK(sr_linne_choose(&g,0,1) && sr_linne_choose(&g,1,2));
        for(int pair=1;pair<=6;++pair) CHECK(sr_linne_choose(&g,0,pair) && sr_linne_choose(&g,1,pair));
        CHECK(g.score==6 && g.attempts==6+wrong && g.matched==63 && g.phase==SR_LINNE_PLAY);
        sr_linne_step(&g,301);CHECK(g.phase==SR_LINNE_RESULT && g.success==(wrong<=6));
        CHECK(g.sound1==(wrong<=6?31:32) && sr_linne_finish(&g) && g.result==tiers[wrong]);
    }
    /* ScoreScript 26 returns a positive tier even if six wrong guesses time out. */
    sr_linne_init(&g);CHECK(sr_linne_start(&g,0));sr_linne_step(&g,300);
    for(int n=0;n<6;++n) CHECK(sr_linne_choose(&g,0,1) && sr_linne_choose(&g,1,2));
    sr_linne_step(&g,2101);CHECK(!g.score && !g.success && sr_linne_finish(&g) && g.result==10);
    sr_linne_init(&g);CHECK(sr_linne_start(&g,UINT32_MAX-99));
    sr_linne_step(&g,199);CHECK(g.phase==SR_LINNE_WATCH);
    sr_linne_step(&g,200);CHECK(g.phase==SR_LINNE_PLAY);
    CHECK(sr_linne_result(5)==0 && sr_linne_result(13)==0);
    puts("Linne timing, selections, matched-card removal, all result tiers and original timeout quirk passed.");
    return 0;
}
