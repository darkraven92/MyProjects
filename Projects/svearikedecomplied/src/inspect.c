#include "director.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void json_tag(const char *s) {
    putchar('"');
    for (int i = 0; i < 4; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c > 126 || c == '"' || c == '\\') printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: %s MOVIE.DIR|CAST.CST\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return 1; }
    long length = ftell(f);
    if (length < 0 || (unsigned long)length > 512UL * 1024 * 1024 || fseek(f, 0, SEEK_SET)) {
        fprintf(stderr, "Invalid or excessive input size\n"); fclose(f); return 1;
    }
    uint8_t *data = malloc(length ? (size_t)length : 1);
    if (!data) { fclose(f); return 1; }
    size_t got = fread(data, 1, (size_t)length, f);
    fclose(f);
    if (got != (size_t)length) { free(data); return 1; }
    SrArchive a;
    const char *error = sr_archive_open(&a, data, got);
    if (error) { fprintf(stderr, "%s: %s\n", argv[1], error); free(data); return 1; }
    printf("{\"form\":\"%s\",\"endian\":\"%s\",\"bytes\":%zu,\"version\":%" PRIu32 ",\"resources\":[\n",
           a.form, a.little_endian ? "little" : "big", a.size, a.version);
    for (uint32_t i = 0; i < a.count; ++i) {
        SrResource r;
        error = sr_archive_resource(&a, i, &r);
        if (error) { free(data); return 1; }
        printf("%s{\"id\":%" PRIu32 ",\"tag\":", i ? ",\n" : "", i);
        json_tag(r.tag);
        printf(",\"offset\":%" PRIu32 ",\"size\":%" PRIu32 ",\"active\":%s}",
               r.offset, r.size, r.active ? "true" : "false");
    }
    puts("\n]}");
    free(data);
    return ferror(stdout) ? 1 : 0;
}
