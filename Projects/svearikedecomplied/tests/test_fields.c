#include "fields.h"
#include "people.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"%d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void) {
    sr_game_seed(1);CHECK(sr_game_begin(3));SrGame g=*sr_game_state(),saved;
    SrFieldContext c={0};char text[4096],small[4];
    CHECK(sr_field_text(&g,&c,130,text,sizeof text)==4 && !strcmp(text,"1523"));
    CHECK(sr_field_text(&g,&c,131,text,sizeof text)==4 && !strcmp(text,"1000"));
    CHECK(sr_field_text(&g,&c,131,small,sizeof small)==-2 && !*small);
    CHECK(sr_field_text(&g,&c,999,text,sizeof text)==-1 && !*text);
    CHECK(sr_field_text(0,&c,130,text,sizeof text)==-1 && !*text);
    CHECK(sr_field_text(&g,0,130,text,sizeof text)==-1 && !*text);
    CHECK(sr_field_text(&g,&c,130,0,0)==-1);
    const double values[]={0,1.49,1.5,-1.49,-1.5,INT_MAX,INT_MIN};
    const char *expected[]={"0","1","2","-1","-2","2147483647","-2147483648"};
    for(int i=0;i<7;++i) {
        g.silver=values[i];saved=g;
        CHECK(sr_field_text(&g,&c,131,text,sizeof text)>0 && !strcmp(text,expected[i]));
        CHECK(!memcmp(&g,&saved,sizeof g));
    }
    g.silver=1e30;CHECK(sr_field_text(&g,&c,131,text,sizeof text)==-1);
    CHECK(sr_field_text(&g,&c,140,text,sizeof text)==1 && !strcmp(text," "));
    g.people[SR_CULTURE].available=(SrRecordList){2,{1,2}};
    CHECK(sr_field_text(&g,&c,140,text,sizeof text)>0);
    CHECK(!strcmp(text,"Johannes Magnus\rLaurentius Andre\xe6"));
    g.people[SR_COMMANDERS].available=(SrRecordList){1,{2}};
    CHECK(sr_field_text(&g,&c,180,text,sizeof text)>0 && !strcmp(text,"Berent von Melen\r"));
    c.selected[SR_CULTURE]=1;
    CHECK(sr_field_text(&g,&c,144,text,sizeof text)>0 && !strcmp(text,"Johannes Magnus"));
    CHECK(sr_field_text(&g,&c,145,text,sizeof text)>0 && !strcmp(text,"Rang: 1"));
    CHECK(sr_field_text(&g,&c,146,text,sizeof text)>0 && !strcmp(text,"Pris: 200"));
    c.selected[SR_CULTURE]=0;c.selected[SR_SCIENCE]=20;
    CHECK(sr_field_text(&g,&c,144,text,sizeof text)==1 && !strcmp(text," "));
    CHECK(sr_field_text(&g,&c,184,text,sizeof text)>0 && !strcmp(text,"Rang: 5"));
    CHECK(sr_field_text(&g,&c,183,text,sizeof text)>0 && !strcmp(text,"Carl von Linn\xe9"));
    CHECK(sr_field_text(&g,&c,185,text,sizeof text)>0 && !strcmp(text,"Pris: 800"));
    c.commander_panel=1;c.selected[SR_COMMANDERS]=2;
    CHECK(sr_field_text(&g,&c,144,text,sizeof text)>0 && !strcmp(text,"Berent von Melen"));
    c.hover_area=2;CHECK(sr_field_text(&g,&c,207,text,sizeof text)>0 && !strcmp(text,"Södermanland"));
    c.hover_area=0;CHECK(sr_field_text(&g,&c,207,text,sizeof text)==1 && !strcmp(text," "));
    g.people[SR_CULTURE].available.count=34;
    CHECK(sr_field_text(&g,&c,140,text,sizeof text)==-1);
    g.areas[1].infantry=2000;SrDismiss d;CHECK(sr_dismiss_begin(&g,&d));c.dismiss=&d;
    CHECK(sr_field_text(&g,&c,222,text,sizeof text)>0 && !strcmp(text,"Uppland\r"));
    CHECK(sr_dismiss_change(&g,&d,SR_INFANTRY,1));
    CHECK(sr_field_text(&g,&c,224,text,sizeof text)>0 && !strcmp(text,"1000"));
    CHECK(sr_field_text(&g,&c,227,text,sizeof text)>0 && !strcmp(text,"1000"));
    CHECK(g.areas[1].infantry==2000);
    const SrFieldLayout *p=sr_field_layout(12,130);
    CHECK(p && p->x==236 && p->y==31 && p->width==58 && p->height==22 && p->size==18 &&
          p->line_height==22 && p->ascent==17 && p->color==0x330000 && !strcmp(p->font_name,"Arial"));
    p=sr_field_layout(12,207);CHECK(p && p->font_id==136 && !strcmp(p->font_name,"MS Sans Serif"));
    p=sr_field_layout(69,140);CHECK(p && p->height==242 && p->width==87 && p->line_height==13);
    CHECK(sr_field_encoding(140)==1 && sr_field_encoding(144)==1 && sr_field_encoding(207)==0);
    CHECK(!sr_field_layout(12,140) && !sr_field_layout(0,130));
    puts("Original field values, rounding, list separators, selection, dismissal and score/font geometry passed.");
    return 0;
}
