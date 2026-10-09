#pragma once

#include "ProfileWorldEvidence.h"

namespace Bot
{
    // Assigned at acquisition, never retroactively to a cached snapshot.
    // sampleSequence identifies one evaluation within monitorSession, not a
    // map/world generation. Times use the caller's same monotonic clock:
    // snapshot time is the read START (bounds the age of every field), and
    // current-check time is the evaluation time AFTER the identity recheck.
    struct ProfileWorldSampleStamp
    {
        std::uint64_t monitorSession = 0;
        std::uint64_t sampleSequence = 0;
        std::uint64_t observedAtMs = 0;
        bool operator==(const ProfileWorldSampleStamp&) const = default;
    };

    struct ProfileWorldSnapshot
    {
        ProfileWorldSampleStamp stamp;
        bool valid = false;
        std::uint32_t manager = 0;
        std::uint64_t playerGuid = 0;
        std::uint32_t localPlayer = 0;
        std::uint32_t level = 0;
        ProfilePosition position;
        bool operator==(const ProfileWorldSnapshot&) const = default;
    };

    // A future sampler must independently recheck the current manager, active
    // GUID and matching local-player object AFTER acquisition. Copying these
    // values from WorldState is not a recheck. valid asserts those reads and
    // the object's type/GUID association succeeded for this evaluation.
    struct ProfileWorldIdentityCheck
    {
        ProfileWorldSampleStamp stamp;
        bool valid = false;
        std::uint32_t manager = 0;
        std::uint64_t playerGuid = 0;
        std::uint32_t localPlayer = 0;
        bool operator==(const ProfileWorldIdentityCheck&) const = default;
    };

    // Pure value adaptation: no readers, commands, clocks, owners or cache.
    // Raw world.valid alone never qualifies evidence. QuestPlannerSnapshot has
    // no acquisition binding, so it is deliberately not an accepted input.
    class ProfileWorldEvidenceAdapter
    {
    public:
        // Member-based projection permits Objects::WorldState without pulling
        // its Windows memory-reader dependencies into this portable policy.
        // Call immediately after the read with its original acquisition stamp.
        // This only copies data; it does not establish identity/freshness.
        template<class WorldSnapshot>
        static ProfileWorldSnapshot Capture(const WorldSnapshot& world,
                                            ProfileWorldSampleStamp stamp)
        {
            ProfileWorldSnapshot snapshot;
            snapshot.stamp = stamp;
            snapshot.valid = world.valid && world.player.valid &&
                world.manager != 0 && world.localPlayer != 0 &&
                world.player.address == world.localPlayer &&
                world.player.descriptors != 0;
            snapshot.manager = world.manager;
            snapshot.playerGuid = world.activePlayerGuid;
            snapshot.localPlayer = world.localPlayer;
            snapshot.level = world.player.level;
            snapshot.position = {world.player.x, world.player.y, world.player.z};
            return snapshot;
        }

        // maximumAgeMs is an explicit caller budget, not a default freshness
        // promise. Zero allows only the same timestamp. A different evaluation
        // is rejected even if the sample is within the age budget.
        static ProfileWorldEvidence Adapt(const ProfileWorldSnapshot& snapshot,
                                          const ProfileWorldIdentityCheck& current,
                                          std::uint64_t maximumAgeMs)
        {
            ProfileWorldEvidence evidence;
            if (!snapshot.valid || !current.valid ||
                snapshot.manager == 0 || snapshot.manager != current.manager ||
                snapshot.localPlayer == 0 || snapshot.localPlayer != current.localPlayer ||
                snapshot.playerGuid == 0 || snapshot.playerGuid != current.playerGuid ||
                snapshot.stamp.monitorSession == 0 ||
                snapshot.stamp.monitorSession != current.stamp.monitorSession ||
                snapshot.stamp.sampleSequence == 0 ||
                snapshot.stamp.sampleSequence != current.stamp.sampleSequence ||
                snapshot.stamp.observedAtMs > current.stamp.observedAtMs ||
                current.stamp.observedAtMs - snapshot.stamp.observedAtMs > maximumAgeMs)
                return evidence;

            evidence.identity.playerGuid = snapshot.playerGuid;
            evidence.identity.monitorSession = snapshot.stamp.monitorSession;
            evidence.freshForPlayer = true;
            if (snapshot.level >= 1 && snapshot.level <= 60)
                evidence.playerLevel = snapshot.level;
            if (snapshot.position.Finite())
                evidence.position = snapshot.position;

            // No qualified current-map/zone/faction/world-generation sources.
            // XYZ has no map association. Unbound quest class/race and owner
            // facts remain unknown; never infer any of these from config/data.
            return evidence;
        }
    };
}
