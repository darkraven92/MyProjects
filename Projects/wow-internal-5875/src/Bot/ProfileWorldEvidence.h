#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace Bot
{
    enum class ProfileFactionGroup { Alliance, Horde };

    struct ProfileEvidenceIdentity
    {
        std::optional<std::uint64_t> playerGuid;
        std::optional<std::uint64_t> monitorSession;
        // Independent evidence only: never substitute monitorSession or a
        // navigation mesh generation. Unknown does not prevent advisory use.
        std::optional<std::uint64_t> worldGeneration;
        bool operator==(const ProfileEvidenceIdentity&) const = default;
    };

    struct ProfilePosition
    {
        float x = 0, y = 0, z = 0;
        bool Finite() const
        {
            return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
        }
        bool operator==(const ProfilePosition&) const = default;
    };

    // Caller-owned, fresh, qualified observations only. This is not a reader.
    // Current production source has no authoritative live map/zone/generation.
    // Never populate map from a route, corpse location, or Grind MapIdValue().
    struct ProfileWorldEvidence
    {
        ProfileEvidenceIdentity identity;
        // The future sampler must qualify all populated fields/facts for the
        // current evaluation and recheck player identity. Default is unknown.
        // This assertion is not established by world.valid alone.
        bool freshForPlayer = false;
        std::optional<std::uint32_t> playerLevel;
        std::optional<std::uint32_t> mapId; // Zero is a valid map, not unknown.
        std::optional<ProfilePosition> position;
        // Map associated with these coordinates in the SAME qualified sample.
        // Current PlayerSnapshot XYZ alone cannot populate this field.
        std::optional<std::uint32_t> positionMapId;
        std::optional<std::uint32_t> zoneId; // Reserved; matching is unavailable.
        // Single-bit identities, using existing QuestGraph/QuestAcquisition
        // masks. Separate, unbound quest snapshots cannot populate these.
        std::optional<std::uint32_t> raceMask, classMask;
        std::optional<ProfileFactionGroup> factionGroup; // Reserved, unavailable.

        // Named facts supplied by future owners; absent/nullopt is unknown,
        // false means unmet. Requirements never invoke those owners.
        std::map<std::string, std::optional<bool>> preparationFacts;

        bool QualifiedIdentity() const
        {
            return freshForPlayer && identity.playerGuid && *identity.playerGuid != 0 &&
                identity.monitorSession && *identity.monitorSession != 0;
        }
        // Masks are already defined by QuestGraph and QuestAcquisitionPolicy.
        static constexpr std::uint32_t VanillaRaces = 0xffu;
        static constexpr std::uint32_t VanillaClasses = 0x5dfu;
        static bool KnownIdentityMask(const std::optional<std::uint32_t>& value,
                                      std::uint32_t allowed)
        {
            return value && *value != 0 && (*value & (*value - 1)) == 0 &&
                (*value & ~allowed) == 0;
        }
        bool operator==(const ProfileWorldEvidence&) const = default;
    };
}
