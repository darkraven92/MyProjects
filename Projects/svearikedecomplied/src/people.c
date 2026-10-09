#include "people.h"
int sr_people_price(int kind,int record) {
    if(kind<SR_CULTURE || kind>SR_COMMANDERS) return 0;
    const SrCatalogEntry *p=sr_catalog_entry(kind,record);
    return p?sr_game_price(10+2*kind,p->level):0;
}
int sr_people_hire(SrGame *g,int kind,int record) {
    if(!g || kind<SR_CULTURE || kind>SR_COMMANDERS) return SR_HIRE_INVALID;
    if(!record) return SR_HIRE_SELECT;
    const SrCatalogEntry *p=sr_catalog_entry(kind,record);
    if(!p || p->level<1 || p->level>5) return SR_HIRE_INVALID;
    SrPeople *people=&g->people[kind];
    int position=-1;
    for(int i=0;i<people->available.count;++i)
        if(people->available.records[i]==record) position=i;
    if(position<0 || people->owned.count>=33) return SR_HIRE_INVALID;
    for(int i=0;i<people->owned.count;++i)
        if(people->owned.records[i]==record) return SR_HIRE_INVALID;
    /* Original order: rank-five prerequisite before the silver check.
       Commanders have no special-building requirement. */
    if(p->level==5 && kind!=SR_COMMANDERS) {
        int found=0;
        for(int i=0;i<g->player_areas.count;++i) {
            int area=g->player_areas.records[i];
            if(area>=1 && area<=SR_AREA_COUNT && g->areas[area].special==3+kind) found=1;
        }
        if(!found) return kind==SR_CULTURE?SR_HIRE_ARCH:SR_HIRE_UNIVERSITY;
    }
    int cost=sr_people_price(kind,record);
    if(cost>g->silver) return SR_HIRE_SILVER;
    people->owned.records[people->owned.count++]=record;
    for(int i=position+1;i<people->available.count;++i)
        people->available.records[i-1]=people->available.records[i];
    --people->available.count;
    g->silver-=cost;
    return SR_HIRE_OK;
}
