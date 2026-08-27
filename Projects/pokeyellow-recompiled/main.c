#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

uint8_t rom[0x100000]; // 1MB ROM Buffer (this cart is an 8Mbit/1MB MBC5 image)
uint8_t wram_hram[0x10000];

// ---- MBC5 mapper state -----------------------------------------------------
// This cart is MBC5+RAM+BATT: ROM is banked into 0x4000-0x7FFF (16KB banks,
// bank 0 fixed at 0x0000-0x3FFF), and external RAM is banked into 0xA000-0xBFFF
// (8KB banks). Unlike MBC1, MBC5 allows bank 0 to be selected in the switchable
// window - no special-casing needed there.
static uint8_t mbc5_rom_bank_lo = 1;   // 0x2000-0x2FFF write target (bits 0-7)
static uint8_t mbc5_rom_bank_hi = 0;   // 0x3000-0x3FFF write target (bit 8)
static uint8_t mbc5_ram_bank = 0;      // 0x4000-0x5FFF write target (0-0xF)
static bool mbc5_ram_enabled = false;  // 0x0000-0x1FFF write target (0x0A enables)
static uint8_t ext_ram[0x20000];       // 128KB max - covers every MBC5 RAM size

// ---- Timing / interrupts ---------------------------------------------------
// Real Game Boy: 4194304 Hz CPU clock, 154 scanlines/frame, 456 cycles/line.
#define CYCLES_PER_SCANLINE 456
#define CYCLES_PER_FRAME (456UL * 154UL) // 70224

// ---- Joypad ----------------------------------------------------------------
// The GB joypad register (0xFF00) is a multiplexed read/write port.
// The game writes bit 5 low to select action buttons (A,B,Select,Start) or
// bit 4 low to select the d-pad (Right,Left,Up,Down). It then reads bits 3-0
// which come back as 0=pressed, 1=not-pressed.
//
// We track two 4-bit nibbles of currently-held buttons, updated from stdin
// in non-blocking raw mode once per frame, and return them when the game
// reads 0xFF00 based on which group it selected.
//
// Key mapping:
//   Arrow keys   -> D-pad
//   Z            -> A button
//   X            -> B button
//   Enter        -> Start
//   Backspace    -> Select

static uint8_t joy_action = 0x0F;  // bits: Start|Select|B|A  (0=pressed)
static uint8_t joy_dpad   = 0x0F;  // bits: Down|Up|Left|Right (0=pressed)
static int joy_hold_frames = 0;    // how many more frames to hold current buttons

static struct termios term_orig;
static int term_raw = 0;

static void joypad_restore_terminal(void) {
    if (term_raw) {
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
        term_raw = 0;
    }
}

static void joypad_init(void) {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &term_orig);
    struct termios raw = term_orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    term_raw = 1;
    atexit(joypad_restore_terminal);
    printf("[JOY] Keyboard input active: Arrows=D-pad  Z=A  X=B  Enter=Start  Bksp=Select\n");
}

static void joypad_poll(void) {
    if (!term_raw) return;

    uint8_t prev_action = joy_action;
    uint8_t prev_dpad   = joy_dpad;

    // Only reset buttons if the hold timer has expired
    if (joy_hold_frames <= 0) {
        joy_action = 0x0F;
        joy_dpad   = 0x0F;
    } else {
        joy_hold_frames--;
    }

    uint8_t buf[16];
    int n;
    while ((n = (int)read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; ) {
            if (buf[i] == 0x1B && i + 2 < n && buf[i+1] == '[') {
                switch (buf[i+2]) {
                    case 'A': joy_dpad   &= ~0x04; break; // Up
                    case 'B': joy_dpad   &= ~0x08; break; // Down
                    case 'C': joy_dpad   &= ~0x01; break; // Right
                    case 'D': joy_dpad   &= ~0x02; break; // Left
                }
                i += 3;
            } else {
                switch (buf[i]) {
                    case 'z': case 'Z': joy_action &= ~0x01; break; // A
                    case 'x': case 'X': joy_action &= ~0x02; break; // B
                    case '\r': case '\n': joy_action &= ~0x08; break; // Start
                    case 127: case '\b': joy_action &= ~0x04; break;  // Select
                }
                i++;
            }
        }
    }

    // If any new button was pressed, hold it for 8 frames so the game's
    // debounce counter sees it consistently. Also fire a joypad interrupt.
    if (joy_action != prev_action || joy_dpad != prev_dpad) {
        joy_hold_frames = 8;
        wram_hram[0xFF0F] |= 0x10; // Joypad interrupt (bit 4 of IF)
    }
}

uint8_t joypad_read(uint8_t select_byte) {
    // select_byte is what the game wrote to 0xFF00.
    // Bit 5 low = action group selected; bit 4 low = dpad group selected.
    // Bits 3-0 of the return value are the button states (0=pressed).
    uint8_t result = 0xCF; // upper bits always 1, bits 3-0 default unpressed
    if (!(select_byte & 0x20)) result = (result & 0xF0) | (joy_action & 0x0F);
    if (!(select_byte & 0x10)) result = (result & 0xF0) | (joy_dpad   & 0x0F);
    return result;
}

void load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("Failed to open %s\n", path);
        return;
    }
    fread(rom, 1, sizeof(rom), f);
    fclose(f);
}

void gb_write8(GB_Context *ctx, uint16_t addr, uint8_t val) {
    // 0x0000-0x7FFF is ROM - the cartridge maps writes here to the MBC5's
    // registers rather than storing them; the ROM contents themselves are
    // never writable.
    if (addr < 0x2000) { mbc5_ram_enabled = ((val & 0x0F) == 0x0A); return; }
    if (addr < 0x3000) { if (getenv("GB_TRACE")) printf("[MBC5] ROM bank lo <- 0x%02X\n", val); mbc5_rom_bank_lo = val; return; }
    if (addr < 0x4000) { mbc5_rom_bank_hi = val & 0x01; return; }
    if (addr < 0x6000) { mbc5_ram_bank = val & 0x0F; return; }
    if (addr < 0x8000) { return; } // unused on MBC5

    if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc5_ram_enabled) {
            uint32_t off = (uint32_t)mbc5_ram_bank * 0x2000 + (addr - 0xA000);
            if (off < sizeof(ext_ram)) ext_ram[off] = val;
        }
        return;
    }

    wram_hram[addr] = val;
    if (addr == 0xFF00) {
        // Game wrote the joypad select byte — store it so joypad_read can
        // use it on the next read. The game typically writes then immediately
        // reads 0xFF00, so we just keep the last-written value here.
        wram_hram[0xFF00] = val;
        return;
    }
    if (addr >= 0xFF00 && getenv("GB_TRACE")) {
        printf("[IO WRITE] 0x%04X <- 0x%02X\n", addr, val);
    }
}

uint8_t gb_read8(GB_Context *ctx, uint16_t addr) {
    if (addr < 0x4000) {
        return rom[addr]; // Bank 0 - always fixed
    }
    if (addr < 0x8000) {
        uint32_t bank = ((uint32_t)mbc5_rom_bank_hi << 8) | mbc5_rom_bank_lo;
        uint32_t off = bank * 0x4000 + (addr - 0x4000);
        return (off < sizeof(rom)) ? rom[off] : 0xFF;
    }
    if (addr == 0xFF00) {
        return joypad_read(wram_hram[0xFF00]);
    }
    // LY (current scanline, 0-153): derived from real elapsed cycles rather
    // than just incrementing on every read, so it's actually tied to time -
    // one frame is 70224 cycles / 154 lines = 456 cycles/line.
    if (addr == 0xFF44) {
        return (ctx->cycles % CYCLES_PER_FRAME) / CYCLES_PER_SCANLINE;
    }
    if (addr >= 0xA000 && addr < 0xC000) {
        if (!mbc5_ram_enabled) return 0xFF;
        uint32_t off = (uint32_t)mbc5_ram_bank * 0x2000 + (addr - 0xA000);
        return (off < sizeof(ext_ram)) ? ext_ram[off] : 0xFF;
    }
    return wram_hram[addr];
}

// fallback_interpreter is now a real software CPU core - see interpreter.c
void step_recompiled(GB_Context *ctx);

int main() {
    load_rom("pokemon_yellow.gb");

    GB_Context ctx = {0};
    ctx.pc = 0x0100;
    // The real Game Boy boot ROM runs "LD SP, $FFFE" before handing control
    // to the cartridge at 0x0100. We skip the boot ROM, so we have to set
    // this up ourselves - otherwise SP starts at 0x0000 and the first CALL's
    // push wraps around onto the Interrupt Enable register at 0xFFFF,
    // corrupting return addresses the moment the game touches IE.
    ctx.sp = 0xFFFE;

    printf("Starting Main Recompiled Execution Loop...\n");

    joypad_init();

    int trap_dumped = 0;
    uint64_t next_vblank_cycles = CYCLES_PER_FRAME;

    long max_steps = 500000;
    if (getenv("GB_STEPS")) max_steps = strtol(getenv("GB_STEPS"), NULL, 0);
    long heartbeat = max_steps / 20 > 0 ? max_steps / 20 : 1;
    int trace = getenv("GB_TRACE") != NULL;

    const char *screenshot_dir = getenv("GB_SCREENSHOT_DIR");
    if (!screenshot_dir) screenshot_dir = "screenshots";
    if (getenv("GB_SCREENSHOT_SAMPLES") || getenv("GB_SCREENSHOT_EVERY_FRAME")) {
        char mkdir_cmd[300];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", screenshot_dir);
        system(mkdir_cmd);
    }

    for (long i = 0; i < max_steps; i++) {
        // Request VBlank once per real frame's worth of cycles. Requesting
        // (setting the IF bit) happens regardless of IME - only *servicing*
        // an interrupt requires IME to be set.
        if (ctx.cycles >= next_vblank_cycles) {
            wram_hram[0xFF0F] |= 0x01;
            next_vblank_cycles += CYCLES_PER_FRAME;
            // Poll joypad: holds buttons for 8 frames, fires joypad interrupt
            // on new press so the game's debounce logic sees it reliably.
            joypad_poll();
            static int frame_no = 0;
            frame_no++;
            if (getenv("GB_SCREENSHOT_EVERY_FRAME")) {
                char path[300];
                snprintf(path, sizeof(path), "%s/frame.png", screenshot_dir);
                ppu_render_frame(path);
            }
            if (getenv("GB_SCREENSHOT_SAMPLES") && frame_no % 50 == 0) {
                char path[300];
                snprintf(path, sizeof(path), "%s/sample_%05d.png", screenshot_dir, frame_no);
                ppu_render_frame(path);
                int nz_td = 0, nz_m0 = 0, nz_m1 = 0;
                for (int k = 0x8000; k < 0x9800; k++) if (wram_hram[k]) nz_td++;
                for (int k = 0x9800; k < 0x9C00; k++) if (wram_hram[k]) nz_m0++;
                for (int k = 0x9C00; k < 0xA000; k++) if (wram_hram[k]) nz_m1++;
                printf("[PPU] frame=%d BGP=0x%02X LCDC=0x%02X WY=%d SCX=%d SCY=%d td=%d m0=%d m1=%d -> %s\n",
                       frame_no, wram_hram[0xFF47], wram_hram[0xFF40], wram_hram[0xFF4A],
                       wram_hram[0xFF43], wram_hram[0xFF42], nz_td, nz_m0, nz_m1, path);
            }
        }

        // Service the highest-priority pending, enabled interrupt (bit 0 =
        // VBlank ... bit 4 = Joypad, in priority order). A HALT wakes up as
        // soon as any enabled interrupt is pending, even before it's
        // serviced; servicing itself still requires IME.
        uint8_t pending = wram_hram[0xFFFF] & wram_hram[0xFF0F] & 0x1F;
        if (pending) {
            if (ctx.halted) ctx.halted = false;
            if (ctx.ime) {
                int bit = 0;
                while (!(pending & (1 << bit))) bit++;
                uint16_t vector = 0x0040 + bit * 8; // 40,48,50,58,60
                if (trace) printf("[INTERRUPT] step=%ld vector=0x%04X (was at 0x%04X)\n", i, vector, ctx.pc);
                ctx.ime = false;
                wram_hram[0xFF0F] &= ~(1 << bit);
                gb_write8(&ctx, --ctx.sp, ctx.pc >> 8);
                gb_write8(&ctx, --ctx.sp, ctx.pc & 0xFF);
                ctx.pc = vector;
                ctx.cycles += 20;
            }
        }

        if (trace) {
            printf("[TRACE] step=%ld PC=0x%04X SP=0x%04X A=0x%02X F=0x%02X IME=%d HALT=%d\n", i, ctx.pc, ctx.sp, ctx.a, ctx.f, ctx.ime, ctx.halted);
        } else if (i % heartbeat == 0) {
            printf("[..] step=%ld/%ld PC=0x%04X SP=0x%04X cycles=%lu\n", i, max_steps, ctx.pc, ctx.sp, ctx.cycles);
        }

        if (ctx.halted) {
            // CPU is idle, just let time (and the frame/interrupt check
            // above) advance until something wakes it.
            ctx.cycles += 4;
            continue;
        }

        record_pc(ctx.pc);
        if (getenv("GB_DUMP_HISTORY") && ctx.pc == 0x0038 && !trap_dumped) {
            dump_pc_history();
            trap_dumped = 1;
        }
        if (getenv("GB_DUMP_HRAM") && ctx.pc == 0xFF80) {
            printf("[HRAM@0xFF80] ");
            for (int k = 0; k < 16; k++) printf("%02X ", wram_hram[0xFF80 + k]);
            printf("\n");
        }
        if (getenv("GB_TRACE_REGION") && ctx.pc >= 0x584E && ctx.pc <= 0x5B70) {
            printf("[REGION] PC=%04X A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X F=%02X SP=%04X\n",
                   ctx.pc, ctx.a, ctx.b, ctx.c, ctx.d, ctx.e, ctx.h, ctx.l, ctx.f, ctx.sp);
        }

        // The AOT-compiled blocks in recompiled_game.c were generated by
        // statically disassembling the ROM file as one flat blob, with no
        // concept of memory mapping. Two distinct hazards fall out of that:
        //   1. Any block address in the switchable ROM window
        //      (0x4000-0x7FFF) was compiled from whatever bank occupies
        //      that file offset - i.e. bank 1 - and is wrong whenever a
        //      different bank is actually selected.
        //   2. Any block address >= 0x8000 was compiled by treating that
        //      address as a literal ROM file offset, even though
        //      0x8000-0xFFFF is VRAM/WRAM/OAM/IO/HRAM and was never ROM at
        //      all (this is how a CALL to a HRAM-resident routine, like the
        //      classic copy-into-HRAM OAM DMA routine, ended up with a
        //      fabricated "block" of unrelated garbage bytes misread from
        //      deep inside the ROM file instead of the real HRAM content).
        // Bank 0 (< 0x4000) is the only region always safe for AOT dispatch;
        // everywhere else we bypass it and go straight to the interpreter,
        // which reads memory through the real, mapping-aware
        // gb_read8/gb_write8.
        int current_rom_bank = ((int)mbc5_rom_bank_hi << 8) | mbc5_rom_bank_lo;
        bool aot_unsafe = (ctx.pc >= 0x8000) ||
                           (ctx.pc >= 0x4000 && current_rom_bank != 1);
        if (aot_unsafe) {
            fallback_interpreter(&ctx, gb_read8(&ctx, ctx.pc));
        } else {
            step_recompiled(&ctx);
        }
    }

    printf("\nCPU State post-execution:\n");
    printf("PC: 0x%04X\n", ctx.pc);
    printf("A:  0x%02X | F: 0x%02X (Z:%d N:%d H:%d C:%d)\n", 
            ctx.a, ctx.f, 
            !!(ctx.f & FLAG_Z), !!(ctx.f & FLAG_N), 
            !!(ctx.f & FLAG_H), !!(ctx.f & FLAG_C));
    printf("BC: 0x%02X%02X | DE: 0x%02X%02X | HL: 0x%02X%02X\n", 
            ctx.b, ctx.c, ctx.d, ctx.e, ctx.h, ctx.l);
    printf("SP: 0x%04X\n", ctx.sp);
    printf("Cycles elapsed: %lu\n", ctx.cycles);

    {
        char final_path[300];
        system("mkdir -p \"screenshots\"");
        const char *dir = getenv("GB_SCREENSHOT_DIR");
        if (!dir) dir = "screenshots";
        snprintf(final_path, sizeof(final_path), "%s/frame.png", dir);
        ppu_render_frame(final_path);
        printf("Saved current VRAM background layer to %s\n", final_path);
    }
    printf("[PPU DEBUG] LCDC=0x%02X BGP=0x%02X SCX=%d SCY=%d\n",
           wram_hram[0xFF40], wram_hram[0xFF47], wram_hram[0xFF43], wram_hram[0xFF42]);
    {
        int nonzero_tiledata = 0, nonzero_map0 = 0, nonzero_map1 = 0;
        for (int i = 0x8000; i < 0x9800; i++) if (wram_hram[i]) nonzero_tiledata++;
        for (int i = 0x9800; i < 0x9C00; i++) if (wram_hram[i]) nonzero_map0++;
        for (int i = 0x9C00; i < 0xA000; i++) if (wram_hram[i]) nonzero_map1++;
        printf("[PPU DEBUG] nonzero tile-data bytes=%d, map@9800=%d, map@9C00=%d, WX=%d WY=%d\n",
               nonzero_tiledata, nonzero_map0, nonzero_map1, wram_hram[0xFF4B], wram_hram[0xFF4A]);
    }

    return 0;
}