// sdl_main.c
//
// SDL2 frontend for the Pokemon Yellow recompiler.
// Replaces main.c: provides a real window, 60fps rendering, and keyboard input.
//
// Build (Linux/Mac):
//   gcc -O2 -Wall -o gbrecomp sdl_main.c interpreter.c ppu.c recompiled_game.c \
//       $(sdl2-config --cflags --libs)
//
// Build (Windows with MinGW):
//   x86_64-w64-mingw32-gcc -O2 -Wall -o gbrecomp.exe sdl_main.c interpreter.c \
//       ppu.c recompiled_game.c -lSDL2 -lSDL2main
//
// Key map:
//   Arrow keys  = D-pad        Z = A button    X = B button
//   Enter       = Start        Backspace       = Select
//   Escape      = Quit         F1              = Save screenshot

#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

// ---- Memory ----------------------------------------------------------------

uint8_t rom[0x100000];
uint8_t wram_hram[0x10000];

// ---- MBC5 mapper state -----------------------------------------------------

static uint8_t mbc5_rom_bank_lo = 1;
static uint8_t mbc5_rom_bank_hi = 0;
static uint8_t mbc5_ram_bank    = 0;
static int     mbc5_ram_enabled = 0;
static uint8_t ext_ram[0x20000];

// ---- Timing ----------------------------------------------------------------

#define CYCLES_PER_SCANLINE 456UL
#define CYCLES_PER_FRAME   (456UL * 154UL)  // 70224

// ---- Joypad ----------------------------------------------------------------
// joy_action bits: 3=Start 2=Select 1=B 0=A   (0=pressed)
// joy_dpad   bits: 3=Down  2=Up     1=Left 0=Right

static uint8_t joy_action = 0x0F;
static uint8_t joy_dpad   = 0x0F;

static void handle_key(SDL_Keycode key, int pressed) {
    uint8_t bit_a = 0, bit_d = 0;
    switch (key) {
        case SDLK_z:         bit_a = 0x01; break; // A
        case SDLK_x:         bit_a = 0x02; break; // B
        case SDLK_BACKSPACE: bit_a = 0x04; break; // Select
        case SDLK_RETURN:    bit_a = 0x08; break; // Start
        case SDLK_RIGHT:     bit_d = 0x01; break;
        case SDLK_LEFT:      bit_d = 0x02; break;
        case SDLK_UP:        bit_d = 0x04; break;
        case SDLK_DOWN:      bit_d = 0x08; break;
        default: return;
    }
    if (pressed) {
        if (bit_a) { joy_action &= ~bit_a; wram_hram[0xFF0F] |= 0x10; }
        if (bit_d) { joy_dpad   &= ~bit_d; wram_hram[0xFF0F] |= 0x10; }
    } else {
        if (bit_a) joy_action |= bit_a;
        if (bit_d) joy_dpad   |= bit_d;
    }
}

uint8_t joypad_read(uint8_t sel) {
    uint8_t r = 0xFF;
    if (!(sel & 0x20)) r &= 0xF0 | (joy_action & 0x0F);
    if (!(sel & 0x10)) r &= 0xF0 | (joy_dpad   & 0x0F);
    return r;
}

// ---- ROM loading -----------------------------------------------------------

static char save_path[512];

static void load_save(void) {
    FILE *f = fopen(save_path, "rb");
    if (!f) return;
    fread(ext_ram, 1, sizeof(ext_ram), f);
    fclose(f);
    printf("[SAV] Loaded save from %s\n", save_path);
}

static void write_save(void) {
    FILE *f = fopen(save_path, "wb");
    if (!f) { fprintf(stderr, "[SAV] Failed to write %s\n", save_path); return; }
    fwrite(ext_ram, 1, sizeof(ext_ram), f);
    fclose(f);
    printf("[SAV] Saved to %s\n", save_path);
}

static void load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); exit(1); }
    fread(rom, 1, sizeof(rom), f);
    fclose(f);

    // Derive save path: replace .gb extension with .sav
    strncpy(save_path, path, sizeof(save_path) - 5);
    save_path[sizeof(save_path) - 5] = '\0';
    char *dot = strrchr(save_path, '.');
    if (dot) *dot = '\0';
    strcat(save_path, ".sav");
    load_save();
}

// ---- Memory bus ------------------------------------------------------------

void gb_write8(GB_Context *ctx, uint16_t addr, uint8_t val) {
    if (addr < 0x2000) { mbc5_ram_enabled = ((val & 0x0F) == 0x0A); return; }
    if (addr < 0x3000) { mbc5_rom_bank_lo = val; return; }
    if (addr < 0x4000) { mbc5_rom_bank_hi = val & 0x01; return; }
    if (addr < 0x6000) { mbc5_ram_bank    = val & 0x0F; return; }
    if (addr < 0x8000) { return; }
    if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc5_ram_enabled) {
            uint32_t off = (uint32_t)mbc5_ram_bank * 0x2000 + (addr - 0xA000);
            if (off < sizeof(ext_ram)) ext_ram[off] = val;
        }
        return;
    }
    wram_hram[addr] = val;
    // OAM DMA: writing to 0xFF46 copies 160 bytes from (val<<8) into OAM.
    if (addr == 0xFF46) {
        uint16_t src = (uint16_t)val << 8;
        for (int i = 0; i < 0xA0; i++) {
            wram_hram[0xFE00 + i] = gb_read8(ctx, src + i);
        }
    }
}

uint8_t gb_read8(GB_Context *ctx, uint16_t addr) {
    if (addr < 0x4000) return rom[addr];
    if (addr < 0x8000) {
        uint32_t bank = ((uint32_t)mbc5_rom_bank_hi << 8) | mbc5_rom_bank_lo;
        uint32_t off  = bank * 0x4000 + (addr - 0x4000);
        return (off < sizeof(rom)) ? rom[off] : 0xFF;
    }
    if (addr == 0xFF00) return joypad_read(wram_hram[0xFF00]);
    if (addr == 0xFF44) return (uint8_t)((ctx->cycles % CYCLES_PER_FRAME) / CYCLES_PER_SCANLINE);
    if (addr >= 0xA000 && addr < 0xC000) {
        if (!mbc5_ram_enabled) return 0xFF;
        uint32_t off = (uint32_t)mbc5_ram_bank * 0x2000 + (addr - 0xA000);
        return (off < sizeof(ext_ram)) ? ext_ram[off] : 0xFF;
    }
    return wram_hram[addr];
}

// ---- Fallback interpreter / AOT dispatch -----------------------------------

void record_pc(uint16_t pc) { (void)pc; }
void dump_pc_history(void)   {}

void fallback_interpreter(GB_Context *ctx, uint8_t opcode);
void step_recompiled(GB_Context *ctx);

static void step(GB_Context *ctx) {
    int bank = ((int)mbc5_rom_bank_hi << 8) | mbc5_rom_bank_lo;
    int aot_unsafe = (ctx->pc >= 0x8000) || (ctx->pc >= 0x4000 && bank != 1);
    if (aot_unsafe)
        fallback_interpreter(ctx, gb_read8(ctx, ctx->pc));
    else
        step_recompiled(ctx);
}

// ---- PPU (render to SDL texture) -------------------------------------------

extern void ppu_render_frame(const char *path); // keep PNG version for F1

static void render_to_texture(SDL_Texture *tex) {
    uint8_t lcdc = wram_hram[0xFF40];
    uint8_t scy  = wram_hram[0xFF42];
    uint8_t scx  = wram_hram[0xFF43];
    uint8_t bgp  = wram_hram[0xFF47];
    uint8_t obp0 = wram_hram[0xFF48];
    uint8_t obp1 = wram_hram[0xFF49];

    static const uint8_t shade_r[4] = {224, 136, 52,  8};
    static const uint8_t shade_g[4] = {248, 192, 104, 24};
    static const uint8_t shade_b[4] = {208, 112, 56,  32};

    uint32_t fb[160 * 144];
    uint8_t  bg_idx[160 * 144]; // raw BG color index (0-3) per pixel for sprite priority
    memset(fb, 0xFF, sizeof(fb));
    memset(bg_idx, 0, sizeof(bg_idx));

    // Background + Window
    if ((lcdc & 0x80) && (lcdc & 0x01)) {
        uint16_t map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
        int unsgn = (lcdc & 0x10) != 0;
        uint16_t tbase = unsgn ? 0x8000 : 0x9000;

        for (int y = 0; y < 144; y++) {
            int by = (y + scy) & 0xFF;
            for (int x = 0; x < 160; x++) {
                int bx = (x + scx) & 0xFF;
                uint16_t ma = map_base + (by/8)*32 + (bx/8);
                uint8_t ti = wram_hram[ma];
                uint16_t ta = unsgn ? (uint16_t)(tbase + ti*16)
                                    : (uint16_t)(tbase + (int8_t)ti*16);
                uint8_t b0 = wram_hram[ta + (by%8)*2];
                uint8_t b1 = wram_hram[ta + (by%8)*2 + 1];
                int bit = 7 - (bx % 8);
                int ci = (((b1>>bit)&1)<<1)|((b0>>bit)&1);
                int si = (bgp >> (ci*2)) & 3;
                bg_idx[y*160+x] = ci;
                fb[y*160+x] = (0xFF000000u)|(shade_r[si]<<16)|(shade_g[si]<<8)|shade_b[si];
            }
        }

        // Window layer
        if (lcdc & 0x20) {
            uint8_t wx = wram_hram[0xFF4B], wy = wram_hram[0xFF4A];
            uint16_t wmap = (lcdc & 0x40) ? 0x9C00 : 0x9800;
            int wx0 = (int)wx - 7;
            for (int y = wy; y < 144; y++) {
                int wy2 = y - wy;
                for (int x = wx0; x < 160; x++) {
                    if (x < 0) continue;
                    int wx2 = x - wx0;
                    uint16_t ma = wmap + (wy2/8)*32 + (wx2/8);
                    uint8_t ti = wram_hram[ma];
                    uint16_t ta = unsgn ? (uint16_t)(tbase + ti*16)
                                        : (uint16_t)(tbase + (int8_t)ti*16);
                    uint8_t b0 = wram_hram[ta + (wy2%8)*2];
                    uint8_t b1 = wram_hram[ta + (wy2%8)*2 + 1];
                    int bit = 7 - (wx2 % 8);
                    int ci = (((b1>>bit)&1)<<1)|((b0>>bit)&1);
                    int si = (bgp >> (ci*2)) & 3;
                    bg_idx[y*160+x] = ci;
                    fb[y*160+x] = (0xFF000000u)|(shade_r[si]<<16)|(shade_g[si]<<8)|shade_b[si];
                }
            }
        }
    }

    // Sprites
    if (lcdc & 0x02) {
        int tall = (lcdc & 0x04) != 0;
        int sh   = tall ? 16 : 8;
        for (int s = 39; s >= 0; s--) {
            uint16_t oa = 0xFE00 + s*4;
            int sy   = (int)wram_hram[oa+0] - 16;
            int sx   = (int)wram_hram[oa+1] - 8;
            uint8_t ti   = wram_hram[oa+2] & (tall ? 0xFE : 0xFF);
            uint8_t attr = wram_hram[oa+3];
            int xf = (attr & 0x20) != 0;
            int yf = (attr & 0x40) != 0;
            int bp = (attr & 0x80) != 0;
            uint8_t pal = (attr & 0x10) ? obp1 : obp0;
            for (int row = 0; row < sh; row++) {
                int py = sy + row;
                if (py < 0 || py >= 144) continue;
                int tr = yf ? (sh-1-row) : row;
                uint8_t tuse = (tr < 8) ? ti : ti+1;
                int fy = tr & 7;
                uint16_t ta = 0x8000 + tuse*16;
                uint8_t b0 = wram_hram[ta+fy*2], b1 = wram_hram[ta+fy*2+1];
                for (int col = 0; col < 8; col++) {
                    int px = sx + col;
                    if (px < 0 || px >= 160) continue;
                    int bit = xf ? col : (7-col);
                    int ci = (((b1>>bit)&1)<<1)|((b0>>bit)&1);
                    if (ci == 0) continue;
                    // BG priority: sprite is behind BG where BG color index != 0
                    if (bp && bg_idx[py*160+px] != 0) continue;
                    int si = (pal >> (ci*2)) & 3;
                    fb[py*160+px] = (0xFF000000u)|(shade_r[si]<<16)|(shade_g[si]<<8)|shade_b[si];
                }
            }
        }
    }

    SDL_UpdateTexture(tex, NULL, fb, 160 * sizeof(uint32_t));
}

// ---- main ------------------------------------------------------------------

int main(int argc, char **argv) {
    const char *rom_path = "pokemon_yellow.gb";
    if (argc > 1) rom_path = argv[1];
    load_rom(rom_path);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Scale the 160x144 screen up to 3x (480x432)
    int scale = 3;
    SDL_Window *win = SDL_CreateWindow(
        "Pokemon Yellow - Recompiled",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        160*scale, 144*scale, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, 160, 144);
    SDL_Texture *tex = SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    // Show a "Loading..." screen
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);

    GB_Context ctx = {0};
    ctx.pc = 0x0100;
    ctx.sp = 0xFFFE;

    uint64_t next_vblank = CYCLES_PER_FRAME;
    int running = 1;
    int screenshot_no = 0;
    Uint32 last_save_ticks = SDL_GetTicks();

    // Steps per SDL frame: tune so emulation runs at roughly real GB speed.
    // Real GB: 4194304 Hz / 60fps = ~69905 cycles/frame.
    // Each step is roughly 4-8 cycles on average. 12000 steps/SDL-frame
    // is a comfortable starting point (runs fast but not blurry).
    const int STEPS_PER_RENDER = 16000;

    printf("Pokemon Yellow Recompiled\n");
    printf("Controls: Arrows=Dpad  Z=A  X=B  Enter=Start  Backspace=Select  F1=Screenshot  Escape=Quit\n");

    while (running) {
        // Handle SDL events
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = 0; break; }
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
                if (ev.key.keysym.sym == SDLK_F1) {
                    char path[64];
                    snprintf(path, sizeof(path), "screenshot_%03d.png", ++screenshot_no);
                    ppu_render_frame(path);
                    printf("Saved %s\n", path);
                }
                handle_key(ev.key.keysym.sym, 1);
            }
            if (ev.type == SDL_KEYUP) handle_key(ev.key.keysym.sym, 0);
        }

        // Run emulation for a batch of steps
        for (int i = 0; i < STEPS_PER_RENDER && running; i++) {
            // VBlank trigger
            if (ctx.cycles >= next_vblank) {
                wram_hram[0xFF0F] |= 0x01;
                next_vblank += CYCLES_PER_FRAME;
            }

            // Service interrupts
            uint8_t pending = wram_hram[0xFFFF] & wram_hram[0xFF0F] & 0x1F;
            if (pending) {
                if (ctx.halted) ctx.halted = 0;
                if (ctx.ime) {
                    int bit = 0;
                    while (!(pending & (1 << bit))) bit++;
                    uint16_t vec = 0x0040 + bit * 8;
                    ctx.ime = 0;
                    wram_hram[0xFF0F] &= ~(1 << bit);
                    gb_write8(&ctx, --ctx.sp, ctx.pc >> 8);
                    gb_write8(&ctx, --ctx.sp, ctx.pc & 0xFF);
                    ctx.pc = vec;
                    ctx.cycles += 20;
                }
            }

            if (ctx.halted) { ctx.cycles += 4; continue; }
            step(&ctx);
        }

        // Auto-save every 60 seconds of real time
        Uint32 now = SDL_GetTicks();
        if (now - last_save_ticks > 60000) {
            write_save();
            last_save_ticks = now;
        }

        // Render to window
        render_to_texture(tex);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    write_save();  // save on clean exit
    return 0;
}