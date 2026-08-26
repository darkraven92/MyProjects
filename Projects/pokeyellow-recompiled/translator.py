import sys

def sign_extend_8(val):
    return val - 256 if val & 0x80 else val

REG_MAP = ["b", "c", "d", "e", "h", "l", "(HL)", "a"]

# Core Base Opcodes
OPCODES = {
    # NOP & Interrupt Control
    0x00: ("NOP", lambda b, pc: "    ctx->cycles += 4; ctx->pc += 1;", 1),
    0xF3: ("DI", lambda b, pc: "    ctx->ime = false; ctx->cycles += 4; ctx->pc += 1;", 1),
    0xFB: ("EI", lambda b, pc: "    ctx->ime = true; ctx->cycles += 4; ctx->pc += 1;", 1),

    # 8-Bit Loads (Immediate & Direct)
    0x06: ("LD B, n", lambda b, pc: f"    ctx->b = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),
    0x0E: ("LD C, n", lambda b, pc: f"    ctx->c = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),
    0x16: ("LD D, n", lambda b, pc: f"    ctx->d = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),
    0x1E: ("LD E, n", lambda b, pc: f"    ctx->e = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),
    0x26: ("LD H, n", lambda b, pc: f"    ctx->h = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),
    0x2E: ("LD L, n", lambda b, pc: f"    ctx->l = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),
    0x3E: ("LD A, n", lambda b, pc: f"    ctx->a = 0x{b[1]:02X}; ctx->cycles += 8; ctx->pc += 2;", 2),

    # 16-Bit Loads & Pointer Arithmetic
    0x01: ("LD BC, nn", lambda b, pc: f"    ctx->b = 0x{b[2]:02X}; ctx->c = 0x{b[1]:02X}; ctx->cycles += 12; ctx->pc += 3;", 3),
    0x11: ("LD DE, nn", lambda b, pc: f"    ctx->d = 0x{b[2]:02X}; ctx->e = 0x{b[1]:02X}; ctx->cycles += 12; ctx->pc += 3;", 3),
    0x21: ("LD HL, nn", lambda b, pc: f"    ctx->h = 0x{b[2]:02X}; ctx->l = 0x{b[1]:02X}; ctx->cycles += 12; ctx->pc += 3;", 3),
    0x31: ("LD SP, nn", lambda b, pc: f"    ctx->sp = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 12; ctx->pc += 3;", 3),
    0x03: ("INC BC", lambda b, pc: "    { uint16_t bc = (ctx->b << 8) | ctx->c; bc++; ctx->b = bc >> 8; ctx->c = bc & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x13: ("INC DE", lambda b, pc: "    { uint16_t de = (ctx->d << 8) | ctx->e; de++; ctx->d = de >> 8; ctx->e = de & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x23: ("INC HL", lambda b, pc: "    { uint16_t hl = (ctx->h << 8) | ctx->l; hl++; ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x0B: ("DEC BC", lambda b, pc: "    { uint16_t bc = (ctx->b << 8) | ctx->c; bc--; ctx->b = bc >> 8; ctx->c = bc & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x1B: ("DEC DE", lambda b, pc: "    { uint16_t de = (ctx->d << 8) | ctx->e; de--; ctx->d = de >> 8; ctx->e = de & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x2B: ("DEC HL", lambda b, pc: "    { uint16_t hl = (ctx->h << 8) | ctx->l; hl--; ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x2A: ("LD A, (HL+)", lambda b, pc: "    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl++); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x22: ("LD (HL+), A", lambda b, pc: "    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl++, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x32: ("LD (HL-), A", lambda b, pc: "    { uint16_t hl = (ctx->h << 8) | ctx->l; gb_write8(ctx, hl--, ctx->a); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x3A: ("LD A, (HL-)", lambda b, pc: "    { uint16_t hl = (ctx->h << 8) | ctx->l; ctx->a = gb_read8(ctx, hl--); ctx->h = hl >> 8; ctx->l = hl & 0xFF; } ctx->cycles += 8; ctx->pc += 1;", 1),
    0x77: ("LD (HL), A", lambda b, pc: "    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->a); ctx->cycles += 8; ctx->pc += 1;", 1),

    # Memory Direct / High RAM Access
    0xEA: ("LD (nn), A", lambda b, pc: f"    gb_write8(ctx, 0x{(b[2]<<8)|b[1]:04X}, ctx->a); ctx->cycles += 16; ctx->pc += 3;", 3),
    0xFA: ("LD A, (nn)", lambda b, pc: f"    ctx->a = gb_read8(ctx, 0x{(b[2]<<8)|b[1]:04X}); ctx->cycles += 16; ctx->pc += 3;", 3),
    0xE0: ("LDH (n), A", lambda b, pc: f"    gb_write8(ctx, 0xFF00 + 0x{b[1]:02X}, ctx->a); ctx->cycles += 12; ctx->pc += 2;", 2),
    0xF0: ("LDH A, (n)", lambda b, pc: f"    ctx->a = gb_read8(ctx, 0xFF00 + 0x{b[1]:02X}); ctx->cycles += 12; ctx->pc += 2;", 2),
    0xE2: ("LD (C), A", lambda b, pc: "    gb_write8(ctx, 0xFF00 + ctx->c, ctx->a); ctx->cycles += 8; ctx->pc += 1;", 1),
    0xF2: ("LD A, (C)", lambda b, pc: "    ctx->a = gb_read8(ctx, 0xFF00 + ctx->c); ctx->cycles += 8; ctx->pc += 1;", 1),

    # Relative Jumps
    0x18: ("JR e", lambda b, pc: f"    ctx->pc = 0x{(pc + 2 + sign_extend_8(b[1])) & 0xFFFF:04X}; ctx->cycles += 12;", 2),
    0x20: ("JR NZ, e", lambda b, pc: f"    if (!(ctx->f & FLAG_Z)) {{ ctx->pc = 0x{(pc + 2 + sign_extend_8(b[1])) & 0xFFFF:04X}; ctx->cycles += 12; }} else {{ ctx->pc += 2; ctx->cycles += 8; }}", 2),
    0x28: ("JR Z, e", lambda b, pc: f"    if (ctx->f & FLAG_Z) {{ ctx->pc = 0x{(pc + 2 + sign_extend_8(b[1])) & 0xFFFF:04X}; ctx->cycles += 12; }} else {{ ctx->pc += 2; ctx->cycles += 8; }}", 2),
    0x30: ("JR NC, e", lambda b, pc: f"    if (!(ctx->f & FLAG_C)) {{ ctx->pc = 0x{(pc + 2 + sign_extend_8(b[1])) & 0xFFFF:04X}; ctx->cycles += 12; }} else {{ ctx->pc += 2; ctx->cycles += 8; }}", 2),
    0x38: ("JR C, e", lambda b, pc: f"    if (ctx->f & FLAG_C) {{ ctx->pc = 0x{(pc + 2 + sign_extend_8(b[1])) & 0xFFFF:04X}; ctx->cycles += 12; }} else {{ ctx->pc += 2; ctx->cycles += 8; }}", 2),

    # Absolute Jumps & Calls
    0xC3: ("JP nn", lambda b, pc: f"    ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 16;", 3),
    0xC2: ("JP NZ, nn", lambda b, pc: f"    if (!(ctx->f & FLAG_Z)) {{ ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 16; }} else {{ ctx->pc += 3; ctx->cycles += 12; }}", 3),
    0xCA: ("JP Z, nn", lambda b, pc: f"    if (ctx->f & FLAG_Z) {{ ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 16; }} else {{ ctx->pc += 3; ctx->cycles += 12; }}", 3),
    0xD2: ("JP NC, nn", lambda b, pc: f"    if (!(ctx->f & FLAG_C)) {{ ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 16; }} else {{ ctx->pc += 3; ctx->cycles += 12; }}", 3),
    0xDA: ("JP C, nn", lambda b, pc: f"    if (ctx->f & FLAG_C) {{ ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 16; }} else {{ ctx->pc += 3; ctx->cycles += 12; }}", 3),
    0xE9: ("JP (HL)", lambda b, pc: "    ctx->pc = (ctx->h << 8) | ctx->l; ctx->cycles += 4;", 1),

    0xCD: ("CALL nn", lambda b, pc: f"    gb_write8(ctx, --ctx->sp, {(pc+3) >> 8}); gb_write8(ctx, --ctx->sp, {(pc+3) & 0xFF}); ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 24;", 3),
    0xC4: ("CALL NZ, nn", lambda b, pc: f"    if (!(ctx->f & FLAG_Z)) {{ gb_write8(ctx, --ctx->sp, {(pc+3) >> 8}); gb_write8(ctx, --ctx->sp, {(pc+3) & 0xFF}); ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 24; }} else {{ ctx->pc += 3; ctx->cycles += 12; }}", 3),
    0xCC: ("CALL Z, nn", lambda b, pc: f"    if (ctx->f & FLAG_Z) {{ gb_write8(ctx, --ctx->sp, {(pc+3) >> 8}); gb_write8(ctx, --ctx->sp, {(pc+3) & 0xFF}); ctx->pc = 0x{(b[2]<<8)|b[1]:04X}; ctx->cycles += 24; }} else {{ ctx->pc += 3; ctx->cycles += 12; }}", 3),

    0xC9: ("RET", lambda b, pc: "    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 16;", 1),
    0xC0: ("RET NZ", lambda b, pc: "    if (!(ctx->f & FLAG_Z)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }", 1),
    0xC8: ("RET Z", lambda b, pc: "    if (ctx->f & FLAG_Z) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }", 1),
    0xD0: ("RET NC", lambda b, pc: "    if (!(ctx->f & FLAG_C)) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }", 1),
    0xD8: ("RET C", lambda b, pc: "    if (ctx->f & FLAG_C) { ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->cycles += 20; } else { ctx->pc += 1; ctx->cycles += 8; }", 1),
    0xD9: ("RETI", lambda b, pc: "    ctx->pc = gb_read8(ctx, ctx->sp) | (gb_read8(ctx, ctx->sp + 1) << 8); ctx->sp += 2; ctx->ime = true; ctx->cycles += 16;", 1),

    # Stack Operations
    0xC5: ("PUSH BC", lambda b, pc: "    gb_write8(ctx, --ctx->sp, ctx->b); gb_write8(ctx, --ctx->sp, ctx->c); ctx->cycles += 16; ctx->pc += 1;", 1),
    0xD5: ("PUSH DE", lambda b, pc: "    gb_write8(ctx, --ctx->sp, ctx->d); gb_write8(ctx, --ctx->sp, ctx->e); ctx->cycles += 16; ctx->pc += 1;", 1),
    0xE5: ("PUSH HL", lambda b, pc: "    gb_write8(ctx, --ctx->sp, ctx->h); gb_write8(ctx, --ctx->sp, ctx->l); ctx->cycles += 16; ctx->pc += 1;", 1),
    0xF5: ("PUSH AF", lambda b, pc: "    gb_write8(ctx, --ctx->sp, ctx->a); gb_write8(ctx, --ctx->sp, ctx->f); ctx->cycles += 16; ctx->pc += 1;", 1),

    0xC1: ("POP BC", lambda b, pc: "    ctx->c = gb_read8(ctx, ctx->sp++); ctx->b = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;", 1),
    0xD1: ("POP DE", lambda b, pc: "    ctx->e = gb_read8(ctx, ctx->sp++); ctx->d = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;", 1),
    0xE1: ("POP HL", lambda b, pc: "    ctx->l = gb_read8(ctx, ctx->sp++); ctx->h = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;", 1),
    0xF1: ("POP AF", lambda b, pc: "    ctx->f = gb_read8(ctx, ctx->sp++) & 0xF0; ctx->a = gb_read8(ctx, ctx->sp++); ctx->cycles += 12; ctx->pc += 1;", 1),

    # Logic & Arithmetic Operations
    0xAF: ("XOR A", lambda b, pc: "    ctx->a = 0; ctx->f = FLAG_Z; ctx->cycles += 4; ctx->pc += 1;", 1),
    0xA7: ("AND A", lambda b, pc: "    ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 4; ctx->pc += 1;", 1),
    0xE6: ("AND n", lambda b, pc: f"    ctx->a &= 0x{b[1]:02X}; ctx->f = (ctx->a == 0 ? FLAG_Z : 0) | FLAG_H; ctx->cycles += 8; ctx->pc += 2;", 2),
    0xEE: ("XOR n", lambda b, pc: f"    ctx->a ^= 0x{b[1]:02X}; ctx->f = (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;", 2),
    0xF6: ("OR n", lambda b, pc: f"    ctx->a |= 0x{b[1]:02X}; ctx->f = (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 8; ctx->pc += 2;", 2),
    0xB0: ("OR B", lambda b, pc: "    ctx->a |= ctx->b; ctx->f = (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0xB1: ("OR C", lambda b, pc: "    ctx->a |= ctx->c; ctx->f = (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0xB7: ("OR A", lambda b, pc: "    ctx->f = (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0xFE: ("CP n", lambda b, pc: f"""    {{
        uint8_t val = 0x{b[1]:02X};
        uint16_t res = (uint16_t)ctx->a - (uint16_t)val;
        ctx->f = FLAG_N;
        if ((res & 0xFF) == 0) ctx->f |= FLAG_Z;
        if ((ctx->a & 0x0F) < (val & 0x0F)) ctx->f |= FLAG_H;
        if (ctx->a < val) ctx->f |= FLAG_C;
    }}
    ctx->cycles += 8; ctx->pc += 2;""", 2),
    0x05: ("DEC B", lambda b, pc: "    ctx->b--; ctx->f = (ctx->f & FLAG_C) | (ctx->b == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x0D: ("DEC C", lambda b, pc: "    ctx->c--; ctx->f = (ctx->f & FLAG_C) | (ctx->c == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x15: ("DEC D", lambda b, pc: "    ctx->d--; ctx->f = (ctx->f & FLAG_C) | (ctx->d == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x1D: ("DEC E", lambda b, pc: "    ctx->e--; ctx->f = (ctx->f & FLAG_C) | (ctx->e == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x25: ("DEC H", lambda b, pc: "    ctx->h--; ctx->f = (ctx->f & FLAG_C) | (ctx->h == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x2D: ("DEC L", lambda b, pc: "    ctx->l--; ctx->f = (ctx->f & FLAG_C) | (ctx->l == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x3D: ("DEC A", lambda b, pc: "    ctx->a--; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0) | FLAG_N; ctx->cycles += 4; ctx->pc += 1;", 1),
    0x04: ("INC B", lambda b, pc: "    ctx->b++; ctx->f = (ctx->f & FLAG_C) | (ctx->b == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0x0C: ("INC C", lambda b, pc: "    ctx->c++; ctx->f = (ctx->f & FLAG_C) | (ctx->c == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0x14: ("INC D", lambda b, pc: "    ctx->d++; ctx->f = (ctx->f & FLAG_C) | (ctx->d == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0x1C: ("INC E", lambda b, pc: "    ctx->e++; ctx->f = (ctx->f & FLAG_C) | (ctx->e == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0x24: ("INC H", lambda b, pc: "    ctx->h++; ctx->f = (ctx->f & FLAG_C) | (ctx->h == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0x2C: ("INC L", lambda b, pc: "    ctx->l++; ctx->f = (ctx->f & FLAG_C) | (ctx->l == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
    0x3C: ("INC A", lambda b, pc: "    ctx->a++; ctx->f = (ctx->f & FLAG_C) | (ctx->a == 0 ? FLAG_Z : 0); ctx->cycles += 4; ctx->pc += 1;", 1),
}

# Dynamically generate LD r, r' instructions (0x40 - 0x7F)
# Real LR35902 encoding: opcode = 0x40 | (dst_idx << 3) | src_idx
# (destination register in bits 5-3, source register in bits 2-0).
for src_idx, src in enumerate(REG_MAP):
    for dst_idx, dst in enumerate(REG_MAP):
        opcode = 0x40 + (dst_idx * 8) + src_idx
        if opcode in OPCODES or opcode == 0x76:  # Skip HALT
            continue

        if src == "(HL)":
            c_code = f"    ctx->{dst} = gb_read8(ctx, (ctx->h << 8) | ctx->l); ctx->cycles += 8; ctx->pc += 1;"
        elif dst == "(HL)":
            c_code = f"    gb_write8(ctx, (ctx->h << 8) | ctx->l, ctx->{src}); ctx->cycles += 8; ctx->pc += 1;"
        else:
            c_code = f"    ctx->{dst} = ctx->{src}; ctx->cycles += 4; ctx->pc += 1;"

        OPCODES[opcode] = (f"LD {dst.upper()}, {src.upper()}", lambda b, pc, code=c_code: code, 1)

# Dynamically generate RST instructions
for i, target_addr in enumerate([0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38]):
    rst_op = 0xC7 + (i * 8)
    OPCODES[rst_op] = (
        f"RST {target_addr:02X}h",
        lambda b, pc, target=target_addr: f"    gb_write8(ctx, --ctx->sp, {(pc+1) >> 8}); gb_write8(ctx, --ctx->sp, {(pc+1) & 0xFF}); ctx->pc = 0x{target:04X}; ctx->cycles += 16;",
        1
    )

# 0xCB Prefix Instruction Generator
CB_OPCODES = {}

def get_reg_read(reg_idx):
    r = REG_MAP[reg_idx]
    return f"gb_read8(ctx, (ctx->h << 8) | ctx->l)" if r == "(HL)" else f"ctx->{r}"

def get_reg_write(reg_idx, val_expr):
    r = REG_MAP[reg_idx]
    if r == "(HL)":
        return f"gb_write8(ctx, (ctx->h << 8) | ctx->l, {val_expr});"
    return f"ctx->{r} = {val_expr};"

for sub_op in range(256):
    reg_idx = sub_op & 0x07
    bit_idx = (sub_op >> 3) & 0x07
    group = sub_op >> 6
    cycles = 16 if REG_MAP[reg_idx] == "(HL)" else 8

    if group == 1:  # BIT bit, r
        r_read = get_reg_read(reg_idx)
        c_code = f"    ctx->f = (ctx->f & FLAG_C) | FLAG_H | (({r_read} & (1 << {bit_idx})) == 0 ? FLAG_Z : 0); ctx->cycles += {cycles}; ctx->pc += 2;"
        CB_OPCODES[sub_op] = (f"BIT {bit_idx}, {REG_MAP[reg_idx].upper()}", c_code)
    elif group == 2:  # RES bit, r
        r_read = get_reg_read(reg_idx)
        w_stmt = get_reg_write(reg_idx, f"{r_read} & ~(1 << {bit_idx})")
        c_code = f"    {w_stmt} ctx->cycles += {cycles}; ctx->pc += 2;"
        CB_OPCODES[sub_op] = (f"RES {bit_idx}, {REG_MAP[reg_idx].upper()}", c_code)
    elif group == 3:  # SET bit, r
        r_read = get_reg_read(reg_idx)
        w_stmt = get_reg_write(reg_idx, f"{r_read} | (1 << {bit_idx})")
        c_code = f"    {w_stmt} ctx->cycles += {cycles}; ctx->pc += 2;"
        CB_OPCODES[sub_op] = (f"SET {bit_idx}, {REG_MAP[reg_idx].upper()}", c_code)
    else:  # Shifts & Rotates (group 0)
        op_type = (sub_op >> 3) & 0x07
        r_read = get_reg_read(reg_idx)
        if op_type == 6:  # SWAP
            w_stmt = get_reg_write(reg_idx, f"(({r_read} << 4) | ({r_read} >> 4)) & 0xFF")
            c_code = f"    {{ uint8_t val = {r_read}; {w_stmt} ctx->f = (val == 0 ? FLAG_Z : 0); }} ctx->cycles += {cycles}; ctx->pc += 2;"
            CB_OPCODES[sub_op] = (f"SWAP {REG_MAP[reg_idx].upper()}", c_code)
        elif op_type == 7:  # SRL
            w_stmt = get_reg_write(reg_idx, f"({r_read} >> 1)")
            c_code = f"    {{ uint8_t val = {r_read}; {w_stmt} ctx->f = ((val & 1) ? FLAG_C : 0) | ((val >> 1) == 0 ? FLAG_Z : 0); }} ctx->cycles += {cycles}; ctx->pc += 2;"
            CB_OPCODES[sub_op] = (f"SRL {REG_MAP[reg_idx].upper()}", c_code)

def translate_rom(rom_path, output_c_path):
    with open(rom_path, "rb") as f:
        rom = f.read()

    blocks_to_parse = [0x0100, 0x0000, 0x0040, 0x0048, 0x0050, 0x0058, 0x0060, 0x0066]
    parsed_blocks = set()

    c_out = []
    c_out.append('#include "runtime.h"\n')

    while blocks_to_parse:
        start_pc = blocks_to_parse.pop(0)
        if start_pc in parsed_blocks or start_pc >= len(rom):
            continue

        parsed_blocks.add(start_pc)
        pc = start_pc
        c_out.append(f"void recompiled_block_{pc:04X}(GB_Context *ctx) {{")

        while pc < len(rom):
            op = rom[pc]

            # Handle 0xCB Prefix
            if op == 0xCB:
                sub_op = rom[pc + 1]
                if sub_op in CB_OPCODES:
                    name, c_code = CB_OPCODES[sub_op]
                    c_out.append(f"    // 0x{pc:04X}: CB {sub_op:02X} -> {name}")
                    c_out.append(f"{c_code}")
                    pc += 2
                    continue
                else:
                    c_out.append(f"    // 0x{pc:04X}: Unhandled CB Sub-Opcode 0x{sub_op:02X}")
                    c_out.append(f"    fallback_interpreter(ctx, 0xCB);")
                    c_out.append("    return;")
                    break

            if op in OPCODES:
                name, gen_c, length = OPCODES[op]
                bytes_chunk = rom[pc:pc+length]
                c_code = gen_c(bytes_chunk, pc)
                c_out.append(f"    // 0x{pc:04X}: {name}")
                c_out.append(f"{c_code}")

                # Control flow discovery
                if op in (0xC3, 0xC2, 0xCA, 0xD2, 0xDA):  # JP / JP cc
                    target = (bytes_chunk[2] << 8) | bytes_chunk[1]
                    blocks_to_parse.append(target)
                    if op == 0xC3:
                        c_out.append("    return;")
                        break
                    else:
                        blocks_to_parse.append(pc + 3)
                        c_out.append("    return;")
                        break
                elif op == 0xE9:  # JP (HL)
                    c_out.append("    return;")
                    break
                elif op in (0xCD, 0xC4, 0xCC):  # CALL / CALL cc
                    target = (bytes_chunk[2] << 8) | bytes_chunk[1]
                    blocks_to_parse.append(target)
                    blocks_to_parse.append(pc + 3)
                    c_out.append("    return;")
                    break
                elif op in (0x18, 0x20, 0x28, 0x30, 0x38):  # JR / JR cc
                    offset = sign_extend_8(bytes_chunk[1])
                    target = (pc + 2 + offset) & 0xFFFF
                    blocks_to_parse.append(target)
                    blocks_to_parse.append(pc + 2)
                    c_out.append("    return;")
                    break
                elif op in (0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF):  # RST
                    target = (op & 0x38)
                    blocks_to_parse.append(target)
                    c_out.append("    return;")
                    break
                elif op in (0xC9, 0xC0, 0xC8, 0xD0, 0xD8, 0xD9):  # RET / RET cc / RETI
                    if op in (0xC9, 0xD9):
                        c_out.append("    return;")
                        break
                    else:
                        blocks_to_parse.append(pc + 1)
                        c_out.append("    return;")
                        break

                pc += length
            else:
                c_out.append(f"    // 0x{pc:04X}: Unknown Opcode 0x{op:02X}")
                c_out.append(f"    fallback_interpreter(ctx, 0x{op:02X});")
                c_out.append("    return;")
                break

        c_out.append("}\n")

    # Dispatch router table
    c_out.append("void step_recompiled(GB_Context *ctx) {")
    c_out.append("    switch(ctx->pc) {")
    for block in sorted(parsed_blocks):
        c_out.append(f"        case 0x{block:04X}: recompiled_block_{block:04X}(ctx); break;")
    c_out.append("        default: fallback_interpreter(ctx, gb_read8(ctx, ctx->pc)); break;")
    c_out.append("    }")
    c_out.append("}")

    with open(output_c_path, "w") as f:
        f.write("\n".join(c_out))

    print(f"Successfully generated {output_c_path} with {len(parsed_blocks)} blocks.")

if __name__ == "__main__":
    translate_rom("pokemon_yellow.gb", "recompiled_game.c")