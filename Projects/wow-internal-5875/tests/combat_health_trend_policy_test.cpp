#include "../src/Bot/CombatHealthTrendPolicy.h"

#include <cassert>

using Bot::CombatHealthTrendInput;
using Bot::CombatHealthTrendPolicy;

static CombatHealthTrendInput Sample(float healthPct,
                                     std::uint64_t observedAtMs,
                                     std::uint64_t targetGuid = 42,
                                     std::uint64_t lifeEpisode = 0)
{
    CombatHealthTrendInput input{};
    input.combatOwned = true;
    input.playerGuid = 7;
    input.lifeEpisode = lifeEpisode;
    input.targetGuid = targetGuid;
    input.healthKnown = true;
    input.healthPct = healthPct;
    input.observedAtMs = observedAtMs;
    return input;
}

int main()
{
    CombatHealthTrendPolicy trend{};
    auto result = trend.Observe(Sample(69.3f, 1000));
    assert(result.trendSamples == 1);
    assert(!result.deteriorating);
    result = trend.Observe(Sample(32.9f, 1250));
    assert(result.declineObservations == 1);
    assert(!result.deteriorating); // One damage event is insufficient.
    result = trend.Observe(Sample(32.9f, 1500));
    assert(!result.deteriorating);
    result = trend.Observe(Sample(9.5f, 1750));
    assert(result.declineObservations == 2);
    assert(result.recentHealthLossPct > 50.0f);
    assert(result.deteriorating);

    // Even a small observed heal invalidates earlier loss; this is
    // conservative and prevents damage/heal oscillation from accumulating.
    result = trend.Observe(Sample(10.0f, 2000));
    assert(result.trendSamples == 1);
    assert(!result.deteriorating);
    result = trend.Observe(Sample(9.8f, 2250));
    assert(!result.deteriorating);

    trend.Reset();
    trend.Observe(Sample(80.0f, 3000));
    trend.Observe(Sample(55.0f, 3250));
    result = trend.Observe(Sample(10.0f, 3500, 43));
    assert(result.trendSamples == 1); // New locked target.
    assert(!result.deteriorating);

    trend.Reset();
    trend.Observe(Sample(80.0f, 4000));
    trend.Observe(Sample(55.0f, 4250));
    result = trend.Observe(Sample(10.0f, 4500, 42, 1));
    assert(result.trendSamples == 1); // New player life.
    assert(!result.deteriorating);

    trend.Reset();
    trend.Observe(Sample(80.0f, 5000));
    trend.Observe(Sample(55.0f, 5250));
    auto dead = Sample(0.0f, 5500);
    dead.healthKnown = false;
    result = trend.Observe(dead);
    assert(result.trendSamples == 0);
    assert(!result.deteriorating);
    result = trend.Observe(Sample(10.0f, 5750));
    assert(result.trendSamples == 1);

    trend.Reset();
    trend.Observe(Sample(80.0f, 6000));
    trend.Observe(Sample(55.0f, 6250));
    auto outsideCombat = Sample(10.0f, 6500);
    outsideCombat.combatOwned = false;
    assert(trend.Observe(outsideCombat).trendSamples == 0);
    assert(trend.Observe(Sample(9.0f, 6750)).trendSamples == 1);

    trend.Reset();
    trend.Observe(Sample(80.0f, 7000));
    auto invalid = Sample(50.0f, 7250);
    invalid.healthPct = NAN;
    assert(trend.Observe(invalid).trendSamples == 0);
    assert(trend.Observe(Sample(10.0f, 7500)).trendSamples == 1);

    trend.Reset();
    trend.Observe(Sample(80.0f, 8000));
    trend.Observe(Sample(55.0f, 8250));
    assert(trend.Observe(Sample(10.0f, 8100)).trendSamples == 1);

    trend.Reset();
    trend.Observe(Sample(80.0f, 10000));
    trend.Observe(Sample(55.0f, 10250));
    result = trend.Observe(Sample(54.0f, 20501));
    assert(result.trendSamples == 1); // Older observations expired.
    assert(!result.deteriorating);

    trend.Reset();
    for (std::size_t i = 0; i < 100; ++i)
        result = trend.Observe(Sample(80.0f, 30000 + i));
    assert(result.trendSamples == CombatHealthTrendPolicy::MaximumSamples);
}
