#include <SDL3/SDL.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "nes_memory.h"
#include "nes_cpu.h"
#include "nes_ppu.h"

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

int main()
{
    const char* romPath =
        "smb.nes";

    NesMemory memory;
    NesPpu ppu;
    NesCpu cpu(memory);

    // ------------------------------------------------------------
    // Load ROM
    // ------------------------------------------------------------

    if (!memory.loadRom(romPath))
    {
        std::cerr
            << "Failed to load ROM: "
            << romPath
            << '\n';

        return 1;
    }

    if (!ppu.loadChrRom(romPath))
    {
        std::cerr
            << "Failed to load CHR-ROM: "
            << romPath
            << '\n';

        return 1;
    }

    memory.connectPpu(&ppu);

    // ------------------------------------------------------------
    // CPU reset
    // ------------------------------------------------------------

    cpu.reset();

    std::cout
        << "SMB1 native execution starting\n";

    std::cout
        << "Reset PC: "
        << hex16(cpu.pc)
        << '\n';

    // ------------------------------------------------------------
    // SDL
    // ------------------------------------------------------------

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr
            << "SDL_Init failed: "
            << SDL_GetError()
            << '\n';

        return 1;
    }

    constexpr int Scale = 3;

    SDL_Window* window =
        SDL_CreateWindow(
            "Super Mario Bros. - Native Reimplementation",
            NesPpu::ScreenWidth * Scale,
            NesPpu::ScreenHeight * Scale,
            0
        );

    if (!window)
    {
        std::cerr
            << "SDL_CreateWindow failed: "
            << SDL_GetError()
            << '\n';

        SDL_Quit();

        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            nullptr
        );

    if (!renderer)
    {
        std::cerr
            << "SDL_CreateRenderer failed: "
            << SDL_GetError()
            << '\n';

        SDL_DestroyWindow(window);
        SDL_Quit();

        return 1;
    }

    SDL_Texture* texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            NesPpu::ScreenWidth,
            NesPpu::ScreenHeight
        );

    if (!texture)
    {
        std::cerr
            << "SDL_CreateTexture failed: "
            << SDL_GetError()
            << '\n';

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 1;
    }

    // ------------------------------------------------------------
    // Execution
    // ------------------------------------------------------------

    bool running = true;

    uint64_t instructionCount = 0;
    uint64_t nmiCount = 0;

    while (running)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
            }
        }

        // --------------------------------------------------------
        // Execute CPU instructions.
        //
        // We keep the old chunked approach for now, but NMI is
        // checked between each instruction.
        // --------------------------------------------------------

        constexpr int InstructionsPerFrame = 5000;

        for (int i = 0;
             i < InstructionsPerFrame && running;
             ++i)
        {
            try
            {
                // ------------------------------------------------
                // PPU -> CPU NMI
                // ------------------------------------------------

                if (ppu.pollNmi())
                {
                    cpu.nmi();

                    nmiCount++;

                    if ((nmiCount % 60) == 0)
                    {
                        std::cout
                            << "NMI count: "
                            << nmiCount
                            << "  PC: "
                            << hex16(cpu.pc)
                            << '\n';
                    }
                }

                // ------------------------------------------------
                // Execute one SMB instruction
                // ------------------------------------------------

                cpu.step();

                instructionCount++;

                if ((instructionCount % 100000) == 0)
                {
                    std::cout
                        << "Instructions: "
                        << instructionCount
                        << "  PC: "
                        << hex16(cpu.pc)
                        << "  A: "
                        << hex8(cpu.a)
                        << "  X: "
                        << hex8(cpu.x)
                        << "  Y: "
                        << hex8(cpu.y)
                        << "  SP: "
                        << hex8(cpu.sp)
                        << "  STATUS: "
                        << hex8(cpu.status)
                        << '\n';
                }

                // ------------------------------------------------
                // Safety limit
                // ------------------------------------------------

                if (instructionCount >= 5000000)
                {
                    std::cout
                        << "Instruction limit reached.\n";

                    running = false;
                }
            }
            catch (const std::exception& exception)
            {
                std::cerr
                    << "\nCPU execution stopped.\n"
                    << "PC: "
                    << hex16(cpu.pc)
                    << "\nException: "
                    << exception.what()
                    << '\n';

                running = false;
            }
        }

        // --------------------------------------------------------
        // Render current PPU state
        // --------------------------------------------------------

        ppu.renderFrame();

        const auto& framebuffer =
            ppu.getFramebuffer();

        SDL_UpdateTexture(
            texture,
            nullptr,
            framebuffer.data(),
            NesPpu::ScreenWidth *
                static_cast<int>(
                    sizeof(uint32_t)
                )
        );

        SDL_RenderClear(renderer);

        SDL_RenderTexture(
            renderer,
            texture,
            nullptr,
            nullptr
        );

        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    // ------------------------------------------------------------
    // Results
    // ------------------------------------------------------------

    std::cout
        << "\nTotal instructions executed: "
        << instructionCount
        << '\n';

    std::cout
        << "Total NMIs: "
        << nmiCount
        << '\n';

    std::cout
        << "Final PC: "
        << hex16(cpu.pc)
        << '\n';

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}