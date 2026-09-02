#include "nes_cpu.h"
#include "nes_memory.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string hex8(uint8_t value)
    {
        std::ostringstream stream;

        stream
            << "$"
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(value);

        return stream.str();
    }

    std::string hex16(uint16_t value)
    {
        std::ostringstream stream;

        stream
            << "$"
            << std::uppercase
            << std::hex
            << std::setw(4)
            << std::setfill('0')
            << value;

        return stream.str();
    }
}

NesCpu::NesCpu(NesMemory& memory)
    : memory(memory)
{
}

void NesCpu::reset()
{
    a = 0;
    x = 0;
    y = 0;

    sp = 0xFD;

    status =
        FLAG_INTERRUPT |
        FLAG_UNUSED;

    pc =
        static_cast<uint16_t>(read(0xFFFC)) |
        static_cast<uint16_t>(read(0xFFFD)) << 8;
}

uint8_t NesCpu::read(uint16_t address)
{
    return memory.read(address);
}

void NesCpu::write(
    uint16_t address,
    uint8_t value
)
{
    memory.write(address, value);
}

uint8_t NesCpu::fetchByte()
{
    const uint8_t value =
        read(pc);

    pc++;

    return value;
}

uint16_t NesCpu::fetchWord()
{
    const uint8_t low =
        fetchByte();

    const uint8_t high =
        fetchByte();

    return static_cast<uint16_t>(low) |
           (static_cast<uint16_t>(high) << 8);
}

void NesCpu::push(uint8_t value)
{
    write(
        static_cast<uint16_t>(0x0100 | sp),
        value
    );

    sp--;
}

uint8_t NesCpu::pull()
{
    sp++;

    return read(
        static_cast<uint16_t>(0x0100 | sp)
    );
}

void NesCpu::setFlag(
    uint8_t flag,
    bool value
)
{
    if (value)
    {
        status |= flag;
    }
    else
    {
        status &= static_cast<uint8_t>(~flag);
    }
}

bool NesCpu::getFlag(uint8_t flag) const
{
    return (status & flag) != 0;
}

void NesCpu::setZeroNegativeFlags(uint8_t value)
{
    setFlag(
        FLAG_ZERO,
        value == 0
    );

    setFlag(
        FLAG_NEGATIVE,
        (value & 0x80) != 0
    );
}

uint32_t NesCpu::step()
{
    const uint16_t instructionPc =
        pc;

    const uint8_t opcode =
        fetchByte();

    uint32_t cycles = 0;

    switch (opcode)
    {
        // --------------------------------------------------------
        // System / interrupt control
        // --------------------------------------------------------

        case 0x78: // SEI
        {
            setFlag(
                FLAG_INTERRUPT,
                true
            );

            cycles = 2;
            break;
        }

        case 0xD8: // CLD
        {
            setFlag(
                FLAG_DECIMAL,
                false
            );

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // LDA
        // --------------------------------------------------------

        case 0xA9: // LDA #imm
        {
            a = fetchByte();

            setZeroNegativeFlags(a);

            cycles = 2;
            break;
        }

        case 0xAD: // LDA abs
        {
            const uint16_t address =
                fetchWord();

            a = read(address);

            setZeroNegativeFlags(a);

            cycles = 4;
            break;
        }

        case 0xBD: // LDA abs,X
        {
            const uint16_t base =
                fetchWord();

            const uint16_t address =
                static_cast<uint16_t>(
                    base + x
                );

            a = read(address);

            setZeroNegativeFlags(a);

            cycles = 4;

            if ((base & 0xFF00) !=
                (address & 0xFF00))
            {
                cycles++;
            }

            break;
        }

        // --------------------------------------------------------
        // ORA
        // --------------------------------------------------------

        case 0x09: // ORA #imm
        {
            const uint8_t value =
                fetchByte();

            a = static_cast<uint8_t>(
                a | value
            );

            setZeroNegativeFlags(a);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // AND
        // --------------------------------------------------------

        case 0x29: // AND #imm
        {
            const uint8_t value =
                fetchByte();

            a = static_cast<uint8_t>(
                a & value
            );

            setZeroNegativeFlags(a);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // STA
        // --------------------------------------------------------

        case 0x85: // STA zp
        {
            const uint8_t address =
                fetchByte();

            write(
                address,
                a
            );

            cycles = 3;
            break;
        }

        case 0x8D: // STA abs
        {
            const uint16_t address =
                fetchWord();

            write(
                address,
                a
            );

            cycles = 4;
            break;
        }

        case 0x91: // STA (zp),Y
        {
            const uint8_t zeroPage =
                fetchByte();

            const uint8_t low =
                read(zeroPage);

            const uint8_t high =
                read(
                    static_cast<uint8_t>(
                        zeroPage + 1
                    )
                );

            const uint16_t base =
                static_cast<uint16_t>(low) |
                (static_cast<uint16_t>(high) << 8);

            const uint16_t address =
                static_cast<uint16_t>(
                    base + y
                );

            write(
                address,
                a
            );

            cycles = 6;
            break;
        }

        case 0x99: // STA abs,Y
        {
            const uint16_t base =
                fetchWord();

            const uint16_t address =
                static_cast<uint16_t>(
                    base + y
                );

            write(
                address,
                a
            );

            cycles = 5;
            break;
        }

        // --------------------------------------------------------
        // STX
        // --------------------------------------------------------

        case 0x86: // STX zp
        {
            const uint8_t address =
                fetchByte();

            write(
                address,
                x
            );

            cycles = 3;
            break;
        }

        // --------------------------------------------------------
        // LDX
        // --------------------------------------------------------

        case 0xA2: // LDX #imm
        {
            x = fetchByte();

            setZeroNegativeFlags(x);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // LDY
        // --------------------------------------------------------

        case 0xA0: // LDY #imm
        {
            y = fetchByte();

            setZeroNegativeFlags(y);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // DEX
        // --------------------------------------------------------

        case 0xCA: // DEX
        {
            x--;

            setZeroNegativeFlags(x);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // DEY
        // --------------------------------------------------------

        case 0x88: // DEY
        {
            y--;

            setZeroNegativeFlags(y);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // INY
        // --------------------------------------------------------

        case 0xC8: // INY
        {
            y++;

            setZeroNegativeFlags(y);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // TXA
        // --------------------------------------------------------

        case 0x8A: // TXA
        {
            a = x;

            setZeroNegativeFlags(a);

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // TXS
        // --------------------------------------------------------

        case 0x9A: // TXS
        {
            sp = x;

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // INC
        // --------------------------------------------------------

        case 0xEE: // INC abs
        {
            const uint16_t address =
                fetchWord();

            uint8_t value =
                read(address);

            value++;

            write(
                address,
                value
            );

            setZeroNegativeFlags(value);

            cycles = 6;
            break;
        }

        // --------------------------------------------------------
        // CMP
        // --------------------------------------------------------

        case 0xC9: // CMP #imm
        {
            const uint8_t value =
                fetchByte();

            const uint16_t result =
                static_cast<uint16_t>(a) -
                static_cast<uint16_t>(value);

            setFlag(
                FLAG_CARRY,
                a >= value
            );

            setZeroNegativeFlags(
                static_cast<uint8_t>(result)
            );

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // CPX
        // --------------------------------------------------------

        case 0xE0: // CPX #imm
        {
            const uint8_t value =
                fetchByte();

            const uint16_t result =
                static_cast<uint16_t>(x) -
                static_cast<uint16_t>(value);

            setFlag(
                FLAG_CARRY,
                x >= value
            );

            setZeroNegativeFlags(
                static_cast<uint8_t>(result)
            );

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // CPY
        // --------------------------------------------------------

        case 0xC0: // CPY #imm
        {
            const uint8_t value =
                fetchByte();

            const uint16_t result =
                static_cast<uint16_t>(y) -
                static_cast<uint16_t>(value);

            setFlag(
                FLAG_CARRY,
                y >= value
            );

            setZeroNegativeFlags(
                static_cast<uint8_t>(result)
            );

            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // BIT
        // --------------------------------------------------------

        case 0x2C: // BIT abs
        {
            const uint16_t address =
                fetchWord();

            const uint8_t value =
                read(address);

            setFlag(
                FLAG_ZERO,
                (a & value) == 0
            );

            setFlag(
                FLAG_OVERFLOW,
                (value & 0x40) != 0
            );

            setFlag(
                FLAG_NEGATIVE,
                (value & 0x80) != 0
            );

            cycles = 4;
            break;
        }

        // --------------------------------------------------------
        // BPL
        // --------------------------------------------------------

        case 0x10: // BPL
        {
            const int8_t offset =
                static_cast<int8_t>(
                    fetchByte()
                );

            cycles = 2;

            if (!getFlag(FLAG_NEGATIVE))
            {
                const uint16_t oldPc =
                    pc;

                pc =
                    static_cast<uint16_t>(
                        pc + offset
                    );

                cycles++;

                if ((oldPc & 0xFF00) !=
                    (pc & 0xFF00))
                {
                    cycles++;
                }
            }

            break;
        }

        // --------------------------------------------------------
        // BCS
        // --------------------------------------------------------

        case 0xB0: // BCS
        {
            const int8_t offset =
                static_cast<int8_t>(
                    fetchByte()
                );

            cycles = 2;

            if (getFlag(FLAG_CARRY))
            {
                const uint16_t oldPc =
                    pc;

                pc =
                    static_cast<uint16_t>(
                        pc + offset
                    );

                cycles++;

                if ((oldPc & 0xFF00) !=
                    (pc & 0xFF00))
                {
                    cycles++;
                }
            }

            break;
        }

        // --------------------------------------------------------
        // BNE
        // --------------------------------------------------------

        case 0xD0: // BNE
        {
            const int8_t offset =
                static_cast<int8_t>(
                    fetchByte()
                );

            cycles = 2;

            if (!getFlag(FLAG_ZERO))
            {
                const uint16_t oldPc =
                    pc;

                pc =
                    static_cast<uint16_t>(
                        pc + offset
                    );

                cycles++;

                if ((oldPc & 0xFF00) !=
                    (pc & 0xFF00))
                {
                    cycles++;
                }
            }

            break;
        }

        // --------------------------------------------------------
        // JMP
        // --------------------------------------------------------

        case 0x4C: // JMP abs
        {
            const uint16_t address =
                fetchWord();

            pc = address;

            cycles = 3;
            break;
        }

        // --------------------------------------------------------
        // JSR
        // --------------------------------------------------------

        case 0x20: // JSR abs
        {
            const uint16_t address =
                fetchWord();

            const uint16_t returnAddress =
                static_cast<uint16_t>(
                    pc - 1
                );

            push(
                static_cast<uint8_t>(
                    returnAddress >> 8
                )
            );

            push(
                static_cast<uint8_t>(
                    returnAddress & 0xFF
                )
            );

            pc = address;

            cycles = 6;
            break;
        }

        // --------------------------------------------------------
        // RTS
        // --------------------------------------------------------

        case 0x60: // RTS
        {
            const uint8_t low =
                pull();

            const uint8_t high =
                pull();

            pc =
                static_cast<uint16_t>(low) |
                (static_cast<uint16_t>(high) << 8);

            pc++;

            cycles = 6;
            break;
        }

        // --------------------------------------------------------
        // NOP
        // --------------------------------------------------------

        case 0xEA: // NOP
        {
            cycles = 2;
            break;
        }

        // --------------------------------------------------------
        // Unsupported
        // --------------------------------------------------------

        default:
        {
            std::ostringstream message;

            message
                << "Unsupported opcode "
                << hex8(opcode)
                << " at PC "
                << hex16(instructionPc)
                << "\n"
                << "A="
                << hex8(a)
                << " X="
                << hex8(x)
                << " Y="
                << hex8(y)
                << " SP="
                << hex8(sp)
                << " STATUS="
                << hex8(status);

            throw std::runtime_error(
                message.str()
            );
        }
    }

    memory.tick(cycles);

    return cycles;
}