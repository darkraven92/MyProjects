#pragma once

#include "../Core/Memory.h"
#include "../Wow5875/Offsets.h"

#include "PlayerSnapshot.h"
#include "UnitSnapshot.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Objects
{
    struct WorldState
    {
        std::uint32_t manager = 0;
        std::uint64_t activePlayerGuid = 0;
        std::uint32_t firstObject = 0;

        std::uint32_t localPlayer = 0;

        PlayerState player{};

        std::vector<UnitState> units;

        bool valid = false;
    };

    enum class WorldReadStage
    {
        Complete,
        ObjectManagerRootUnreadable,
        ObjectManagerMissing,
        ObjectManagerUnreadable,
        ActivePlayerGuidUnreadable,
        ActivePlayerGuidMissing,
        FirstObjectUnreadable,
        FirstObjectMissing,
        LocalPlayerMissing,
        PlayerSnapshotUnreadable
    };

    inline const char* WorldReadStageName(WorldReadStage stage)
    {
        switch (stage)
        {
            case WorldReadStage::Complete: return "complete";
            case WorldReadStage::ObjectManagerRootUnreadable: return "manager_root_unreadable";
            case WorldReadStage::ObjectManagerMissing: return "manager_missing";
            case WorldReadStage::ObjectManagerUnreadable: return "manager_unreadable";
            case WorldReadStage::ActivePlayerGuidUnreadable: return "active_guid_unreadable";
            case WorldReadStage::ActivePlayerGuidMissing: return "active_guid_missing";
            case WorldReadStage::FirstObjectUnreadable: return "first_object_unreadable";
            case WorldReadStage::FirstObjectMissing: return "first_object_missing";
            case WorldReadStage::LocalPlayerMissing: return "local_player_missing";
            case WorldReadStage::PlayerSnapshotUnreadable: return "player_snapshot_unreadable";
        }
        return "unknown";
    }

    class WorldStateReader
    {
    public:
        static bool Read(WorldState& state, WorldReadStage* stage = nullptr)
        {
            state = {};

            const auto fail = [stage](WorldReadStage reason)
            {
                if (stage)
                    *stage = reason;
                return false;
            };

            // ---------------------------------------------
            // ObjectManager
            // ---------------------------------------------

            if (!Core::Memory::Read(
                    Wow5875::Offsets::
                        ObjectManager::Root,
                    state.manager))
            {
                return fail(WorldReadStage::ObjectManagerRootUnreadable);
            }

            if (state.manager == 0)
                return fail(WorldReadStage::ObjectManagerMissing);

            if (!Core::Memory::IsReadable(
                    state.manager,
                    0xC8))
            {
                return fail(WorldReadStage::ObjectManagerUnreadable);
            }

            // ---------------------------------------------
            // Active player GUID
            // ---------------------------------------------

            if (!Core::Memory::Read(
                    state.manager +
                        Wow5875::Offsets::
                            ObjectManager::
                            ActivePlayerGuid,
                    state.activePlayerGuid))
            {
                return fail(WorldReadStage::ActivePlayerGuidUnreadable);
            }

            if (state.activePlayerGuid == 0)
                return fail(WorldReadStage::ActivePlayerGuidMissing);

            // ---------------------------------------------
            // First object
            // ---------------------------------------------

            if (!Core::Memory::Read(
                    state.manager +
                        Wow5875::Offsets::
                            ObjectManager::
                            FirstObject,
                    state.firstObject))
            {
                return fail(WorldReadStage::FirstObjectUnreadable);
            }

            if (state.firstObject == 0 ||
                (state.firstObject & 1) != 0)
            {
                return fail(WorldReadStage::FirstObjectMissing);
            }

            // ---------------------------------------------
            // Enumerate world objects
            // ---------------------------------------------

            std::uint32_t current =
                state.firstObject;

            for (int i = 0;
                 i < 4096 &&
                 current != 0 &&
                 (current & 1) == 0;
                 ++i)
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

                // -----------------------------------------
                // Local player
                // -----------------------------------------

                if (type == 4 &&
                    guid ==
                        state.activePlayerGuid)
                {
                    state.localPlayer =
                        current;
                }

                // -----------------------------------------
                // Unit
                // -----------------------------------------

                if (type == 3)
                {
                    UnitState unit{};

                    if (UnitSnapshot::Read(
                            current,
                            unit))
                    {
                        state.units.push_back(
                            unit
                        );
                    }
                }

                if (next == current)
                    break;

                current = next;
            }

            if (state.localPlayer == 0)
                return fail(WorldReadStage::LocalPlayerMissing);

            // ---------------------------------------------
            // Local player state
            // ---------------------------------------------

            if (!PlayerSnapshot::Read(
                    state.localPlayer,
                    state.player))
            {
                return fail(WorldReadStage::PlayerSnapshotUnreadable);
            }

            // ---------------------------------------------
            // Unit distances
            // ---------------------------------------------

            for (auto& unit :
                 state.units)
            {
                unit.distance =
                    UnitSnapshot::Distance(
                        state.player.x,
                        state.player.y,
                        state.player.z,

                        unit.x,
                        unit.y,
                        unit.z
                    );
            }

            std::sort(
                state.units.begin(),
                state.units.end(),
                [](const UnitState& a,
                   const UnitState& b)
                {
                    return
                        a.distance <
                        b.distance;
                }
            );

            state.valid = true;

            if (stage)
                *stage = WorldReadStage::Complete;

            return true;
        }
    };
}
