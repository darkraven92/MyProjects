#pragma once

#include <cstdint>

class NesMemory;

class NesCpu
{
public:
    explicit NesCpu(NesMemory& memory);

    void reset();

    // Execute a hardware-style NMI.
    void nmi();

    // Execute one 6502 instruction.
    uint32_t step();

    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    uint8_t a = 0;
    uint8_t x = 0;
    uint8_t y = 0;

    uint8_t sp = 0xFD;

    uint8_t status = 0x24;

    uint16_t pc = 0;

    static constexpr uint8_t FLAG_CARRY     = 0x01;
    static constexpr uint8_t FLAG_ZERO      = 0x02;
    static constexpr uint8_t FLAG_INTERRUPT = 0x04;
    static constexpr uint8_t FLAG_DECIMAL   = 0x08;
    static constexpr uint8_t FLAG_BREAK     = 0x10;
    static constexpr uint8_t FLAG_UNUSED    = 0x20;
    static constexpr uint8_t FLAG_OVERFLOW  = 0x40;
    static constexpr uint8_t FLAG_NEGATIVE  = 0x80;

private:
    NesMemory& memory;

    uint8_t fetchByte();
    uint16_t fetchWord();

    void push(uint8_t value);
    uint8_t pull();

    void setFlag(
        uint8_t flag,
        bool value
    );

    bool getFlag(
        uint8_t flag
    ) const;

    void setZeroNegativeFlags(
        uint8_t value
    );
};