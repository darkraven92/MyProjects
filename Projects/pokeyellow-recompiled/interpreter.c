// interpreter.c
//
#include <stdlib.h>
// A complete software interpreter for the Game Boy's LR35902/SM83 CPU.
//
// This exists as the *fallback* engine for the static recompiler: translator.py
// only emits native C for addresses it can discover through static control-flow
// analysis (straight-line JP/JR/CALL/RST with immediate targets). Anything it
// can't see ahead of time - dynamic jumps (JP (HL)), code in banked ROM/RAM,
// mid-block entry points, addresses reached only via a popped return address,
// etc. - has no compiled block, and step_recompiled() falls through to
// fallback_interpreter(). That function used to be a no-op stub that printed
// a message and returned without ever moving PC, which meant *any* miss was a
// permanent hang. This file makes fallback_interpreter() actually execute the
// instruction at ctx->pc, so execution always makes forward progress: the
// recompiled blocks are a fast path, and this interpreter is the ground truth
// that can run 100% of the instruction set on its own.
//
// Register field encoding follows the real LR35902 encoding (matches
// translator.py's REG_MAP): 0=B 1=C 2=D 3=E 4=H 5=L 6=(HL) 7=A.

#include "runtime.h"

static int history_dumped = 0;

// ---- 16-bit register pair helpers -----------------------------------------

static inline uint16_t get_bc(GB_Context *ctx) { return ((uint16_t)ctx->b << 8) | ctx->c; }
static inline uint16_t get_de(GB_Context *ctx) { return ((uint16_t)ctx->d << 8) | ctx->e; }
static inline uint16_t get_hl(GB_Context *ctx) { return ((uint16_t)ctx->h << 8) | ctx->l; }
static inline uint16_t get_af(GB_Context *ctx) { return ((uint16_t)ctx->a << 8) | (ctx->f & 0xF0); }

static inline void set_bc(GB_Context *ctx, uint16_t v) { ctx->b = v >> 8; ctx->c = v & 0xFF; }
static inline void set_de(GB_Context *ctx, uint16_t v) { ctx->d = v >> 8; ctx->e = v & 0xFF; }
static inline void set_hl(GB_Context *ctx, uint16_t v) { ctx->h = v >> 8; ctx->l = v & 0xFF; }
static inline void set_af(GB_Context *ctx, uint16_t v) { ctx->a = v >> 8; ctx->f = v & 0xF0; }

static inline uint16_t gb_read16(GB_Context *ctx, uint16_t addr) {
    return gb_read8(ctx, addr) | ((uint16_t)gb_read8(ctx, addr + 1) << 8);
}

static inline void push16(GB_Context *ctx, uint16_t v) {
    gb_write8(ctx, --ctx->sp, v >> 8);
    gb_write8(ctx, --ctx->sp, v & 0xFF);
}

static inline uint16_t pop16(GB_Context *ctx) {
    uint8_t lo = gb_read8(ctx, ctx->sp++);
    uint8_t hi = gb_read8(ctx, ctx->sp++);
    return ((uint16_t)hi << 8) | lo;
}

static inline int8_t sign_extend(uint8_t v) { return (int8_t)v; }

static inline void set_flag(GB_Context *ctx, uint8_t mask, int cond) {
    if (cond) ctx->f |= mask; else ctx->f &= ~mask;
}

// ---- 8-bit register field read/write (3-bit r operand, HW-standard order) -

static uint8_t read_r8(GB_Context *ctx, int idx) {
    switch (idx) {
        case 0: return ctx->b;
        case 1: return ctx->c;
        case 2: return ctx->d;
        case 3: return ctx->e;
        case 4: return ctx->h;
        case 5: return ctx->l;
        case 6: return gb_read8(ctx, get_hl(ctx));
        case 7: return ctx->a;
    }
    return 0; // unreachable
}

static void write_r8(GB_Context *ctx, int idx, uint8_t val) {
    switch (idx) {
        case 0: ctx->b = val; return;
        case 1: ctx->c = val; return;
        case 2: ctx->d = val; return;
        case 3: ctx->e = val; return;
        case 4: ctx->h = val; return;
        case 5: ctx->l = val; return;
        case 6: gb_write8(ctx, get_hl(ctx), val); return;
        case 7: ctx->a = val; return;
    }
}

// ---- ALU core ---------------------------------------------------------------

static void alu_add(GB_Context *ctx, uint8_t val, int with_carry) {
    uint8_t carry_in = (with_carry && (ctx->f & FLAG_C)) ? 1 : 0;
    uint16_t res = (uint16_t)ctx->a + val + carry_in;
    set_flag(ctx, FLAG_H, ((ctx->a & 0xF) + (val & 0xF) + carry_in) > 0xF);
    set_flag(ctx, FLAG_C, res > 0xFF);
    ctx->a = res & 0xFF;
    set_flag(ctx, FLAG_Z, ctx->a == 0);
    set_flag(ctx, FLAG_N, 0);
}

static void alu_sub(GB_Context *ctx, uint8_t val, int with_carry, int store) {
    uint8_t carry_in = (with_carry && (ctx->f & FLAG_C)) ? 1 : 0;
    int16_t res = (int16_t)ctx->a - val - carry_in;
    set_flag(ctx, FLAG_H, ((int)(ctx->a & 0xF) - (int)(val & 0xF) - carry_in) < 0);
    set_flag(ctx, FLAG_C, res < 0);
    uint8_t res8 = (uint8_t)res;
    set_flag(ctx, FLAG_Z, res8 == 0);
    set_flag(ctx, FLAG_N, 1);
    if (store) ctx->a = res8;
}

static void alu_and(GB_Context *ctx, uint8_t val) {
    ctx->a &= val;
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H;
}

static void alu_xor(GB_Context *ctx, uint8_t val) {
    ctx->a ^= val;
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0);
}

static void alu_or(GB_Context *ctx, uint8_t val) {
    ctx->a |= val;
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0);
}

static void alu_inc8(GB_Context *ctx, int idx) {
    uint8_t v = read_r8(ctx, idx);
    uint8_t res = v + 1;
    set_flag(ctx, FLAG_H, (v & 0xF) == 0xF);
    set_flag(ctx, FLAG_Z, res == 0);
    set_flag(ctx, FLAG_N, 0);
    write_r8(ctx, idx, res);
}

static void alu_dec8(GB_Context *ctx, int idx) {
    uint8_t v = read_r8(ctx, idx);
    uint8_t res = v - 1;
    set_flag(ctx, FLAG_H, (v & 0xF) == 0);
    set_flag(ctx, FLAG_Z, res == 0);
    set_flag(ctx, FLAG_N, 1);
    write_r8(ctx, idx, res);
}

static void alu_add_hl(GB_Context *ctx, uint16_t val) {
    uint16_t hl = get_hl(ctx);
    uint32_t res = (uint32_t)hl + val;
    set_flag(ctx, FLAG_H, ((hl & 0xFFF) + (val & 0xFFF)) > 0xFFF);
    set_flag(ctx, FLAG_C, res > 0xFFFF);
    set_flag(ctx, FLAG_N, 0);
    set_hl(ctx, res & 0xFFFF);
}

// Shared by ADD SP,e and LD HL,SP+e - both use the same 8-bit-on-16-bit flag rule.
static uint16_t alu_sp_plus_e(GB_Context *ctx, int8_t e) {
    uint16_t sp = ctx->sp;
    uint16_t res = sp + e;
    set_flag(ctx, FLAG_C, ((sp & 0xFF) + (uint8_t)e) > 0xFF);
    set_flag(ctx, FLAG_H, ((sp & 0xF) + (e & 0xF)) > 0xF);
    set_flag(ctx, FLAG_Z, 0);
    set_flag(ctx, FLAG_N, 0);
    return res;
}

// ---- CB-prefixed (rotate/shift/BIT/RES/SET) --------------------------------

static void exec_cb(GB_Context *ctx, uint8_t sub_op) {
    int idx = sub_op & 0x07;
    int bit = (sub_op >> 3) & 0x07;
    int group = sub_op >> 6;
    uint8_t v = read_r8(ctx, idx);
    uint8_t res;

    if (group == 1) { // BIT b, r
        set_flag(ctx, FLAG_Z, (v & (1 << bit)) == 0);
        set_flag(ctx, FLAG_H, 1);
        set_flag(ctx, FLAG_N, 0);
    } else if (group == 2) { // RES b, r
        write_r8(ctx, idx, v & ~(1 << bit));
    } else if (group == 3) { // SET b, r
        write_r8(ctx, idx, v | (1 << bit));
    } else { // group 0: rotates/shifts, sub-selected by `bit` field
        switch (bit) {
            case 0: // RLC
                res = (v << 1) | (v >> 7);
                set_flag(ctx, FLAG_C, v & 0x80);
                break;
            case 1: // RRC
                res = (v >> 1) | (v << 7);
                set_flag(ctx, FLAG_C, v & 0x01);
                break;
            case 2: { // RL
                uint8_t carry_in = (ctx->f & FLAG_C) ? 1 : 0;
                res = (v << 1) | carry_in;
                set_flag(ctx, FLAG_C, v & 0x80);
                break;
            }
            case 3: { // RR
                uint8_t carry_in = (ctx->f & FLAG_C) ? 0x80 : 0;
                res = (v >> 1) | carry_in;
                set_flag(ctx, FLAG_C, v & 0x01);
                break;
            }
            case 4: // SLA
                res = v << 1;
                set_flag(ctx, FLAG_C, v & 0x80);
                break;
            case 5: // SRA (arithmetic: bit 7 preserved)
                res = (v >> 1) | (v & 0x80);
                set_flag(ctx, FLAG_C, v & 0x01);
                break;
            case 6: // SWAP
                res = (v << 4) | (v >> 4);
                set_flag(ctx, FLAG_C, 0);
                break;
            default: // 7: SRL
                res = v >> 1;
                set_flag(ctx, FLAG_C, v & 0x01);
                break;
        }
        set_flag(ctx, FLAG_H, 0);
        set_flag(ctx, FLAG_N, 0);
        set_flag(ctx, FLAG_Z, res == 0);
        write_r8(ctx, idx, res);
    }

    ctx->cycles += (idx == 6) ? ((group == 1) ? 12 : 16) : 8;
}

// ---- Main dispatch ----------------------------------------------------------

// Ring buffer of the last few PCs, so we can see what led into a wild jump.
#define PC_HISTORY_LEN 4096
static uint16_t pc_history[PC_HISTORY_LEN];
static int pc_history_pos = 0;

void record_pc(uint16_t pc) __attribute__((weak));
void record_pc(uint16_t pc) {
    pc_history[pc_history_pos] = pc;
    pc_history_pos = (pc_history_pos + 1) % PC_HISTORY_LEN;
}

void dump_pc_history(void) __attribute__((weak));
void dump_pc_history(void) {
    printf("[HISTORY] last %d PCs (oldest first, one per line):\n", PC_HISTORY_LEN);
    for (int i = 0; i < PC_HISTORY_LEN; i++) {
        int idx = (pc_history_pos + i) % PC_HISTORY_LEN;
        printf("%04X\n", pc_history[idx]);
    }
}

void fallback_interpreter(GB_Context *ctx, uint8_t opcode) {
    uint16_t pc = ctx->pc;

    if (opcode == 0xCB) {
        uint8_t sub = gb_read8(ctx, pc + 1);
        exec_cb(ctx, sub);
        ctx->pc = pc + 2;
        return;
    }

    // Generic pattern: 01dddsss = LD r, r' (0x76 is HALT, handled below)
    if (opcode >= 0x40 && opcode <= 0x7F && opcode != 0x76) {
        int dst = (opcode >> 3) & 7, src = opcode & 7;
        write_r8(ctx, dst, read_r8(ctx, src));
        ctx->cycles += (dst == 6 || src == 6) ? 8 : 4;
        ctx->pc = pc + 1;
        return;
    }

    // Generic pattern: 10oooeee = ALU A, r  (ADD/ADC/SUB/SBC/AND/XOR/OR/CP)
    if (opcode >= 0x80 && opcode <= 0xBF) {
        int op = (opcode >> 3) & 7, src = opcode & 7;
        uint8_t val = read_r8(ctx, src);
        switch (op) {
            case 0: alu_add(ctx, val, 0); break;
            case 1: alu_add(ctx, val, 1); break;
            case 2: alu_sub(ctx, val, 0, 1); break;
            case 3: alu_sub(ctx, val, 1, 1); break;
            case 4: alu_and(ctx, val); break;
            case 5: alu_xor(ctx, val); break;
            case 6: alu_or(ctx, val); break;
            case 7: alu_sub(ctx, val, 0, 0); break; // CP
        }
        ctx->cycles += (src == 6) ? 8 : 4;
        ctx->pc = pc + 1;
        return;
    }

    // Generic pattern: 00ddd100 = INC r
    if ((opcode & 0xC7) == 0x04) {
        int idx = (opcode >> 3) & 7;
        alu_inc8(ctx, idx);
        ctx->cycles += (idx == 6) ? 12 : 4;
        ctx->pc = pc + 1;
        return;
    }

    // Generic pattern: 00ddd101 = DEC r
    if ((opcode & 0xC7) == 0x05) {
        int idx = (opcode >> 3) & 7;
        alu_dec8(ctx, idx);
        ctx->cycles += (idx == 6) ? 12 : 4;
        ctx->pc = pc + 1;
        return;
    }

    // Generic pattern: 00ddd110 = LD r, n
    if ((opcode & 0xC7) == 0x06) {
        int idx = (opcode >> 3) & 7;
        uint8_t n = gb_read8(ctx, pc + 1);
        write_r8(ctx, idx, n);
        ctx->cycles += (idx == 6) ? 12 : 8;
        ctx->pc = pc + 2;
        return;
    }

    switch (opcode) {
        case 0x00: // NOP
            ctx->cycles += 4; ctx->pc = pc + 1; break;

        case 0x01: set_bc(ctx, gb_read16(ctx, pc + 1)); ctx->cycles += 12; ctx->pc = pc + 3; break;
        case 0x11: set_de(ctx, gb_read16(ctx, pc + 1)); ctx->cycles += 12; ctx->pc = pc + 3; break;
        case 0x21: set_hl(ctx, gb_read16(ctx, pc + 1)); ctx->cycles += 12; ctx->pc = pc + 3; break;
        case 0x31: ctx->sp = gb_read16(ctx, pc + 1); ctx->cycles += 12; ctx->pc = pc + 3; break;

        case 0x02: gb_write8(ctx, get_bc(ctx), ctx->a); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x12: gb_write8(ctx, get_de(ctx), ctx->a); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x0A: ctx->a = gb_read8(ctx, get_bc(ctx)); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x1A: ctx->a = gb_read8(ctx, get_de(ctx)); ctx->cycles += 8; ctx->pc = pc + 1; break;

        case 0x22: { uint16_t hl = get_hl(ctx); gb_write8(ctx, hl, ctx->a); set_hl(ctx, hl + 1); ctx->cycles += 8; ctx->pc = pc + 1; break; }
        case 0x32: { uint16_t hl = get_hl(ctx); gb_write8(ctx, hl, ctx->a); set_hl(ctx, hl - 1); ctx->cycles += 8; ctx->pc = pc + 1; break; }
        case 0x2A: { uint16_t hl = get_hl(ctx); ctx->a = gb_read8(ctx, hl); set_hl(ctx, hl + 1); ctx->cycles += 8; ctx->pc = pc + 1; break; }
        case 0x3A: { uint16_t hl = get_hl(ctx); ctx->a = gb_read8(ctx, hl); set_hl(ctx, hl - 1); ctx->cycles += 8; ctx->pc = pc + 1; break; }

        case 0x03: set_bc(ctx, get_bc(ctx) + 1); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x13: set_de(ctx, get_de(ctx) + 1); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x23: set_hl(ctx, get_hl(ctx) + 1); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x33: ctx->sp += 1; ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x0B: set_bc(ctx, get_bc(ctx) - 1); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x1B: set_de(ctx, get_de(ctx) - 1); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x2B: set_hl(ctx, get_hl(ctx) - 1); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x3B: ctx->sp -= 1; ctx->cycles += 8; ctx->pc = pc + 1; break;

        case 0x09: alu_add_hl(ctx, get_bc(ctx)); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x19: alu_add_hl(ctx, get_de(ctx)); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x29: alu_add_hl(ctx, get_hl(ctx)); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0x39: alu_add_hl(ctx, ctx->sp); ctx->cycles += 8; ctx->pc = pc + 1; break;

        case 0x07: { // RLCA
            uint8_t c = ctx->a & 0x80;
            ctx->a = (ctx->a << 1) | (c ? 1 : 0);
            ctx->f = c ? FLAG_C : 0;
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        }
        case 0x0F: { // RRCA
            uint8_t c = ctx->a & 0x01;
            ctx->a = (ctx->a >> 1) | (c ? 0x80 : 0);
            ctx->f = c ? FLAG_C : 0;
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        }
        case 0x17: { // RLA
            uint8_t carry_in = (ctx->f & FLAG_C) ? 1 : 0;
            uint8_t c = ctx->a & 0x80;
            ctx->a = (ctx->a << 1) | carry_in;
            ctx->f = c ? FLAG_C : 0;
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        }
        case 0x1F: { // RRA
            uint8_t carry_in = (ctx->f & FLAG_C) ? 0x80 : 0;
            uint8_t c = ctx->a & 0x01;
            ctx->a = (ctx->a >> 1) | carry_in;
            ctx->f = c ? FLAG_C : 0;
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        }

        case 0x08: { // LD (nn), SP
            uint16_t addr = gb_read16(ctx, pc + 1);
            gb_write8(ctx, addr, ctx->sp & 0xFF);
            gb_write8(ctx, addr + 1, ctx->sp >> 8);
            ctx->cycles += 20; ctx->pc = pc + 3; break;
        }

        case 0x10: // STOP (2 bytes; low-power state not modeled)
            ctx->cycles += 4; ctx->pc = pc + 2; break;

        case 0x76: // HALT - pause execution until an interrupt is pending.
            // (The real "HALT bug" edge case - re-executing the next byte
            // when IME is off and an interrupt is already pending - isn't
            // modeled; this is the common, correct-when-IME-is-set path.)
            ctx->halted = true;
            ctx->cycles += 4; ctx->pc = pc + 1; break;

        case 0x27: { // DAA
            uint8_t a = ctx->a;
            int carry = ctx->f & FLAG_C;
            if (!(ctx->f & FLAG_N)) {
                if ((ctx->f & FLAG_H) || (a & 0x0F) > 9) a += 0x06;
                if (carry || a > 0x9F) { a += 0x60; carry = 1; }
            } else {
                if (ctx->f & FLAG_H) a -= 0x06;
                if (carry) a -= 0x60;
            }
            ctx->a = a;
            set_flag(ctx, FLAG_Z, ctx->a == 0);
            set_flag(ctx, FLAG_H, 0);
            set_flag(ctx, FLAG_C, carry);
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        }
        case 0x2F: // CPL
            ctx->a = ~ctx->a;
            set_flag(ctx, FLAG_N, 1); set_flag(ctx, FLAG_H, 1);
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        case 0x37: // SCF
            set_flag(ctx, FLAG_C, 1); set_flag(ctx, FLAG_N, 0); set_flag(ctx, FLAG_H, 0);
            ctx->cycles += 4; ctx->pc = pc + 1; break;
        case 0x3F: // CCF
            set_flag(ctx, FLAG_C, !(ctx->f & FLAG_C)); set_flag(ctx, FLAG_N, 0); set_flag(ctx, FLAG_H, 0);
            ctx->cycles += 4; ctx->pc = pc + 1; break;

        case 0x18: ctx->pc = pc + 2 + sign_extend(gb_read8(ctx, pc + 1)); ctx->cycles += 12; break;
        case 0x20: if (!(ctx->f & FLAG_Z)) { ctx->pc = pc + 2 + sign_extend(gb_read8(ctx, pc + 1)); ctx->cycles += 12; } else { ctx->pc = pc + 2; ctx->cycles += 8; } break;
        case 0x28: if (ctx->f & FLAG_Z)  { ctx->pc = pc + 2 + sign_extend(gb_read8(ctx, pc + 1)); ctx->cycles += 12; } else { ctx->pc = pc + 2; ctx->cycles += 8; } break;
        case 0x30: if (!(ctx->f & FLAG_C)) { ctx->pc = pc + 2 + sign_extend(gb_read8(ctx, pc + 1)); ctx->cycles += 12; } else { ctx->pc = pc + 2; ctx->cycles += 8; } break;
        case 0x38: if (ctx->f & FLAG_C)  { ctx->pc = pc + 2 + sign_extend(gb_read8(ctx, pc + 1)); ctx->cycles += 12; } else { ctx->pc = pc + 2; ctx->cycles += 8; } break;

        case 0xC3: ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 16; break;
        case 0xC2: if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 16; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xCA: if (ctx->f & FLAG_Z)  { ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 16; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xD2: if (!(ctx->f & FLAG_C)) { ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 16; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xDA: if (ctx->f & FLAG_C)  { ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 16; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xE9: ctx->pc = get_hl(ctx); ctx->cycles += 4; break;

        case 0xCD: push16(ctx, pc + 3); ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 24; break;
        case 0xC4: if (!(ctx->f & FLAG_Z)) { push16(ctx, pc + 3); ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 24; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xCC: if (ctx->f & FLAG_Z)  { push16(ctx, pc + 3); ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 24; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xD4: if (!(ctx->f & FLAG_C)) { push16(ctx, pc + 3); ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 24; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;
        case 0xDC: if (ctx->f & FLAG_C)  { push16(ctx, pc + 3); ctx->pc = gb_read16(ctx, pc + 1); ctx->cycles += 24; } else { ctx->pc = pc + 3; ctx->cycles += 12; } break;

        case 0xC9: ctx->pc = pop16(ctx); ctx->cycles += 16; break;
        case 0xD9: ctx->pc = pop16(ctx); ctx->ime = true; ctx->cycles += 16; break;
        case 0xC0: if (!(ctx->f & FLAG_Z)) { ctx->pc = pop16(ctx); ctx->cycles += 20; } else { ctx->pc = pc + 1; ctx->cycles += 8; } break;
        case 0xC8: if (ctx->f & FLAG_Z)  { ctx->pc = pop16(ctx); ctx->cycles += 20; } else { ctx->pc = pc + 1; ctx->cycles += 8; } break;
        case 0xD0: if (!(ctx->f & FLAG_C)) { ctx->pc = pop16(ctx); ctx->cycles += 20; } else { ctx->pc = pc + 1; ctx->cycles += 8; } break;
        case 0xD8: if (ctx->f & FLAG_C)  { ctx->pc = pop16(ctx); ctx->cycles += 20; } else { ctx->pc = pc + 1; ctx->cycles += 8; } break;

        case 0xC5: push16(ctx, get_bc(ctx)); ctx->cycles += 16; ctx->pc = pc + 1; break;
        case 0xD5: push16(ctx, get_de(ctx)); ctx->cycles += 16; ctx->pc = pc + 1; break;
        case 0xE5: push16(ctx, get_hl(ctx)); ctx->cycles += 16; ctx->pc = pc + 1; break;
        case 0xF5: push16(ctx, get_af(ctx)); ctx->cycles += 16; ctx->pc = pc + 1; break;
        case 0xC1: set_bc(ctx, pop16(ctx)); ctx->cycles += 12; ctx->pc = pc + 1; break;
        case 0xD1: set_de(ctx, pop16(ctx)); ctx->cycles += 12; ctx->pc = pc + 1; break;
        case 0xE1: set_hl(ctx, pop16(ctx)); ctx->cycles += 12; ctx->pc = pc + 1; break;
        case 0xF1: set_af(ctx, pop16(ctx)); ctx->cycles += 12; ctx->pc = pc + 1; break;

        case 0xC6: alu_add(ctx, gb_read8(ctx, pc + 1), 0); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xCE: alu_add(ctx, gb_read8(ctx, pc + 1), 1); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xD6: alu_sub(ctx, gb_read8(ctx, pc + 1), 0, 1); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xDE: alu_sub(ctx, gb_read8(ctx, pc + 1), 1, 1); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xE6: alu_and(ctx, gb_read8(ctx, pc + 1)); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xEE: alu_xor(ctx, gb_read8(ctx, pc + 1)); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xF6: alu_or(ctx, gb_read8(ctx, pc + 1)); ctx->cycles += 8; ctx->pc = pc + 2; break;
        case 0xFE: alu_sub(ctx, gb_read8(ctx, pc + 1), 0, 0); ctx->cycles += 8; ctx->pc = pc + 2; break;

        case 0xC7: push16(ctx, pc + 1); ctx->pc = 0x00; ctx->cycles += 16; break;
        case 0xCF: push16(ctx, pc + 1); ctx->pc = 0x08; ctx->cycles += 16; break;
        case 0xD7: push16(ctx, pc + 1); ctx->pc = 0x10; ctx->cycles += 16; break;
        case 0xDF: push16(ctx, pc + 1); ctx->pc = 0x18; ctx->cycles += 16; break;
        case 0xE7: push16(ctx, pc + 1); ctx->pc = 0x20; ctx->cycles += 16; break;
        case 0xEF: push16(ctx, pc + 1); ctx->pc = 0x28; ctx->cycles += 16; break;
        case 0xF7: push16(ctx, pc + 1); ctx->pc = 0x30; ctx->cycles += 16; break;
        case 0xFF: push16(ctx, pc + 1); ctx->pc = 0x38; ctx->cycles += 16; break;

        case 0xE0: gb_write8(ctx, 0xFF00 + gb_read8(ctx, pc + 1), ctx->a); ctx->cycles += 12; ctx->pc = pc + 2; break;
        case 0xF0: ctx->a = gb_read8(ctx, 0xFF00 + gb_read8(ctx, pc + 1)); ctx->cycles += 12; ctx->pc = pc + 2; break;
        case 0xE2: gb_write8(ctx, 0xFF00 + ctx->c, ctx->a); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0xF2: ctx->a = gb_read8(ctx, 0xFF00 + ctx->c); ctx->cycles += 8; ctx->pc = pc + 1; break;
        case 0xEA: gb_write8(ctx, gb_read16(ctx, pc + 1), ctx->a); ctx->cycles += 16; ctx->pc = pc + 3; break;
        case 0xFA: ctx->a = gb_read8(ctx, gb_read16(ctx, pc + 1)); ctx->cycles += 16; ctx->pc = pc + 3; break;

        case 0xF3: ctx->ime = false; ctx->cycles += 4; ctx->pc = pc + 1; break;
        case 0xFB: ctx->ime = true; ctx->cycles += 4; ctx->pc = pc + 1; break;

        case 0xE8: { // ADD SP, e
            int8_t e = sign_extend(gb_read8(ctx, pc + 1));
            ctx->sp = alu_sp_plus_e(ctx, e);
            ctx->cycles += 16; ctx->pc = pc + 2; break;
        }
        case 0xF8: { // LD HL, SP+e
            int8_t e = sign_extend(gb_read8(ctx, pc + 1));
            set_hl(ctx, alu_sp_plus_e(ctx, e));
            ctx->cycles += 12; ctx->pc = pc + 2; break;
        }
        case 0xF9: // LD SP, HL
            ctx->sp = get_hl(ctx); ctx->cycles += 8; ctx->pc = pc + 1; break;

        default:
            // Every real opcode is covered above; only 0xD3/0xDB/0xDD/0xE3/0xE4/
            // 0xEB-0xED/0xF4/0xFC/0xFD (illegal on real hardware) land here.
            printf("[WARN] Illegal/unrecognized opcode 0x%02X at PC 0x%04X - treating as NOP\n", opcode, pc);
            if (!history_dumped && getenv("GB_DUMP_HISTORY")) { dump_pc_history(); history_dumped = 1; }
            ctx->cycles += 4; ctx->pc = pc + 1; break;
    }
}