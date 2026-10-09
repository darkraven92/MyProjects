#include "../src/Bot/EscapeDecisionPolicy.h"

#include <cassert>

int main()
{
    using Bot::EscapeDecision;
    using Bot::EscapeDecisionInput;
    using Bot::EscapeDecisionPolicy;
    using Bot::EscapeDecisionReason;

    const auto sample = [](float playerHealth, float targetHealth,
                           unsigned aggressors, bool hardStall = false)
    {
        EscapeDecisionInput input{};
        input.combatOwned = true;
        input.targetGuid = 42;
        input.playerHealthKnown = true;
        input.playerHealthPct = playerHealth;
        input.targetHealthKnown = true;
        input.targetHealthPct = targetHealth;
        input.aggressorsKnown = true;
        input.observedDirectAggressors = aggressors;
        input.combatHardStall = hardStall;
        return input;
    };

    EscapeDecisionPolicy policy{};
    auto result = policy.Observe(sample(90.0f, 75.0f, 1));
    assert(result.decision == EscapeDecision::Continue);
    assert(!result.changed);
    result = policy.Observe(sample(55.0f, 75.0f, 2));
    assert(result.decision == EscapeDecision::Continue);

    // Low HP or a healthy remaining target alone is insufficient evidence.
    result = policy.Observe(sample(19.0f, 85.0f, 1));
    assert(result.decision == EscapeDecision::Warning);
    assert(result.reason == EscapeDecisionReason::LowHealthOnly);
    assert(result.changed);
    for (int i = 0; i < 4; ++i)
        assert(policy.Observe(sample(19.0f, 85.0f, 1)).decision ==
               EscapeDecision::Warning);
    assert(policy.Observe(sample(21.0f, 85.0f, 1)).decision ==
           EscapeDecision::Warning); // One HP sample cannot flicker clear.
    assert(policy.Observe(sample(19.0f, 85.0f, 1)).decision ==
           EscapeDecision::Warning);

    // Two currently observed attackers must persist for three fresh samples.
    result = policy.Observe(sample(19.0f, 80.0f, 2));
    assert(result.decision == EscapeDecision::Warning);
    assert(result.corroboratedSamples == 1);
    result = policy.Observe(sample(19.0f, 80.0f, 2));
    assert(result.decision == EscapeDecision::Warning);
    result = policy.Observe(sample(19.0f, 80.0f, 2));
    assert(result.decision == EscapeDecision::EscapeCandidate);
    assert(result.reason == EscapeDecisionReason::LowHealthMultiAggro);
    assert(result.changed);

    // One transient missing aggressor does not oscillate the decision.
    result = policy.Observe(sample(19.0f, 80.0f, 1));
    assert(result.decision == EscapeDecision::EscapeCandidate);
    assert(!result.changed);
    result = policy.Observe(sample(19.0f, 80.0f, 2));
    assert(result.decision == EscapeDecision::EscapeCandidate);
    assert(!result.changed);

    // Two known healthy observations release the candidate latch.
    result = policy.Observe(sample(75.0f, 80.0f, 1));
    assert(result.decision == EscapeDecision::EscapeCandidate);
    result = policy.Observe(sample(75.0f, 80.0f, 1));
    assert(result.decision == EscapeDecision::Continue);
    assert(result.changed);

    policy.Reset();
    assert(policy.Observe(sample(90.0f, 90.0f, 1, true)).decision ==
           EscapeDecision::Continue); // Hard stall alone is not flee evidence.
    result = policy.Observe(sample(19.0f, 90.0f, 1, true));
    assert(result.decision == EscapeDecision::EscapeCandidate);
    assert(result.reason == EscapeDecisionReason::LowHealthHardStall);

    // Missing required evidence fails closed, including a missing target.
    EscapeDecisionInput unknown = sample(19.0f, 90.0f, 2);
    unknown.aggressorsKnown = false;
    result = policy.Observe(unknown);
    assert(result.decision == EscapeDecision::Continue);
    assert(result.reason == EscapeDecisionReason::UnknownInput);
    unknown = sample(19.0f, 90.0f, 2);
    unknown.playerHealthKnown = false;
    assert(policy.Observe(unknown).decision == EscapeDecision::Continue);
    unknown = sample(19.0f, 90.0f, 2);
    unknown.targetHealthKnown = false;
    assert(policy.Observe(unknown).decision == EscapeDecision::Continue);
    unknown = sample(19.0f, 90.0f, 2);
    unknown.targetGuid = 0;
    assert(policy.Observe(unknown).decision == EscapeDecision::Continue);

    // A new target never inherits the old fight's corroboration streak.
    policy.Reset();
    policy.Observe(sample(19.0f, 80.0f, 2));
    policy.Observe(sample(19.0f, 80.0f, 2));
    EscapeDecisionInput nextTarget = sample(19.0f, 80.0f, 2);
    nextTarget.targetGuid = 43;
    result = policy.Observe(nextTarget);
    assert(result.decision == EscapeDecision::Warning);
    assert(result.corroboratedSamples == 1);

    const auto timedSample = [](float playerHealth, float targetHealth,
                                std::uint64_t observedAtMs,
                                std::uint64_t targetGuid = 42)
    {
        EscapeDecisionInput input{};
        input.combatOwned = true;
        input.playerGuid = 7;
        input.lifeEpisode = 1;
        input.targetGuid = targetGuid;
        input.observedAtMs = observedAtMs;
        input.playerHealthKnown = true;
        input.playerHealthPct = playerHealth;
        input.targetHealthKnown = true;
        input.targetHealthPct = targetHealth;
        input.aggressorsKnown = true;
        input.observedDirectAggressors = 1;
        return input;
    };

    // Healthy deterioration alone never selects an escape route.
    policy.Reset();
    assert(policy.Observe(timedSample(90.0f, 83.0f, 1000)).decision ==
           EscapeDecision::Continue);
    assert(policy.Observe(timedSample(75.0f, 83.0f, 1250)).decision ==
           EscapeDecision::Continue);
    assert(policy.Observe(timedSample(60.0f, 83.0f, 1500)).decision ==
           EscapeDecision::Continue);
    assert(policy.HealthTrend().deteriorating);

    // One low-HP sample and one damage event remain warnings.
    policy.Reset();
    result = policy.Observe(timedSample(19.0f, 83.0f, 2000));
    assert(result.decision == EscapeDecision::Warning);
    result = policy.Observe(timedSample(10.0f, 83.0f, 2250));
    assert(result.decision == EscapeDecision::Warning);
    assert(!policy.HealthTrend().deteriorating);

    // Verified 1v1 pattern: several distinct HP drops during the same
    // combat/life, then emergency HP while the target retains 83% HP.
    policy.Reset();
    const float falling[] = {69.3f, 58.1f, 44.7f, 32.9f, 22.3f};
    for (std::size_t index = 0; index < 5; ++index)
        assert(policy.Observe(timedSample(
            falling[index], 83.0f, 3000 + index * 250)).decision ==
            EscapeDecision::Continue);
    result = policy.Observe(timedSample(9.5f, 83.0f, 4250));
    assert(result.decision == EscapeDecision::EscapeCandidate);
    assert(result.reason == EscapeDecisionReason::LowHealthDeteriorating);
    assert(policy.HealthTrend().recentHealthLossPct > 50.0f);
    assert(policy.HealthTrend().declineObservations >= 2);
    // The trend persists across unchanged samples; no candidate/warning
    // oscillation occurs simply because one polling tick has no damage.
    assert(policy.Observe(timedSample(9.5f, 83.0f, 4500)).decision ==
           EscapeDecision::EscapeCandidate);

    // A nearly dead target does not corroborate abandoning the finisher.
    policy.Reset();
    policy.Observe(timedSample(69.3f, 4.0f, 5000));
    policy.Observe(timedSample(44.7f, 4.0f, 5250));
    result = policy.Observe(timedSample(9.5f, 4.0f, 5500));
    assert(policy.HealthTrend().deteriorating);
    assert(result.decision == EscapeDecision::Warning);
    assert(result.reason == EscapeDecisionReason::LowHealthOnly);

    // Healing clears deterioration; 15C.0's existing two-sample decision
    // release still applies, independent of the trend window.
    policy.Reset();
    policy.Observe(timedSample(69.3f, 83.0f, 6000));
    policy.Observe(timedSample(44.7f, 83.0f, 6250));
    assert(policy.Observe(timedSample(9.5f, 83.0f, 6500)).decision ==
           EscapeDecision::EscapeCandidate);
    assert(policy.Observe(timedSample(60.0f, 83.0f, 6750)).decision ==
           EscapeDecision::EscapeCandidate);
    assert(!policy.HealthTrend().deteriorating);
    assert(policy.Observe(timedSample(60.0f, 83.0f, 7000)).decision ==
           EscapeDecision::Continue);
}
