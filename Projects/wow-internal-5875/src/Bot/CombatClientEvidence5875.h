#pragma once
#include "GameThreadDispatcher.h"
#include "../Core/Memory.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"
#include <array>

namespace Bot
{
    // UI selection is NOT UNIT_FIELD_TARGET. The server also writes that
    // replicated field for CMSG_SET_SELECTION (Player::SetSelectionGuid),
    // besides Unit::Attack/AttackStop; it is not exclusive Attack ownership.
    // SetTarget 493540 compares B4E2D8/DC at 493605..493617 and returns
    // without sending CMSG_SET_SELECTION if it already equals the request.
    struct CombatClientEvidence5875
    {
        struct LivingEvidence
        {
            bool identity=false, alive=false, known=false, combat=false;
            bool scanComplete=false, aggressor=false;
            std::uint64_t attacker=0;
            std::uint32_t hp=0, maxHp=0;
        };
        // Targetless read only. Uses the same 5875 fields as Execution/FreshHealth.
        // A complete client enumeration is NOT a hostile-visibility guarantee.
        static LivingEvidence Living(const Objects::WorldState& world)
        {
            LivingEvidence e{};
            GameThreadDispatcher::Invoke([&]
            {
                std::uint64_t selected=0, guid=0;
                std::uint32_t descriptor=0, flags=0, life=0;
                const auto identity = [&]() {
                    return world.valid && world.player.valid && world.localPlayer &&
                        world.localPlayer==world.player.address && Selection(world,selected) &&
                        Core::Memory::Read(world.player.address+0x30,guid) && guid==world.activePlayerGuid &&
                        Core::Memory::Read(world.player.address+0x08,descriptor) && descriptor &&
                        descriptor==world.player.descriptors;
                };
                if (!GameThreadDispatcher::IsGameThread() || !identity()) return;
                e.identity=true;
                e.known=Core::Memory::Read(descriptor+0x58,e.hp) &&
                    Core::Memory::Read(descriptor+0x70,e.maxHp) && e.maxHp &&
                    Core::Memory::Read(descriptor+0x2f8,life) &&
                    Core::Memory::Read(descriptor+0xb8,flags);
                if (!e.known) return;
                e.alive=e.hp>1 && !(life&0x10u);
                e.combat=(flags&0x80000u)!=0;
                std::uint32_t current=0;
                bool complete=Core::Memory::Read(world.manager+
                    Wow5875::Offsets::ObjectManager::FirstObject,current) && current;
                bool playerSeen=false;
                unsigned count=0;
                for (; complete && current && !(current&1u) && count<4096; ++count)
                {
                    std::uint64_t unitGuid=0, victim=0;
                    std::uint32_t type=0, next=0, desc=0, hp=0;
                    complete=Core::Memory::Read(current+0x14,type) &&
                        Core::Memory::Read(current+0x30,unitGuid) &&
                        Core::Memory::Read(current+0x3c,next) && next!=current;
                    if (!complete) break;
                    if (type==3 || type==4)
                    {
                        complete=unitGuid && Core::Memory::Read(current+0x08,desc) && desc &&
                            Core::Memory::Read(desc+0x58,hp) && Core::Memory::Read(desc+0x40,victim);
                        if (!complete) break;
                        if (unitGuid==world.activePlayerGuid) playerSeen=current==world.localPlayer;
                        else if (hp && victim==world.activePlayerGuid)
                        {
                            e.aggressor=true; // positive pressure even if later enumeration fails
                            // WorldState exposes type-3 units only. Never invent a type-4 combat target.
                            for (const auto& unit:world.units)
                                if (!e.attacker && type==3 && unit.valid && unit.guid==unitGuid &&
                                    unit.address==current && unit.descriptors==desc && unit.health)
                                    e.attacker=unitGuid;
                        }
                    }
                    current=next;
                }
                e.scanComplete=complete && playerSeen && count<4096 && (!current || (current&1u));
                if (!identity()) e={};
            });
            return e;
        }
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
