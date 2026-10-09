#include "catalog.h"
#include "generated/catalog_data.h"
#define COUNT(a) ((int)(sizeof(a)/sizeof((a)[0])))
int sr_catalog_count(int kind) {
    switch(kind) {
    case SR_CULTURE:return COUNT(sr_culture);
    case SR_SCIENCE:return COUNT(sr_science);
    case SR_COMMANDERS:return COUNT(sr_commanders);
    case SR_EVENTS:return COUNT(sr_events);
    default:return 0;
    }
}
const SrCatalogEntry *sr_catalog_entry(int kind,int record) {
    if(record<1 || record>sr_catalog_count(kind)) return 0;
    switch(kind) {
    case SR_CULTURE:return &sr_culture[record-1];
    case SR_SCIENCE:return &sr_science[record-1];
    case SR_COMMANDERS:return &sr_commanders[record-1];
    case SR_EVENTS:return &sr_events[record-1];
    default:return 0;
    }
}
int sr_catalog_period_count(int kind,int period) {
    if(period<1 || period>59) return 0;
    int count=0;
    for(int i=1;i<=sr_catalog_count(kind);++i)
        if(sr_catalog_entry(kind,i)->periods & (UINT64_C(1)<<(period-1))) ++count;
    return count;
}
int sr_catalog_period_record(int kind,int period,int position) {
    if(period<1 || period>59 || position<1) return 0;
    for(int i=1;i<=sr_catalog_count(kind);++i)
        if(sr_catalog_entry(kind,i)->periods & (UINT64_C(1)<<(period-1)))
            if(--position==0) return i;
    return 0;
}
int sr_catalog_dated_count(int kind) {
    return kind==SR_EVENTS_A ? COUNT(sr_events_a) : kind==SR_EVENTS_B ? COUNT(sr_events_b) : 0;
}
const SrDatedEvent *sr_catalog_dated(int kind,int record) {
    if(record<1 || record>sr_catalog_dated_count(kind)) return 0;
    return kind==SR_EVENTS_A ? &sr_events_a[record-1] : &sr_events_b[record-1];
}
