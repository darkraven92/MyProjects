// ppu.c
//
// A minimal Game Boy PPU - background layer only, no sprites, no window,
// no scanline-accurate timing. It just reads whatever's currently in VRAM
// (tile data + tile map) and the BGP palette, and rasterizes a single
// 160x144 frame straight to a PNG. This exists to answer one question
// cheaply: "is the CPU core actually producing correct pixel data?" before
// investing in a real scanline-timed PPU.
//
// Includes a tiny standalone PNG encoder (8-bit grayscale, uncompressed
// "stored" deflate blocks) so this has no external library dependency -
// it only needs libc.

#include "runtime.h"
#include <string.h>
#include <stdlib.h>

extern uint8_t wram_hram[0x10000]; // VRAM (0x8000-0x9FFF) lives in here

// ---- CRC32 / Adler32 (needed for PNG chunk + zlib stream checksums) -------

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void make_crc32_table(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[n] = c;
    }
    crc32_table_ready = 1;
}

static uint32_t crc32(const uint8_t *buf, size_t len) {
    if (!crc32_table_ready) make_crc32_table();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static uint32_t adler32(const uint8_t *buf, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + buf[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// ---- Minimal PNG writer ----------------------------------------------------

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t len_buf[4];
    put_be32(len_buf, len);
    fwrite(len_buf, 1, 4, f);

    uint8_t *crc_buf = malloc(4 + len);
    memcpy(crc_buf, type, 4);
    if (len) memcpy(crc_buf + 4, data, len);
    fwrite(crc_buf, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t crc = crc32(crc_buf, 4 + len);
    free(crc_buf);

    uint8_t crc_be[4];
    put_be32(crc_be, crc);
    fwrite(crc_be, 1, 4, f);
}

// Writes an 8-bit grayscale PNG. `pixels` is width*height bytes, one per
// pixel, row-major, top-to-bottom.
static void write_png_grayscale(const char *path, int width, int height, const uint8_t *pixels) {
    FILE *f = fopen(path, "wb");
    if (!f) { printf("[PPU] Failed to open %s for writing\n", path); return; }

    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13];
    put_be32(ihdr + 0, width);
    put_be32(ihdr + 4, height);
    ihdr[8] = 8;  // bit depth
    ihdr[9] = 0;  // color type: grayscale
    ihdr[10] = 0; // compression method
    ihdr[11] = 0; // filter method
    ihdr[12] = 0; // interlace method
    write_chunk(f, "IHDR", ihdr, sizeof(ihdr));

    // Build the raw (pre-deflate) scanline stream: one filter-type byte
    // (0 = None) followed by `width` pixel bytes, per row.
    size_t raw_len = (size_t)height * (1 + width);
    uint8_t *raw = malloc(raw_len);
    for (int y = 0; y < height; y++) {
        uint8_t *row = raw + (size_t)y * (1 + width);
        row[0] = 0; // filter type: None
        memcpy(row + 1, pixels + (size_t)y * width, width);
    }

    // zlib stream: 2-byte header, deflate data as "stored" (uncompressed)
    // blocks (max 65535 bytes each), then 4-byte big-endian Adler32.
    size_t max_idat = raw_len + 5 * ((raw_len / 65535) + 1) + 6;
    uint8_t *idat = malloc(max_idat);
    size_t p = 0;
    idat[p++] = 0x78; idat[p++] = 0x01; // zlib header: deflate, default window, no dict

    size_t off = 0;
    while (off < raw_len) {
        size_t chunk = raw_len - off;
        if (chunk > 65535) chunk = 65535;
        int is_final = (off + chunk >= raw_len);
        idat[p++] = is_final ? 1 : 0; // BFINAL + BTYPE=00 (stored), byte-aligned
        uint16_t len16 = (uint16_t)chunk;
        idat[p++] = len16 & 0xFF; idat[p++] = (len16 >> 8) & 0xFF;
        uint16_t nlen16 = (uint16_t)(~len16);
        idat[p++] = nlen16 & 0xFF; idat[p++] = (nlen16 >> 8) & 0xFF;
        memcpy(idat + p, raw + off, chunk);
        p += chunk;
        off += chunk;
    }

    uint32_t adler = adler32(raw, raw_len);
    uint8_t adler_be[4];
    put_be32(adler_be, adler);
    memcpy(idat + p, adler_be, 4);
    p += 4;

    write_chunk(f, "IDAT", idat, (uint32_t)p);
    write_chunk(f, "IEND", NULL, 0);

    free(raw);
    free(idat);
    fclose(f);
}

// ---- Background-layer rendering --------------------------------------------

void ppu_render_frame(const char *path) {
    uint8_t lcdc = wram_hram[0xFF40];
    uint8_t scy = wram_hram[0xFF42];
    uint8_t scx = wram_hram[0xFF43];
    uint8_t bgp = wram_hram[0xFF47];

    // DMG 4-shade grayscale palette (index 0 = lightest).
    static const uint8_t shade[4] = {255, 170, 85, 0};

    uint8_t *fb = malloc(160 * 144);

    if (!(lcdc & 0x80) || !(lcdc & 0x01)) {
        // LCD off, or background layer disabled: nothing to draw.
        memset(fb, 255, 160 * 144);
        write_png_grayscale(path, 160, 144, fb);
        free(fb);
        return;
    }

    uint16_t map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    int unsigned_addressing = (lcdc & 0x10) != 0;
    uint16_t tile_data_base = unsigned_addressing ? 0x8000 : 0x9000;

    for (int y = 0; y < 144; y++) {
        int bg_y = (y + scy) & 0xFF;
        int tile_row = bg_y / 8;
        int fine_y = bg_y % 8;
        for (int x = 0; x < 160; x++) {
            int bg_x = (x + scx) & 0xFF;
            int tile_col = bg_x / 8;
            int fine_x = bg_x % 8;

            uint16_t map_addr = map_base + tile_row * 32 + tile_col;
            uint8_t tile_idx = wram_hram[map_addr];

            uint16_t tile_addr = unsigned_addressing
                ? (uint16_t)(tile_data_base + tile_idx * 16)
                : (uint16_t)(tile_data_base + (int8_t)tile_idx * 16);

            uint8_t b0 = wram_hram[tile_addr + fine_y * 2];
            uint8_t b1 = wram_hram[tile_addr + fine_y * 2 + 1];
            int bit = 7 - fine_x;
            int lo = (b0 >> bit) & 1;
            int hi = (b1 >> bit) & 1;
            int color_idx = (hi << 1) | lo;
            int shade_idx = (bgp >> (color_idx * 2)) & 3;

            fb[y * 160 + x] = shade[shade_idx];
        }
    }

    // Window layer: a second, non-scrolling background drawn on top,
    // positioned by WX/WY (0xFF4B/0xFF4A - WX is offset by 7 on real
    // hardware). Very commonly used for full-screen text boxes and
    // overlays, so a lot of real content only shows up here.
    if (lcdc & 0x20) {
        uint8_t wx = wram_hram[0xFF4B];
        uint8_t wy = wram_hram[0xFF4A];
        int win_x0 = (int)wx - 7;
        uint16_t win_map_base = (lcdc & 0x40) ? 0x9C00 : 0x9800;

        for (int y = 0; y < 144; y++) {
            if (y < wy) continue;
            int win_y = y - wy;
            int tile_row = win_y / 8;
            int fine_y = win_y % 8;
            for (int x = 0; x < 160; x++) {
                if (x < win_x0) continue;
                int win_x = x - win_x0;
                int tile_col = win_x / 8;
                int fine_x = win_x % 8;

                uint16_t map_addr = win_map_base + tile_row * 32 + tile_col;
                uint8_t tile_idx = wram_hram[map_addr];

                uint16_t tile_addr = unsigned_addressing
                    ? (uint16_t)(tile_data_base + tile_idx * 16)
                    : (uint16_t)(tile_data_base + (int8_t)tile_idx * 16);

                uint8_t b0 = wram_hram[tile_addr + fine_y * 2];
                uint8_t b1 = wram_hram[tile_addr + fine_y * 2 + 1];
                int bit = 7 - fine_x;
                int lo = (b0 >> bit) & 1;
                int hi = (b1 >> bit) & 1;
                int color_idx = (hi << 1) | lo;
                int shade_idx = (bgp >> (color_idx * 2)) & 3;

                fb[y * 160 + x] = shade[shade_idx];
            }
        }
    }

    write_png_grayscale(path, 160, 144, fb);
    free(fb);
}