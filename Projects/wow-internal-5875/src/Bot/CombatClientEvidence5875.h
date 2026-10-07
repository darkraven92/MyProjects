#pragma once
#include "GameThreadDispatcher.h"
#include "../Core/Memory.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"
#include <array>

namespace Bot
{
    // UI selection is NOT UNIT_FIELD_TARGET (the server's attack victim).
    // SetTarget 493540 compares B4E2D8/DC at 493605..493617 and returns
    // without sending CMSG_SET_SELECTION if it already equals the request.
    struct CombatClientEvidence5875
    {
        static bool Selection(const Objects::WorldState& world, std::uint64_t& guid)
        {
            guid=0;
            bool known=false;
            GameThreadDispatcher::Invoke([&]
            {
                if (!GameThreadDispatcher::IsGameThread() || !world.valid || !world.player.valid) return;
                const auto base=Wow5875::Client::Base();
                constexpr std::array<unsigned char,24> expected{
                    0x8b,0x0d,0xd8,0xe2,0xb4,0x00,0x3b,0xf9,
                    0xa1,0xdc,0xe2,0xb4,0x00,0x75,0x09,0x39,
                    0x45,0x0c,0x0f,0x84,0xe0,0x02,0x00,0x00};
                std::array<unsigned char,24> actual{};
                std::uint32_t manager=0;
                std::uint64_t player=0;
                known=Core::Memory::Read(base+0x93605,actual) && actual==expected &&
                    Core::Memory::Read(Wow5875::Offsets::ObjectManager::Root,manager) &&
                    manager && manager==world.manager &&
                    Core::Memory::Read(manager+Wow5875::Offsets::ObjectManager::ActivePlayerGuid,player) &&
                    player==world.activePlayerGuid && player!=0 &&
                    Core::Memory::Read(base+0x74e2d8,guid);
            });
            if (!known) guid=0;
            return known;
        }
        static bool FreshHealth(const Objects::WorldState& world,
            const Objects::UnitState& target, std::uint32_t& hp,
            std::uint32_t& playerHp, std::uint32_t& attackPeriod)
        {
            std::uint64_t guid=0;
            std::uint32_t flags=0, descriptor=0;
            return world.valid && world.player.valid && target.valid &&
                Core::Memory::Read(target.address+Wow5875::Offsets::Object::Guid,guid) && guid==target.guid &&
                Core::Memory::Read(target.address+Wow5875::Offsets::Object::Descriptor,descriptor) &&
                descriptor==target.descriptors && descriptor &&
                Core::Memory::Read(descriptor+0x58,hp) &&
                Core::Memory::Read(world.player.address+Wow5875::Offsets::Object::Guid,guid) && guid==world.activePlayerGuid &&
                Core::Memory::Read(world.player.address+Wow5875::Offsets::Object::Descriptor,descriptor) &&
                descriptor==world.player.descriptors && descriptor &&
                Core::Memory::Read(descriptor+0x58,playerHp) && playerHp>0 &&
                Core::Memory::Read(descriptor+0x2f8,flags) && !(flags&0x10u) &&
                Core::Memory::Read(descriptor+0x1f8,attackPeriod) && attackPeriod>0 && attackPeriod<=10000;
        }
    };
}
