#ifndef SVEA_FAMILY_H
#define SVEA_FAMILY_H
#include "game.h"
void sr_family_init(SrGame *game);
int sr_family_update(SrGame *game);
const char *sr_family_title(const SrFamilyHead *head);
/* Load/rebuild invokes fixTitle again, including a new draw for negative AP. */
void sr_family_rebuild_titles(SrGame *game);
#endif
