#pragma once

#include <array>
#include <cstdint>
#include <string>

class NesPpu
{
public:
    static constexpr int ScreenWidth = 256;
    static constexpr int ScreenHeight = 240;

    static constexpr std::size_t ChrRomSize = 0x2000;
    static constexpr std::size_t NametableRamSize = 0x1000;
    static constexpr std::size_t PaletteRamSize = 0x20;

    NesPpu();

    bool loadChrRom(const std::string& filename);

    // CPU-visible PPU registers: $2000-$2007
    uint8_t cpuRead(uint16_t address);
    void cpuWrite(uint16_t address, uint8_t value);

    // Advance PPU timing.
    void tick(uint32_t cpuCycles);

    // Check whether the PPU has requested an NMI.
    //
    // Returns true once and clears the pending request.
    bool pollNmi();

    // Render the current PPU state into the framebuffer.
    void renderFrame();

    const std::array<uint32_t, ScreenWidth * ScreenHeight>&
    getFramebuffer() const;

    // Temporary test helper.
    void fillTestNametable();

private:
    std::array<uint8_t, ChrRomSize> chrRom{};
    std::array<uint8_t, NametableRamSize> nametableRam{};
    std::array<uint8_t, PaletteRamSize> paletteRam{};

    std::array<uint32_t, ScreenWidth * ScreenHeight>
        framebuffer{};

    // $2000 - PPUCTRL
    uint8_t ppuCtrl = 0;

    // $2001 - PPUMASK
    uint8_t ppuMask = 0;

    // $2002 - PPUSTATUS
    uint8_t ppuStatus = 0;

    // $2005 - PPUSCROLL
    uint8_t scrollX = 0;
    uint8_t scrollY = 0;

    // $2006/$2007
    uint16_t vramAddress = 0;

    bool addressLatch = false;

    // Set when VBlank begins and PPUCTRL bit 7 is enabled.
    bool nmiRequested = false;

    // Simplified PPU timing.
    uint64_t ppuCycles = 0;

    uint16_t ppuScanline = 0;
    uint16_t ppuCycle = 0;

    void incrementVramAddress();

    uint8_t readVram(uint16_t address) const;
    void writeVram(uint16_t address, uint8_t value);

    uint8_t getChrPixel(
        uint16_t tileIndex,
        int x,
        int y
    ) const;

    uint8_t getBackgroundPaletteIndex(
        int tileX,
        int tileY,
        uint8_t pixelValue
    ) const;

    uint8_t getPaletteColorIndex(
        uint8_t paletteIndex
    ) const;

    static uint32_t nesColor(uint8_t colorIndex);
};