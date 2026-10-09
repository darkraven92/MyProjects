#ifndef SR_TRADE_ORDERS_H
#define SR_TRADE_ORDERS_H
#include "game.h"
enum { SR_TRADE_OK, SR_TRADE_INVALID, SR_TRADE_MAX_LEVEL, SR_TRADE_HARBOR,
    SR_TRADE_REQUIREMENTS, SR_TRADE_RESOURCES, SR_TRADE_CAPACITY };
enum { SR_TRADE_CROPS, SR_TRADE_METAL };
/* MovieScripts 2, 265 and 269. Positive loads sell, negative loads buy. */
int sr_trade_capacity(const SrGame *g,int country);
int sr_trade_upgrade(SrGame *g,int country);
int sr_trade_arrow(SrGame *g,int country,int commodity,int amount);
int sr_trade_marker(const SrGame *g,int country,int commodity,int setup);
typedef struct {
    int active,country,commodity,capacity,load,x,offset;
    double price,min,max,silver,crops,metal;
} SrTradeDrag;
int sr_trade_drag_begin(const SrGame *g,SrTradeDrag *d,int country,int commodity,int mouse_x,int marker_x);
void sr_trade_drag_move(SrTradeDrag *d,int mouse_x);
void sr_trade_drag_resources(const SrTradeDrag *d,double *silver,double *crops,double *metal);
int sr_trade_drag_commit(SrGame *g,SrTradeDrag *d);
#endif
