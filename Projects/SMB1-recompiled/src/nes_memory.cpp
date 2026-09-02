#include "nes_memory.h"
#include "nes_ppu.h"

#include <array>
#include <fstream>
#include <iostream>

NesMemory::NesMemory()
{
    clear();
}

void NesMemory::clear()
{
    ram.fill(0);
    prgRom.fill(0);
    chrRom.fill(0);

    romLoaded = false;
}

bool NesMemory::loadRom(const std::string& filename)
{
    std::ifstream file(
        filename,
        std::ios::binary
    );

    if (!file) {
        std::cerr
            << "Failed to open ROM: "
            << filename
            << '\n';

        return false;
    }

    std::array<uint8_t, 16> header{};

    file.read(
        reinterpret_cast<char*>(header.data()),
        header.size()
    );

    if (!file) {
        std::cerr
            << "Failed to read NES header.\n";

        return false;
    }

    if (header[0] != 'N' ||
        header[1] != 'E' ||
        header[2] != 'S' ||
        header[3] != 0x1A) {

        std::cerr
            << "Invalid NES ROM header.\n";

        return false;
    }

    const std::size_t prgSize =
        static_cast<std::size_t>(header[4]) *
        16384;

    const std::size_t chrSize =
        static_cast<std::size_t>(header[5]) *
        8192;

    if (prgSize != PrgRomSize) {
        std::cerr
            << "Expected 32 KiB PRG-ROM, found "
            << prgSize
            << " bytes.\n";

        return false;
    }

    if (chrSize != ChrRomSize) {
        std::cerr
            << "Expected 8 KiB CHR-ROM, found "
            << chrSize
            << " bytes.\n";

        return false;
    }

    file.read(
        reinterpret_cast<char*>(prgRom.data()),
        prgRom.size()
    );

    if (!file) {
        std::cerr
            << "Failed to read PRG-ROM.\n";

        return false;
    }

    file.read(
        reinterpret_cast<char*>(chrRom.data()),
        chrRom.size()
    );

    if (!file) {
        std::cerr
            << "Failed to read CHR-ROM.\n";

        return false;
    }

    romLoaded = true;

    return true;
}

uint8_t NesMemory::read(uint16_t address)
{
    // --------------------------------------------------
    // $0000-$1FFF
    // Internal RAM + mirrors
    // --------------------------------------------------

    if (address < 0x2000) {
        return ram[address & 0x07FF];
    }

    // --------------------------------------------------
    // $2000-$3FFF
    // PPU registers + mirrors
    // --------------------------------------------------

    if (address >= 0x2000 &&
        address < 0x4000) {

        if (ppu != nullptr) {
            return ppu->cpuRead(address);
        }

        return 0;
    }

    // --------------------------------------------------
    // $4000-$7FFF
    // APU / I/O
    // --------------------------------------------------

    if (address >= 0x4000 &&
        address < 0x8000) {

        return 0;
    }

    // --------------------------------------------------
    // $8000-$FFFF
    // PRG-ROM
    // --------------------------------------------------

    if (address >= 0x8000 &&
        romLoaded) {

        const std::size_t offset =
            static_cast<std::size_t>(
                address - 0x8000
            );

        return prgRom[offset];
    }

    return 0;
}

void NesMemory::write(
    uint16_t address,
    uint8_t value
)
{
    // --------------------------------------------------
    // Internal RAM
    // --------------------------------------------------

    if (address < 0x2000) {
        ram[address & 0x07FF] = value;
        return;
    }

    // --------------------------------------------------
    // PPU registers
    // --------------------------------------------------

    if (address >= 0x2000 &&
        address < 0x4000) {

        if (ppu != nullptr) {
            ppu->cpuWrite(
                address,
                value
            );
        }

        return;
    }

    // Other hardware will be implemented later.
}

void NesMemory::tick(uint32_t cpuCycles)
{
    if (ppu != nullptr) {
        ppu->tick(cpuCycles);
    }
}

void NesMemory::connectPpu(NesPpu* ppu)
{
    this->ppu = ppu;
}