#ifndef SVEA_DIRECTOR_H
#define SVEA_DIRECTOR_H
#include <stddef.h>
#include <stdint.h>

/* Non-owning views: the caller must keep the input buffer alive. */
typedef struct {
    const uint8_t *data;
    size_t size, entries;
    uint32_t count, version;
    uint16_t stride;
    int little_endian;
    char form[5];
} SrArchive;

typedef struct {
    char tag[5];
    uint32_t id, offset, size;
    int active;
} SrResource;

/* NULL means success. No allocation, platform dependencies or global state. */
const char *sr_archive_open(SrArchive *archive, const void *data, size_t size);
const char *sr_archive_resource(const SrArchive *archive, uint32_t id, SrResource *out);
/* STXT payload numbers are big endian even inside XFIR archives. */
const char *sr_text(const uint8_t *data, size_t size, const uint8_t **text, size_t *length);
#endif
