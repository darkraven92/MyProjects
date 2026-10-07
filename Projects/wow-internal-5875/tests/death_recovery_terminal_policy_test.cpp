#include "../src/Bot/DeathRecoveryTerminalPolicy.h"
#include "../src/Bot/DeathRecoveryPolicy.h"

#include <cassert>
#include <cmath>

int main()
{
    using Policy = Bot::DeathRecoveryTerminalPolicy;
    using Point = Policy::Point;
    const Point corpse{0.0f, 0.0f, 0.0f};
    const Point spawn{500.0f, 0.0f, 0.0f};
    const Point progressed{400.0f, 0.0f, 0.0f};
    const Point stationary{499.0f, 0.0f, 0.0f};

    // A deadline during genuine corpse-distance progress may authorize one
    // different approach; mere motion in place cannot.
    assert(Policy::MayEscalate(true, true, false, false, true,
        spawn, progressed, corpse, false));
    assert(!Policy::MayEscalate(true, true, false, false, true,
        spawn, stationary, corpse, false));
    assert(!Policy::MayEscalate(true, true, false, false, true,
        spawn, {510.0f, 0.0f, 0.0f}, corpse, false));

    // A proven directed transition is independent new route evidence; an
    // arbitrary route failure, release failure, or unknown ghost is not.
    assert(Policy::MayEscalate(true, true, false, false, true,
        spawn, stationary, corpse, true));
    assert(!Policy::MayEscalate(false, true, false, false, true,
        spawn, progressed, corpse, true));
    assert(!Policy::MayEscalate(true, false, false, false, true,
        spawn, progressed, corpse, true));
    assert(!Policy::MayEscalate(true, true, false, false, false,
        spawn, progressed, corpse, false));

    // No identical replay, and no second strategic attempt even if the
    // world position or route evidence changes again.
    assert(!Policy::MayEscalate(true, true, false, true, true,
        spawn, progressed, corpse, true));
    assert(!Policy::MayEscalate(true, true, true, false, true,
        spawn, {200.0f, 0.0f, 0.0f}, corpse, true));

    const auto signature = Policy::MakeSignature(
        1, corpse, progressed, 1, 3, 0xA, 0xB, 0x1234);
    assert(signature == Policy::MakeSignature(
        1, corpse, {405.0f, 0.0f, 0.0f}, 1, 3,
        0xA, 0xB, 0x1234));
    assert(!(signature == Policy::MakeSignature(
        1, corpse, {420.0f, 0.0f, 0.0f}, 1, 3,
        0xA, 0xB, 0x1234)));
    assert(!(signature == Policy::MakeSignature(
        1, corpse, progressed, 1, 3, 0xB, 0xA, 0x1234)));
    assert(Policy::Fingerprint(signature) ==
        Policy::Fingerprint(signature));

    const auto approach = Policy::AlternateApproach(corpse, progressed);
    assert(std::fabs(Policy::Distance(approach, corpse) -
        Policy::AlternateApproachRadius) < 0.0001f);
    assert(Policy::Distance(approach, corpse) < 8.0f);
    assert(Policy::Distance(approach, progressed) <
        Policy::Distance(corpse, progressed));
    const auto coincident = Policy::AlternateApproach(corpse, corpse);
    assert(Policy::Distance(coincident, corpse) ==
        Policy::AlternateApproachRadius);

    // The original numeric limits and reclaim confirmation stay intact.
    static_assert(Bot::DeathRecoveryLivenessPolicy::MaximumEpisodeAgeMs ==
        300000);
    static_assert(Bot::DeathRecoveryLivenessPolicy::MaximumRouteAttempts == 18);
    static_assert(Bot::DeathRecoveryPolicy::ReclaimDistance == 32.0f);
    static_assert(Bot::DeathRecoveryPolicy::AliveConfirmationProbes == 2);
    assert(!Bot::DeathRecoveryPolicy::CanRetrieveCorpse(
        true, true, 0.0f, 33.0f));
}
