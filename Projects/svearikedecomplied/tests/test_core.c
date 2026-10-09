#include "director.h"
#include "trade.h"
#include <stdio.h>
#include <string.h>

/* Keep checks active in Release builds, unlike assert(). */
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
static void put32(uint8_t *p, uint32_t v, int le) {
    for (int i = 0; i < 4; ++i) p[le ? i : 3-i] = (uint8_t)(v >> (8*i));
}
static void put16(uint8_t *p, uint16_t v, int le) {
    p[le ? 0 : 1] = (uint8_t)v; p[le ? 1 : 0] = (uint8_t)(v >> 8);
}
static void puttag(uint8_t *p, const char *t, int le) {
    for (int i = 0; i < 4; ++i) p[le ? 3-i : i] = (uint8_t)t[i];
}
static void fixture(uint8_t b[128], int le) {
    memset(b, 0, 128);
    puttag(b, "RIFX", le); put32(b+4, 120, le); puttag(b+8, "MV93", le);
    puttag(b+12, "imap", le); put32(b+16, 24, le);
    put32(b+24, 44, le); put32(b+28, 1217, le);
    puttag(b+44, "mmap", le); put32(b+48, 44, le);
    put16(b+52, 24, le); put16(b+54, 20, le);
    put32(b+56, 1, le); put32(b+60, 1, le);
    puttag(b+76, "STXT", le); put32(b+80, 23, le); put32(b+84, 96, le);
    puttag(b+96, "STXT", le); put32(b+100, 23, le);
    put32(b+104, 12, 0); put32(b+108, 5, 0);
    memcpy(b+116, "Svea!", 5);
}
int main(void) {
    for (int le = 0; le < 2; ++le) {
        uint8_t b[128]; fixture(b, le);
        SrArchive a; SrResource r; const uint8_t *text; size_t length;
        CHECK(!sr_archive_open(&a, b, sizeof b));
        CHECK(a.little_endian == le && a.count == 1 && a.version == 1217);
        CHECK(!sr_archive_resource(&a, 0, &r));
        CHECK(!strcmp(r.tag, "STXT") && r.offset == 96 && r.size == 23);
        CHECK(!sr_text(b+r.offset+8, r.size, &text, &length));
        CHECK(length == 5 && !memcmp(text, "Svea!", 5));
        CHECK(sr_archive_resource(&a, 1, &r));
        CHECK(!sr_archive_open(&a, b, 127)); /* Only final padding missing. */
        for (size_t n = 0; n < 127; ++n) CHECK(sr_archive_open(&a, b, n));
        put32(b+60, UINT32_MAX, le); CHECK(sr_archive_open(&a, b, 128));
        fixture(b, le); put32(b+84, UINT32_MAX, le); CHECK(sr_archive_open(&a, b, 128));
        fixture(b, le); put16(b+54, 0, le); CHECK(sr_archive_open(&a, b, 128));
        fixture(b, le); b[96] = 'X'; CHECK(sr_archive_open(&a, b, 128));
        fixture(b, le); put32(b+108, UINT32_MAX, 0); CHECK(sr_text(b+104, 24, &text, &length));
        fixture(b, le); puttag(b+76, "free", le); put32(b+84, UINT32_MAX, le);
        CHECK(!sr_archive_open(&a, b, 128)); CHECK(!sr_archive_resource(&a, 0, &r) && !r.active);
    }
    /* Expected transitions, transcribed from the recovered branch structure. */
    const int expected[11][3] = {
        {1,2,2}, {1,3,3}, {2,4,4}, {3,5,0}, {4,6,0}, {5,7,0},
        {6,8,0}, {7,9,0}, {10,8,8}, {11,9,9}, {11,10,10}
    };
    for (int index = 1; index <= 11; ++index) {
        for (int trigger = 2; trigger <= 4; ++trigger)
            CHECK(sr_trade_price_step(index, trigger, 0) == index);
        for (int direction = 1; direction <= 3; ++direction)
            CHECK(sr_trade_price_step(index, 1, direction) == expected[index-1][direction-1]);
    }
    CHECK(!sr_trade_price_step(0, 1, 1));
    CHECK(!sr_trade_price_step(12, 1, 1));
    CHECK(!sr_trade_price_step(6, 0, 1));
    puts("Archive bounds, both byte orders, STXT, and trade transitions passed.");
    return 0;
}
