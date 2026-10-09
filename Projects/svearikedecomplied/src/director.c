#include "director.h"

static uint32_t u32(const uint8_t *p, int le) {
    if (le) return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    return (uint32_t)p[3] | (uint32_t)p[2] << 8 |
        (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24;
}
static uint16_t u16(const uint8_t *p, int le) {
    return le ? (uint16_t)(p[0] | p[1] << 8) : (uint16_t)(p[1] | p[0] << 8);
}
static int tag_is(const char *a, const char *b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}
static void tag_read(char out[5], const uint8_t *p, int le) {
    for (int i = 0; i < 4; ++i) out[i] = (char)p[le ? 3 - i : i];
    out[4] = 0;
}
static int fits(size_t total, size_t start, size_t length) {
    return start <= total && length <= total - start;
}

const char *sr_archive_resource(const SrArchive *a, uint32_t id, SrResource *out) {
    if (!a || !a->data || !out || id >= a->count) return "invalid resource id";
    const uint8_t *p = a->data + a->entries + (size_t)id * a->stride;
    tag_read(out->tag, p, a->little_endian);
    out->id = id;
    out->size = u32(p + 4, a->little_endian);
    out->offset = u32(p + 8, a->little_endian);
    out->active = !tag_is(out->tag, "free") && !tag_is(out->tag, "junk");
    if (!out->active) return NULL; /* Stale offsets in unused slots are normal. */
    if (!fits(a->size, out->offset, 8)) return "resource header outside file";
    char actual[5];
    tag_read(actual, a->data + out->offset, a->little_endian);
    if (!tag_is(actual, out->tag)) return "resource tag does not match mmap";
    if (u32(a->data + out->offset + 4, a->little_endian) != out->size)
        return "resource size does not match mmap";
    /* The container record includes a sometimes absent final alignment byte. */
    if (id == 0 && out->offset == 0 && tag_is(out->tag, "RIFX")) return NULL;
    if (!fits(a->size, (size_t)out->offset + 8, out->size)) return "resource payload outside file";
    return NULL;
}

const char *sr_archive_open(SrArchive *out, const void *data, size_t size) {
    if (!out) return "null archive";
    *out = (SrArchive){0};
    if (!data || size < 32) return "truncated Director header";
    const uint8_t *b = data;
    int le;
    if (tag_is((const char *)b, "RIFX")) le = 0;
    else if (tag_is((const char *)b, "XFIR")) le = 1;
    else return "expected RIFX or XFIR";
    uint64_t declared = (uint64_t)u32(b + 4, le) + 8;
    if (declared != size && !(size % 2 && declared == (uint64_t)size + 1))
        return "container size mismatch (only one missing alignment byte allowed)";
    SrArchive a = {0};
    a.data = b; a.size = size; a.little_endian = le;
    tag_read(a.form, b + 8, le);
    if (!tag_is(a.form, "MV93") && !tag_is(a.form, "MC95"))
        return "unsupported Director form (expected MV93 or MC95)";
    char tag[5];
    tag_read(tag, b + 12, le);
    if (!tag_is(tag, "imap")) return "missing imap";
    uint32_t imap_size = u32(b + 16, le);
    if (imap_size < 12 || !fits(size, 20, imap_size)) return "invalid imap size";
    size_t map = u32(b + 24, le);
    a.version = u32(b + 28, le);
    if (!fits(size, map, 32)) return "truncated mmap";
    tag_read(tag, b + map, le);
    if (!tag_is(tag, "mmap")) return "missing mmap";
    size_t map_size = u32(b + map + 4, le);
    if (!fits(size, map + 8, map_size)) return "mmap outside file";
    uint16_t header = u16(b + map + 8, le);
    a.stride = u16(b + map + 10, le);
    uint32_t capacity = u32(b + map + 12, le);
    a.count = u32(b + map + 16, le);
    if (header < 24 || header > map_size || a.stride < 20 || a.count > capacity ||
        capacity > (map_size - header) / a.stride) return "invalid mmap dimensions";
    a.entries = map + 8 + header;
    for (uint32_t i = 0; i < a.count; ++i) {
        SrResource r;
        const char *error = sr_archive_resource(&a, i, &r);
        if (error) return error;
    }
    *out = a;
    return NULL;
}

const char *sr_text(const uint8_t *data, size_t size, const uint8_t **text, size_t *length) {
    if (!text || !length) return "null text output";
    *text = NULL; *length = 0;
    if (!data || size < 12) return "truncated STXT header";
    uint32_t offset = u32(data, 0), n = u32(data + 4, 0);
    if (offset != 12 || !fits(size, offset, n)) return "invalid STXT text range";
    *text = data + offset; *length = n;
    return NULL;
}
