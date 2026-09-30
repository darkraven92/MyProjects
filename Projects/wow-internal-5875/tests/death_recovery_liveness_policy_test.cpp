#include "../src/Bot/DeathRecoveryPolicy.h"

#include <cassert>

int main()
{
    using Policy = Bot::DeathRecoveryLivenessPolicy;
    using Reason = Policy::FailureReason;

    // Controller ticks are irrelevant to these monotonic elapsed-time decisions.
    const std::uint64_t barelyAdvancedTick = 2;
    (void)barelyAdvancedTick;
    assert(Policy::Evaluate(180001, 180001, 1, 0, false) ==
        Reason::NoPhysicalProgress);
    assert(Policy::Evaluate(300000, 1000, 1, 0, false) ==
        Reason::EpisodeDeadline);
    assert(Policy::Evaluate(1000, 1000, Policy::MaximumRouteAttempts, 0, true) ==
        Reason::RouteAttemptBudget);
    assert(Policy::Evaluate(1000, 1000, 1,
        Policy::MaximumStationaryRouteFailures, false) ==
        Reason::StationaryFailureBudget);

    assert(Policy::AllowFullMapFallback(0));
    assert(!Policy::AllowFullMapFallback(
        Policy::MaximumFullMapFallbackAttempts));

    // A monitor update consumes one variant; the next update gets the next.
    int nextVariant = 0;
    assert(Policy::MayAttemptRouteThisUpdate(false, false));
    assert(Policy::ConsumeNextVariant(nextVariant) == 0);
    assert(nextVariant == 1);
    assert(!Policy::MayAttemptRouteThisUpdate(false, true));
    assert(Policy::MayAttemptRouteThisUpdate(false, false));
    assert(Policy::ConsumeNextVariant(nextVariant) == 1);
    assert(nextVariant == 2);

    // Movement resets the no-progress clock, never the absolute episode clock.
    assert(Policy::Evaluate(170000, 170000, 2, 2, false) == Reason::None);
    assert(Policy::Evaluate(170000, 0, 2, 0, false) == Reason::None);
    assert(Policy::Evaluate(300000, 0, 2, 0, false) == Reason::EpisodeDeadline);

    // Successful ordinary routing remains possible within the budget.
    assert(Policy::Evaluate(30000, 20000, 1, 0, false) == Reason::None);
    assert(!Policy::MayAttemptRouteThisUpdate(true, false));

    // Terminal ownership is released only after fresh positive alive probes.
    assert(!Policy::ManualAliveConfirmed(true, 1, 308, true, true,
        false, false, 2));
    assert(!Policy::ManualAliveConfirmed(true, 188, 308, true, true,
        false, false, 1));
    assert(!Policy::ManualAliveConfirmed(true, 188, 308, true, true,
        true, false, 2));
    assert(!Policy::ManualAliveConfirmed(true, 188, 308, false, true,
        false, false, 2));
    assert(Policy::ManualAliveConfirmed(true, 188, 308, true, true,
        false, false, 2));

    // The existing corpse-reclaim gate is not broadened.
    assert(Bot::DeathRecoveryPolicy::CanRetrieveCorpse(true, true, 0.0f, 7.5f));
    assert(!Bot::DeathRecoveryPolicy::CanRetrieveCorpse(true, true, 0.0f, 33.0f));
    assert(!Bot::DeathRecoveryPolicy::CanRetrieveCorpse(true, false, 0.0f, 7.5f));
}
