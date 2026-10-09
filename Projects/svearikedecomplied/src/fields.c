#include "fields.h"
#include "people.h"
#include <limits.h>
#include <stdint.h>
#include "generated/field_layout.h"
typedef struct { char *out;size_t capacity,length;int invalid; } Writer;
static void append(Writer *w,const char *text) {
    while(*text) {
        if(w->length+1<w->capacity) w->out[w->length]=*text;
        ++w->length;++text;
    }
}
static void number(Writer *w,int value) {
    char digits[12];int n=0;
    uint32_t magnitude=value<0?0u-(uint32_t)value:(uint32_t)value;
    do {digits[n++]=(char)('0'+magnitude%10);magnitude/=10;} while(magnitude);
    if(value<0) digits[n++]='-';
    while(n) {char c[2]={digits[--n],0};append(w,c);}
}
static void resource(Writer *w,double value) {
    /* Director 5 integer(), also used by sr_game_value: ties away from zero. */
    double rounded=value+(value<0?-0.5:0.5);
    if(!(rounded>(double)INT_MIN-1 && rounded<(double)INT_MAX+1)) {w->invalid=1;return;}
    number(w,(int)rounded);
}
const SrFieldLayout *sr_field_layout(int frame,int member) {
    for(size_t i=0;i<sizeof layouts/sizeof *layouts;++i)
        if(layouts[i].frame==frame && layouts[i].member==member) return &layouts[i];
    return 0;
}
int sr_field_encoding(int member) {
    return (member>=140 && member<=144) || member==180 || member==181 || member==183;
}
static void people(Writer *w,const SrGame *g,int kind,int owned) {
    const SrRecordList *list=owned?&g->people[kind].owned:&g->people[kind].available;
    if(list->count<0 || list->count>33) {w->invalid=1;return;}
    if(!list->count) {append(w," ");return;}
    for(int i=0;i<list->count;++i) {
        const SrCatalogEntry *entry=sr_catalog_entry(kind,list->records[i]);
        if(!entry) {w->invalid=1;return;}
        append(w,entry->name_bytes);
        /* setUpCultAndSci deletes the final empty line; commanders retain it. */
        if(i+1<list->count || kind==SR_COMMANDERS) append(w,"\r");
    }
}
int sr_field_text(const SrGame *g,const SrFieldContext *c,int member,char *out,size_t capacity) {
    if(out && capacity) out[0]=0;
    if(!g || !c || !out || !capacity || g->current_area<1 || g->current_area>SR_AREA_COUNT) return -1;
    Writer w={out,capacity,0,0};const SrAreaState *a=&g->areas[g->current_area];
    double silver=g->silver,crops=g->crops,metal=g->metal;
    if(c->trade && c->trade->active) sr_trade_drag_resources(c->trade,&silver,&crops,&metal);
    if(member>=159 && member<=165 && member%2) {
        number(&w,sr_trade_capacity(g,(member-159)/2+1));
    } else if(member>=170 && member<=177) {
        int country=(member-170)%4;double price=member<174?g->countries[country].crops_price_mod:3*g->countries[country].metal_price_mod;
        if(!g->trade_prices_updated) append(&w,member<174?"1.0":"3.0");
        else {
            /* start() sets floatPrecision=2 before fixTradeBalance's puts. */
            int cents=(int)(price*100+0.5);number(&w,cents/100);append(&w,".");
            char digits[]={(char)('0'+cents/10%10),(char)('0'+cents%10),0};append(&w,digits);
        }
    } else if(member>=255 && member<=262) {
        static const int countries[]={1,4,3,2};int country=countries[(member-255)/2],commodity=(member-255)%2;
        int load=commodity?g->trade_metal[country-1]:g->trade_crops[country-1];
        if(c->trade && c->trade->active && c->trade->country==country && c->trade->commodity==commodity) load=c->trade->load;
        number(&w,load<0?-load:load);
    } else if(member>=304 && member<=311) {
        int value=g->player_relation_mod[(member-304)/2];
        number(&w,(member-304)%2?(value>0?value:0):(value<0?-value:0));
    } else if(member>=319 && member<=330) {
        const SrCountry *country=&g->countries[(member-319)/3];
        int troop=(member-319)%3;
        resource(&w,troop==0?country->infantry:troop==1?country->cavalry:country->artillery);
    } else switch(member) {
    case 130:number(&w,g->year);break;
    case 131:resource(&w,silver);break;
    case 132:resource(&w,crops);break;
    case 133:resource(&w,metal);break;
    case 140:case 141:case 142:case 143:people(&w,g,(member-140)%2,member>=142);break;
    case 180:case 181:people(&w,g,SR_COMMANDERS,member==181);break;
    case 144:case 145:case 146:case 183:case 184:case 185: {
        int science=member>=183,field=member-(science?183:144);
        int kind=science?SR_SCIENCE:c->commander_panel?SR_COMMANDERS:SR_CULTURE;
        const SrCatalogEntry *entry=sr_catalog_entry(kind,c->selected[kind]);
        if(!entry) append(&w," ");
        else if(field==0) append(&w,entry->name_bytes);
        else {append(&w,field==1?"Rang: ":"Pris: ");number(&w,field==1?entry->level:sr_people_price(kind,entry->number));}
        break;
    }
    case 147:number(&w,a->infantry);break;
    case 148:number(&w,a->artillery);break;
    case 149:number(&w,a->cavalry);break;
    case 207:append(&w,c->hover_area>=1 && c->hover_area<=SR_AREA_COUNT?sr_game_area_name(c->hover_area):" ");break;
    case 208:number(&w,a->population);break;
    case 332:number(&w,25*g->troop_level);break;
    case 333:number(&w,100*g->troop_level);break;
    case 334:number(&w,250*g->troop_level);break;
    case 222:case 223:case 224:case 225:case 226:case 227:case 228:case 229: {
        const SrDismiss *d=c->dismiss;
        if(!d || !d->active || d->area<1 || d->area>SR_AREA_COUNT || d->areas.count<1 || d->areas.count>33) {w.invalid=1;break;}
        if(member==222) {
            for(int i=0;i<d->areas.count;++i) {
                int area=d->areas.records[i];
                if(area<1 || area>SR_AREA_COUNT) {w.invalid=1;break;}
                append(&w,sr_game_area_name(area));append(&w,"\r");
            }
        } else if(member==223) append(&w,sr_game_area_name(d->area));
        else if(member<=226) number(&w,sr_dismiss_remaining(g,d,member-224));
        else number(&w,d->home[d->area][member-227]);
        break;
    }
    default:w.invalid=1;break;
    }
    if(w.invalid || w.length>=capacity) {out[0]=0;return w.invalid?-1:-2;}
    out[w.length]=0;return (int)w.length;
}
