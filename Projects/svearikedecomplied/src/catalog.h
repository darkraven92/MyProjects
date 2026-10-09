#ifndef SVEA_CATALOG_H
#define SVEA_CATALOG_H
#include <stdint.h>
enum { SR_CULTURE, SR_SCIENCE, SR_COMMANDERS, SR_EVENTS };
enum { SR_EVENTS_A, SR_EVENTS_B };
/* Text is stored in its original legacy byte encoding, not UTF-8. Handler names
   are data for explicit C translations; they are never evaluated as Lingo. */
typedef struct {
    int number, level;
    uint64_t periods;
    const char *name_bytes, *years_bytes, *note_bytes;
    const char *result_handler, *test_handler, *minigame_bytes, *file_name;
} SrCatalogEntry;
typedef struct {
    int number, base_year; /* Events B add random(15) during runtime initialization. */
    const char *name_bytes, *type, *test_handler, *result_handler, *minigame_bytes, *file_name;
} SrDatedEvent;
int sr_catalog_count(int kind);
const SrCatalogEntry *sr_catalog_entry(int kind,int record);
int sr_catalog_period_count(int kind,int period);
/* One-based record and position, preserving fixWhenList's insertion order. */
int sr_catalog_period_record(int kind,int period,int position);
int sr_catalog_dated_count(int kind);
const SrDatedEvent *sr_catalog_dated(int kind,int record);
#endif
