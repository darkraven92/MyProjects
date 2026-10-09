#include "../src/Bot/ProfileWorldEvidenceAdapter.h"
#include "../src/Bot/LevelingProfileSelector.h"

#include <cassert>
#include <limits>

#ifdef _WIN32
// Cross-compilation instantiates Capture against the actual runtime shape.
#include "../src/Objects/WorldState.h"
using WorldFixture = Objects::WorldState;
#else
// Only the fields consumed by Capture; no memory-reader stubs or callbacks.
struct WorldFixture
{
    struct Player
    {
        bool valid = false;
        std::uint32_t address = 0, descriptors = 0, level = 0;
        float x = 0, y = 0, z = 0;
        bool operator==(const Player&) const = default;
    } player;
    bool valid = false;
    std::uint32_t manager = 0, localPlayer = 0;
    std::uint64_t activePlayerGuid = 0;
    bool operator==(const WorldFixture&) const = default;
};
#endif

using namespace Bot;
using Adapter = ProfileWorldEvidenceAdapter;

static WorldFixture World()
{
    WorldFixture world;
    world.valid = world.player.valid = true;
    world.manager = 0x1000;
    world.localPlayer = world.player.address = 0x2000;
    world.player.descriptors = 0x3000;
    world.activePlayerGuid = 123;
    world.player.level = 12;
    world.player.x = -12.5f;
    world.player.y = 0;
    world.player.z = 8.25f;
    return world;
}

static ProfileWorldSampleStamp Stamp() { return {7, 19, 1000}; }

static ProfileWorldIdentityCheck Check()
{
    return {{7, 19, 1010}, true, 0x1000, 123, 0x2000};
}

static ProfileWorldEvidence Convert(const WorldFixture& world)
{
    return Adapter::Adapt(Adapter::Capture(world, Stamp()), Check(), 10);
}

static void UnsupportedUnknown(const ProfileWorldEvidence& evidence)
{
    assert(!evidence.mapId && !evidence.positionMapId && !evidence.zoneId);
    assert(!evidence.factionGroup && !evidence.identity.worldGeneration);
    assert(!evidence.raceMask && !evidence.classMask);
    assert(evidence.preparationFacts.empty());
}

static void ValidAndFieldValidation()
{
    auto world = World();
    const auto evidence = Convert(world);
    assert(evidence.QualifiedIdentity() && evidence.freshForPlayer);
    assert(evidence.identity.playerGuid == 123 && evidence.identity.monitorSession == 7);
    assert(evidence.playerLevel == 12);
    assert((evidence.position == ProfilePosition{-12.5f, 0, 8.25f}));
    UnsupportedUnknown(evidence);
    for (const auto level : {1u, 60u})
    {
        world.player.level = level;
        assert(Convert(world).playerLevel == level);
    }
    for (const auto level : {0u, 61u, std::numeric_limits<std::uint32_t>::max()})
    {
        world.player.level = level;
        const auto result = Convert(world);
        assert(!result.playerLevel && result.QualifiedIdentity());
        assert(result.position == evidence.position);
        UnsupportedUnknown(result);
    }
    world = World();
    for (const auto value : {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity()})
    {
        for (int coordinate = 0; coordinate < 3; ++coordinate)
        {
            auto invalid = world;
            if (coordinate == 0) invalid.player.x = value;
            if (coordinate == 1) invalid.player.y = value;
            if (coordinate == 2) invalid.player.z = value;
            const auto result = Convert(invalid);
            assert(!result.position && result.playerLevel == 12 && result.QualifiedIdentity());
            UnsupportedUnknown(result);
        }
    }
    world.player.x = world.player.y = world.player.z = 0;
    assert(Convert(world).position == ProfilePosition{});
}

static void InvalidSnapshotsAndIdentity()
{
    assert(Adapter::Adapt({}, {}, 10) == ProfileWorldEvidence{});
    assert(Convert(WorldFixture{}) == ProfileWorldEvidence{});
    const auto rejectWorld = [](auto change) {
        auto world = World();
        change(world);
        assert(Convert(world) == ProfileWorldEvidence{});
    };
    rejectWorld([](auto& w) { w.valid = false; });
    rejectWorld([](auto& w) { w.player.valid = false; });
    rejectWorld([](auto& w) { w.activePlayerGuid = 0; });
    rejectWorld([](auto& w) { w.manager = 0; });
    rejectWorld([](auto& w) { w.localPlayer = 0; });
    rejectWorld([](auto& w) { w.player.address = 0; });
    rejectWorld([](auto& w) { ++w.player.address; });
    rejectWorld([](auto& w) { w.player.descriptors = 0; });

    const auto snapshot = Adapter::Capture(World(), Stamp());
    const auto rejectCheck = [&](auto change) {
        auto current = Check();
        change(current);
        assert(Adapter::Adapt(snapshot, current, 10) == ProfileWorldEvidence{});
    };
    rejectCheck([](auto& c) { c.valid = false; });
    rejectCheck([](auto& c) { c.playerGuid = 0; });
    rejectCheck([](auto& c) { ++c.playerGuid; });
    rejectCheck([](auto& c) { c.manager = 0; });
    rejectCheck([](auto& c) { ++c.manager; });
    rejectCheck([](auto& c) { c.localPlayer = 0; });
    rejectCheck([](auto& c) { ++c.localPlayer; });
    rejectCheck([](auto& c) { c.stamp.monitorSession = 0; });
    rejectCheck([](auto& c) { ++c.stamp.monitorSession; });
    rejectCheck([](auto& c) { c.stamp.sampleSequence = 0; });
    rejectCheck([](auto& c) { ++c.stamp.sampleSequence; });

    for (const auto stamp : {ProfileWorldSampleStamp{},
                            ProfileWorldSampleStamp{0, 19, 1000},
                            ProfileWorldSampleStamp{7, 0, 1000}})
        assert(Adapter::Adapt(Adapter::Capture(World(), stamp), Check(), 10) ==
               ProfileWorldEvidence{});
}

static void FreshnessAndPurity()
{
    const auto world = World();
    const auto worldBefore = world;
    const auto snapshot = Adapter::Capture(world, Stamp());
    const auto snapshotBefore = snapshot;
    const auto current = Check();
    const auto currentBefore = current;
    const auto good = Adapter::Adapt(snapshot, current, 10);
    assert(good.QualifiedIdentity()); // Inclusive age boundary.
    assert(Adapter::Adapt(snapshot, current, 9) == ProfileWorldEvidence{});
    auto future = snapshot;
    future.stamp.observedAtMs = current.stamp.observedAtMs + 1;
    assert(Adapter::Adapt(future, current, 10) == ProfileWorldEvidence{});
    auto stale = snapshot;
    stale.stamp.sampleSequence--;
    // Recent time alone cannot qualify a previous evaluation.
    assert(Adapter::Adapt(stale, current, 1000) == ProfileWorldEvidence{});
    auto sameTime = current;
    sameTime.stamp.observedAtMs = snapshot.stamp.observedAtMs;
    assert(Adapter::Adapt(snapshot, sameTime, 0).QualifiedIdentity());
    assert(Adapter::Adapt(snapshot, current, 0) == ProfileWorldEvidence{});
    auto zeroTime = snapshot;
    zeroTime.stamp.observedAtMs = sameTime.stamp.observedAtMs = 0;
    assert(Adapter::Adapt(zeroTime, sameTime, 0).QualifiedIdentity());
    auto farFuture = current;
    farFuture.stamp.observedAtMs = std::numeric_limits<std::uint64_t>::max();
    assert(Adapter::Adapt(snapshot, farFuture, 10) == ProfileWorldEvidence{});
    for (int i = 0; i < 3; ++i)
    {
        assert(Adapter::Adapt(snapshot, current, 10) == good);
        assert(Adapter::Adapt({}, current, 10) == ProfileWorldEvidence{});
    }
    assert(snapshot == snapshotBefore && current == currentBefore);
#ifndef _WIN32
    assert(world == worldBefore);
#endif
    assert(Adapter::Capture(world, Stamp()) == Adapter::Capture(worldBefore, Stamp()));
}

static void UnknownPropagation()
{
    LevelingProfile profile;
    profile.id = "synthetic";
    LevelingProfileSegment segment;
    segment.id = "level_only";
    segment.minimumLevel = 1;
    segment.maximumLevel = 60;
    profile.segments = {segment};
    const auto evidence = Convert(World());
    assert(LevelingProfileSelector::Select({profile}, evidence).status ==
           LevelingSelectionStatus::Selected);
    profile.segments[0].expectedMap = 0; // Map zero is known data, not Unknown.
    assert(LevelingProfileSelector::Select({profile}, evidence).status ==
           LevelingSelectionStatus::EvidenceUnavailable);
    profile.segments[0].expectedMap.reset();
    profile.segments[0].allowedClasses = 1;
    assert(LevelingProfileSelector::Select({profile}, evidence).status ==
           LevelingSelectionStatus::EvidenceUnavailable);
    auto invalid = World();
    invalid.player.level = 61;
    assert(LevelingProfileSelector::Select({profile}, Convert(invalid)).status ==
           LevelingSelectionStatus::EvidenceUnavailable);
}

int main()
{
    ValidAndFieldValidation();
    InvalidSnapshotsAndIdentity();
    FreshnessAndPurity();
    UnknownPropagation();
}
