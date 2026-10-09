#include "family.h"
static char *append(char *p,const char *s) {
    while(*s) *p++=*s++;
    *p=0; return p;
}
static void name(SrGame *g,SrFamilyHead *head,const SrFamilyHead *previous) {
    static const char *early[]={"Birger","Erik","Gustaf","Jacob","Johan","Jöran","Jöns","Karl","Klas","Krister","Kristian","Lars","Magnus","Nils","Olof","Per","Petter","Sten","Svante","Ture"};
    static const char *middle[]={"Axel","Bengt","Carl","Claes","Erik","Filip","Gabriel","Gustav","Hans","Jacob","Johan","Klas","Lennart","Ludvig","Magnus","Otto","Per"};
    static const char *late[]={"Axel","Carl","Clas","Eric","Fredrik","Georg","Gustav","Henrik","Jacob","Johan","Magnus","Michael","Nils","Olof","Samuel","Ulrik"};
    static const char *surnames[]={"Tre Rosor","Brahe","Eka","Grip","Sture"};
    const char *const *names=g->year<1611?early:g->year<1719?middle:late;
    int count=g->year<1611?20:g->year<1719?17:16;
    char *p=append(head->name,names[sr_random_next(&g->random,count)-1]);
    p=append(p," ");
    if(previous) {
        const char *s=previous->name;
        while(*s && *s!=' ') *p++=*s++;
        *p=0;
    } else p=append(p,names[sr_random_next(&g->random,count)-1]);
    if(p[-1]=='s') --p;
    p=append(p,"sson "); append(p,surnames[g->family-1]);
}
void sr_family_init(SrGame *g) {
    g->point_mod-=sr_game_points(g); g->points=sr_game_points(g);
    SrFamilyHead *head=&g->family_heads[0];
    *head=(SrFamilyHead){0}; name(g,head,0);
    head->start_year=1523; head->start_points=g->points;
    head->type=sr_random_next(&g->random,2); head->title_type=head->type;
    g->family_head_count=1;
}
static void title_type(SrGame *g,SrFamilyHead *h) {
    h->title_type=h->points<0?sr_random_next(&g->random,2):h->type;
}
int sr_family_update(SrGame *g) {
    if(!g || g->family<1 || g->family>5 || g->family_head_count<1 ||
       g->family_head_count>=SR_FAMILY_HEAD_CAPACITY) return 0;
    SrFamilyHead *head=&g->family_heads[g->family_head_count-1];
    g->points=sr_game_points(g); head->points=g->points-head->start_points;
    /* The death roll happens even when AP loss or age already forces succession. */
    int replace=sr_random_next(&g->random,100)<25;
    if(head->points<0 || g->year-head->start_year>=50) replace=1;
    title_type(g,head);
    if(replace) {
        if(head->points>=100) {
            char *p=head->name;
            while(*p) ++p;
            while(p>head->name && p[-1]!=' ') --p;
            append(p,"Palpatine");
        }
        head->stop_year=g->year;
        SrFamilyHead *next=&g->family_heads[g->family_head_count];
        *next=(SrFamilyHead){0}; name(g,next,head);
        next->start_year=g->year; next->start_points=g->points;
        next->type=sr_random_next(&g->random,2); next->title_type=next->type;
        ++g->family_head_count;
    }
    return 1;
}
void sr_family_rebuild_titles(SrGame *g) {
    if(g) for(int i=0;i<g->family_head_count;++i) title_type(g,&g->family_heads[i]);
}
const char *sr_family_title(const SrFamilyHead *h) {
    static const char *titles[2][17]={
        {"Fängslad","Hovgunstling","Kunglig ämbetsman","Överste","General","Fältmarskalk","Guvernör","Generalguvernör","Ambassadör","Riksamiral","Riksmarsk","Rikskansler","Riksdrots","Riksråd","Riksråd","Riksråd","Riksföreståndare"},
        {"Förvisad","Hovgunstling","Kunglig ämbetsman","Domprost","Universitetskansler","Rådman","Biskop","Generalguvernör","Ambassadör","Biskop","Ärkebiskop","Rikskansler","Riksdrots","Riksråd","Riksråd","Riksråd","Kanslipresident"}
    };
    if(!h) return "";
    if(h->points>=100) return "Rymdimperiets kejsare";
    return titles[h->title_type==1?0:1][h->points<0?0:h->points>=15?16:h->points+1];
}
