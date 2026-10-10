#include "../src/Bot/DeathRecoveryRepeatDeathPolicy.h"
#include "../src/Bot/DeathRecoveryPolicy.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

int main()
{
    using P = Bot::DeathRecoveryRepeatDeathPolicy;
    P policy;
    const P::Point reclaim{2010.356f, -2581.570f, 96.356f};
    // First death has no verified automatic resurrection: no veto.
    policy.BeginDeath(42, 1, 1000);
    assert(!policy.ObserveCorpse(reclaim));
    assert(!policy.ShouldBlock(true));

    // Observed repeated-death location from the preserved capture. This test
    // supplies synthetic monotonic time; log ticks do not prove wall time.
    policy.RecordAutomaticAlive(42, 1, reclaim, 1000);
    policy.BeginDeath(42, 1, 21000);
    assert(policy.ObserveCorpse(reclaim));
    assert(policy.AgeAtDeathMs() == 20000);
    assert(policy.Separation() == 0);
    assert(!policy.ShouldBlock(false)); // Release/positive Ghost confirmation first.
    assert(policy.ShouldBlock(true));
    assert(!policy.ObserveCorpse({100, 100, 100}));
    assert(policy.ShouldBlock(true)); // Later positions cannot clear a verdict.

    // Timing is captured at death, not when server location finally arrives.
    policy.RecordAutomaticAlive(42, 1, reclaim, 1000);
    policy.BeginDeath(42, 1, 1000 + P::RecentReclaimMs);
    assert(!policy.Latched()); // Ghost bootstrap has no body evidence yet.
    assert(!policy.ShouldBlock(true));
    assert(policy.ObserveCorpse(reclaim)); // Even after a lengthy anchor wait/run.
    assert(policy.ShouldBlock(true));

    // Exact time/radius boundaries, including vertical separation.
    for (const auto age : {P::RecentReclaimMs, P::RecentReclaimMs + 1})
    {
        policy.RecordAutomaticAlive(42, 1, {0, 0, 0}, 1000);
        policy.BeginDeath(42, 1, 1000 + age);
        assert(policy.ObserveCorpse({0, 0, 8}) == (age == P::RecentReclaimMs));
    }
    policy.RecordAutomaticAlive(42, 1, {0, 0, 0}, 1000);
    policy.BeginDeath(42, 1, 1001);
    assert(!policy.ObserveCorpse({0, 0, 8.01f}));
    assert(!policy.ObserveCorpse({6, 6, 0}));
    assert(!policy.ShouldBlock(true));
    assert(policy.ObserveCorpse({0, 0, 8}));

    // Endurance capture geometry: first redeath is outside the engineered
    // 8-yard bound (8.942 yd), second is inside (2.054 yd). First age is
    // synthetic: this test must not invent a runtime monotonic timestamp.
    policy.RecordAutomaticAlive(112743,1,{1814.078f,-2438.831f,87.153f},1000);
    policy.BeginDeath(112743,1,21000);
    assert(!policy.ObserveCorpse({1805.453f,-2440.467f,88.854f}));
    assert(!policy.ShouldBlock(true));
    policy.RecordAutomaticAlive(112743,1,{1812.097f,-2439.518f,87.578f},30000);
    policy.BeginDeath(112743,1,30000+24421);
    assert(policy.ObserveCorpse({1810.099f,-2439.740f,87.999f}));
    assert(!policy.ShouldBlock(false));
    assert(policy.ShouldBlock(true));

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    for (const auto invalid : {P::Point{nan, 0, 0}, P::Point{0, inf, 0}, P::Point{0, 0, nan}})
    {
        policy.RecordAutomaticAlive(42, 1, invalid, 1000);
        policy.BeginDeath(42, 1, 1001);
        assert(!policy.ObserveCorpse({0, 0, 0}));
        policy.RecordAutomaticAlive(42, 1, {0, 0, 0}, 1000);
        policy.BeginDeath(42, 1, 1001);
        assert(!policy.ObserveCorpse(invalid));
        assert(!policy.ShouldBlock(true));
    }
    // Foreign player/map, missing identity, rollback, or expired observation
    // discards history; a later matching identity cannot resurrect that proof.
    const auto mismatch = [&](std::uint64_t guid, std::uint32_t map, std::uint64_t now) {
        policy.RecordAutomaticAlive(42, 1, reclaim, 1000);
        policy.BeginDeath(guid, map, now);
        assert(!policy.ObserveCorpse(reclaim));
        policy.BeginDeath(42, 1, 1001);
        assert(!policy.ObserveCorpse(reclaim));
    };
    mismatch(43, 1, 1001);
    mismatch(0, 1, 1001);
    mismatch(42, 2, 1001);
    mismatch(42, 1, 999);
    mismatch(42, 1, 1001 + P::RecentReclaimMs);

    policy.RecordAutomaticAlive(0, 1, reclaim, 1000);
    policy.BeginDeath(0, 1, 1001);
    assert(!policy.ObserveCorpse(reclaim));
    policy.RecordAutomaticAlive(42, 1, reclaim, 1000);
    const auto retainedAcrossRearm = policy;
    policy.Reset(); // Explicit reset, world gap, or manual alive clears history.
    policy.BeginDeath(42, 1, 1001);
    assert(!policy.ObserveCorpse(reclaim));
    policy = retainedAcrossRearm;
    policy.BeginDeath(42, 1, 1001);
    assert(policy.ObserveCorpse(reclaim));
    policy.Reset(); // Clearing history cannot itself release controller Failed.
    assert(!policy.Latched());

    // Production integration sentinels: the portable policy must sit on the
    // command path, with source-qualified positions and correct reset lifetime.
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream file(root / "src/Bot/DeathRecoveryController.h");
    assert(file);
    const std::string source(std::istreambuf_iterator<char>(file), {});
    const auto gate = source.find("if (repeatDeath_.ShouldBlock(freshProbe && lastProbe_.isGhost))");
    assert(gate != std::string::npos);
    assert(gate < source.find("\"if RetrieveCorpse then RetrieveCorpse(); end;\""));
    assert(source.find("repeatDeath_.Latched() && lastProbe_.valid && lastProbe_.isGhost", gate) != std::string::npos);
    assert(source.find("repeatDeath_.Latched() || !TryStrategicContinuation(player, tick, reason)") != std::string::npos);
    assert(source.find("if (!confirmedGhost)\n                ObserveRepeatDeathCorpse(\"observed_dead_body\");") != std::string::npos);
    assert(source.find("deathPosition_ = {serverPoint.x, serverPoint.y, serverPoint.z};\n                ObserveRepeatDeathCorpse(\"current_server_corpse\");") != std::string::npos);
    assert(source.find("repeatDeath_.RecordAutomaticAlive(episodePlayerGuid_, mapId_,\n                {world.player.x, world.player.y, world.player.z}, MonotonicMs());") != std::string::npos);
    assert(source.find("repeatDeath_.Reset();\n                            FinalizeAliveEpisode(tick,") != std::string::npos);
    assert(source.find("const auto recentReclaim = repeatDeath_;\n            Reset();\n            repeatDeath_ = recentReclaim;") != std::string::npos);
    const auto gap = source.find("void InvalidateTerminalAliveEvidenceOnWorldGap()");
    assert(source.find("repeatDeath_.Reset();", gap) < source.find("if (IsActive()", gap));
    // Automatic confirmation is still command + Ghost + two fresh alive probes.
    assert(!Bot::DeathRecoveryPolicy::AliveAfterCorpseRun(true, false, 50, 100, true, true, 1));
    assert(Bot::DeathRecoveryPolicy::AliveAfterCorpseRun(true, false, 50, 100, true, true, 2));
}
