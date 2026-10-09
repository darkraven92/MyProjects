#include "diplomacy.h"
const char *sr_diplomacy_country_name(int country) {
    /* SVEA ScoreScripts 110–113; display columns use order 1,4,3,2. */
    static const char *names[]={"","Danmark","Kejsarstaterna","Polen","Ryssland"};
    return country>=1 && country<=4?names[country]:"";
}
int sr_diplomacy_relation_icon(int relation) {
    return relation<=-3?1:relation==-2?2:relation<=3?3:relation<=6?4:5;
}
int sr_diplomacy_upgrade(SrGame *g,int country) {
    if(!g || country<1 || country>4) return SR_DIPLOMACY_INVALID;
    SrCountry *c=&g->countries[country-1];
    if(c->diplomacy<1) return SR_DIPLOMACY_INVALID;
    if(c->diplomacy>=5) return SR_DIPLOMACY_MAX_LEVEL;
    int next=c->diplomacy+1,hq=next==3?2:next==4?3:0;
    if(next==5) {
        int embassy=0;
        for(int n=0;n<g->player_areas.count;++n) {
            int area=g->player_areas.records[n];
            if(area>=1 && area<=SR_AREA_COUNT && g->areas[area].special==5) embassy=1;
        }
        if(!embassy) return SR_DIPLOMACY_EMBASSY;
    }
    double cost=sr_game_price(6,next)*g->diplomacy_upgrade_mod;
    if(g->silver<cost || g->hq_level<hq) return SR_DIPLOMACY_REQUIREMENTS;
    g->silver-=cost;c->diplomacy=next;
    return SR_DIPLOMACY_OK;
}
int sr_diplomacy_adjust(SrGame *g,int country,int amount) {
    /* Only the original two controls call bribe, with +100 and -100. */
    if(!g || country<1 || country>4 || (amount!=100 && amount!=-100)) return SR_DIPLOMACY_INVALID;
    int old=g->player_relation_mod[country-1];
    if(old<-1000 || old>1000) return SR_DIPLOMACY_INVALID;
    int next=old+amount;
    if(next<-1000 || next>1000) return SR_DIPLOMACY_LIMIT;
    int spending=(amount>0 && old>=0) || (amount<0 && old<=0);
    if(spending && g->silver<100) return SR_DIPLOMACY_SILVER;
    g->silver+=spending?-100:100;
    g->player_relation_mod[country-1]=next;
    return SR_DIPLOMACY_OK;
}
