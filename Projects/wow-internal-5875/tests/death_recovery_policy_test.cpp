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

    // Living at one HP retains ordinary ownership.
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 308, true, false));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 308, false, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromDead(true, 1, 308, true, true, false, false));
    assert(!DeathRecoveryPolicy::CanBootstrapFromDead(true, 1, 308, true, false, true, false));
    assert(!DeathRecoveryPolicy::CanBootstrapFromDead(true, 1, 308, false, true, true, false));
    assert(DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 308, true, true));
    assert(DeathRecoveryPolicy::CanBootstrapFromDead(true, 1, 308, true, true, true, false));
    assert(!DeathRecoveryPolicy::CanBootstrapFromDead(true, 1, 308, true, true, true, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 308, 308, true, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromDead(true, 308, 308, true, true, true, false));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(false, 1, 308, true, true));
    assert(!DeathRecoveryPolicy::CanBootstrapFromGhost(true, 1, 0, true, true));

    // Dead before release uses the existing release path; ghosts bypass it.
    assert(DeathRecoveryPolicy::EntryFor(true, false, true) == EntryState::ReleasingSpirit);
    assert(DeathRecoveryPolicy::EntryFor(false, true, true) == EntryState::WaitingForGhost);
    assert(DeathRecoveryPolicy::EntryFor(false, false, false) == EntryState::ReleasingSpirit);
    assert(DeathRecoveryPolicy::EntryFor(true, false, false) == EntryState::FailedMissingAnchor);
    assert(DeathRecoveryPolicy::EntryFor(false, true, false) == EntryState::FailedMissingAnchor);
    assert(!DeathRecoveryPolicy::CanAttemptSpiritRelease(true, 100, 100));
    assert(DeathRecoveryPolicy::CanAttemptSpiritRelease(false, 100, 100));
}
