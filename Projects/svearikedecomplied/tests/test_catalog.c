#include "catalog.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    CHECK(sr_catalog_count(SR_CULTURE)==32 && sr_catalog_count(SR_SCIENCE)==33);
    CHECK(sr_catalog_count(SR_COMMANDERS)==27 && sr_catalog_count(SR_EVENTS)==24);
    CHECK(!strcmp(sr_catalog_entry(SR_CULTURE,1)->name_bytes,"Johannes Magnus"));
    CHECK(!strcmp(sr_catalog_entry(SR_SCIENCE,1)->name_bytes,"Willem Boy"));
    CHECK(!strcmp(sr_catalog_entry(SR_COMMANDERS,1)->file_name,"ARMBORST"));
    CHECK(sr_catalog_entry(SR_COMMANDERS,1)->level==1);
    CHECK(!strcmp(sr_catalog_entry(SR_EVENTS,1)->result_handler,"resultC1"));
    CHECK(sr_catalog_period_count(SR_SCIENCE,1)==0);
    CHECK(sr_catalog_period_record(SR_CULTURE,1,1)==1);
    CHECK(sr_catalog_period_record(SR_SCIENCE,6,1)==1);
    CHECK(sr_catalog_period_record(SR_EVENTS,1,1)==1);
    for(int kind=0;kind<4;++kind) for(int period=1;period<=59;++period) {
        int previous=0,count=sr_catalog_period_count(kind,period);
        for(int pos=1;pos<=count;++pos) {
            int record=sr_catalog_period_record(kind,period,pos);
            CHECK(record>previous && record<=sr_catalog_count(kind)); previous=record;
        }
        CHECK(!sr_catalog_period_record(kind,period,count+1));
    }
    CHECK(sr_catalog_dated_count(SR_EVENTS_A)==22 && sr_catalog_dated_count(SR_EVENTS_B)==6);
    const SrDatedEvent *a=sr_catalog_dated(SR_EVENTS_A,1),*b=sr_catalog_dated(SR_EVENTS_B,1);
    CHECK(a->base_year==1523 && !strcmp(a->name_bytes,"Gustav Vasa") && !strcmp(a->type,"king"));
    CHECK(b->base_year==1523 && !strcmp(b->test_handler,"eventB1") && !strcmp(b->file_name,"SALA"));
    CHECK(!sr_catalog_entry(-1,1) && !sr_catalog_entry(0,0) && !sr_catalog_entry(0,33));
    CHECK(!sr_catalog_period_record(0,60,1) && !sr_catalog_period_record(0,1,0));
    CHECK(!sr_catalog_dated(2,1) && !sr_catalog_dated(SR_EVENTS_B,7));
    puts("144 original catalog records, ordered period eligibility and dated event tables passed.");
    return 0;
}
