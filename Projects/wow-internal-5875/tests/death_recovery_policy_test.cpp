#include "../src/Bot/DeathRecoveryPolicy.h"

#include <cassert>

int main()
{
    using Bot::DeathRecoveryPolicy;
    using EntryState = DeathRecoveryPolicy::EntryState;

    // The original zero-health entry is unchanged; one HP alone is insufficient.
    assert(!DeathRecoveryPolicy::CanStartFromDeath(true, 1, 308));
    assert(DeathRecoveryPolicy::CanStartFromDeath(true, 0, 308));
    assert(!DeathRecoveryPolicy::CanStartFromDeath(false, 0, 308));

    // Normal healthy snapshots refresh the anchor without a Lua ghost probe.
    assert(DeathRecoveryPolicy::ShouldRememberClearlyAlivePosition(true, 308, 308));
    assert(!DeathRecoveryPolicy::ShouldProbeIdleGhost(true, 308, 308));
    assert(!DeathRecoveryPolicy::ShouldRememberClearlyAlivePosition(true, 1, 308));
    assert(DeathRecoveryPolicy::ShouldProbeIdleGhost(true, 1, 308));

    // Living at one HP retains ordinary ownership; only a positive probe starts.
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 308, true, false));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 308, false, true));
    assert(DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 308, true, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 308, 308, true, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(false, 1, 308, true, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 0, true, true));

    // A bootstrapped ghost either owns the corpse run or fails safely.
    assert(DeathRecoveryPolicy::EntryFor(true, true) == EntryState::WaitingForGhost);
    assert(DeathRecoveryPolicy::EntryFor(true, false) == EntryState::FailedMissingAnchor);
    assert(DeathRecoveryPolicy::EntryFor(false, false) == EntryState::ReleasingSpirit);
    assert(!DeathRecoveryPolicy::CanAttemptSpiritRelease(true, 100, 100));
    assert(DeathRecoveryPolicy::CanAttemptSpiritRelease(false, 100, 100));
}
