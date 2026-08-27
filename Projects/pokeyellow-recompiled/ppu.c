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

// Forward declaration - defined later in this file
static void ppu_render_sprites(uint8_t *fb, uint8_t lcdc);

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

    // Composite sprites on top of background + window layers
    ppu_render_sprites(fb, lcdc);

    write_png_grayscale(path, 160, 144, fb);
    free(fb);
}

// ---- OAM sprite rendering --------------------------------------------------
//
// The Game Boy holds up to 40 sprites in OAM (0xFE00-0xFE9F). Each entry is
// 4 bytes: [Y_pos, X_pos, tile_index, attributes].
//
// Hardware rules we implement here:
//   - Sprites are always 8x8 (or 8x16 when LCDC bit 2 is set).
//   - Tile data always uses unsigned addressing from 0x8000 (unlike BG/Win
//     which can use 0x9000-based signed addressing).
//   - Color index 0 is transparent - those pixels don't overwrite the BG.
//   - OBP0 (0xFF48) is used when attribute bit 4 is clear; OBP1 (0xFF49)
//     when set. Color index 0 is always transparent in both palettes.
//   - Attribute bit 5: X-flip. Attribute bit 6: Y-flip.
//   - Attribute bit 7: BG priority. When set, sprite pixels with color
//     index 1-3 are only drawn where the BG color index is 0 (i.e. the
//     sprite goes *behind* non-transparent BG).
//   - Hardware X/Y positions are offset: Y is 16 above screen top, X is 8
//     left of screen left (so Y=16,X=8 puts a sprite at the top-left).
//   - On real hardware only 10 sprites are drawn per scanline (priority by
//     X then OAM order). We skip that limit since we're not scanline-
//     accurate - all 40 are composited at once.
//   - Z-ordering: lower OAM index = higher priority (drawn last so it wins).
//     We draw high-index sprites first, low-index last (painter's algorithm).

static void ppu_render_sprites(uint8_t *fb, uint8_t lcdc) {
    if (!(lcdc & 0x02)) return; // sprites disabled

    static const uint8_t shade[4] = {255, 170, 85, 0};
    uint8_t obp0 = wram_hram[0xFF48];
    uint8_t obp1 = wram_hram[0xFF49];
    int tall = (lcdc & 0x04) != 0; // 8x16 mode
    int sprite_h = tall ? 16 : 8;

    // Build a per-pixel "background is non-zero color" mask so we can
    // implement BG-priority correctly without re-decoding BG tiles.
    // We encode the original BG color index (0-3) per pixel in a
    // separate shadow buffer. We'll need to re-derive it from the fb
    // grayscale value using the current BGP - simpler to just track it
    // directly. We use a small stack buffer since this is called per-frame.
    // Actually, since we only need to know "is bg color index 0 or not"
    // we can derive it from the fb pixel value: shade[0]=255 means BG
    // color index was 0 (white/transparent for priority purposes).
    // This is correct for the standard DMG palette.

    // Iterate sprites in reverse OAM order so lower-index sprites are
    // painted last (win over higher-index in overlapping pixels).
    for (int s = 39; s >= 0; s--) {
        uint16_t oam_addr = 0xFE00 + s * 4;
        int sy = (int)wram_hram[oam_addr + 0] - 16;
        int sx = (int)wram_hram[oam_addr + 1] - 8;
        uint8_t tile_idx = wram_hram[oam_addr + 2];
        uint8_t attr = wram_hram[oam_addr + 3];

        if (tall) tile_idx &= 0xFE; // In 8x16 mode the LSB of tile index is ignored

        int x_flip = (attr & 0x20) != 0;
        int y_flip = (attr & 0x40) != 0;
        int bg_prio = (attr & 0x80) != 0;
        uint8_t pal = (attr & 0x10) ? obp1 : obp0;

        for (int row = 0; row < sprite_h; row++) {
            int screen_y = sy + row;
            if (screen_y < 0 || screen_y >= 144) continue;

            int tile_row = y_flip ? (sprite_h - 1 - row) : row;
            // In 8x16 mode the top tile is tile_idx, bottom tile is tile_idx+1
            uint8_t this_tile = (tile_row < 8) ? tile_idx : (tile_idx + 1);
            int fine_y = tile_row & 7;

            uint16_t tile_addr = 0x8000 + this_tile * 16;
            uint8_t b0 = wram_hram[tile_addr + fine_y * 2];
            uint8_t b1 = wram_hram[tile_addr + fine_y * 2 + 1];

            for (int col = 0; col < 8; col++) {
                int screen_x = sx + col;
                if (screen_x < 0 || screen_x >= 160) continue;

                int bit = x_flip ? col : (7 - col);
                int lo = (b0 >> bit) & 1;
                int hi = (b1 >> bit) & 1;
                int color_idx = (hi << 1) | lo;

                if (color_idx == 0) continue; // transparent

                // BG priority: if the attribute says "behind BG", only draw
                // this sprite pixel where the BG pixel is color index 0
                // (white = 255 with standard BGP 0xE4).
                if (bg_prio && fb[screen_y * 160 + screen_x] != 255) continue;

                int shade_idx = (pal >> (color_idx * 2)) & 3;
                fb[screen_y * 160 + screen_x] = shade[shade_idx];
            }
        }
    }
}