#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define FLAG_Z (1 << 7)
#define FLAG_N (1 << 6)
#define FLAG_H (1 << 5)
#define FLAG_C (1 << 4)

typedef struct {
    uint8_t a, b, c, d, e, h, l;
    uint8_t f;
    uint16_t sp;
    uint16_t pc;
    bool ime;
    bool halted;   // true while CPU is paused in a HALT, waiting for an interrupt
    uint64_t cycles;
} GB_Context;

void gb_write8(GB_Context *ctx, uint16_t addr, uint8_t val);
uint8_t gb_read8(GB_Context *ctx, uint16_t addr);
void fallback_interpreter(GB_Context *ctx, uint8_t opcode);
void record_pc(uint16_t pc);   // debug: ring-buffer of recent PCs
void dump_pc_history(void);    // debug: print that ring buffer
void ppu_render_frame(const char *path);   // renders current VRAM BG+Window+Sprites to a PNG

#endif