#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    class ExperienceTracker
    {
    public:
        struct Snapshot
        {
            bool valid = false;
            bool rateReady = false;
            std::uint32_t level = 0;
            std::uint32_t currentXp = 0;
            std::uint32_t nextLevelXp = 0;
            std::uint32_t remainingXp = 0;
            std::uint64_t sessionXpGained = 0;
            double xpPerMinute = 0.0;
            double etaMinutes = 0.0;
        };

    private:
        using Clock = std::chrono::steady_clock;

        struct RateSample
        {
            Clock::time_point time{};
            std::uint64_t cumulativeXp = 0;
        };

        static constexpr auto RateWindow = std::chrono::minutes(5);
        static constexpr auto MinimumRateSpan = std::chrono::seconds(10);

        bool initialized_ = false;
        std::uint32_t previousLevel_ = 0;
        std::uint32_t previousXp_ = 0;
        std::uint32_t previousNextLevelXp_ = 0;
        std::uint64_t cumulativeXp_ = 0;
        std::deque<RateSample> samples_{};
        Snapshot snapshot_{};

        template <typename PlayerLike>
        void ResetBaseline(
            const PlayerLike& player,
            Clock::time_point now)
        {
            initialized_ = true;
            previousLevel_ = player.level;
            previousXp_ = player.currentXp;
            previousNextLevelXp_ = player.nextLevelXp;
            samples_.clear();
            samples_.push_back(RateSample{now, cumulativeXp_});
        }

    public:
        template <typename PlayerLike>
        void Update(const PlayerLike& player)
        {
            snapshot_ = Snapshot{};

            if (!player.valid || !player.xpValid || player.level == 0)
                return;

            const auto now = Clock::now();

            if (!initialized_)
            {
                ResetBaseline(player, now);
            }
            else
            {
                std::uint64_t gained = 0;

                if (player.level == previousLevel_)
                {
                    if (player.currentXp >= previousXp_)
                        gained = player.currentXp - previousXp_;
                }
                else if (
                    player.level == previousLevel_ + 1 &&
                    previousNextLevelXp_ >= previousXp_)
                {
                    gained =
                        static_cast<std::uint64_t>(
                            previousNextLevelXp_ - previousXp_) +
                        player.currentXp;
                }
                else
                {
                    // Unexpected jump/relog/GM level change. Keep the accumulated
                    // session total but restart the rate baseline instead of
                    // fabricating XP across an unknown number of levels.
                    previousLevel_ = player.level;
                    previousXp_ = player.currentXp;
                    previousNextLevelXp_ = player.nextLevelXp;
                    samples_.clear();
                    samples_.push_back(RateSample{now, cumulativeXp_});
                }

                cumulativeXp_ += gained;
                previousLevel_ = player.level;
                previousXp_ = player.currentXp;
                previousNextLevelXp_ = player.nextLevelXp;
            }

            samples_.push_back(RateSample{now, cumulativeXp_});

            while (
                samples_.size() > 2 &&
                (now - samples_[1].time) > RateWindow)
            {
                samples_.pop_front();
            }

            snapshot_.valid = true;
            snapshot_.level = player.level;
            snapshot_.currentXp = player.currentXp;
            snapshot_.nextLevelXp = player.nextLevelXp;
            snapshot_.sessionXpGained = cumulativeXp_;

            if (
                player.level < 60 &&
                player.nextLevelXp > player.currentXp)
            {
                snapshot_.remainingXp =
                    player.nextLevelXp - player.currentXp;
            }

            if (samples_.size() >= 2)
            {
                const auto span = now - samples_.front().time;
                const double minutes =
                    std::chrono::duration<double, std::ratio<60>>(span).count();
                const std::uint64_t windowXp =
                    cumulativeXp_ - samples_.front().cumulativeXp;

                if (
                    span >= MinimumRateSpan &&
                    minutes > 0.0 &&
                    windowXp > 0)
                {
                    snapshot_.xpPerMinute =
                        static_cast<double>(windowXp) / minutes;
                    snapshot_.rateReady = true;

                    if (snapshot_.remainingXp > 0)
                    {
                        snapshot_.etaMinutes =
                            static_cast<double>(snapshot_.remainingXp) /
                            snapshot_.xpPerMinute;
                    }
                }
            }
        }

        const Snapshot& Current() const
        {
            return snapshot_;
        }

        static std::string FormatRate(double xpPerMinute)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1) << xpPerMinute;
            return stream.str();
        }

        static std::string FormatEta(double etaMinutes)
        {
            if (!std::isfinite(etaMinutes) || etaMinutes <= 0.0)
                return "calculating";

            const auto totalSeconds = static_cast<std::uint64_t>(
                std::llround(etaMinutes * 60.0));
            const auto hours = totalSeconds / 3600;
            const auto minutes = (totalSeconds % 3600) / 60;
            const auto seconds = totalSeconds % 60;

            std::ostringstream stream;
            if (hours > 0)
                stream << hours << "h ";
            if (hours > 0 || minutes > 0)
                stream << minutes << "m ";
            stream << seconds << "s";
            return stream.str();
        }
    };
}
