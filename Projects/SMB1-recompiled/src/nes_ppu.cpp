#include "nes_ppu.h"

#include <array>
#include <fstream>

namespace
{
    constexpr uint8_t PPUSTATUS_VBLANK = 0x80;

    constexpr std::array<uint32_t, 64> NES_PALETTE =
    {
        0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088,
        0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
        0xFF202A00, 0xFF083A00, 0xFF004200, 0xFF004038,
        0xFF00323C, 0xFF000000, 0xFF000000, 0xFF000000,

        0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4,
        0xFF8814B0, 0xFFA01464, 0xFFA02A20, 0xFF884600,
        0xFF606A00, 0xFF288000, 0xFF008A00, 0xFF00886C,
        0xFF0078A0, 0xFF000000, 0xFF000000, 0xFF000000,

        0xFFECEEE4, 0xFF4C9AEC, 0xFF787CF4, 0xFFB062EC,
        0xFFD45ED4, 0xFFE05C8C, 0xFFF07858, 0xFFECA044,
        0xFFB8C43C, 0xFF70D02C, 0xFF4CD05C, 0xFF4CC4A0,
        0xFF58B0D8, 0xFF3C3C3C, 0xFF000000, 0xFF000000,

        0xFFECEEE4, 0xFFA8CCEC, 0xFFBCBCEC, 0xFFD4B2EC,
        0xFFECAEE4, 0xFFECAECC, 0xFFF0B4A8, 0xFFECC4A0,
        0xFFD8D898, 0xFFB8E08C, 0xFFA8E0A8, 0xFFA8D8C8,
        0xFFA8CCEC, 0xFFA8A8A8, 0xFF000000, 0xFF000000
    };
}

NesPpu::NesPpu()
{
    paletteRam.fill(0);
    nametableRam.fill(0);
    framebuffer.fill(0xFF000000);
}

bool NesPpu::loadChrRom(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        return false;
    }

    std::array<uint8_t, 16> header{};

    file.read(
        reinterpret_cast<char*>(header.data()),
        header.size()
    );

    if (!file)
    {
        return false;
    }

    if (header[0] != 'N' ||
        header[1] != 'E' ||
        header[2] != 'S' ||
        header[3] != 0x1A)
    {
        return false;
    }

    const std::size_t prgSize =
        static_cast<std::size_t>(header[4]) * 16384;

    const std::size_t chrSize =
        static_cast<std::size_t>(header[5]) * 8192;

    if (prgSize != 0x8000)
    {
        return false;
    }

    if (chrSize != 0x2000)
    {
        return false;
    }

    file.seekg(
        static_cast<std::streamoff>(16 + prgSize),
        std::ios::beg
    );

    file.read(
        reinterpret_cast<char*>(chrRom.data()),
        chrRom.size()
    );

    return static_cast<bool>(file);
}

uint8_t NesPpu::cpuRead(uint16_t address)
{
    address = 0x2000 | (address & 0x0007);

    switch (address)
    {
        case 0x2002:
        {
            const uint8_t value = ppuStatus;

            // Reading PPUSTATUS:
            // - clears VBlank
            // - resets $2005/$2006 latch
            ppuStatus &= static_cast<uint8_t>(
                ~PPUSTATUS_VBLANK
            );

            addressLatch = false;

            return value;
        }

        case 0x2007:
        {
            const uint8_t value =
                readVram(vramAddress);

            incrementVramAddress();

            return value;
        }

        default:
            return 0;
    }
}

void NesPpu::cpuWrite(
    uint16_t address,
    uint8_t value
)
{
    address = 0x2000 | (address & 0x0007);

    switch (address)
    {
        case 0x2000:
        {
            ppuCtrl = value;
            break;
        }

        case 0x2001:
        {
            ppuMask = value;
            break;
        }

        case 0x2002:
        {
            // Read-only register.
            break;
        }

        case 0x2003:
        {
            // OAMADDR.
            // OAM is not implemented yet.
            break;
        }

        case 0x2004:
        {
            // OAMDATA.
            // OAM is not implemented yet.
            break;
        }

        case 0x2005:
        {
            if (!addressLatch)
            {
                scrollX = value;
                addressLatch = true;
            }
            else
            {
                scrollY = value;
                addressLatch = false;
            }

            break;
        }

        case 0x2006:
        {
            if (!addressLatch)
            {
                vramAddress =
                    static_cast<uint16_t>(
                        value & 0x3F
                    ) << 8;

                addressLatch = true;
            }
            else
            {
                vramAddress =
                    static_cast<uint16_t>(
                        (vramAddress & 0x3F00) |
                        value
                    );

                addressLatch = false;
            }

            break;
        }

        case 0x2007:
        {
            writeVram(
                vramAddress,
                value
            );

            incrementVramAddress();

            break;
        }
    }
}

void NesPpu::tick(uint32_t cpuCycles)
{
    // NES PPU runs at approximately 3x CPU frequency.
    const uint32_t addedPpuCycles =
        cpuCycles * 3;

    ppuCycles += addedPpuCycles;

    while (ppuCycles > 0)
    {
        ppuCycles--;

        ppuCycle++;

        if (ppuCycle >= 341)
        {
            ppuCycle = 0;
            ppuScanline++;

            if (ppuScanline == 241)
            {
                // Start of VBlank.
                ppuStatus |= PPUSTATUS_VBLANK;

                // PPUCTRL bit 7 enables NMI during VBlank.
                if (ppuCtrl & 0x80)
                {
                    nmiRequested = true;
                }
            }

            if (ppuScanline >= 262)
            {
                // New frame.
                ppuScanline = 0;

                ppuStatus &= static_cast<uint8_t>(
                    ~PPUSTATUS_VBLANK
                );
            }
        }
    }
}

bool NesPpu::pollNmi()
{
    if (!nmiRequested)
    {
        return false;
    }

    nmiRequested = false;

    return true;
}

void NesPpu::renderFrame()
{
    for (int y = 0; y < ScreenHeight; ++y)
    {
        const int tileY = y / 8;
        const int pixelY = y % 8;

        for (int x = 0; x < ScreenWidth; ++x)
        {
            const int tileX = x / 8;
            const int pixelX = x % 8;

            const std::size_t tileAddress =
                static_cast<std::size_t>(
                    tileY * 32 + tileX
                );

            const uint8_t tileIndex =
                nametableRam[tileAddress];

            const uint8_t pixelValue =
                getChrPixel(
                    tileIndex,
                    pixelX,
                    pixelY
                );

            const uint8_t paletteIndex =
                getBackgroundPaletteIndex(
                    tileX,
                    tileY,
                    pixelValue
                );

            const uint8_t colorIndex =
                getPaletteColorIndex(
                    paletteIndex
                );

            framebuffer[
                static_cast<std::size_t>(
                    y * ScreenWidth + x
                )
            ] = nesColor(colorIndex);
        }
    }
}

const std::array<uint32_t, NesPpu::ScreenWidth * NesPpu::ScreenHeight>&
NesPpu::getFramebuffer() const
{
    return framebuffer;
}

void NesPpu::fillTestNametable()
{
    nametableRam.fill(0);

    for (int y = 0; y < 30; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            const uint8_t tile =
                static_cast<uint8_t>(
                    (y * 32 + x) & 0xFF
                );

            nametableRam[
                static_cast<std::size_t>(
                    y * 32 + x
                )
            ] = tile;
        }
    }

    for (int attributeY = 0;
         attributeY < 8;
         ++attributeY)
    {
        for (int attributeX = 0;
             attributeX < 8;
             ++attributeX)
        {
            uint8_t attribute = 0;

            const uint8_t topLeftPalette =
                static_cast<uint8_t>(
                    (attributeX + attributeY) & 0x03
                );

            const uint8_t topRightPalette =
                static_cast<uint8_t>(
                    (attributeX + attributeY + 1) & 0x03
                );

            const uint8_t bottomLeftPalette =
                static_cast<uint8_t>(
                    (attributeX + attributeY + 2) & 0x03
                );

            const uint8_t bottomRightPalette =
                static_cast<uint8_t>(
                    (attributeX + attributeY + 3) & 0x03
                );

            attribute |=
                (topLeftPalette & 0x03);

            attribute |=
                (topRightPalette & 0x03) << 2;

            attribute |=
                (bottomLeftPalette & 0x03) << 4;

            attribute |=
                (bottomRightPalette & 0x03) << 6;

            nametableRam[
                static_cast<std::size_t>(
                    0x03C0 +
                    attributeY * 8 +
                    attributeX
                )
            ] = attribute;
        }
    }

    paletteRam[0x00] = 0x0F;

    paletteRam[0x01] = 0x16;
    paletteRam[0x02] = 0x27;
    paletteRam[0x03] = 0x18;

    paletteRam[0x05] = 0x0F;
    paletteRam[0x06] = 0x30;
    paletteRam[0x07] = 0x21;

    paletteRam[0x09] = 0x0F;
    paletteRam[0x0A] = 0x16;
    paletteRam[0x0B] = 0x27;

    paletteRam[0x0D] = 0x0F;
    paletteRam[0x0E] = 0x30;
    paletteRam[0x0F] = 0x18;
}

uint8_t NesPpu::readVram(
    uint16_t address
) const
{
    address &= 0x3FFF;

    if (address < 0x2000)
    {
        return chrRom[address];
    }

    if (address < 0x3F00)
    {
        const uint16_t nametableAddress =
            address & 0x0FFF;

        return nametableRam[
            nametableAddress
        ];
    }

    uint16_t paletteAddress =
        (address - 0x3F00) & 0x1F;

    if (paletteAddress == 0x10)
        paletteAddress = 0x00;

    if (paletteAddress == 0x14)
        paletteAddress = 0x04;

    if (paletteAddress == 0x18)
        paletteAddress = 0x08;

    if (paletteAddress == 0x1C)
        paletteAddress = 0x0C;

    return paletteRam[
        paletteAddress
    ];
}

void NesPpu::writeVram(
    uint16_t address,
    uint8_t value
)
{
    address &= 0x3FFF;

    if (address < 0x2000)
    {
        // CHR-ROM is read-only.
        return;
    }

    if (address < 0x3F00)
    {
        const uint16_t nametableAddress =
            address & 0x0FFF;

        nametableRam[
            nametableAddress
        ] = value;

        return;
    }

    uint16_t paletteAddress =
        (address - 0x3F00) & 0x1F;

    if (paletteAddress == 0x10)
        paletteAddress = 0x00;

    if (paletteAddress == 0x14)
        paletteAddress = 0x04;

    if (paletteAddress == 0x18)
        paletteAddress = 0x08;

    if (paletteAddress == 0x1C)
        paletteAddress = 0x0C;

    paletteRam[
        paletteAddress
    ] = value & 0x3F;
}

void NesPpu::incrementVramAddress()
{
    if (ppuCtrl & 0x04)
    {
        vramAddress =
            static_cast<uint16_t>(
                vramAddress + 32
            );
    }
    else
    {
        vramAddress =
            static_cast<uint16_t>(
                vramAddress + 1
            );
    }

    vramAddress &= 0x3FFF;
}

uint8_t NesPpu::getChrPixel(
    uint16_t tileIndex,
    int x,
    int y
) const
{
    tileIndex &= 0x01FF;

    if (x < 0 || x >= 8 ||
        y < 0 || y >= 8)
    {
        return 0;
    }

    const std::size_t tileAddress =
        static_cast<std::size_t>(
            tileIndex * 16
        );

    const uint8_t plane0 =
        chrRom[
            tileAddress + y
        ];

    const uint8_t plane1 =
        chrRom[
            tileAddress + 8 + y
        ];

    const int bit =
        7 - x;

    const uint8_t low =
        (plane0 >> bit) & 1;

    const uint8_t high =
        (plane1 >> bit) & 1;

    return static_cast<uint8_t>(
        low | (high << 1)
    );
}

uint8_t NesPpu::getBackgroundPaletteIndex(
    int tileX,
    int tileY,
    uint8_t pixelValue
) const
{
    if (pixelValue == 0)
    {
        return 0;
    }

    const int attributeX =
        tileX / 4;

    const int attributeY =
        tileY / 4;

    const std::size_t attributeAddress =
        static_cast<std::size_t>(
            0x03C0 +
            attributeY * 8 +
            attributeX
        );

    const uint8_t attribute =
        nametableRam[
            attributeAddress
        ];

    const int quadrantX =
        (tileX % 4) / 2;

    const int quadrantY =
        (tileY % 4) / 2;

    const int shift =
        (quadrantY * 2 + quadrantX) * 2;

    const uint8_t palette =
        (attribute >> shift) & 0x03;

    return static_cast<uint8_t>(
        palette * 4 +
        pixelValue
    );
}

uint8_t NesPpu::getPaletteColorIndex(
    uint8_t paletteIndex
) const
{
    paletteIndex &= 0x1F;

    if (paletteIndex % 4 == 0)
    {
        return paletteRam[0x00] & 0x3F;
    }

    return paletteRam[
        paletteIndex
    ] & 0x3F;
}

uint32_t NesPpu::nesColor(
    uint8_t colorIndex
)
{
    return NES_PALETTE[
        colorIndex & 0x3F
    ];
}