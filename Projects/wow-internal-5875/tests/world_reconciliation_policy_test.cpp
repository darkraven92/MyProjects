#include "../src/Bot/BotDeathOwnershipPolicy.h"

#include <cassert>
#include <cstdint>

int main()
{
    using Bot::BotDeathOwnershipPolicy;
    using Bot::DeathRecoveryPolicy;

    const auto owns = [](std::uint32_t hp, bool bootstrap = false,
                         bool recoveryActive = false, bool recoveryFailed = false)
    {
        return BotDeathOwnershipPolicy::ShouldOwn(
            recoveryActive, recoveryFailed, true, hp, 438, bootstrap);
    };
    const auto holds = [](std::uint32_t hp, bool owner, bool aliveProbe,
                          bool unitsAvailable)
    {
        return BotDeathOwnershipPolicy::HoldNormalMode(
            owner, true, hp, 438, aliveProbe, unitsAvailable);
    };

    // Body death and positive dead/ghost bootstraps belong to the existing
    // recovery FSM; one HP by itself never proves either life or death.
    assert(owns(0));
    assert(!owns(1));
    assert(owns(1, true)); // positive dead or ghost Lua result
    assert(!owns(100));
    assert(holds(0, owns(0), false, true));
    assert(holds(1, owns(1, true), false, false));
    assert(!holds(100, owns(100), false, true));

    // Both Questing and Grind use this same mode-independent gate. An empty
    // world at HP1 holds even after a fresh positive alive probe, but an
    // empty world at clearly alive HP does not imply death.
    assert(holds(1, false, false, true));
    assert(holds(1, false, false, false));
    assert(holds(1, false, true, false));
    assert(!holds(1, false, true, true));
    assert(!holds(100, false, false, false));
    assert(!owns(100));

    // An alive verdict applies to precisely its probe observation. The
    // existing four-tick cadence therefore keeps intervening HP1 snapshots
    // held, and a failed probe cannot open the gate.
    constexpr std::uint64_t aliveProbeTick = 40;
    assert(DeathRecoveryPolicy::FreshIdleAliveEvidence(
        true, aliveProbeTick, aliveProbeTick));
    assert(!DeathRecoveryPolicy::FreshIdleAliveEvidence(
        true, aliveProbeTick, aliveProbeTick + 1));
    assert(!DeathRecoveryPolicy::FreshIdleAliveEvidence(
        false, aliveProbeTick, aliveProbeTick));
    assert(holds(1, false, DeathRecoveryPolicy::FreshIdleAliveEvidence(
        false, aliveProbeTick, aliveProbeTick), true));
    assert(holds(1, false, false, true)); // failed/unresolved retry

    bool cachedAlive = true;
    std::uint64_t nextProbeTick = 44;
    std::uint64_t deathConfirmedTick = 0;
    DeathRecoveryPolicy::ClearIdleProbeEvidenceAfterAlive(
        cachedAlive, nextProbeTick, deathConfirmedTick);
    assert(!cachedAlive && nextProbeTick == 0);
    assert(holds(1, false, cachedAlive, true)); // later new HP1 probes anew

    // Movement hold is issued on entry, not on every ambiguous snapshot.
    assert(BotDeathOwnershipPolicy::BeginReconciliationHold(false, true));
    assert(!BotDeathOwnershipPolicy::BeginReconciliationHold(true, true));
    assert(!BotDeathOwnershipPolicy::BeginReconciliationHold(true, false));

    // Terminal ownership remains exclusive until the old episode positively
    // finalizes alive. A post-death escape flag is intentionally not an input.
    assert(owns(1, false, false, true));
    assert(holds(1, owns(1, false, false, true), false, false));
    assert(!owns(100, false, false, false));
    assert(owns(0)); // next distinct death may start a new episode

    static_assert(DeathRecoveryPolicy::ProbeIntervalTicks == 4);
    static_assert(Bot::DeathRecoveryLivenessPolicy::MaximumRouteAttempts == 18);
}
