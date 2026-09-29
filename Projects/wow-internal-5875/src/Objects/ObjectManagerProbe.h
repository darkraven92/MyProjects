#pragma once

#include "../Core/Memory.h"
#include "../Debug/Logger.h"
#include "../Wow5875/Offsets.h"

#include "PlayerSnapshot.h"
#include "UnitSnapshot.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Objects
{
    class ObjectManagerProbe
    {
    private:
        static std::string Hex32(
            std::uint32_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(8)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static std::string Hex64(
            std::uint64_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(16)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(2)
                << value;

            return stream.str();
        }

        static const char* TypeName(
            std::uint32_t type)
        {
            switch (type)
            {
                case 1: return "Item";
                case 2: return "Container";
                case 3: return "Unit";
                case 4: return "Player";
                case 5: return "GameObject";
                case 6: return "DynamicObject";
                case 7: return "Corpse";
                default: return "Unknown";
            }
        }

    public:
        static void Run()
        {
            Debug::Logger::Info(
                "=== ObjectManager read-only probe ==="
            );

            std::uint32_t manager = 0;

            if (!Core::Memory::Read(
                    Wow5875::Offsets::
                        ObjectManager::Root,
                    manager))
            {
                Debug::Logger::Info(
                    "Failed reading ObjectManager root."
                );

                return;
            }

            Debug::Logger::Info(
                "ObjectManager root: " +
                Hex32(
                    static_cast<std::uint32_t>(
                        Wow5875::Offsets::
                            ObjectManager::Root
                    )
                )
            );

            Debug::Logger::Info(
                "ObjectManager instance: " +
                Hex32(manager)
            );

            if (manager == 0)
                return;

            std::uint64_t activeGuid = 0;
            std::uint32_t current = 0;

            if (!Core::Memory::Read(
                    manager +
                        Wow5875::Offsets::
                            ObjectManager::
                            ActivePlayerGuid,
                    activeGuid))
            {
                return;
            }

            if (!Core::Memory::Read(
                    manager +
                        Wow5875::Offsets::
                            ObjectManager::
                            FirstObject,
                    current))
            {
                return;
            }

            Debug::Logger::Info(
                "ActivePlayerGuid: " +
                Hex64(activeGuid)
            );

            Debug::Logger::Info(
                "FirstObject: " +
                Hex32(current)
            );

            std::uint32_t localPlayer = 0;

            std::vector<std::uint32_t>
                unitAddresses;

            int total = 0;
            int items = 0;
            int containers = 0;
            int units = 0;
            int players = 0;
            int gameObjects = 0;
            int dynamicObjects = 0;
            int corpses = 0;
            int unknown = 0;

            while (
                current != 0 &&
                (current & 1) == 0 &&
                total < 4096)
            {
                if (!Core::Memory::IsReadable(
                        current,
                        0x40))
                {
                    break;
                }

                std::uint32_t type = 0;
                std::uint64_t guid = 0;
                std::uint32_t next = 0;

                if (!Core::Memory::Read(
                        current +
                            Wow5875::Offsets::
                                Object::Type,
                        type))
                {
                    break;
                }

                if (!Core::Memory::Read(
                        current +
                            Wow5875::Offsets::
                                Object::Guid,
                        guid))
                {
                    break;
                }

                if (!Core::Memory::Read(
                        current +
                            Wow5875::Offsets::
                                Object::Next,
                        next))
                {
                    break;
                }

                if (total < 20)
                {
                    Debug::Logger::Info(
                        "Object[" +
                        std::to_string(total) +
                        "] addr=" +
                        Hex32(current) +
                        " type=" +
                        TypeName(type) +
                        "(" +
                        std::to_string(type) +
                        ")" +
                        " guid=" +
                        Hex64(guid) +
                        " next=" +
                        Hex32(next)
                    );
                }

                switch (type)
                {
                    case 1:
                        ++items;
                        break;

                    case 2:
                        ++containers;
                        break;

                    case 3:
                        ++units;

                        unitAddresses.push_back(
                            current
                        );

                        break;

                    case 4:
                        ++players;
                        break;

                    case 5:
                        ++gameObjects;
                        break;

                    case 6:
                        ++dynamicObjects;
                        break;

                    case 7:
                        ++corpses;
                        break;

                    default:
                        ++unknown;
                        break;
                }

                if (guid == activeGuid)
                {
                    localPlayer =
                        current;

                    Debug::Logger::Info(
                        "*** LOCAL PLAYER FOUND ***"
                    );

                    Debug::Logger::Info(
                        "LocalPlayer address: " +
                        Hex32(localPlayer)
                    );

                    Debug::Logger::Info(
                        "LocalPlayer GUID: " +
                        Hex64(guid)
                    );
                }

                ++total;

                if (next == current)
                    break;

                current = next;
            }

            Debug::Logger::Info(
                "=== Object counts ==="
            );

            Debug::Logger::Info(
                "Total: " +
                std::to_string(total)
            );

            Debug::Logger::Info(
                "Players: " +
                std::to_string(players)
            );

            Debug::Logger::Info(
                "Units: " +
                std::to_string(units)
            );

            Debug::Logger::Info(
                "GameObjects: " +
                std::to_string(gameObjects)
            );

            Debug::Logger::Info(
                "Items: " +
                std::to_string(items)
            );

            Debug::Logger::Info(
                "Containers: " +
                std::to_string(containers)
            );

            Debug::Logger::Info(
                "DynamicObjects: " +
                std::to_string(dynamicObjects)
            );

            Debug::Logger::Info(
                "Corpses: " +
                std::to_string(corpses)
            );

            Debug::Logger::Info(
                "Unknown: " +
                std::to_string(unknown)
            );

            Debug::Logger::Info(
                "LocalPlayer address: " +
                Hex32(localPlayer)
            );

            if (localPlayer == 0)
            {
                Debug::Logger::Info(
                    "LocalPlayer not found."
                );

                return;
            }

            // ------------------------------------------------
            // Player state
            // ------------------------------------------------

            PlayerState player{};

            if (!PlayerSnapshot::Read(
                    localPlayer,
                    player))
            {
                Debug::Logger::Info(
                    "Failed reading PlayerSnapshot."
                );

                return;
            }

            PlayerSnapshot::Run(
                localPlayer
            );

            // ------------------------------------------------
            // Units
            // ------------------------------------------------

            std::vector<UnitState>
                unitStates;

            for (const auto address :
                 unitAddresses)
            {
                UnitState unit{};

                if (!UnitSnapshot::Read(
                        address,
                        unit))
                {
                    continue;
                }

                unit.distance =
                    UnitSnapshot::Distance(
                        player.x,
                        player.y,
                        player.z,
                        unit.x,
                        unit.y,
                        unit.z
                    );

                unitStates.push_back(
                    unit
                );
            }

            std::sort(
                unitStates.begin(),
                unitStates.end(),
                [](const UnitState& a,
                   const UnitState& b)
                {
                    return
                        a.distance <
                        b.distance;
                }
            );

            Debug::Logger::Info(
                "=== Nearby units ==="
            );

            Debug::Logger::Info(
                "Readable units: " +
                std::to_string(
                    unitStates.size()
                )
            );

            const std::size_t showCount =
                std::min<std::size_t>(
                    unitStates.size(),
                    20
                );

            for (std::size_t i = 0;
                 i < showCount;
                 ++i)
            {
                const UnitState& unit =
                    unitStates[i];

                Debug::Logger::Info(
                    "Unit[" +
                    std::to_string(i) +
                    "]" +
                    " addr=" +
                    Hex32(unit.address) +
                    " entry=" +
                    std::to_string(
                        unit.entryId
                    ) +
                    " guid=" +
                    Hex64(unit.guid) +
                    " lvl=" +
                    std::to_string(
                        unit.level
                    ) +
                    " hp=" +
                    std::to_string(
                        unit.health
                    ) +
                    "/" +
                    std::to_string(
                        unit.maxHealth
                    ) +
                    " faction=" +
                    std::to_string(
                        unit.factionTemplate
                    ) +
                    " dist=" +
                    Float(unit.distance) +
                    " pos=(" +
                    Float(unit.x) +
                    "," +
                    Float(unit.y) +
                    "," +
                    Float(unit.z) +
                    ")" +
                    " target=" +
                    Hex64(unit.targetGuid)
                );
            }

            Debug::Logger::Info(
                "ObjectManager probe completed."
            );
        }
    };
}
