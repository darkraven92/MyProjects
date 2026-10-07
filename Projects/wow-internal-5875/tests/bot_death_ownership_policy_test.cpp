#include "../src/Bot/BotDeathOwnershipPolicy.h"
#include "../src/Bot/DeathRecoveryAnchorStore.h"

#include <cassert>

int main()
{
    using Bot::BotDeathOwnershipPolicy;
    using Bot::DeathRecoveryPolicy;

    // Mode is deliberately absent: the same trigger applies to Questing and
    // Grind, including the first snapshot after a process restart.
    assert(!BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 100, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 0, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 1, 100, true));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        true, false, true, 1, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, true, true, 1, 100, false));
    assert(!BotDeathOwnershipPolicy::ShouldOwn(
        false, false, false, 0, 100, false));

    assert(BotDeathOwnershipPolicy::HoldQuestPlanner(
        true, true, 0, 100, false));
    assert(BotDeathOwnershipPolicy::HoldQuestPlanner(
        false, true, 1, 100, false)); // unresolved ghost probe
    assert(!BotDeathOwnershipPolicy::HoldQuestPlanner(
        false, true, 1, 100, true)); // verified alive at HP1
    assert(!BotDeathOwnershipPolicy::HoldQuestPlanner(
        false, true, 2, 100, false));
    // Automatic resurrection: confirmed alive releases ownership, and a
    // later body death is eligible for a distinct episode.
    assert(!BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 100, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 0, 100, false));
    assert(BotDeathOwnershipPolicy::HoldQuestPlanner(
        true, true, 0, 100, false));
    // Terminal failure owns the tick until the two-probe manual-alive path
    // finalizes it; the next death uses the same idle entry rule.
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, true, true, 100, 100, false));
    assert(!BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 100, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 0, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 1, 100, true));
    // Missing startup-ghost confirmation still fails closed, but a later
    // genuinely new body death is eligible for a fresh recovery episode.
    assert(!BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 1, 100, false));
    assert(BotDeathOwnershipPolicy::ShouldOwn(
        false, false, true, 0, 100, false));
    // Existing bounded death FSM remains the authority for spirit release,
    // corpse anchor validity and actual post-reclaim alive confirmation.
    assert(DeathRecoveryPolicy::EntryFor(false, false, false) ==
        DeathRecoveryPolicy::EntryState::ReleasingSpirit);
    assert(DeathRecoveryPolicy::EntryFor(false, true, true) ==
        DeathRecoveryPolicy::EntryState::WaitingForGhost);
    assert(DeathRecoveryPolicy::EntryFor(false, true, false) ==
        DeathRecoveryPolicy::EntryState::AwaitingCorpseAnchor);
    assert(DeathRecoveryPolicy::CanAttemptSpiritRelease(false, 8, 8));
    assert(!DeathRecoveryPolicy::CanAttemptSpiritRelease(true, 8, 8));
    assert(!DeathRecoveryPolicy::AliveAfterCorpseRun(
        true, false, 100, 100, true, false, 2));
    assert(DeathRecoveryPolicy::AliveAfterCorpseRun(
        true, false, 100, 100, true, true, 2));
    static_assert(DeathRecoveryPolicy::MaximumRouteVariants == 9);
    static_assert(Bot::DeathRecoveryLivenessPolicy::MaximumRouteAttempts == 18);

    constexpr Bot::DeathAnchorPosition anchor{-500.0f, -2000.0f, 95.0f};
    assert(Bot::DeathRecoveryAnchorPolicy::Eligible(
        42, 1, 42, 1, 12, true, anchor));
    assert(!Bot::DeathRecoveryAnchorPolicy::Eligible(
        42, 1, 43, 1, 12, true, anchor));
    assert(!Bot::DeathRecoveryAnchorPolicy::Eligible(
        42, 1, 42, 0, 12, true, anchor));
    assert(!Bot::DeathRecoveryAnchorPolicy::Eligible(
        42, 1, 42, 1, -1, true, anchor));
    assert(!Bot::DeathRecoveryAnchorPolicy::Eligible(
        42, 1, 42, 1,
        Bot::DeathRecoveryAnchorPolicy::MaximumAgeSeconds + 1,
        true, anchor));
    assert(!Bot::DeathRecoveryAnchorPolicy::Eligible(
        42, 1, 42, 1, 12, false, anchor));
}
