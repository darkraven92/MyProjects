#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace Bot
{
    struct CombatHealthTrendInput
    {
        bool combatOwned = false;
        std::uint64_t playerGuid = 0;
        std::uint64_t lifeEpisode = 0;
        std::uint64_t targetGuid = 0;
        bool healthKnown = false;
        float healthPct = 0.0f;
        std::uint64_t observedAtMs = 0;
    };

    struct CombatHealthTrendAssessment
    {
        float recentHealthLossPct = 0.0f;
        unsigned trendSamples = 0;
        unsigned declineObservations = 0;
        bool deteriorating = false;
    };

    // Observation of sampled health, not incoming DPS or time-to-death.
    // The 10-second window covers the verified 124/179 -> 59/179 sequence
    // (about 20 monitor ticks) without carrying old damage indefinitely.
    class CombatHealthTrendPolicy
    {
    private:
        struct Sample
        {
            std::uint64_t observedAtMs = 0;
            float healthPct = 0.0f;
        };

        std::deque<Sample> samples_{};
        std::uint64_t playerGuid_ = 0;
        std::uint64_t lifeEpisode_ = 0;
        std::uint64_t targetGuid_ = 0;
        CombatHealthTrendAssessment assessment_{};

    public:
        static constexpr std::uint64_t WindowMs = 10000;
        static constexpr std::size_t MaximumSamples = 48;
        // Material deterioration means losing a full emergency-HP reserve
        // (the existing 15C.0 20% threshold), across at least two distinct
        // observed decreases. One damage event cannot prove a trend.
        static constexpr float MaterialHealthLossPercent = 20.0f;
        static constexpr unsigned RequiredDeclineObservations = 2;

        void Reset()
        {
            samples_.clear();
            playerGuid_ = 0;
            lifeEpisode_ = 0;
            targetGuid_ = 0;
            assessment_ = {};
        }

        CombatHealthTrendAssessment Observe(const CombatHealthTrendInput& input)
        {
            if (!input.combatOwned || input.playerGuid == 0 ||
                input.targetGuid == 0 || !input.healthKnown ||
                !std::isfinite(input.healthPct) ||
                input.healthPct <= 0.0f || input.healthPct > 100.0f ||
                input.observedAtMs == 0)
            {
                Reset();
                return assessment_;
            }

            const bool newIdentity = playerGuid_ != input.playerGuid ||
                lifeEpisode_ != input.lifeEpisode ||
                targetGuid_ != input.targetGuid;
            const bool timeRollback = !samples_.empty() &&
                input.observedAtMs < samples_.back().observedAtMs;
            const bool healed = !samples_.empty() &&
                input.healthPct > samples_.back().healthPct;
            if (newIdentity || timeRollback || healed)
            {
                Reset();
                playerGuid_ = input.playerGuid;
                lifeEpisode_ = input.lifeEpisode;
                targetGuid_ = input.targetGuid;
            }

            while (!samples_.empty() &&
                   (input.observedAtMs - samples_.front().observedAtMs >
                        WindowMs ||
                    samples_.size() >= MaximumSamples))
                samples_.pop_front();
            samples_.push_back({input.observedAtMs, input.healthPct});

            assessment_ = {};
            assessment_.trendSamples =
                static_cast<unsigned>(samples_.size());
            for (std::size_t index = 1; index < samples_.size(); ++index)
                if (samples_[index].healthPct < samples_[index - 1].healthPct)
                    ++assessment_.declineObservations;
            assessment_.recentHealthLossPct =
                samples_.front().healthPct - samples_.back().healthPct;
            assessment_.deteriorating =
                assessment_.trendSamples >= 3 &&
                assessment_.declineObservations >=
                    RequiredDeclineObservations &&
                assessment_.recentHealthLossPct >=
                    MaterialHealthLossPercent;
            return assessment_;
        }

        CombatHealthTrendAssessment Current() const { return assessment_; }
    };
}
