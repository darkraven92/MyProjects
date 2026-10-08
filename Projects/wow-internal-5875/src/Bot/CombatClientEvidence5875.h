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
        struct ExecutionEvidence
        {
            bool known=false, aggressorsKnown=false, aggressor=false;
            std::uint32_t aggressorCount=0;
            std::uint64_t selected=0, playerVictim=0, targetVictim=0;
            std::uint32_t targetHp=0, targetMaxHp=0, playerHp=0, attackPeriodMs=0;
            std::uint32_t playerFlags=0, targetFlags=0, dynamicFlags=0, faction=0;
            std::uint32_t targetMovementFlags=0, playerMovementFlags=0;
            bool PlayerCombat() const { return (playerFlags&0x80000u)!=0; }
            bool TargetCombat() const { return (targetFlags&0x80000u)!=0; }
        };
        static ExecutionEvidence Execution(const Objects::WorldState& world,
            const Objects::UnitState& target)
        {
            ExecutionEvidence e{};
            GameThreadDispatcher::Invoke([&]
            {
                if (!GameThreadDispatcher::IsGameThread() || !Selection(world,e.selected) ||
                    !FreshHealth(world,target,e.targetHp,e.playerHp,e.attackPeriodMs)) return;
                // 5875 replicated update fields. Base attack period is not an
                // executed swing or a next-swing timer; those remain unknown.
                e.known=Core::Memory::Read(target.descriptors+0x70,e.targetMaxHp) &&
                    Core::Memory::Read(target.descriptors+0x40,e.targetVictim) &&
                    Core::Memory::Read(target.descriptors+0xb8,e.targetFlags) &&
                    Core::Memory::Read(target.descriptors+0x23c,e.dynamicFlags) &&
                    Core::Memory::Read(target.descriptors+0x8c,e.faction) &&
                    target.movement && Core::Memory::Read(target.movement+0x40,e.targetMovementFlags) &&
                    Core::Memory::Read(world.player.descriptors+0x40,e.playerVictim) &&
                    Core::Memory::Read(world.player.descriptors+0xb8,e.playerFlags) &&
                    world.player.movement && Core::Memory::Read(world.player.movement+0x40,e.playerMovementFlags);
                if (!e.known) return;
                // Do not infer "no aggressor" from a possibly incomplete
                // cached unit vector. Walk the live manager on the game thread.
                std::uint32_t current=0;
                if (!Core::Memory::Read(world.manager+Wow5875::Offsets::ObjectManager::FirstObject,current) || !current)
                    return;
                bool playerSeen=false;
                unsigned count=0;
                for (; current && !(current&1u) && count<4096; ++count)
                {
                    std::uint64_t guid=0, victim=0;
                    std::uint32_t descriptor=0, hp=0, type=0, next=0;
                    if (!Core::Memory::Read(current+0x14,type) || !Core::Memory::Read(current+0x30,guid) ||
                        !Core::Memory::Read(current+0x3c,next) || next==current) return;
                    if (type==3 || type==4)
                    {
                        if (!guid || !Core::Memory::Read(current+0x08,descriptor) || !descriptor ||
                            !Core::Memory::Read(descriptor+0x58,hp) || !Core::Memory::Read(descriptor+0x40,victim)) return;
                        if (guid==world.activePlayerGuid) playerSeen=current==world.player.address;
                        else if (hp && victim==world.activePlayerGuid)
                        { e.aggressor=true; ++e.aggressorCount; }
                    }
                    current=next;
                }
                e.aggressorsKnown=playerSeen && (!current || (current&1u)) && count<4096;
            });
            return e;
        }
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
