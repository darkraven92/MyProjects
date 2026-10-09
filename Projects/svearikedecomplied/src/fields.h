#ifndef SVEA_FIELDS_H
#define SVEA_FIELDS_H
#include <stddef.h>
#include "dismiss.h"
#include "trade_orders.h"
/* SVEA's original dynamic field members. Text content and score geometry are
   independent of glyph rasterization; no saved authoring text is substituted. */
typedef struct {
    int frame,member,channel,x,y,width,height,ink;
    int font_id,size,line_height,ascent,alignment;
    unsigned color;
    const char *font_name;
    int face;
} SrFieldLayout;
typedef struct {
    int selected[3],commander_panel,hover_area;
    const SrDismiss *dismiss;
    const SrTradeDrag *trade;
} SrFieldContext;
const SrFieldLayout *sr_field_layout(int frame,int member);
/* 0: UTF-8/ASCII, 1: original person-name bytes (Windows-1252). Event strings
   are not covered: some of them mix legacy encodings in the original data. */
int sr_field_encoding(int member);
/* Byte count on success; -1 unsupported/invalid state, -2 insufficient buffer.
   On failure a supplied nonempty output buffer is cleared. */
int sr_field_text(const SrGame *game,const SrFieldContext *context,int member,char *out,size_t capacity);
#endif
