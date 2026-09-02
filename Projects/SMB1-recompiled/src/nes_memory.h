#pragma once

#include <array>
#include <cstdint>
#include <string>

class NesPpu;

class NesMemory
{
public:
    static constexpr std::size_t RamSize = 0x0800;
    static constexpr std::size_t PrgRomSize = 0x8000;
    static constexpr std::size_t ChrRomSize = 0x2000;

    NesMemory();

    bool loadRom(const std::string& filename);

    void clear();

    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    void tick(uint32_t cpuCycles);

    void connectPpu(NesPpu* ppu);

private:
    std::array<uint8_t, RamSize> ram{};
    std::array<uint8_t, PrgRomSize> prgRom{};
    std::array<uint8_t, ChrRomSize> chrRom{};

    bool romLoaded = false;

    NesPpu* ppu = nullptr;
};