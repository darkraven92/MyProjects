#ifndef SVEA_LINNE_H
#define SVEA_LINNE_H
#include <stdint.h>
enum { SR_LINNE_INTRO, SR_LINNE_WATCH, SR_LINNE_PLAY, SR_LINNE_RESULT, SR_LINNE_DONE };
typedef struct {
    int phase,flower,name,score,attempts,success,result;
    unsigned matched;
    uint32_t start;
    int sound1,sound2; /* Original members; -1 stops, -2 finishes the current loop. */
} SrLinne;
void sr_linne_init(SrLinne *game);
int sr_linne_start(SrLinne *game,uint32_t ticks);
void sr_linne_step(SrLinne *game,uint32_t ticks);
int sr_linne_choose(SrLinne *game,int name,int pair);
int sr_linne_finish(SrLinne *game);
int sr_linne_result(int attempts);
#endif
