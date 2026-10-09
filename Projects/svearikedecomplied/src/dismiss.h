#ifndef SVEA_DISMISS_H
#define SVEA_DISMISS_H
#include "game.h"
/* Original gHomeList: changes remain provisional until sendTroopsHomeOK. */
typedef struct {
    int active,area;
    SrRecordList areas;
    int home[SR_AREA_COUNT+1][3];
} SrDismiss;
int sr_dismiss_begin(const SrGame *game,SrDismiss *dismiss);
int sr_dismiss_select(SrDismiss *dismiss,int line);
int sr_dismiss_remaining(const SrGame *game,const SrDismiss *dismiss,int troop);
int sr_dismiss_change(const SrGame *game,SrDismiss *dismiss,int troop,int home);
int sr_dismiss_commit(SrGame *game,SrDismiss *dismiss);
#endif
