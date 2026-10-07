#pragma once

#include "WaterEvidencePolicy5875.h"
#include "../Core/Memory.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <array>
#include <cstdint>

namespace Bot
{
    // A read-only observation adapter for the one verified live water signal.
    // This adapter never invokes client input, Lua scripts, or native commands.
    class WaterEvidence5875
    {
    public:
        static bool Supported()
        {
            constexpr std::array<unsigned char,13> expected{
                0x8b,0x8e,0x18,0x01,0x00,0x00,0xf7,0x41,0x40,
                0x00,0x00,0x20,0x00};
            std::array<unsigned char,13> actual{};
            return Core::Memory::Read(Wow5875::Client::Base()+0x20e0d4,actual) &&
                actual==expected;
        }

        static WaterEvidenceSnapshot5875 Read(const Objects::WorldState& world)
        {
            WaterEvidenceSnapshot5875 result;
            if (!world.valid || !world.player.valid || !world.activePlayerGuid ||
                !world.localPlayer || world.player.address!=world.localPlayer ||
                !world.player.movement || !Supported())
                return result;
            result.movementKnown=Core::Memory::Read(
                std::uintptr_t(world.player.movement)+0x40,result.movementFlags);
            return result;
        }
    };
}
