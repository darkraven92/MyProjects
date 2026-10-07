#pragma once
#include "DeathRecoveryEvidencePolicy.h"
#include "../Core/Memory.h"
#include "../Wow5875/Client.h"
#include "../Wow5875/Offsets.h"
#include "../Objects/WorldState.h"
#include <array>

namespace Bot
{
    // Read-only cache of MSG_CORPSE_QUERY. 5875 handler 48F734 reads found,
    // display map, XYZ, actual corpse map; 492010 stores them. Ghost-flag
    // change and world initialization call 491F50: invalidate then query.
    // A dungeon entrance is NOT the corpse: differing maps fail closed.
    struct CorpseLocation5875
    {
        template<std::size_t N>
        static bool Signature(std::uintptr_t va,
            const std::array<unsigned char, N>& expected)
        {
            std::array<unsigned char, N> actual{};
            return Core::Memory::Read(Wow5875::Client::Base() + va - 0x400000,
                actual) && actual == expected;
        }
        static bool Read(const Objects::WorldState& world,
            std::uint64_t episodeGuid, std::uint32_t map,
            DeathRecoveryEvidencePolicy::Point& point)
        {
            point = {};
            if (!Signature(0x492019, std::array<unsigned char,6>{0x89,0x3d,0x1c,0xe3,0xb4,0x00}) ||
                !Signature(0x492023, std::array<unsigned char,6>{0x89,0x0d,0x84,0xe2,0xb4,0x00}) ||
                !Signature(0x49202f, std::array<unsigned char,6>{0x89,0x15,0x88,0xe2,0xb4,0x00}) ||
                !Signature(0x49203d, std::array<unsigned char,11>{0xa3,0x8c,0xe2,0xb4,0x00,0x89,0x0d,0x20,0xe3,0xb4,0x00}) ||
                !Signature(0x491f57, std::array<unsigned char,8>{0x6a,0xff,0x8d,0x55,0xf4,0x83,0xc9,0xff}) ||
                !Signature(0x5eeac1, std::array<unsigned char,5>{0xe8,0x8a,0x34,0xea,0xff}))
                return false;
            static_assert(sizeof(DeathRecoveryEvidencePolicy::Point) == 12);
            const auto base = Wow5875::Client::Base();
            std::int32_t displayMap=-1, corpseMap=-1;
            std::uint64_t liveGuid=0, objectGuid=0;
            std::uint32_t manager=0, descriptor=0, flags=0;
            if (!world.valid || !world.player.valid ||
                !Core::Memory::Read(Wow5875::Offsets::ObjectManager::Root, manager) ||
                manager != world.manager || manager == 0 ||
                !Core::Memory::Read(world.manager +
                    Wow5875::Offsets::ObjectManager::ActivePlayerGuid, liveGuid) ||
                !Core::Memory::Read(world.player.address +
                    Wow5875::Offsets::Object::Guid, objectGuid) || objectGuid != liveGuid ||
                !Core::Memory::Read(world.player.address +
                    Wow5875::Offsets::Object::Descriptor, descriptor) ||
                !Core::Memory::Read(descriptor + 0x2f8, flags) ||
                !Core::Memory::Read(base + 0x74e31c, displayMap) ||
                !Core::Memory::Read(base + 0x74e320, corpseMap) ||
                !Core::Memory::Read(base + 0x74e284, point))
                return false;
            return DeathRecoveryEvidencePolicy::ServerAnchorEligible(
                world.valid, (flags & 0x10) != 0, episodeGuid, liveGuid,
                map, displayMap, corpseMap, point);
        }
    };
}
