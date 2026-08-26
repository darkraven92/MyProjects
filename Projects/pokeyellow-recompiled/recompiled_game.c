#include "runtime.h"

void recompiled_block_0100(GB_Context *ctx) {
    // 0x0100: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0x0101: JP nn
    ctx->pc = 0x01AB; ctx->cycles += 16;
    return;
}

void recompiled_block_0000(GB_Context *ctx) {
    // 0x0000: RST 38h
    gb_write8(ctx, --ctx->sp, 0); gb_write8(ctx, --ctx->sp, 1); ctx->pc = 0x0038; ctx->cycles += 16;
    return;
}

void recompiled_block_0040(GB_Context *ctx) {
    // 0x0040: JP nn
    ctx->pc = 0x1DE5; ctx->cycles += 16;
    return;
}

void recompiled_block_0048(GB_Context *ctx) {
    // 0x0048: JP nn
    ctx->pc = 0x15AC; ctx->cycles += 16;
    return;
}

void recompiled_block_0050(GB_Context *ctx) {
    // 0x0050: JP nn
    ctx->pc = 0x216A; ctx->cycles += 16;
    return;
}

void recompiled_block_0058(GB_Context *ctx) {
    // 0x0058: JP nn
    ctx->pc = 0x1F79; ctx->cycles += 16;
    return;
}

void recompiled_block_0060(GB_Context *ctx) {
    // 0x0060: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_0066(GB_Context *ctx) {
    // 0x0066: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x0067: CB 87 -> RES 0, A
    ctx->a = ctx->a & ~(1 << 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x0069: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xFF, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x006B: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x44); ctx->cycles += 12; ctx->pc += 2;
    // 0x006D: CP n
    {
        uint8_t val = 0x91;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x006F: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x006B; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_01AB(GB_Context *ctx) {
    // 0x01AB: CP n
    {
        uint8_t val = 0x11;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x01AD: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x01B2; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_0038(GB_Context *ctx) {
    // 0x0038: RST 38h
    gb_write8(ctx, --ctx->sp, 0); gb_write8(ctx, --ctx->sp, 57); ctx->pc = 0x0038; ctx->cycles += 16;
    return;
}

void recompiled_block_1DE5(GB_Context *ctx) {
    // 0x1DE5: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x1DE6: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x1DE7: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x1DE8: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x1DE9: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x4F); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DEB: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x1DEC: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1DED: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x4F, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DEF: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DF1: LD (nn), A
    gb_write8(ctx, 0xD121, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1DF4: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xAE); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DF6: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x43, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DF8: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xAF); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DFA: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x42, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1DFC: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD09F); ctx->cycles += 16; ctx->pc += 3;
    // 0x1DFF: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1E00: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1E06; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_15AC(GB_Context *ctx) {
    // 0x15AC: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x15AD: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xF4); ctx->cycles += 12; ctx->pc += 2;
    // 0x15AF: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x15B0: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x15C1; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_216A(GB_Context *ctx) {
    // 0x216A: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_1F79(GB_Context *ctx) {
    // 0x1F79: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x1F7A: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x1F7B: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x1F7C: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x1F7D: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD499); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F80: CB 47 -> BIT 0, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 0)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x1F82: JP NZ, nn
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x2162; ctx->cycles += 16; } else { ctx->pc += 3; ctx->cycles += 12; }
    return;
}

void recompiled_block_006B(GB_Context *ctx) {
    // 0x006B: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x44); ctx->cycles += 12; ctx->pc += 2;
    // 0x006D: CP n
    {
        uint8_t val = 0x91;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x006F: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x006B; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_0071(GB_Context *ctx) {
    // 0x0071: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x40); ctx->cycles += 12; ctx->pc += 2;
    // 0x0073: AND n
    ctx->a &= 0x7F; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;
    // 0x0075: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x40, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x0077: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x0078: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xFF, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x007A: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_01B2(GB_Context *ctx) {
    // 0x01B2: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x01B4: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xFE, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x01B6: JP nn
    ctx->pc = 0x1D10; ctx->cycles += 16;
    return;
}

void recompiled_block_01AF(GB_Context *ctx) {
    // 0x01AF: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x01B0: JR e
    ctx->pc = 0x01B4; ctx->cycles += 12;
    return;
}

void recompiled_block_1E06(GB_Context *ctx) {
    // 0x1E06: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 9); ctx->pc = 0x1B30; ctx->cycles += 24;
    return;
}

void recompiled_block_1E02(GB_Context *ctx) {
    // 0x1E02: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB0); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E04: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x4A, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E06: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 9); ctx->pc = 0x1B30; ctx->cycles += 24;
    return;
}

void recompiled_block_15C1(GB_Context *ctx) {
    // 0x15C1: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x15C2: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_15B2(GB_Context *ctx) {
    // 0x15B2: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x15B3: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x44); ctx->cycles += 12; ctx->pc += 2;
    // 0x15B5: LD L, A
    ctx->l = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x15B6: LD H, n
    ctx->h = 0xC7; ctx->cycles += 8; ctx->pc += 2;
    // 0x15B8: LD H, (HL)
    ctx->h = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x15B9: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xF4); ctx->cycles += 12; ctx->pc += 2;
    // 0x15BB: LD L, A
    ctx->l = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x15BC: LD A, H
    ctx->a = ctx->h; ctx->cycles += 4; ctx->pc += 1;
    // 0x15BD: LD H, n
    ctx->h = 0xFF; ctx->cycles += 8; ctx->pc += 2;
    // 0x15BF: LD (HL), A
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;
    // 0x15C0: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x15C1: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x15C2: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_2162(GB_Context *ctx) {
    // 0x2162: CALL nn
    gb_write8(ctx, --ctx->sp, 33); gb_write8(ctx, --ctx->sp, 101); ctx->pc = 0x2FA7; ctx->cycles += 24;
    return;
}

void recompiled_block_1F85(GB_Context *ctx) {
    // 0x1F85: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xAA); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F87: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1F88: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1F9E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1D10(GB_Context *ctx) {
    // 0x1D10: DI
    ctx->ime = false; ctx->cycles += 4; ctx->pc += 1;
    // 0x1D11: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1D12: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x0F, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D14: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xFF, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D16: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x43, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D18: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x42, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D1A: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x01, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D1C: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x02, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D1E: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x4B, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D20: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x4A, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D22: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x06, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D24: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x07, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D26: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x47, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D28: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x48, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D2A: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x49, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D2C: LD A, n
    ctx->a = 0x80; ctx->cycles += 8; ctx->pc += 2;
    // 0x1D2E: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x40, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1D30: CALL nn
    gb_write8(ctx, --ctx->sp, 29); gb_write8(ctx, --ctx->sp, 51); ctx->pc = 0x0061; ctx->cycles += 24;
    return;
}

void recompiled_block_01B4(GB_Context *ctx) {
    // 0x01B4: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xFE, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x01B6: JP nn
    ctx->pc = 0x1D10; ctx->cycles += 16;
    return;
}

void recompiled_block_1B30(GB_Context *ctx) {
    // 0x1B30: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xBA); ctx->cycles += 12; ctx->pc += 2;
    // 0x1B32: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1B33: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E09(GB_Context *ctx) {
    // 0x1E09: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 12); ctx->pc = 0x1BB5; ctx->cycles += 24;
    return;
}

void recompiled_block_2FA7(GB_Context *ctx) {
    // 0x2FA7: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x2FA9: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x2FAA: LD A, n
    ctx->a = 0x3A; ctx->cycles += 8; ctx->pc += 2;
    // 0x2FAC: CALL nn
    gb_write8(ctx, --ctx->sp, 47); gb_write8(ctx, --ctx->sp, 175); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_2165(GB_Context *ctx) {
    // 0x2165: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2166: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2167: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2168: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2169: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_1F9E(GB_Context *ctx) {
    // 0x1F9E: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x01); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FA0: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xAD, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FA2: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xAA, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FA4: CP n
    {
        uint8_t val = 0x02;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1FA6: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1FBB; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1F8A(GB_Context *ctx) {
    // 0x1F8A: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x01); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F8C: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xAD, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F8E: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xAC); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F90: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x01, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F92: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xAA); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F94: CP n
    {
        uint8_t val = 0x02;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1F96: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1FBE; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_0061(GB_Context *ctx) {
    // 0x0061: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x0062: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x0F, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x0064: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xFF); ctx->cycles += 12; ctx->pc += 2;
    // 0x0066: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x0067: CB 87 -> RES 0, A
    ctx->a = ctx->a & ~(1 << 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x0069: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xFF, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x006B: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x44); ctx->cycles += 12; ctx->pc += 2;
    // 0x006D: CP n
    {
        uint8_t val = 0x91;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x006F: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x006B; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1D33(GB_Context *ctx) {
    // 0x1D33: LD SP, nn
    ctx->sp = 0xDFFF; ctx->cycles += 12; ctx->pc += 3;
    // 0x1D36: LD HL, nn
    ctx->h = 0xC0; ctx->l = 0x00; ctx->cycles += 12; ctx->pc += 3;
    // 0x1D39: LD BC, nn
    ctx->b = 0x20; ctx->c = 0x00; ctx->cycles += 12; ctx->pc += 3;
    // 0x1D3C: Unknown Opcode 0x36
    fallback_interpreter(ctx, 0x36);
    return;
}

void recompiled_block_1B34(GB_Context *ctx) {
    // 0x1B34: Unknown Opcode 0x08
    fallback_interpreter(ctx, 0x08);
    return;
}

void recompiled_block_1BB5(GB_Context *ctx) {
    // 0x1BB5: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xC1); ctx->cycles += 12; ctx->pc += 2;
    // 0x1BB7: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1BB8: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E0C(GB_Context *ctx) {
    // 0x1E0C: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 15); ctx->pc = 0x1ADA; ctx->cycles += 24;
    return;
}

void recompiled_block_3E7E(GB_Context *ctx) {
    // 0x3E7E: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x3E80: LD (nn), A
    gb_write8(ctx, 0x2000, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x3E83: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_2FAF(GB_Context *ctx) {
    // 0x2FAF: CALL nn
    gb_write8(ctx, --ctx->sp, 47); gb_write8(ctx, --ctx->sp, 178); ctx->pc = 0x4A5E; ctx->cycles += 24;
    return;
}

void recompiled_block_1FBB(GB_Context *ctx) {
    // 0x1FBB: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1FBC: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x01, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FBE: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x1FC0: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xA9, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FC2: LD A, n
    ctx->a = 0xFE; ctx->cycles += 8; ctx->pc += 2;
    // 0x1FC4: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xAC, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FC6: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FC7: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FC8: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FC9: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FCA: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_1FA8(GB_Context *ctx) {
    // 0x1FA8: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1FA9: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x01, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FAB: LD A, n
    ctx->a = 0x03; ctx->cycles += 8; ctx->pc += 2;
    // 0x1FAD: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x04, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FAF: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x04); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FB1: CB 7F -> BIT 7, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 7)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x1FB3: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1FAF; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1FBE(GB_Context *ctx) {
    // 0x1FBE: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x1FC0: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xA9, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FC2: LD A, n
    ctx->a = 0xFE; ctx->cycles += 8; ctx->pc += 2;
    // 0x1FC4: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xAC, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FC6: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FC7: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FC8: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FC9: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1FCA: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_1F98(GB_Context *ctx) {
    // 0x1F98: LD A, n
    ctx->a = 0x80; ctx->cycles += 8; ctx->pc += 2;
    // 0x1F9A: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x02, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F9C: JR e
    ctx->pc = 0x1FBE; ctx->cycles += 12;
    return;
}

void recompiled_block_1BB9(GB_Context *ctx) {
    // 0x1BB9: Unknown Opcode 0x08
    fallback_interpreter(ctx, 0x08);
    return;
}

void recompiled_block_1ADA(GB_Context *ctx) {
    // 0x1ADA: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD0); ctx->cycles += 12; ctx->pc += 2;
    // 0x1ADC: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1ADD: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E0F(GB_Context *ctx) {
    // 0x1E0F: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 18); ctx->pc = 0x1C21; ctx->cycles += 24;
    return;
}

void recompiled_block_4A5E(GB_Context *ctx) {
    // 0x4A5E: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_2FB2(GB_Context *ctx) {
    // 0x2FB2: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2FB3: CALL nn
    gb_write8(ctx, --ctx->sp, 47); gb_write8(ctx, --ctx->sp, 182); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_1FAF(GB_Context *ctx) {
    // 0x1FAF: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x04); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FB1: CB 7F -> BIT 7, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 7)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x1FB3: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1FAF; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1FB5(GB_Context *ctx) {
    // 0x1FB5: LD A, n
    ctx->a = 0x80; ctx->cycles += 8; ctx->pc += 2;
    // 0x1FB7: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x02, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1FB9: JR e
    ctx->pc = 0x1FBE; ctx->cycles += 12;
    return;
}

void recompiled_block_1ADE(GB_Context *ctx) {
    // 0x1ADE: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x1ADF: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1AE0: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xD0, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1AE2: DEC B
    ctx->b--; ctx->f = (ctx->f & FLAG_C) | (ctx->b == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x1AE3: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1B09; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1C21(GB_Context *ctx) {
    // 0x1C21: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xC6); ctx->cycles += 12; ctx->pc += 2;
    // 0x1C23: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1C24: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E12(GB_Context *ctx) {
    // 0x1E12: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 21); ctx->pc = 0x1BD1; ctx->cycles += 24;
    return;
}

void recompiled_block_2FB6(GB_Context *ctx) {
    // 0x2FB6: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_1B09(GB_Context *ctx) {
    // 0x1B09: LD HL, nn
    ctx->h = 0xCB; ctx->l = 0xFC; ctx->cycles += 12; ctx->pc += 3;
    // 0x1B0C: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD1); ctx->cycles += 12; ctx->pc += 2;
    // 0x1B0E: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x1B0F: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD2); ctx->cycles += 12; ctx->pc += 2;
    // 0x1B11: LD D, A
    ctx->d = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x1B12: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x1B13: CALL nn
    gb_write8(ctx, --ctx->sp, 27); gb_write8(ctx, --ctx->sp, 22); ctx->pc = 0x1B1B; ctx->cycles += 24;
    return;
}

void recompiled_block_1AE5(GB_Context *ctx) {
    // 0x1AE5: LD HL, nn
    ctx->h = 0xCB; ctx->l = 0xFC; ctx->cycles += 12; ctx->pc += 3;
    // 0x1AE8: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD1); ctx->cycles += 12; ctx->pc += 2;
    // 0x1AEA: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x1AEB: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD2); ctx->cycles += 12; ctx->pc += 2;
    // 0x1AED: LD D, A
    ctx->d = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x1AEE: LD C, n
    ctx->c = 0x12; ctx->cycles += 8; ctx->pc += 2;
    // 0x1AF0: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x1AF1: Unknown Opcode 0x12
    fallback_interpreter(ctx, 0x12);
    return;
}

void recompiled_block_1C25(GB_Context *ctx) {
    // 0x1C25: Unknown Opcode 0x08
    fallback_interpreter(ctx, 0x08);
    return;
}

void recompiled_block_1BD1(GB_Context *ctx) {
    // 0x1BD1: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xCB); ctx->cycles += 12; ctx->pc += 2;
    // 0x1BD3: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1BD4: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E15(GB_Context *ctx) {
    // 0x1E15: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 24); ctx->pc = 0x1C75; ctx->cycles += 24;
    return;
}

void recompiled_block_1B1B(GB_Context *ctx) {
    // 0x1B1B: LD C, n
    ctx->c = 0x0A; ctx->cycles += 8; ctx->pc += 2;
    // 0x1B1D: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x1B1E: Unknown Opcode 0x12
    fallback_interpreter(ctx, 0x12);
    return;
}

void recompiled_block_1B16(GB_Context *ctx) {
    // 0x1B16: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1B17: LD A, n
    ctx->a = 0x20; ctx->cycles += 8; ctx->pc += 2;
    // 0x1B19: Unknown Opcode 0x83
    fallback_interpreter(ctx, 0x83);
    return;
}

void recompiled_block_1BD5(GB_Context *ctx) {
    // 0x1BD5: Unknown Opcode 0x08
    fallback_interpreter(ctx, 0x08);
    return;
}

void recompiled_block_1C75(GB_Context *ctx) {
    // 0x1C75: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD7); ctx->cycles += 12; ctx->pc += 2;
    // 0x1C77: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1C78: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E18(GB_Context *ctx) {
    // 0x1E18: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 27); ctx->pc = 0xFF80; ctx->cycles += 24;
    return;
}

void recompiled_block_1C79(GB_Context *ctx) {
    // 0x1C79: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x44); ctx->cycles += 12; ctx->pc += 2;
    // 0x1C7B: CP n
    {
        uint8_t val = 0x90;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1C7D: RET C
    if (ctx->f & FLAG_C) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_FF80(GB_Context *ctx) {
    // 0xFF80: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF81: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF82: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF83: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF84: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF85: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF86: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF87: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF88: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF89: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF8A: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF8B: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF8C: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF8D: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF8E: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF8F: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF90: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF91: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF92: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF93: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF94: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF95: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF96: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF97: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF98: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF99: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF9A: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF9B: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF9C: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF9D: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF9E: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFF9F: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA0: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA1: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA2: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA3: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA4: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA5: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA6: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA7: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA8: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFA9: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFAA: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFAB: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFAC: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFAD: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFAE: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFAF: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB0: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB1: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB2: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB3: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB4: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB5: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB6: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB7: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB8: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFB9: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFBA: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFBB: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFBC: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFBD: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFBE: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFBF: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC0: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC1: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC2: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC3: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC4: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC5: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC6: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC7: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC8: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFC9: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFCA: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFCB: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFCC: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFCD: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFCE: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFCF: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD0: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD1: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD2: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD3: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD4: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD5: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD6: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD7: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD8: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFD9: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFDA: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFDB: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFDC: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFDD: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFDE: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFDF: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE0: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE1: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE2: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE3: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE4: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE5: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE6: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE7: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE8: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFE9: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFEA: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFEB: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFEC: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFED: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFEE: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFEF: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF0: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF1: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF2: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF3: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF4: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF5: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF6: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF7: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF8: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFF9: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFFA: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFFB: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFFC: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFFD: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFFE: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0xFFFF: NOP
    ctx->cycles += 4; ctx->pc += 1;
    // 0x10000: INC BC
    { uint16_t bc = (ctx->b << 8) | ctx->c; bc++; ctx->b = bc >> 8; ctx->c = bc & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x10001: INC BC
    { uint16_t bc = (ctx->b << 8) | ctx->c; bc++; ctx->b = bc >> 8; ctx->c = bc & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x10002: Unknown Opcode 0x02
    fallback_interpreter(ctx, 0x02);
    return;
}

void recompiled_block_1E1B(GB_Context *ctx) {
    // 0x1E1B: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x1E1D: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E1F: LD (nn), A
    gb_write8(ctx, 0x2000, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1E22: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 37); ctx->pc = 0x499B; ctx->cycles += 24;
    return;
}

void recompiled_block_1C7E(GB_Context *ctx) {
    // 0x1C7E: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD8); ctx->cycles += 12; ctx->pc += 2;
    // 0x1C80: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1C81: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xD8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1C83: CP n
    {
        uint8_t val = 0x14;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1C85: RET C
    if (ctx->f & FLAG_C) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_499B(GB_Context *ctx) {
    // 0x499B: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFCA); ctx->cycles += 16; ctx->pc += 3;
    // 0x499E: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x499F: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x49AA; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E25(GB_Context *ctx) {
    // 0x1E25: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 40); ctx->pc = 0x1EF5; ctx->cycles += 24;
    return;
}

void recompiled_block_1C86(GB_Context *ctx) {
    // 0x1C86: CP n
    {
        uint8_t val = 0x15;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1C88: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1CB2; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_49AA(GB_Context *ctx) {
    // 0x49AA: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x49AB: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x90, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x49AD: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x8F, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x49AF: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x49B0: LD D, n
    ctx->d = 0xC1; ctx->cycles += 8; ctx->pc += 2;
    // 0x49B2: Unknown Opcode 0x1A
    fallback_interpreter(ctx, 0x1A);
    return;
}

void recompiled_block_49A1(GB_Context *ctx) {
    // 0x49A1: CP n
    {
        uint8_t val = 0xFF;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x49A3: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1EF5(GB_Context *ctx) {
    // 0x1EF5: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 248); ctx->pc = 0x1F54; ctx->cycles += 24;
    return;
}

void recompiled_block_1E28(GB_Context *ctx) {
    // 0x1E28: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 43); ctx->pc = 0x3E6D; ctx->cycles += 24;
    return;
}

void recompiled_block_1CB2(GB_Context *ctx) {
    // 0x1CB2: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1CB3: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xD8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1CB5: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD084); ctx->cycles += 16; ctx->pc += 3;
    // 0x1CB8: AND n
    ctx->a &= 0x03; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;
    // 0x1CBA: CP n
    {
        uint8_t val = 0x02;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1CBC: LD HL, nn
    ctx->h = 0x1C; ctx->l = 0xD5; ctx->cycles += 12; ctx->pc += 3;
    // 0x1CBF: JR C, e
    if (ctx->f & FLAG_C) { ctx->pc = 0x1CC9; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1C8A(GB_Context *ctx) {
    // 0x1C8A: LD HL, nn
    ctx->h = 0x91; ctx->l = 0x40; ctx->cycles += 12; ctx->pc += 3;
    // 0x1C8D: LD C, n
    ctx->c = 0x10; ctx->cycles += 8; ctx->pc += 2;
    // 0x1C8F: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD084); ctx->cycles += 16; ctx->pc += 3;
    // 0x1C92: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1C93: AND n
    ctx->a &= 0x07; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;
    // 0x1C95: LD (nn), A
    gb_write8(ctx, 0xD084, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1C98: AND n
    ctx->a &= 0x04; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;
    // 0x1C9A: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1CA4; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_49A4(GB_Context *ctx) {
    // 0x49A4: LD (nn), A
    gb_write8(ctx, 0xCFCA, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x49A7: JP nn
    ctx->pc = 0x008D; ctx->cycles += 16;
    return;
}

void recompiled_block_1F54(GB_Context *ctx) {
    // 0x1F54: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD139); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F57: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F58: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1F5E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1EF8(GB_Context *ctx) {
    // 0x1EF8: LD HL, nn
    ctx->h = 0xD4; ctx->l = 0x79; ctx->cycles += 12; ctx->pc += 3;
    // 0x1EFB: CB 46 -> BIT 0, (HL)
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((gb_read8(ctx, (ctx->h << 8) | ctx->l) & (1 << 0)) == 0 ? FLAG_Z : 0); ctx->cycles += 16; ctx->pc += 2;
    // 0x1EFD: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x1F43; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_3E6D(GB_Context *ctx) {
    // 0x3E6D: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x3E6E: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x3E6F: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x3E70: LD B, n
    ctx->b = 0x3D; ctx->cycles += 8; ctx->pc += 2;
    // 0x3E72: LD HL, nn
    ctx->h = 0x67; ctx->l = 0xDC; ctx->cycles += 12; ctx->pc += 3;
    // 0x3E75: CALL nn
    gb_write8(ctx, --ctx->sp, 62); gb_write8(ctx, --ctx->sp, 120); ctx->pc = 0x3E84; ctx->cycles += 24;
    return;
}

void recompiled_block_1E2B(GB_Context *ctx) {
    // 0x1E2B: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 46); ctx->pc = 0x01C8; ctx->cycles += 24;
    return;
}

void recompiled_block_1CC9(GB_Context *ctx) {
    // 0x1CC9: LD DE, nn
    ctx->d = 0x90; ctx->e = 0x30; ctx->cycles += 12; ctx->pc += 3;
    // 0x1CCC: LD C, n
    ctx->c = 0x10; ctx->cycles += 8; ctx->pc += 2;
    // 0x1CCE: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x1CCF: Unknown Opcode 0x12
    fallback_interpreter(ctx, 0x12);
    return;
}

void recompiled_block_1CC1(GB_Context *ctx) {
    // 0x1CC1: LD HL, nn
    ctx->h = 0x1C; ctx->l = 0xE5; ctx->cycles += 12; ctx->pc += 3;
    // 0x1CC4: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1CC9; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1CA4(GB_Context *ctx) {
    // 0x1CA4: LD A, (HL)
    ctx->a = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x1CA5: Unknown Opcode 0x07
    fallback_interpreter(ctx, 0x07);
    return;
}

void recompiled_block_1C9C(GB_Context *ctx) {
    // 0x1C9C: LD A, (HL)
    ctx->a = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x1C9D: Unknown Opcode 0x0F
    fallback_interpreter(ctx, 0x0F);
    return;
}

void recompiled_block_008D(GB_Context *ctx) {
    // 0x008D: LD A, n
    ctx->a = 0xA0; ctx->cycles += 8; ctx->pc += 2;
    // 0x008F: LD HL, nn
    ctx->h = 0xC3; ctx->l = 0x00; ctx->cycles += 12; ctx->pc += 3;
    // 0x0092: LD DE, nn
    ctx->d = 0x00; ctx->e = 0x04; ctx->cycles += 12; ctx->pc += 3;
    // 0x0095: LD B, n
    ctx->b = 0x28; ctx->cycles += 8; ctx->pc += 2;
    // 0x0097: LD (HL), A
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;
    // 0x0098: Unknown Opcode 0x19
    fallback_interpreter(ctx, 0x19);
    return;
}

void recompiled_block_1F5E(GB_Context *ctx) {
    // 0x1F5E: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F5F: LD (nn), A
    gb_write8(ctx, 0xD139, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F62: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F63: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1F5A(GB_Context *ctx) {
    // 0x1F5A: LD A, n
    ctx->a = 0xFF; ctx->cycles += 8; ctx->pc += 2;
    // 0x1F5C: JR e
    ctx->pc = 0x1F5F; ctx->cycles += 12;
    return;
}

void recompiled_block_1F43(GB_Context *ctx) {
    // 0x1F43: LD A, n
    ctx->a = 0x3B; ctx->cycles += 8; ctx->pc += 2;
    // 0x1F45: LD (nn), A
    gb_write8(ctx, 0xDA43, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F48: LD (nn), A
    gb_write8(ctx, 0xDA42, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F4B: LD A, n
    ctx->a = 0xFF; ctx->cycles += 8; ctx->pc += 2;
    // 0x1F4D: LD (nn), A
    gb_write8(ctx, 0xDA40, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F50: LD (nn), A
    gb_write8(ctx, 0xDA41, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F53: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_1EFF(GB_Context *ctx) {
    // 0x1EFF: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD731); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F02: CB 47 -> BIT 0, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 0)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x1F04: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_3E84(GB_Context *ctx) {
    // 0x3E84: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x3E86: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x3E87: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x3E88: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x3E8A: LD (nn), A
    gb_write8(ctx, 0x2000, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x3E8D: CALL nn
    gb_write8(ctx, --ctx->sp, 62); gb_write8(ctx, --ctx->sp, 144); ctx->pc = 0x3E98; ctx->cycles += 24;
    return;
}

void recompiled_block_3E78(GB_Context *ctx) {
    // 0x3E78: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD3); ctx->cycles += 12; ctx->pc += 2;
    // 0x3E7A: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x3E7B: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x3E7C: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x3E7D: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_01C8(GB_Context *ctx) {
    // 0x01C8: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x01CA: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x01CB: LD A, n
    ctx->a = 0x03; ctx->cycles += 8; ctx->pc += 2;
    // 0x01CD: CALL nn
    gb_write8(ctx, --ctx->sp, 1); gb_write8(ctx, --ctx->sp, 208); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_1E2E(GB_Context *ctx) {
    // 0x1E2E: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD6); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E30: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1E31: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1E36; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1CC6(GB_Context *ctx) {
    // 0x1CC6: LD HL, nn
    ctx->h = 0x1C; ctx->l = 0xF5; ctx->cycles += 12; ctx->pc += 3;
    // 0x1CC9: LD DE, nn
    ctx->d = 0x90; ctx->e = 0x30; ctx->cycles += 12; ctx->pc += 3;
    // 0x1CCC: LD C, n
    ctx->c = 0x10; ctx->cycles += 8; ctx->pc += 2;
    // 0x1CCE: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x1CCF: Unknown Opcode 0x12
    fallback_interpreter(ctx, 0x12);
    return;
}

void recompiled_block_1F64(GB_Context *ctx) {
    // 0x1F64: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD72F); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F67: CB 8F -> RES 1, A
    ctx->a = ctx->a & ~(1 << 1); ctx->cycles += 8; ctx->pc += 2;
    // 0x1F69: CB 97 -> RES 2, A
    ctx->a = ctx->a & ~(1 << 2); ctx->cycles += 8; ctx->pc += 2;
    // 0x1F6B: CB 6F -> BIT 5, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 5)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x1F6D: CB AF -> RES 5, A
    ctx->a = ctx->a & ~(1 << 5); ctx->cycles += 8; ctx->pc += 2;
    // 0x1F6F: LD (nn), A
    gb_write8(ctx, 0xD72F, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F72: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1F5F(GB_Context *ctx) {
    // 0x1F5F: LD (nn), A
    gb_write8(ctx, 0xD139, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F62: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F63: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1F05(GB_Context *ctx) {
    // 0x1F05: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xDA41); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F08: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F09: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_3E98(GB_Context *ctx) {
    // 0x3E98: JP (HL)
    ctx->pc = (ctx->h << 8) | ctx->l; ctx->cycles += 4;
    return;
}

void recompiled_block_3E90(GB_Context *ctx) {
    // 0x3E90: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x3E91: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x3E92: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x3E94: LD (nn), A
    gb_write8(ctx, 0x2000, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x3E97: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_01D0(GB_Context *ctx) {
    // 0x01D0: CALL nn
    gb_write8(ctx, --ctx->sp, 1); gb_write8(ctx, --ctx->sp, 211); ctx->pc = 0x4000; ctx->cycles += 24;
    return;
}

void recompiled_block_1E36(GB_Context *ctx) {
    // 0x1E36: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD5); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E38: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1E39: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1E3E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E33(GB_Context *ctx) {
    // 0x1E33: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1E34: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xD6, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E36: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xD5); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E38: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x1E39: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x1E3E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1F73(GB_Context *ctx) {
    // 0x1F73: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F74: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB3, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F76: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB4, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1F78: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_1F0A(GB_Context *ctx) {
    // 0x1F0A: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xDA44); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F0D: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1F0E: LD (nn), A
    gb_write8(ctx, 0xDA44, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F11: CP n
    {
        uint8_t val = 0x3C;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1F13: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_4000(GB_Context *ctx) {
    // 0x4000: LD B, D
    ctx->b = ctx->d; ctx->cycles += 4; ctx->pc += 1;
    // 0x4001: LD B, B
    ctx->b = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x4002: LD D, E
    ctx->d = ctx->e; ctx->cycles += 4; ctx->pc += 1;
    // 0x4003: LD B, B
    ctx->b = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x4004: LD B, D
    ctx->b = ctx->d; ctx->cycles += 4; ctx->pc += 1;
    // 0x4005: LD B, B
    ctx->b = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x4006: LD H, H
    ctx->h = ctx->h; ctx->cycles += 4; ctx->pc += 1;
    // 0x4007: LD B, B
    ctx->b = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x4008: LD (HL), L
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x4009: LD B, B
    ctx->b = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x400A: Unknown Opcode 0x86
    fallback_interpreter(ctx, 0x86);
    return;
}

void recompiled_block_01D3(GB_Context *ctx) {
    // 0x01D3: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x01D4: JP nn
    ctx->pc = 0x3E7E; ctx->cycles += 16;
    return;
}

void recompiled_block_1E3E(GB_Context *ctx) {
    // 0x1E3E: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 65); ctx->pc = 0x27C2; ctx->cycles += 24;
    return;
}

void recompiled_block_1E3B(GB_Context *ctx) {
    // 0x1E3B: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x1E3C: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xD5, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E3E: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 65); ctx->pc = 0x27C2; ctx->cycles += 24;
    return;
}

void recompiled_block_1F14(GB_Context *ctx) {
    // 0x1F14: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F15: LD (nn), A
    gb_write8(ctx, 0xDA44, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F18: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xDA43); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F1B: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1F1C: LD (nn), A
    gb_write8(ctx, 0xDA43, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F1F: CP n
    {
        uint8_t val = 0x3C;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1F21: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_27C2(GB_Context *ctx) {
    // 0x27C2: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC6); ctx->cycles += 16; ctx->pc += 3;
    // 0x27C5: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x27C6: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x27D3; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E41(GB_Context *ctx) {
    // 0x1E41: LD A, n
    ctx->a = 0x08; ctx->cycles += 8; ctx->pc += 2;
    // 0x1E43: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 70); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_1F22(GB_Context *ctx) {
    // 0x1F22: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F23: LD (nn), A
    gb_write8(ctx, 0xDA43, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F26: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xDA42); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F29: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1F2A: LD (nn), A
    gb_write8(ctx, 0xDA42, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F2D: CP n
    {
        uint8_t val = 0x3C;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1F2F: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_27D3(GB_Context *ctx) {
    // 0x27D3: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC8); ctx->cycles += 16; ctx->pc += 3;
    // 0x27D6: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x27D7: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x27DE; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_27C8(GB_Context *ctx) {
    // 0x27C8: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD72B); ctx->cycles += 16; ctx->pc += 3;
    // 0x27CB: CB 4F -> BIT 1, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 1)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x27CD: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E46(GB_Context *ctx) {
    // 0x1E46: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 73); ctx->pc = 0x531E; ctx->cycles += 24;
    return;
}

void recompiled_block_1F30(GB_Context *ctx) {
    // 0x1F30: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x1F31: LD (nn), A
    gb_write8(ctx, 0xDA42, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F34: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xDA40); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F37: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x1F38: LD (nn), A
    gb_write8(ctx, 0xDA40, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F3B: CP n
    {
        uint8_t val = 0xFF;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x1F3D: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_27DE(GB_Context *ctx) {
    // 0x27DE: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC7); ctx->cycles += 16; ctx->pc += 3;
    // 0x27E1: LD (nn), A
    gb_write8(ctx, 0xCFC8, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x27E4: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0x24); ctx->cycles += 12; ctx->pc += 2;
    // 0x27E6: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x27E7: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x27FA; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_27D9(GB_Context *ctx) {
    // 0x27D9: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x27DA: LD (nn), A
    gb_write8(ctx, 0xCFC8, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x27DD: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_27CE(GB_Context *ctx) {
    // 0x27CE: LD A, n
    ctx->a = 0x77; ctx->cycles += 8; ctx->pc += 2;
    // 0x27D0: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x24, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x27D2: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_531E(GB_Context *ctx) {
    // 0x531E: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_1E49(GB_Context *ctx) {
    // 0x1E49: LD A, n
    ctx->a = 0x02; ctx->cycles += 8; ctx->pc += 2;
    // 0x1E4B: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 78); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_1F3E(GB_Context *ctx) {
    // 0x1F3E: LD HL, nn
    ctx->h = 0xD4; ctx->l = 0x79; ctx->cycles += 12; ctx->pc += 3;
    // 0x1F41: CB C6 -> SET 0, (HL)
    gb_write8(ctx, (ctx->h << 8) | ctx->l, gb_read8(ctx, (ctx->h << 8) | ctx->l) | (1 << 0)); ctx->cycles += 16; ctx->pc += 2;
    // 0x1F43: LD A, n
    ctx->a = 0x3B; ctx->cycles += 8; ctx->pc += 2;
    // 0x1F45: LD (nn), A
    gb_write8(ctx, 0xDA43, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F48: LD (nn), A
    gb_write8(ctx, 0xDA42, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F4B: LD A, n
    ctx->a = 0xFF; ctx->cycles += 8; ctx->pc += 2;
    // 0x1F4D: LD (nn), A
    gb_write8(ctx, 0xDA40, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F50: LD (nn), A
    gb_write8(ctx, 0xDA41, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1F53: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_27FA(GB_Context *ctx) {
    // 0x27FA: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC6); ctx->cycles += 16; ctx->pc += 3;
    // 0x27FD: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x27FE: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x27FF: LD (nn), A
    gb_write8(ctx, 0xCFC6, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2802: CALL nn
    gb_write8(ctx, --ctx->sp, 40); gb_write8(ctx, --ctx->sp, 5); ctx->pc = 0x2233; ctx->cycles += 24;
    return;
}

void recompiled_block_27E9(GB_Context *ctx) {
    // 0x27E9: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x27EA: AND n
    ctx->a &= 0x0F; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;
    // 0x27EC: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x27ED: LD C, A
    ctx->c = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x27EE: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x27EF: AND n
    ctx->a &= 0xF0; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;
    // 0x27F1: CB 37 -> SWAP A
    { uint8_t val = ctx->a; ctx->a = ((ctx->a << 4) | (ctx->a >> 4)) & 0xFF; ctx->f = (val == 0 ? FLAG_Z : 0); } ctx->cycles += 8; ctx->pc += 2;
    // 0x27F3: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x27F4: CB 37 -> SWAP A
    { uint8_t val = ctx->a; ctx->a = ((ctx->a << 4) | (ctx->a >> 4)) & 0xFF; ctx->f = (val == 0 ? FLAG_Z : 0); } ctx->cycles += 8; ctx->pc += 2;
    // 0x27F6: OR C
    ctx->a |= ctx->c; ctx->f = (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x27F7: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x24, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x27F9: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_1E4E(GB_Context *ctx) {
    // 0x1E4E: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 81); ctx->pc = 0x509D; ctx->cycles += 24;
    return;
}

void recompiled_block_2233(GB_Context *ctx) {
    // 0x2233: LD A, n
    ctx->a = 0xFF; ctx->cycles += 8; ctx->pc += 2;
    // 0x2235: LD (nn), A
    gb_write8(ctx, 0xC0EE, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2238: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x2239: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x223A: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x223B: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x223C: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xC0EE); ctx->cycles += 16; ctx->pc += 3;
    // 0x223F: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x2240: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x224F; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2805(GB_Context *ctx) {
    // 0x2805: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xC0F0); ctx->cycles += 16; ctx->pc += 3;
    // 0x2808: LD (nn), A
    gb_write8(ctx, 0xC0EF, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x280B: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x280C: LD (nn), A
    gb_write8(ctx, 0xC0EE, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x280F: JP nn
    ctx->pc = 0x2238; ctx->cycles += 16;
    return;
}

void recompiled_block_509D(GB_Context *ctx) {
    // 0x509D: LD C, A
    ctx->c = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x509E: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC4); ctx->cycles += 16; ctx->pc += 3;
    // 0x50A1: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x50A2: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x50B9; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E51(GB_Context *ctx) {
    // 0x1E51: CALL nn
    gb_write8(ctx, --ctx->sp, 30); gb_write8(ctx, --ctx->sp, 84); ctx->pc = 0x2FB7; ctx->cycles += 24;
    return;
}

void recompiled_block_224F(GB_Context *ctx) {
    // 0x224F: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC6); ctx->cycles += 16; ctx->pc += 3;
    // 0x2252: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x2253: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x226A; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2242(GB_Context *ctx) {
    // 0x2242: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x2243: LD (nn), A
    gb_write8(ctx, 0xC02A, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2246: LD (nn), A
    gb_write8(ctx, 0xC02B, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2249: LD (nn), A
    gb_write8(ctx, 0xC02C, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x224C: LD (nn), A
    gb_write8(ctx, 0xC02D, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x224F: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC6); ctx->cycles += 16; ctx->pc += 3;
    // 0x2252: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x2253: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x226A; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2238(GB_Context *ctx) {
    // 0x2238: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x2239: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x223A: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x223B: LD B, A
    ctx->b = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x223C: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xC0EE); ctx->cycles += 16; ctx->pc += 3;
    // 0x223F: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x2240: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x224F; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_50B9(GB_Context *ctx) {
    // 0x50B9: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_50A4(GB_Context *ctx) {
    // 0x50A4: CALL nn
    gb_write8(ctx, --ctx->sp, 80); gb_write8(ctx, --ctx->sp, 167); ctx->pc = 0x50BA; ctx->cycles += 24;
    return;
}

void recompiled_block_2FB7(GB_Context *ctx) {
    // 0x2FB7: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD499); ctx->cycles += 16; ctx->pc += 3;
    // 0x2FBA: CB 47 -> BIT 0, A
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((ctx->a & (1 << 0)) == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;
    // 0x2FBC: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_1E54(GB_Context *ctx) {
    // 0x1E54: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD121); ctx->cycles += 16; ctx->pc += 3;
    // 0x1E57: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E59: LD (nn), A
    gb_write8(ctx, 0x2000, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x1E5C: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1E5D: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0x4F, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E5F: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1E60: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1E61: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1E62: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x1E63: RETI
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;
    return;
}

void recompiled_block_226A(GB_Context *ctx) {
    // 0x226A: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x226B: LD (nn), A
    gb_write8(ctx, 0xC0EE, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x226E: CALL nn
    gb_write8(ctx, --ctx->sp, 34); gb_write8(ctx, --ctx->sp, 113); ctx->pc = 0x22EC; ctx->cycles += 24;
    return;
}

void recompiled_block_2255(GB_Context *ctx) {
    // 0x2255: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xC0EE); ctx->cycles += 16; ctx->pc += 3;
    // 0x2258: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x2259: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x2284; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_50BA(GB_Context *ctx) {
    // 0x50BA: LD H, n
    ctx->h = 0xC1; ctx->cycles += 8; ctx->pc += 2;
    // 0x50BC: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xDA); ctx->cycles += 12; ctx->pc += 2;
    // 0x50BE: Unknown Opcode 0xC6
    fallback_interpreter(ctx, 0xC6);
    return;
}

void recompiled_block_50A7(GB_Context *ctx) {
    // 0x50A7: INC H
    ctx->h++; ctx->f = (ctx->f & FLAG_C) | (ctx->h == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x50A8: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xDA); ctx->cycles += 12; ctx->pc += 2;
    // 0x50AA: Unknown Opcode 0xC6
    fallback_interpreter(ctx, 0xC6);
    return;
}

void recompiled_block_2FBD(GB_Context *ctx) {
    // 0x2FBD: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD49A); ctx->cycles += 16; ctx->pc += 3;
    // 0x2FC0: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x2FC1: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_22EC(GB_Context *ctx) {
    // 0x22EC: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x22EE: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x22EF: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xC0EF); ctx->cycles += 16; ctx->pc += 3;
    // 0x22F2: CALL nn
    gb_write8(ctx, --ctx->sp, 34); gb_write8(ctx, --ctx->sp, 245); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_2271(GB_Context *ctx) {
    // 0x2271: JR e
    ctx->pc = 0x2284; ctx->cycles += 12;
    return;
}

void recompiled_block_2284(GB_Context *ctx) {
    // 0x2284: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2285: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2286: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2287: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_225B(GB_Context *ctx) {
    // 0x225B: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x225C: LD (nn), A
    gb_write8(ctx, 0xC0EE, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x225F: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC9); ctx->cycles += 16; ctx->pc += 3;
    // 0x2262: CP n
    {
        uint8_t val = 0xFF;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x2264: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x2273; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2FC2(GB_Context *ctx) {
    // 0x2FC2: LD HL, nn
    ctx->h = 0xC9; ctx->l = 0x72; ctx->cycles += 12; ctx->pc += 3;
    // 0x2FC5: Unknown Opcode 0x34
    fallback_interpreter(ctx, 0x34);
    return;
}

void recompiled_block_22F5(GB_Context *ctx) {
    // 0x22F5: CP n
    {
        uint8_t val = 0x02;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x22F7: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x22FF; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2273(GB_Context *ctx) {
    // 0x2273: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x2274: LD (nn), A
    gb_write8(ctx, 0xCFC9, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2277: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFC6); ctx->cycles += 16; ctx->pc += 3;
    // 0x227A: LD (nn), A
    gb_write8(ctx, 0xCFC7, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x227D: LD (nn), A
    gb_write8(ctx, 0xCFC8, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2280: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x2281: LD (nn), A
    gb_write8(ctx, 0xCFC6, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2284: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2285: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2286: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2287: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_2266(GB_Context *ctx) {
    // 0x2266: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x2267: LD (nn), A
    gb_write8(ctx, 0xCFC6, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x226A: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x226B: LD (nn), A
    gb_write8(ctx, 0xC0EE, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x226E: CALL nn
    gb_write8(ctx, --ctx->sp, 34); gb_write8(ctx, --ctx->sp, 113); ctx->pc = 0x22EC; ctx->cycles += 24;
    return;
}

void recompiled_block_22FF(GB_Context *ctx) {
    // 0x22FF: CP n
    {
        uint8_t val = 0x08;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x2301: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x2309; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_22F9(GB_Context *ctx) {
    // 0x22F9: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x22FA: CALL nn
    gb_write8(ctx, --ctx->sp, 34); gb_write8(ctx, --ctx->sp, 253); ctx->pc = 0x584E; ctx->cycles += 24;
    return;
}

void recompiled_block_2309(GB_Context *ctx) {
    // 0x2309: CP n
    {
        uint8_t val = 0x1F;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x230B: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x2313; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2303(GB_Context *ctx) {
    // 0x2303: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x2304: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 7); ctx->pc = 0x58BB; ctx->cycles += 24;
    return;
}

void recompiled_block_584E(GB_Context *ctx) {
    // 0x584E: LD HL, nn
    ctx->h = 0xC4; ctx->l = 0xCC; ctx->cycles += 12; ctx->pc += 3;
    // 0x5851: LD BC, nn
    ctx->b = 0x01; ctx->c = 0x09; ctx->cycles += 12; ctx->pc += 3;
    // 0x5854: CALL nn
    gb_write8(ctx, --ctx->sp, 88); gb_write8(ctx, --ctx->sp, 87); ctx->pc = 0x5B67; ctx->cycles += 24;
    return;
}

void recompiled_block_22FD(GB_Context *ctx) {
    // 0x22FD: JR e
    ctx->pc = 0x2317; ctx->cycles += 12;
    return;
}

void recompiled_block_2313(GB_Context *ctx) {
    // 0x2313: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x2314: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 23); ctx->pc = 0x6BD4; ctx->cycles += 24;
    return;
}

void recompiled_block_230D(GB_Context *ctx) {
    // 0x230D: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x230E: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 17); ctx->pc = 0x510D; ctx->cycles += 24;
    return;
}

void recompiled_block_58BB(GB_Context *ctx) {
    // 0x58BB: CALL nn
    gb_write8(ctx, --ctx->sp, 88); gb_write8(ctx, --ctx->sp, 190); ctx->pc = 0x1723; ctx->cycles += 24;
    return;
}

void recompiled_block_2307(GB_Context *ctx) {
    // 0x2307: JR e
    ctx->pc = 0x2317; ctx->cycles += 12;
    return;
}

void recompiled_block_5B67(GB_Context *ctx) {
    // 0x5B67: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x5B68: LD A, n
    ctx->a = 0x78; ctx->cycles += 8; ctx->pc += 2;
    // 0x5B6A: LD (HL+), A
    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x5B6B: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x5B6C: CALL nn
    gb_write8(ctx, --ctx->sp, 91); gb_write8(ctx, --ctx->sp, 111); ctx->pc = 0x5B94; ctx->cycles += 24;
    return;
}

void recompiled_block_5857(GB_Context *ctx) {
    // 0x5857: LD HL, nn
    ctx->h = 0xC4; ctx->l = 0xE2; ctx->cycles += 12; ctx->pc += 3;
    // 0x585A: LD DE, nn
    ctx->d = 0x58; ctx->e = 0x60; ctx->cycles += 12; ctx->pc += 3;
    // 0x585D: JP nn
    ctx->pc = 0x1723; ctx->cycles += 16;
    return;
}

void recompiled_block_2317(GB_Context *ctx) {
    // 0x2317: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2318: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 27); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_6BD4(GB_Context *ctx) {
    // 0x6BD4: LD HL, nn
    ctx->h = 0xCD; ctx->l = 0x6D; ctx->cycles += 12; ctx->pc += 3;
    // 0x6BD7: LD DE, nn
    ctx->d = 0xD0; ctx->e = 0x35; ctx->cycles += 12; ctx->pc += 3;
    // 0x6BDA: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x0B; ctx->cycles += 12; ctx->pc += 3;
    // 0x6BDD: CALL nn
    gb_write8(ctx, --ctx->sp, 107); gb_write8(ctx, --ctx->sp, 224); ctx->pc = 0x00B1; ctx->cycles += 24;
    return;
}

void recompiled_block_510D(GB_Context *ctx) {
    // 0x510D: LD (nn), A
    gb_write8(ctx, 0xCFCA, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x5110: CALL nn
    gb_write8(ctx, --ctx->sp, 81); gb_write8(ctx, --ctx->sp, 19); ctx->pc = 0x4BF7; ctx->cycles += 24;
    return;
}

void recompiled_block_2311(GB_Context *ctx) {
    // 0x2311: JR e
    ctx->pc = 0x2317; ctx->cycles += 12;
    return;
}

void recompiled_block_1723(GB_Context *ctx) {
    // 0x1723: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x1724: Unknown Opcode 0x1A
    fallback_interpreter(ctx, 0x1A);
    return;
}

void recompiled_block_58BE(GB_Context *ctx) {
    // 0x58BE: LD HL, nn
    ctx->h = 0xC3; ctx->l = 0xB6; ctx->cycles += 12; ctx->pc += 3;
    // 0x58C1: LD DE, nn
    ctx->d = 0xD1; ctx->e = 0x63; ctx->cycles += 12; ctx->pc += 3;
    // 0x58C4: CALL nn
    gb_write8(ctx, --ctx->sp, 88); gb_write8(ctx, --ctx->sp, 199); ctx->pc = 0x58CD; ctx->cycles += 24;
    return;
}

void recompiled_block_5B94(GB_Context *ctx) {
    // 0x5B94: LD D, C
    ctx->d = ctx->c; ctx->cycles += 4; ctx->pc += 1;
    // 0x5B95: LD (HL+), A
    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x5B96: DEC D
    ctx->d--; ctx->f = (ctx->f & FLAG_C) | (ctx->d == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x5B97: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x5B95; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_5B6F(GB_Context *ctx) {
    // 0x5B6F: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x5B70: LD (HL), A
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;
    // 0x5B71: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x5B72: LD DE, nn
    ctx->d = 0x00; ctx->e = 0x14; ctx->cycles += 12; ctx->pc += 3;
    // 0x5B75: Unknown Opcode 0x19
    fallback_interpreter(ctx, 0x19);
    return;
}

void recompiled_block_231B(GB_Context *ctx) {
    // 0x231B: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_00B1(GB_Context *ctx) {
    // 0x00B1: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x00B2: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x00B3: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x00C1; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_6BE0(GB_Context *ctx) {
    // 0x6BE0: LD HL, nn
    ctx->h = 0xD1; ctx->l = 0x72; ctx->cycles += 12; ctx->pc += 3;
    // 0x6BE3: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x2C; ctx->cycles += 12; ctx->pc += 3;
    // 0x6BE6: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCF91); ctx->cycles += 16; ctx->pc += 3;
    // 0x6BE9: CALL nn
    gb_write8(ctx, --ctx->sp, 107); gb_write8(ctx, --ctx->sp, 236); ctx->pc = 0x3A74; ctx->cycles += 24;
    return;
}

void recompiled_block_4BF7(GB_Context *ctx) {
    // 0x4BF7: LD H, n
    ctx->h = 0xC1; ctx->cycles += 8; ctx->pc += 2;
    // 0x4BF9: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xDA); ctx->cycles += 12; ctx->pc += 2;
    // 0x4BFB: LD L, A
    ctx->l = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x4BFC: LD A, (HL)
    ctx->a = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x4BFD: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x4BFE: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_5113(GB_Context *ctx) {
    // 0x5113: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x5114: LD (nn), A
    gb_write8(ctx, 0xCFCA, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x5117: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x5118: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x5119: LD H, n
    ctx->h = 0xC1; ctx->cycles += 8; ctx->pc += 2;
    // 0x511B: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xDA); ctx->cycles += 12; ctx->pc += 2;
    // 0x511D: Unknown Opcode 0xC6
    fallback_interpreter(ctx, 0xC6);
    return;
}

void recompiled_block_58CD(GB_Context *ctx) {
    // 0x58CD: LD C, n
    ctx->c = 0x00; ctx->cycles += 8; ctx->pc += 2;
    // 0x58CF: Unknown Opcode 0x1A
    fallback_interpreter(ctx, 0x1A);
    return;
}

void recompiled_block_58C7(GB_Context *ctx) {
    // 0x58C7: LD HL, nn
    ctx->h = 0xC4; ctx->l = 0x56; ctx->cycles += 12; ctx->pc += 3;
    // 0x58CA: LD DE, nn
    ctx->d = 0xD8; ctx->e = 0x9C; ctx->cycles += 12; ctx->pc += 3;
    // 0x58CD: LD C, n
    ctx->c = 0x00; ctx->cycles += 8; ctx->pc += 2;
    // 0x58CF: Unknown Opcode 0x1A
    fallback_interpreter(ctx, 0x1A);
    return;
}

void recompiled_block_5B95(GB_Context *ctx) {
    // 0x5B95: LD (HL+), A
    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x5B96: DEC D
    ctx->d--; ctx->f = (ctx->f & FLAG_C) | (ctx->d == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x5B97: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x5B95; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_5B99(GB_Context *ctx) {
    // 0x5B99: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_00C1(GB_Context *ctx) {
    // 0x00C1: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x00C2: Unknown Opcode 0x12
    fallback_interpreter(ctx, 0x12);
    return;
}

void recompiled_block_00B5(GB_Context *ctx) {
    // 0x00B5: LD A, C
    ctx->a = ctx->c; ctx->cycles += 4; ctx->pc += 1;
    // 0x00B6: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x00B7: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x00BA; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_3A74(GB_Context *ctx) {
    // 0x3A74: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x3A75: RET Z
    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_6BEC(GB_Context *ctx) {
    // 0x6BEC: LD D, H
    ctx->d = ctx->h; ctx->cycles += 4; ctx->pc += 1;
    // 0x6BED: LD E, L
    ctx->e = ctx->l; ctx->cycles += 4; ctx->pc += 1;
    // 0x6BEE: LD B, n
    ctx->b = 0x04; ctx->cycles += 8; ctx->pc += 2;
    // 0x6BF0: LD A, (HL)
    ctx->a = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x6BF1: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x6BF2: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x6C10; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_4BFF(GB_Context *ctx) {
    // 0x4BFF: LD A, L
    ctx->a = ctx->l; ctx->cycles += 4; ctx->pc += 1;
    // 0x4C00: Unknown Opcode 0xC6
    fallback_interpreter(ctx, 0xC6);
    return;
}

void recompiled_block_00BA(GB_Context *ctx) {
    // 0x00BA: CALL nn
    gb_write8(ctx, --ctx->sp, 0); gb_write8(ctx, --ctx->sp, 189); ctx->pc = 0x00C1; ctx->cycles += 24;
    return;
}

void recompiled_block_00B9(GB_Context *ctx) {
    // 0x00B9: INC B
    ctx->b++; ctx->f = (ctx->f & FLAG_C) | (ctx->b == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x00BA: CALL nn
    gb_write8(ctx, --ctx->sp, 0); gb_write8(ctx, --ctx->sp, 189); ctx->pc = 0x00C1; ctx->cycles += 24;
    return;
}

void recompiled_block_3A76(GB_Context *ctx) {
    // 0x3A76: Unknown Opcode 0x09
    fallback_interpreter(ctx, 0x09);
    return;
}

void recompiled_block_6C10(GB_Context *ctx) {
    // 0x6C10: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD0DF); ctx->cycles += 16; ctx->pc += 3;
    // 0x6C13: LD (HL), A
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;
    // 0x6C14: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x15; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C17: Unknown Opcode 0x09
    fallback_interpreter(ctx, 0x09);
    return;
}

void recompiled_block_6BF4(GB_Context *ctx) {
    // 0x6BF4: INC HL
    { uint16_t hl = (ctx->h << 8) | ctx->l; hl++; ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x6BF5: DEC B
    ctx->b--; ctx->f = (ctx->f & FLAG_C) | (ctx->b == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x6BF6: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x6BF0; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_00BD(GB_Context *ctx) {
    // 0x00BD: DEC B
    ctx->b--; ctx->f = (ctx->f & FLAG_C) | (ctx->b == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x00BE: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x00BA; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_6BF0(GB_Context *ctx) {
    // 0x6BF0: LD A, (HL)
    ctx->a = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x6BF1: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x6BF2: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x6C10; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_6BF8(GB_Context *ctx) {
    // 0x6BF8: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x6BF9: CALL nn
    gb_write8(ctx, --ctx->sp, 107); gb_write8(ctx, --ctx->sp, 252); ctx->pc = 0x6C8C; ctx->cycles += 24;
    return;
}

void recompiled_block_00C0(GB_Context *ctx) {
    // 0x00C0: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_6C8C(GB_Context *ctx) {
    // 0x6C8C: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x6C8D: LD HL, nn
    ctx->h = 0x6D; ctx->l = 0x47; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C90: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 147); ctx->pc = 0x3C36; ctx->cycles += 24;
    return;
}

void recompiled_block_6BFC(GB_Context *ctx) {
    // 0x6BFC: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x6BFD: JP C, nn
    if (ctx->f & FLAG_C) { ctx->pc = 0x6C5F; ctx->cycles += 16; } else { ctx->pc += 3; ctx->cycles += 12; }
    return;
}

void recompiled_block_3C36(GB_Context *ctx) {
    // 0x3C36: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x3C37: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x3C39: LD (nn), A
    gb_write8(ctx, 0xD124, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x3C3C: CALL nn
    gb_write8(ctx, --ctx->sp, 60); gb_write8(ctx, --ctx->sp, 63); ctx->pc = 0x3010; ctx->cycles += 24;
    return;
}

void recompiled_block_6C93(GB_Context *ctx) {
    // 0x6C93: LD HL, nn
    ctx->h = 0xC4; ctx->l = 0x3A; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C96: LD BC, nn
    ctx->b = 0x08; ctx->c = 0x0F; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C99: LD A, n
    ctx->a = 0x14; ctx->cycles += 8; ctx->pc += 2;
    // 0x6C9B: LD (nn), A
    gb_write8(ctx, 0xD124, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x6C9E: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 161); ctx->pc = 0x3010; ctx->cycles += 24;
    return;
}

void recompiled_block_6C5F(GB_Context *ctx) {
    // 0x6C5F: LD HL, nn
    ctx->h = 0x6D; ctx->l = 0x3D; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C62: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 101); ctx->pc = 0x3C36; ctx->cycles += 24;
    return;
}

void recompiled_block_6C00(GB_Context *ctx) {
    // 0x6C00: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x6C01: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x6C02: LD (nn), A
    gb_write8(ctx, 0xD11D, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x6C05: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 8); ctx->pc = 0x2F4D; ctx->cycles += 24;
    return;
}

void recompiled_block_3010(GB_Context *ctx) {
    // 0x3010: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x3012: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x3013: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x3015: CALL nn
    gb_write8(ctx, --ctx->sp, 48); gb_write8(ctx, --ctx->sp, 24); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_3C3F(GB_Context *ctx) {
    // 0x3C3F: CALL nn
    gb_write8(ctx, --ctx->sp, 60); gb_write8(ctx, --ctx->sp, 66); ctx->pc = 0x231C; ctx->cycles += 24;
    return;
}

void recompiled_block_6CA1(GB_Context *ctx) {
    // 0x6CA1: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x6CA2: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCC26); ctx->cycles += 16; ctx->pc += 3;
    // 0x6CA5: Unknown Opcode 0x1F
    fallback_interpreter(ctx, 0x1F);
    return;
}

void recompiled_block_6C65(GB_Context *ctx) {
    // 0x6C65: LD HL, nn
    ctx->h = 0xC4; ctx->l = 0x3A; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C68: LD BC, nn
    ctx->b = 0x08; ctx->c = 0x0F; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C6B: LD A, n
    ctx->a = 0x14; ctx->cycles += 8; ctx->pc += 2;
    // 0x6C6D: LD (nn), A
    gb_write8(ctx, 0xD124, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x6C70: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 115); ctx->pc = 0x3010; ctx->cycles += 24;
    return;
}

void recompiled_block_2F4D(GB_Context *ctx) {
    // 0x2F4D: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x2F4E: LD A, n
    ctx->a = 0x02; ctx->cycles += 8; ctx->pc += 2;
    // 0x2F50: LD (nn), A
    gb_write8(ctx, 0xD0B5, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2F53: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD11D); ctx->cycles += 16; ctx->pc += 3;
    // 0x2F56: LD (nn), A
    gb_write8(ctx, 0xD0B4, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2F59: LD A, n
    ctx->a = 0x2F; ctx->cycles += 8; ctx->pc += 2;
    // 0x2F5B: LD (nn), A
    gb_write8(ctx, 0xD0B6, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2F5E: CALL nn
    gb_write8(ctx, --ctx->sp, 47); gb_write8(ctx, --ctx->sp, 97); ctx->pc = 0x3762; ctx->cycles += 24;
    return;
}

void recompiled_block_6C08(GB_Context *ctx) {
    // 0x6C08: LD HL, nn
    ctx->h = 0x6D; ctx->l = 0x4C; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C0B: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 14); ctx->pc = 0x3C36; ctx->cycles += 24;
    return;
}

void recompiled_block_3018(GB_Context *ctx) {
    // 0x3018: CALL nn
    gb_write8(ctx, --ctx->sp, 48); gb_write8(ctx, --ctx->sp, 27); ctx->pc = 0x71BF; ctx->cycles += 24;
    return;
}

void recompiled_block_231C(GB_Context *ctx) {
    // 0x231C: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCFCA); ctx->cycles += 16; ctx->pc += 3;
    // 0x231F: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x2320: RET NZ
    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }
    return;
}

void recompiled_block_3C42(GB_Context *ctx) {
    // 0x3C42: CALL nn
    gb_write8(ctx, --ctx->sp, 60); gb_write8(ctx, --ctx->sp, 69); ctx->pc = 0x3DDB; ctx->cycles += 24;
    return;
}

void recompiled_block_6C73(GB_Context *ctx) {
    // 0x6C73: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xCC26); ctx->cycles += 16; ctx->pc += 3;
    // 0x6C76: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x6C77: JP NZ, nn
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x6BE0; ctx->cycles += 16; } else { ctx->pc += 3; ctx->cycles += 12; }
    return;
}

void recompiled_block_3762(GB_Context *ctx) {
    // 0x3762: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD0B4); ctx->cycles += 16; ctx->pc += 3;
    // 0x3765: LD (nn), A
    gb_write8(ctx, 0xD11D, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x3768: CP n
    {
        uint8_t val = 0xC4;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x376A: JP NC, nn
    if (!(ctx->f & FLAG_C)) { ctx->pc = 0x2EE8; ctx->cycles += 16; } else { ctx->pc += 3; ctx->cycles += 12; }
    return;
}

void recompiled_block_2F61(GB_Context *ctx) {
    // 0x2F61: LD DE, nn
    ctx->d = 0xCD; ctx->e = 0x6D; ctx->cycles += 12; ctx->pc += 3;
    // 0x2F64: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2F65: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_6C0E(GB_Context *ctx) {
    // 0x6C0E: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x6C0F: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x6C10: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD0DF); ctx->cycles += 16; ctx->pc += 3;
    // 0x6C13: LD (HL), A
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;
    // 0x6C14: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x15; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C17: Unknown Opcode 0x09
    fallback_interpreter(ctx, 0x09);
    return;
}

void recompiled_block_71BF(GB_Context *ctx) {
    // 0x71BF: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD124); ctx->cycles += 16; ctx->pc += 3;
    // 0x71C2: CP n
    {
        uint8_t val = 0x14;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x71C4: JP Z, nn
    if (ctx->f & FLAG_Z) { ctx->pc = 0x742D; ctx->cycles += 16; } else { ctx->pc += 3; ctx->cycles += 12; }
    return;
}

void recompiled_block_301B(GB_Context *ctx) {
    // 0x301B: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x301C: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x301D: CALL nn
    gb_write8(ctx, --ctx->sp, 48); gb_write8(ctx, --ctx->sp, 32); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_2321(GB_Context *ctx) {
    // 0x2321: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x2323: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x2324: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x2326: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 41); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_3DDB(GB_Context *ctx) {
    // 0x3DDB: LD C, n
    ctx->c = 0x03; ctx->cycles += 8; ctx->pc += 2;
    // 0x3DDD: JP nn
    ctx->pc = 0x372F; ctx->cycles += 16;
    return;
}

void recompiled_block_3C45(GB_Context *ctx) {
    // 0x3C45: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x3C46: LD BC, nn
    ctx->b = 0xC4; ctx->c = 0xB9; ctx->cycles += 12; ctx->pc += 3;
    // 0x3C49: JP nn
    ctx->pc = 0x1919; ctx->cycles += 16;
    return;
}

void recompiled_block_6C7A(GB_Context *ctx) {
    // 0x6C7A: LD HL, nn
    ctx->h = 0x6D; ctx->l = 0x42; ctx->cycles += 12; ctx->pc += 3;
    // 0x6C7D: CALL nn
    gb_write8(ctx, --ctx->sp, 108); gb_write8(ctx, --ctx->sp, 128); ctx->pc = 0x3C36; ctx->cycles += 24;
    return;
}

void recompiled_block_2EE8(GB_Context *ctx) {
    // 0x2EE8: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x2EE9: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x2EEA: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x2EEB: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD11D); ctx->cycles += 16; ctx->pc += 3;
    // 0x2EEE: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x2EEF: CP n
    {
        uint8_t val = 0xC9;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x2EF1: JR NC, e
    if (!(ctx->f & FLAG_C)) { ctx->pc = 0x2F00; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_376D(GB_Context *ctx) {
    // 0x376D: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x376F: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x3770: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x3771: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x3772: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x3773: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD0B5); ctx->cycles += 16; ctx->pc += 3;
    // 0x3776: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x3777: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x3784; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_742D(GB_Context *ctx) {
    // 0x742D: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x742E: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD72F); ctx->cycles += 16; ctx->pc += 3;
    // 0x7431: CB F7 -> SET 6, A
    ctx->a = ctx->a | (1 << 6); ctx->cycles += 8; ctx->pc += 2;
    // 0x7433: LD (nn), A
    gb_write8(ctx, 0xD72F, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7436: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x7437: LD (nn), A
    gb_write8(ctx, 0xD12C, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x743A: LD (nn), A
    gb_write8(ctx, 0xD12D, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x743D: LD A, n
    ctx->a = 0x03; ctx->cycles += 8; ctx->pc += 2;
    // 0x743F: LD (nn), A
    gb_write8(ctx, 0xCC29, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7442: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x7444: LD (nn), A
    gb_write8(ctx, 0xCC28, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7447: LD A, B
    ctx->a = ctx->b; ctx->cycles += 4; ctx->pc += 1;
    // 0x7448: LD (nn), A
    gb_write8(ctx, 0xCC24, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x744B: LD A, C
    ctx->a = ctx->c; ctx->cycles += 4; ctx->pc += 1;
    // 0x744C: LD (nn), A
    gb_write8(ctx, 0xCC25, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x744F: XOR A
    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;
    // 0x7450: LD (nn), A
    gb_write8(ctx, 0xCC2A, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7453: LD (nn), A
    gb_write8(ctx, 0xCC37, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7456: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x7457: LD HL, nn
    ctx->h = 0xD1; ctx->l = 0x2B; ctx->cycles += 12; ctx->pc += 3;
    // 0x745A: CB 7E -> BIT 7, (HL)
    ctx->f = (ctx->f & FLAG_C) | FLAG_H | ((gb_read8(ctx, (ctx->h << 8) | ctx->l) & (1 << 7)) == 0 ? FLAG_Z : 0); ctx->cycles += 16; ctx->pc += 2;
    // 0x745C: CB BE -> RES 7, (HL)
    gb_write8(ctx, (ctx->h << 8) | ctx->l, gb_read8(ctx, (ctx->h << 8) | ctx->l) & ~(1 << 7)); ctx->cycles += 16; ctx->pc += 2;
    // 0x745E: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x7461; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_71C7(GB_Context *ctx) {
    // 0x71C7: LD C, A
    ctx->c = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x71C8: LD HL, nn
    ctx->h = 0x72; ctx->l = 0x5C; ctx->cycles += 12; ctx->pc += 3;
    // 0x71CB: LD DE, nn
    ctx->d = 0x00; ctx->e = 0x03; ctx->cycles += 12; ctx->pc += 3;
    // 0x71CE: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 209); ctx->pc = 0x7221; ctx->cycles += 24;
    return;
}

void recompiled_block_3020(GB_Context *ctx) {
    // 0x3020: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_2329(GB_Context *ctx) {
    // 0x2329: LD A, n
    ctx->a = 0xFF; ctx->cycles += 8; ctx->pc += 2;
    // 0x232B: LD (nn), A
    gb_write8(ctx, 0xCFCA, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x232E: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 49); ctx->pc = 0x4BB7; ctx->cycles += 24;
    return;
}

void recompiled_block_372F(GB_Context *ctx) {
    // 0x372F: CALL nn
    gb_write8(ctx, --ctx->sp, 55); gb_write8(ctx, --ctx->sp, 50); ctx->pc = 0x1E64; ctx->cycles += 24;
    return;
}

void recompiled_block_1919(GB_Context *ctx) {
    // 0x1919: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD357); ctx->cycles += 16; ctx->pc += 3;
    // 0x191C: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x191D: CB CF -> SET 1, A
    ctx->a = ctx->a | (1 << 1); ctx->cycles += 8; ctx->pc += 2;
    // 0x191F: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x1920: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xF9); ctx->cycles += 12; ctx->pc += 2;
    // 0x1922: Unknown Opcode 0xAB
    fallback_interpreter(ctx, 0xAB);
    return;
}

void recompiled_block_6C80(GB_Context *ctx) {
    // 0x6C80: LD B, n
    ctx->b = 0x00; ctx->cycles += 8; ctx->pc += 2;
    // 0x6C82: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_2F00(GB_Context *ctx) {
    // 0x2F00: LD HL, nn
    ctx->h = 0x2F; ctx->l = 0x31; ctx->cycles += 12; ctx->pc += 3;
    // 0x2F03: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x02; ctx->cycles += 12; ctx->pc += 3;
    // 0x2F06: LD DE, nn
    ctx->d = 0xCD; ctx->e = 0x6D; ctx->cycles += 12; ctx->pc += 3;
    // 0x2F09: CALL nn
    gb_write8(ctx, --ctx->sp, 47); gb_write8(ctx, --ctx->sp, 12); ctx->pc = 0x00B1; ctx->cycles += 24;
    return;
}

void recompiled_block_2EF3(GB_Context *ctx) {
    // 0x2EF3: Unknown Opcode 0xC6
    fallback_interpreter(ctx, 0xC6);
    return;
}

void recompiled_block_3784(GB_Context *ctx) {
    // 0x3784: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD0B6); ctx->cycles += 16; ctx->pc += 3;
    // 0x3787: CALL nn
    gb_write8(ctx, --ctx->sp, 55); gb_write8(ctx, --ctx->sp, 138); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_3779(GB_Context *ctx) {
    // 0x3779: CALL nn
    gb_write8(ctx, --ctx->sp, 55); gb_write8(ctx, --ctx->sp, 124); ctx->pc = 0x2E93; ctx->cycles += 24;
    return;
}

void recompiled_block_7461(GB_Context *ctx) {
    // 0x7461: LD (nn), A
    gb_write8(ctx, 0xCC26, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7464: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x7465: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x7466: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x7467: CALL nn
    gb_write8(ctx, --ctx->sp, 116); gb_write8(ctx, --ctx->sp, 106); ctx->pc = 0x74E7; ctx->cycles += 24;
    return;
}

void recompiled_block_7460(GB_Context *ctx) {
    // 0x7460: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x7461: LD (nn), A
    gb_write8(ctx, 0xCC26, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7464: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x7465: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x7466: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x7467: CALL nn
    gb_write8(ctx, --ctx->sp, 116); gb_write8(ctx, --ctx->sp, 106); ctx->pc = 0x74E7; ctx->cycles += 24;
    return;
}

void recompiled_block_7221(GB_Context *ctx) {
    // 0x7221: DEC DE
    { uint16_t de = (ctx->d << 8) | ctx->e; de--; ctx->d = de >> 8; ctx->e = de & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x7222: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x7223: CP n
    {
        uint8_t val = 0xFF;
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }
    ctx->cycles += 8; ctx->pc += 2;
    // 0x7225: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x722E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_71D1(GB_Context *ctx) {
    // 0x71D1: JR C, e
    if (ctx->f & FLAG_C) { ctx->pc = 0x71EA; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_4BB7(GB_Context *ctx) {
    // 0x4BB7: LD H, n
    ctx->h = 0xC1; ctx->cycles += 8; ctx->pc += 2;
    // 0x4BB9: INC H
    ctx->h++; ctx->f = (ctx->f & FLAG_C) | (ctx->h == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x4BBA: LD A, n
    ctx->a = 0x0E; ctx->cycles += 8; ctx->pc += 2;
    // 0x4BBC: LD L, A
    ctx->l = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x4BBD: Unknown Opcode 0xD6
    fallback_interpreter(ctx, 0xD6);
    return;
}

void recompiled_block_2331(GB_Context *ctx) {
    // 0x2331: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x2333: LD (nn), A
    gb_write8(ctx, 0xCFCA, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2336: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x2337: CALL nn
    gb_write8(ctx, --ctx->sp, 35); gb_write8(ctx, --ctx->sp, 58); ctx->pc = 0x3E7E; ctx->cycles += 24;
    return;
}

void recompiled_block_1E64(GB_Context *ctx) {
    // 0x1E64: LD A, n
    ctx->a = 0x01; ctx->cycles += 8; ctx->pc += 2;
    // 0x1E66: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xD6, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x1E68: Unknown Opcode 0x76
    fallback_interpreter(ctx, 0x76);
    return;
}

void recompiled_block_3732(GB_Context *ctx) {
    // 0x3732: DEC C
    ctx->c--; ctx->f = (ctx->f & FLAG_C) | (ctx->c == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x3733: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x372F; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2F0C(GB_Context *ctx) {
    // 0x2F0C: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD11D); ctx->cycles += 16; ctx->pc += 3;
    // 0x2F0F: Unknown Opcode 0xD6
    fallback_interpreter(ctx, 0xD6);
    return;
}

void recompiled_block_378A(GB_Context *ctx) {
    // 0x378A: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD0B5); ctx->cycles += 16; ctx->pc += 3;
    // 0x378D: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x378E: Unknown Opcode 0x87
    fallback_interpreter(ctx, 0x87);
    return;
}

void recompiled_block_2E93(GB_Context *ctx) {
    // 0x2E93: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x2E94: LDH A, (n)
    ctx->a = gb_read8(ctx, 0xFF00 + 0xB8); ctx->cycles += 12; ctx->pc += 2;
    // 0x2E96: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x2E97: LD A, n
    ctx->a = 0x3A; ctx->cycles += 8; ctx->pc += 2;
    // 0x2E99: LDH (n), A
    gb_write8(ctx, 0xFF00 + 0xB8, ctx->a); ctx->cycles += 12; ctx->pc += 2;
    // 0x2E9B: LD (nn), A
    gb_write8(ctx, 0x2000, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x2E9E: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD11D); ctx->cycles += 16; ctx->pc += 3;
    // 0x2EA1: DEC A
    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x2EA2: LD HL, nn
    ctx->h = 0x40; ctx->l = 0x00; ctx->cycles += 12; ctx->pc += 3;
    // 0x2EA5: LD C, n
    ctx->c = 0x0A; ctx->cycles += 8; ctx->pc += 2;
    // 0x2EA7: LD B, n
    ctx->b = 0x00; ctx->cycles += 8; ctx->pc += 2;
    // 0x2EA9: CALL nn
    gb_write8(ctx, --ctx->sp, 46); gb_write8(ctx, --ctx->sp, 172); ctx->pc = 0x3A74; ctx->cycles += 24;
    return;
}

void recompiled_block_377C(GB_Context *ctx) {
    // 0x377C: LD HL, nn
    ctx->h = 0x00; ctx->l = 0x0B; ctx->cycles += 12; ctx->pc += 3;
    // 0x377F: Unknown Opcode 0x19
    fallback_interpreter(ctx, 0x19);
    return;
}

void recompiled_block_74E7(GB_Context *ctx) {
    // 0x74E7: LD DE, nn
    ctx->d = 0xCE; ctx->e = 0xE9; ctx->cycles += 12; ctx->pc += 3;
    // 0x74EA: LD BC, nn
    ctx->b = 0x05; ctx->c = 0x06; ctx->cycles += 12; ctx->pc += 3;
    // 0x74ED: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x74EE: Unknown Opcode 0x12
    fallback_interpreter(ctx, 0x12);
    return;
}

void recompiled_block_746A(GB_Context *ctx) {
    // 0x746A: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD12B); ctx->cycles += 16; ctx->pc += 3;
    // 0x746D: LD HL, nn
    ctx->h = 0x75; ctx->l = 0x1A; ctx->cycles += 12; ctx->pc += 3;
    // 0x7470: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x7471: LD D, n
    ctx->d = 0x00; ctx->cycles += 8; ctx->pc += 2;
    // 0x7473: LD A, n
    ctx->a = 0x05; ctx->cycles += 8; ctx->pc += 2;
    // 0x7475: Unknown Opcode 0x19
    fallback_interpreter(ctx, 0x19);
    return;
}

void recompiled_block_722E(GB_Context *ctx) {
    // 0x722E: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_7227(GB_Context *ctx) {
    // 0x7227: Unknown Opcode 0xB9
    fallback_interpreter(ctx, 0xB9);
    return;
}

void recompiled_block_71EA(GB_Context *ctx) {
    // 0x71EA: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x71EB: LD H, (HL)
    ctx->h = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x71EC: LD L, A
    ctx->l = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x71ED: LD DE, nn
    ctx->d = 0x71; ctx->e = 0xE9; ctx->cycles += 12; ctx->pc += 3;
    // 0x71F0: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x71F1: JP (HL)
    ctx->pc = (ctx->h << 8) | ctx->l; ctx->cycles += 4;
    return;
}

void recompiled_block_71D3(GB_Context *ctx) {
    // 0x71D3: LD HL, nn
    ctx->h = 0x72; ctx->l = 0x66; ctx->cycles += 12; ctx->pc += 3;
    // 0x71D6: LD DE, nn
    ctx->d = 0x00; ctx->e = 0x05; ctx->cycles += 12; ctx->pc += 3;
    // 0x71D9: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 220); ctx->pc = 0x7221; ctx->cycles += 24;
    return;
}

void recompiled_block_233A(GB_Context *ctx) {
    // 0x233A: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_3735(GB_Context *ctx) {
    // 0x3735: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_2EAC(GB_Context *ctx) {
    // 0x2EAC: LD DE, nn
    ctx->d = 0xCD; ctx->e = 0x6D; ctx->cycles += 12; ctx->pc += 3;
    // 0x2EAF: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x2EB0: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x0A; ctx->cycles += 12; ctx->pc += 3;
    // 0x2EB3: CALL nn
    gb_write8(ctx, --ctx->sp, 46); gb_write8(ctx, --ctx->sp, 182); ctx->pc = 0x00B1; ctx->cycles += 24;
    return;
}

void recompiled_block_71DC(GB_Context *ctx) {
    // 0x71DC: JR C, e
    if (ctx->f & FLAG_C) { ctx->pc = 0x71F2; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_2EB6(GB_Context *ctx) {
    // 0x2EB6: LD HL, nn
    ctx->h = 0xCD; ctx->l = 0x77; ctx->cycles += 12; ctx->pc += 3;
    // 0x2EB9: Unknown Opcode 0x36
    fallback_interpreter(ctx, 0x36);
    return;
}

void recompiled_block_71F2(GB_Context *ctx) {
    // 0x71F2: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 245); ctx->pc = 0x722F; ctx->cycles += 24;
    return;
}

void recompiled_block_71DE(GB_Context *ctx) {
    // 0x71DE: LD HL, nn
    ctx->h = 0x72; ctx->l = 0x85; ctx->cycles += 12; ctx->pc += 3;
    // 0x71E1: LD DE, nn
    ctx->d = 0x00; ctx->e = 0x09; ctx->cycles += 12; ctx->pc += 3;
    // 0x71E4: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 231); ctx->pc = 0x7221; ctx->cycles += 24;
    return;
}

void recompiled_block_722F(GB_Context *ctx) {
    // 0x722F: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x7230: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x7231: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x7232: LD D, A
    ctx->d = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x7233: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x7234: Unknown Opcode 0x93
    fallback_interpreter(ctx, 0x93);
    return;
}

void recompiled_block_71F5(GB_Context *ctx) {
    // 0x71F5: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 248); ctx->pc = 0x724A; ctx->cycles += 24;
    return;
}

void recompiled_block_71E7(GB_Context *ctx) {
    // 0x71E7: JR C, e
    if (ctx->f & FLAG_C) { ctx->pc = 0x71FC; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_724A(GB_Context *ctx) {
    // 0x724A: PUSH BC
    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;
    // 0x724B: LD HL, nn
    ctx->h = 0xC3; ctx->l = 0xA0; ctx->cycles += 12; ctx->pc += 3;
    // 0x724E: LD BC, nn
    ctx->b = 0x00; ctx->c = 0x14; ctx->cycles += 12; ctx->pc += 3;
    // 0x7251: LD A, D
    ctx->a = ctx->d; ctx->cycles += 4; ctx->pc += 1;
    // 0x7252: AND A
    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;
    // 0x7253: JR Z, e
    if (ctx->f & FLAG_Z) { ctx->pc = 0x7259; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_71F8(GB_Context *ctx) {
    // 0x71F8: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 251); ctx->pc = 0x16F0; ctx->cycles += 24;
    return;
}

void recompiled_block_71FC(GB_Context *ctx) {
    // 0x71FC: CALL nn
    gb_write8(ctx, --ctx->sp, 113); gb_write8(ctx, --ctx->sp, 255); ctx->pc = 0x722F; ctx->cycles += 24;
    return;
}

void recompiled_block_71E9(GB_Context *ctx) {
    // 0x71E9: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_7259(GB_Context *ctx) {
    // 0x7259: POP BC
    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x725A: Unknown Opcode 0x19
    fallback_interpreter(ctx, 0x19);
    return;
}

void recompiled_block_7255(GB_Context *ctx) {
    // 0x7255: Unknown Opcode 0x09
    fallback_interpreter(ctx, 0x09);
    return;
}

void recompiled_block_16F0(GB_Context *ctx) {
    // 0x16F0: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x16F1: LD A, n
    ctx->a = 0x79; ctx->cycles += 8; ctx->pc += 2;
    // 0x16F3: LD (HL+), A
    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x16F4: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x16F5: CALL nn
    gb_write8(ctx, --ctx->sp, 22); gb_write8(ctx, --ctx->sp, 248); ctx->pc = 0x171D; ctx->cycles += 24;
    return;
}

void recompiled_block_71FB(GB_Context *ctx) {
    // 0x71FB: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_71FF(GB_Context *ctx) {
    // 0x71FF: PUSH HL
    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;
    // 0x7200: CALL nn
    gb_write8(ctx, --ctx->sp, 114); gb_write8(ctx, --ctx->sp, 3); ctx->pc = 0x724A; ctx->cycles += 24;
    return;
}

void recompiled_block_171D(GB_Context *ctx) {
    // 0x171D: LD D, C
    ctx->d = ctx->c; ctx->cycles += 4; ctx->pc += 1;
    // 0x171E: LD (HL+), A
    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x171F: DEC D
    ctx->d--; ctx->f = (ctx->f & FLAG_C) | (ctx->d == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x1720: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x171E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_16F8(GB_Context *ctx) {
    // 0x16F8: INC A
    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;
    // 0x16F9: LD (HL), A
    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;
    // 0x16FA: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x16FB: LD DE, nn
    ctx->d = 0x00; ctx->e = 0x14; ctx->cycles += 12; ctx->pc += 3;
    // 0x16FE: Unknown Opcode 0x19
    fallback_interpreter(ctx, 0x19);
    return;
}

void recompiled_block_7203(GB_Context *ctx) {
    // 0x7203: CALL nn
    gb_write8(ctx, --ctx->sp, 114); gb_write8(ctx, --ctx->sp, 6); ctx->pc = 0x16F0; ctx->cycles += 24;
    return;
}

void recompiled_block_171E(GB_Context *ctx) {
    // 0x171E: LD (HL+), A
    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x171F: DEC D
    ctx->d--; ctx->f = (ctx->f & FLAG_C) | (ctx->d == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;
    // 0x1720: JR NZ, e
    if (!(ctx->f & FLAG_Z)) { ctx->pc = 0x171E; ctx->cycles += 12; } else { ctx->pc += 2; ctx->cycles += 8; }
    return;
}

void recompiled_block_1722(GB_Context *ctx) {
    // 0x1722: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_7206(GB_Context *ctx) {
    // 0x7206: POP HL
    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x7207: CALL nn
    gb_write8(ctx, --ctx->sp, 114); gb_write8(ctx, --ctx->sp, 10); ctx->pc = 0x723C; ctx->cycles += 24;
    return;
}

void recompiled_block_723C(GB_Context *ctx) {
    // 0x723C: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x723D: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x723E: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x723F: LD D, A
    ctx->d = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x7240: PUSH DE
    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;
    // 0x7241: LD A, (HL+)
    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;
    // 0x7242: LD E, A
    ctx->e = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x7243: LD A, (HL)
    ctx->a = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;
    // 0x7244: LD D, A
    ctx->d = ctx->a; ctx->cycles += 4; ctx->pc += 1;
    // 0x7245: CALL nn
    gb_write8(ctx, --ctx->sp, 114); gb_write8(ctx, --ctx->sp, 72); ctx->pc = 0x724A; ctx->cycles += 24;
    return;
}

void recompiled_block_720A(GB_Context *ctx) {
    // 0x720A: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD72F); ctx->cycles += 16; ctx->pc += 3;
    // 0x720D: PUSH AF
    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;
    // 0x720E: LD A, (nn)
    ctx->a = gb_read8(ctx, 0xD72F); ctx->cycles += 16; ctx->pc += 3;
    // 0x7211: CB F7 -> SET 6, A
    ctx->a = ctx->a | (1 << 6); ctx->cycles += 8; ctx->pc += 2;
    // 0x7213: LD (nn), A
    gb_write8(ctx, 0xD72F, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x7216: CALL nn
    gb_write8(ctx, --ctx->sp, 114); gb_write8(ctx, --ctx->sp, 25); ctx->pc = 0x1723; ctx->cycles += 24;
    return;
}

void recompiled_block_7248(GB_Context *ctx) {
    // 0x7248: POP DE
    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x7249: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void recompiled_block_7219(GB_Context *ctx) {
    // 0x7219: POP AF
    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;
    // 0x721A: LD (nn), A
    gb_write8(ctx, 0xD72F, ctx->a); ctx->cycles += 16; ctx->pc += 3;
    // 0x721D: CALL nn
    gb_write8(ctx, --ctx->sp, 114); gb_write8(ctx, --ctx->sp, 32); ctx->pc = 0x231C; ctx->cycles += 24;
    return;
}

void recompiled_block_7220(GB_Context *ctx) {
    // 0x7220: RET
    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;
    return;
}

void step_recompiled(GB_Context *ctx) {
    switch(ctx->pc) {
        case 0x0000: recompiled_block_0000(ctx); break;
        case 0x0038: recompiled_block_0038(ctx); break;
        case 0x0040: recompiled_block_0040(ctx); break;
        case 0x0048: recompiled_block_0048(ctx); break;
        case 0x0050: recompiled_block_0050(ctx); break;
        case 0x0058: recompiled_block_0058(ctx); break;
        case 0x0060: recompiled_block_0060(ctx); break;
        case 0x0061: recompiled_block_0061(ctx); break;
        case 0x0066: recompiled_block_0066(ctx); break;
        case 0x006B: recompiled_block_006B(ctx); break;
        case 0x0071: recompiled_block_0071(ctx); break;
        case 0x008D: recompiled_block_008D(ctx); break;
        case 0x00B1: recompiled_block_00B1(ctx); break;
        case 0x00B5: recompiled_block_00B5(ctx); break;
        case 0x00B9: recompiled_block_00B9(ctx); break;
        case 0x00BA: recompiled_block_00BA(ctx); break;
        case 0x00BD: recompiled_block_00BD(ctx); break;
        case 0x00C0: recompiled_block_00C0(ctx); break;
        case 0x00C1: recompiled_block_00C1(ctx); break;
        case 0x0100: recompiled_block_0100(ctx); break;
        case 0x01AB: recompiled_block_01AB(ctx); break;
        case 0x01AF: recompiled_block_01AF(ctx); break;
        case 0x01B2: recompiled_block_01B2(ctx); break;
        case 0x01B4: recompiled_block_01B4(ctx); break;
        case 0x01C8: recompiled_block_01C8(ctx); break;
        case 0x01D0: recompiled_block_01D0(ctx); break;
        case 0x01D3: recompiled_block_01D3(ctx); break;
        case 0x15AC: recompiled_block_15AC(ctx); break;
        case 0x15B2: recompiled_block_15B2(ctx); break;
        case 0x15C1: recompiled_block_15C1(ctx); break;
        case 0x16F0: recompiled_block_16F0(ctx); break;
        case 0x16F8: recompiled_block_16F8(ctx); break;
        case 0x171D: recompiled_block_171D(ctx); break;
        case 0x171E: recompiled_block_171E(ctx); break;
        case 0x1722: recompiled_block_1722(ctx); break;
        case 0x1723: recompiled_block_1723(ctx); break;
        case 0x1919: recompiled_block_1919(ctx); break;
        case 0x1ADA: recompiled_block_1ADA(ctx); break;
        case 0x1ADE: recompiled_block_1ADE(ctx); break;
        case 0x1AE5: recompiled_block_1AE5(ctx); break;
        case 0x1B09: recompiled_block_1B09(ctx); break;
        case 0x1B16: recompiled_block_1B16(ctx); break;
        case 0x1B1B: recompiled_block_1B1B(ctx); break;
        case 0x1B30: recompiled_block_1B30(ctx); break;
        case 0x1B34: recompiled_block_1B34(ctx); break;
        case 0x1BB5: recompiled_block_1BB5(ctx); break;
        case 0x1BB9: recompiled_block_1BB9(ctx); break;
        case 0x1BD1: recompiled_block_1BD1(ctx); break;
        case 0x1BD5: recompiled_block_1BD5(ctx); break;
        case 0x1C21: recompiled_block_1C21(ctx); break;
        case 0x1C25: recompiled_block_1C25(ctx); break;
        case 0x1C75: recompiled_block_1C75(ctx); break;
        case 0x1C79: recompiled_block_1C79(ctx); break;
        case 0x1C7E: recompiled_block_1C7E(ctx); break;
        case 0x1C86: recompiled_block_1C86(ctx); break;
        case 0x1C8A: recompiled_block_1C8A(ctx); break;
        case 0x1C9C: recompiled_block_1C9C(ctx); break;
        case 0x1CA4: recompiled_block_1CA4(ctx); break;
        case 0x1CB2: recompiled_block_1CB2(ctx); break;
        case 0x1CC1: recompiled_block_1CC1(ctx); break;
        case 0x1CC6: recompiled_block_1CC6(ctx); break;
        case 0x1CC9: recompiled_block_1CC9(ctx); break;
        case 0x1D10: recompiled_block_1D10(ctx); break;
        case 0x1D33: recompiled_block_1D33(ctx); break;
        case 0x1DE5: recompiled_block_1DE5(ctx); break;
        case 0x1E02: recompiled_block_1E02(ctx); break;
        case 0x1E06: recompiled_block_1E06(ctx); break;
        case 0x1E09: recompiled_block_1E09(ctx); break;
        case 0x1E0C: recompiled_block_1E0C(ctx); break;
        case 0x1E0F: recompiled_block_1E0F(ctx); break;
        case 0x1E12: recompiled_block_1E12(ctx); break;
        case 0x1E15: recompiled_block_1E15(ctx); break;
        case 0x1E18: recompiled_block_1E18(ctx); break;
        case 0x1E1B: recompiled_block_1E1B(ctx); break;
        case 0x1E25: recompiled_block_1E25(ctx); break;
        case 0x1E28: recompiled_block_1E28(ctx); break;
        case 0x1E2B: recompiled_block_1E2B(ctx); break;
        case 0x1E2E: recompiled_block_1E2E(ctx); break;
        case 0x1E33: recompiled_block_1E33(ctx); break;
        case 0x1E36: recompiled_block_1E36(ctx); break;
        case 0x1E3B: recompiled_block_1E3B(ctx); break;
        case 0x1E3E: recompiled_block_1E3E(ctx); break;
        case 0x1E41: recompiled_block_1E41(ctx); break;
        case 0x1E46: recompiled_block_1E46(ctx); break;
        case 0x1E49: recompiled_block_1E49(ctx); break;
        case 0x1E4E: recompiled_block_1E4E(ctx); break;
        case 0x1E51: recompiled_block_1E51(ctx); break;
        case 0x1E54: recompiled_block_1E54(ctx); break;
        case 0x1E64: recompiled_block_1E64(ctx); break;
        case 0x1EF5: recompiled_block_1EF5(ctx); break;
        case 0x1EF8: recompiled_block_1EF8(ctx); break;
        case 0x1EFF: recompiled_block_1EFF(ctx); break;
        case 0x1F05: recompiled_block_1F05(ctx); break;
        case 0x1F0A: recompiled_block_1F0A(ctx); break;
        case 0x1F14: recompiled_block_1F14(ctx); break;
        case 0x1F22: recompiled_block_1F22(ctx); break;
        case 0x1F30: recompiled_block_1F30(ctx); break;
        case 0x1F3E: recompiled_block_1F3E(ctx); break;
        case 0x1F43: recompiled_block_1F43(ctx); break;
        case 0x1F54: recompiled_block_1F54(ctx); break;
        case 0x1F5A: recompiled_block_1F5A(ctx); break;
        case 0x1F5E: recompiled_block_1F5E(ctx); break;
        case 0x1F5F: recompiled_block_1F5F(ctx); break;
        case 0x1F64: recompiled_block_1F64(ctx); break;
        case 0x1F73: recompiled_block_1F73(ctx); break;
        case 0x1F79: recompiled_block_1F79(ctx); break;
        case 0x1F85: recompiled_block_1F85(ctx); break;
        case 0x1F8A: recompiled_block_1F8A(ctx); break;
        case 0x1F98: recompiled_block_1F98(ctx); break;
        case 0x1F9E: recompiled_block_1F9E(ctx); break;
        case 0x1FA8: recompiled_block_1FA8(ctx); break;
        case 0x1FAF: recompiled_block_1FAF(ctx); break;
        case 0x1FB5: recompiled_block_1FB5(ctx); break;
        case 0x1FBB: recompiled_block_1FBB(ctx); break;
        case 0x1FBE: recompiled_block_1FBE(ctx); break;
        case 0x2162: recompiled_block_2162(ctx); break;
        case 0x2165: recompiled_block_2165(ctx); break;
        case 0x216A: recompiled_block_216A(ctx); break;
        case 0x2233: recompiled_block_2233(ctx); break;
        case 0x2238: recompiled_block_2238(ctx); break;
        case 0x2242: recompiled_block_2242(ctx); break;
        case 0x224F: recompiled_block_224F(ctx); break;
        case 0x2255: recompiled_block_2255(ctx); break;
        case 0x225B: recompiled_block_225B(ctx); break;
        case 0x2266: recompiled_block_2266(ctx); break;
        case 0x226A: recompiled_block_226A(ctx); break;
        case 0x2271: recompiled_block_2271(ctx); break;
        case 0x2273: recompiled_block_2273(ctx); break;
        case 0x2284: recompiled_block_2284(ctx); break;
        case 0x22EC: recompiled_block_22EC(ctx); break;
        case 0x22F5: recompiled_block_22F5(ctx); break;
        case 0x22F9: recompiled_block_22F9(ctx); break;
        case 0x22FD: recompiled_block_22FD(ctx); break;
        case 0x22FF: recompiled_block_22FF(ctx); break;
        case 0x2303: recompiled_block_2303(ctx); break;
        case 0x2307: recompiled_block_2307(ctx); break;
        case 0x2309: recompiled_block_2309(ctx); break;
        case 0x230D: recompiled_block_230D(ctx); break;
        case 0x2311: recompiled_block_2311(ctx); break;
        case 0x2313: recompiled_block_2313(ctx); break;
        case 0x2317: recompiled_block_2317(ctx); break;
        case 0x231B: recompiled_block_231B(ctx); break;
        case 0x231C: recompiled_block_231C(ctx); break;
        case 0x2321: recompiled_block_2321(ctx); break;
        case 0x2329: recompiled_block_2329(ctx); break;
        case 0x2331: recompiled_block_2331(ctx); break;
        case 0x233A: recompiled_block_233A(ctx); break;
        case 0x27C2: recompiled_block_27C2(ctx); break;
        case 0x27C8: recompiled_block_27C8(ctx); break;
        case 0x27CE: recompiled_block_27CE(ctx); break;
        case 0x27D3: recompiled_block_27D3(ctx); break;
        case 0x27D9: recompiled_block_27D9(ctx); break;
        case 0x27DE: recompiled_block_27DE(ctx); break;
        case 0x27E9: recompiled_block_27E9(ctx); break;
        case 0x27FA: recompiled_block_27FA(ctx); break;
        case 0x2805: recompiled_block_2805(ctx); break;
        case 0x2E93: recompiled_block_2E93(ctx); break;
        case 0x2EAC: recompiled_block_2EAC(ctx); break;
        case 0x2EB6: recompiled_block_2EB6(ctx); break;
        case 0x2EE8: recompiled_block_2EE8(ctx); break;
        case 0x2EF3: recompiled_block_2EF3(ctx); break;
        case 0x2F00: recompiled_block_2F00(ctx); break;
        case 0x2F0C: recompiled_block_2F0C(ctx); break;
        case 0x2F4D: recompiled_block_2F4D(ctx); break;
        case 0x2F61: recompiled_block_2F61(ctx); break;
        case 0x2FA7: recompiled_block_2FA7(ctx); break;
        case 0x2FAF: recompiled_block_2FAF(ctx); break;
        case 0x2FB2: recompiled_block_2FB2(ctx); break;
        case 0x2FB6: recompiled_block_2FB6(ctx); break;
        case 0x2FB7: recompiled_block_2FB7(ctx); break;
        case 0x2FBD: recompiled_block_2FBD(ctx); break;
        case 0x2FC2: recompiled_block_2FC2(ctx); break;
        case 0x3010: recompiled_block_3010(ctx); break;
        case 0x3018: recompiled_block_3018(ctx); break;
        case 0x301B: recompiled_block_301B(ctx); break;
        case 0x3020: recompiled_block_3020(ctx); break;
        case 0x372F: recompiled_block_372F(ctx); break;
        case 0x3732: recompiled_block_3732(ctx); break;
        case 0x3735: recompiled_block_3735(ctx); break;
        case 0x3762: recompiled_block_3762(ctx); break;
        case 0x376D: recompiled_block_376D(ctx); break;
        case 0x3779: recompiled_block_3779(ctx); break;
        case 0x377C: recompiled_block_377C(ctx); break;
        case 0x3784: recompiled_block_3784(ctx); break;
        case 0x378A: recompiled_block_378A(ctx); break;
        case 0x3A74: recompiled_block_3A74(ctx); break;
        case 0x3A76: recompiled_block_3A76(ctx); break;
        case 0x3C36: recompiled_block_3C36(ctx); break;
        case 0x3C3F: recompiled_block_3C3F(ctx); break;
        case 0x3C42: recompiled_block_3C42(ctx); break;
        case 0x3C45: recompiled_block_3C45(ctx); break;
        case 0x3DDB: recompiled_block_3DDB(ctx); break;
        case 0x3E6D: recompiled_block_3E6D(ctx); break;
        case 0x3E78: recompiled_block_3E78(ctx); break;
        case 0x3E7E: recompiled_block_3E7E(ctx); break;
        case 0x3E84: recompiled_block_3E84(ctx); break;
        case 0x3E90: recompiled_block_3E90(ctx); break;
        case 0x3E98: recompiled_block_3E98(ctx); break;
        case 0x4000: recompiled_block_4000(ctx); break;
        case 0x499B: recompiled_block_499B(ctx); break;
        case 0x49A1: recompiled_block_49A1(ctx); break;
        case 0x49A4: recompiled_block_49A4(ctx); break;
        case 0x49AA: recompiled_block_49AA(ctx); break;
        case 0x4A5E: recompiled_block_4A5E(ctx); break;
        case 0x4BB7: recompiled_block_4BB7(ctx); break;
        case 0x4BF7: recompiled_block_4BF7(ctx); break;
        case 0x4BFF: recompiled_block_4BFF(ctx); break;
        case 0x509D: recompiled_block_509D(ctx); break;
        case 0x50A4: recompiled_block_50A4(ctx); break;
        case 0x50A7: recompiled_block_50A7(ctx); break;
        case 0x50B9: recompiled_block_50B9(ctx); break;
        case 0x50BA: recompiled_block_50BA(ctx); break;
        case 0x510D: recompiled_block_510D(ctx); break;
        case 0x5113: recompiled_block_5113(ctx); break;
        case 0x531E: recompiled_block_531E(ctx); break;
        case 0x584E: recompiled_block_584E(ctx); break;
        case 0x5857: recompiled_block_5857(ctx); break;
        case 0x58BB: recompiled_block_58BB(ctx); break;
        case 0x58BE: recompiled_block_58BE(ctx); break;
        case 0x58C7: recompiled_block_58C7(ctx); break;
        case 0x58CD: recompiled_block_58CD(ctx); break;
        case 0x5B67: recompiled_block_5B67(ctx); break;
        case 0x5B6F: recompiled_block_5B6F(ctx); break;
        case 0x5B94: recompiled_block_5B94(ctx); break;
        case 0x5B95: recompiled_block_5B95(ctx); break;
        case 0x5B99: recompiled_block_5B99(ctx); break;
        case 0x6BD4: recompiled_block_6BD4(ctx); break;
        case 0x6BE0: recompiled_block_6BE0(ctx); break;
        case 0x6BEC: recompiled_block_6BEC(ctx); break;
        case 0x6BF0: recompiled_block_6BF0(ctx); break;
        case 0x6BF4: recompiled_block_6BF4(ctx); break;
        case 0x6BF8: recompiled_block_6BF8(ctx); break;
        case 0x6BFC: recompiled_block_6BFC(ctx); break;
        case 0x6C00: recompiled_block_6C00(ctx); break;
        case 0x6C08: recompiled_block_6C08(ctx); break;
        case 0x6C0E: recompiled_block_6C0E(ctx); break;
        case 0x6C10: recompiled_block_6C10(ctx); break;
        case 0x6C5F: recompiled_block_6C5F(ctx); break;
        case 0x6C65: recompiled_block_6C65(ctx); break;
        case 0x6C73: recompiled_block_6C73(ctx); break;
        case 0x6C7A: recompiled_block_6C7A(ctx); break;
        case 0x6C80: recompiled_block_6C80(ctx); break;
        case 0x6C8C: recompiled_block_6C8C(ctx); break;
        case 0x6C93: recompiled_block_6C93(ctx); break;
        case 0x6CA1: recompiled_block_6CA1(ctx); break;
        case 0x71BF: recompiled_block_71BF(ctx); break;
        case 0x71C7: recompiled_block_71C7(ctx); break;
        case 0x71D1: recompiled_block_71D1(ctx); break;
        case 0x71D3: recompiled_block_71D3(ctx); break;
        case 0x71DC: recompiled_block_71DC(ctx); break;
        case 0x71DE: recompiled_block_71DE(ctx); break;
        case 0x71E7: recompiled_block_71E7(ctx); break;
        case 0x71E9: recompiled_block_71E9(ctx); break;
        case 0x71EA: recompiled_block_71EA(ctx); break;
        case 0x71F2: recompiled_block_71F2(ctx); break;
        case 0x71F5: recompiled_block_71F5(ctx); break;
        case 0x71F8: recompiled_block_71F8(ctx); break;
        case 0x71FB: recompiled_block_71FB(ctx); break;
        case 0x71FC: recompiled_block_71FC(ctx); break;
        case 0x71FF: recompiled_block_71FF(ctx); break;
        case 0x7203: recompiled_block_7203(ctx); break;
        case 0x7206: recompiled_block_7206(ctx); break;
        case 0x720A: recompiled_block_720A(ctx); break;
        case 0x7219: recompiled_block_7219(ctx); break;
        case 0x7220: recompiled_block_7220(ctx); break;
        case 0x7221: recompiled_block_7221(ctx); break;
        case 0x7227: recompiled_block_7227(ctx); break;
        case 0x722E: recompiled_block_722E(ctx); break;
        case 0x722F: recompiled_block_722F(ctx); break;
        case 0x723C: recompiled_block_723C(ctx); break;
        case 0x7248: recompiled_block_7248(ctx); break;
        case 0x724A: recompiled_block_724A(ctx); break;
        case 0x7255: recompiled_block_7255(ctx); break;
        case 0x7259: recompiled_block_7259(ctx); break;
        case 0x742D: recompiled_block_742D(ctx); break;
        case 0x7460: recompiled_block_7460(ctx); break;
        case 0x7461: recompiled_block_7461(ctx); break;
        case 0x746A: recompiled_block_746A(ctx); break;
        case 0x74E7: recompiled_block_74E7(ctx); break;
        case 0xFF80: recompiled_block_FF80(ctx); break;
        default: fallback_interpreter(ctx, gb_read8(ctx, ctx->pc)); break;
    }
}