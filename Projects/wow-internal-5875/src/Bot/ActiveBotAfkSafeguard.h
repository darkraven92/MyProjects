#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    struct ActiveBotAfkSample
    {
        bool active = false;
        bool safeIdle = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct ActiveBotAfkEvent
    {
        bool due = false;
        std::uint64_t safeIdleTicks = 0;
        int request = 0;
    };

    /*
     * Phase 14K.1.7 -- Active Bot AFK Safeguard
     *
     * This timer is deliberately independent from RuntimeRobustnessSupervisor.
     * Runtime recovery is allowed to reset its own liveness epochs; those resets
     * must not hide a long user-visible stationary acquisition period from the
     * AFK safeguard.
     *
     * A pulse is requested only while the grind bot is alive and in a caller-
     * verified safe acquisition idle. Combat, recovery, vendor, First Aid and
     * death-recovery owners are excluded by the caller. Real physical movement
     * resets the safe-idle window.
     */
    class ActiveBotAfkSafeguard
    {
    private:
        static constexpr float MeaningfulMovement = 2.5f;
        static constexpr std::uint64_t TriggerTicks = 240;       // ~60 s
        static constexpr std::uint64_t RetryCooldownTicks = 80;  // ~20 s

        bool initialized_ = false;
        bool safeIdleActive_ = false;
        float anchorX_ = 0.0f;
        float anchorY_ = 0.0f;
        float anchorZ_ = 0.0f;
        std::uint64_t safeIdleSinceTick_ = 0;
        std::uint64_t lastRequestTick_ = 0;
        int requests_ = 0;

        static float Distance3D(
            float ax,
            float ay,
            float az,
            float bx,
            float by,
            float bz)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        void BaselinePosition(const ActiveBotAfkSample& sample)
        {
            anchorX_ = sample.x;
            anchorY_ = sample.y;
            anchorZ_ = sample.z;
        }

        void ResetSafeIdle(const ActiveBotAfkSample& sample)
        {
            safeIdleActive_ = false;
            safeIdleSinceTick_ = 0;
            BaselinePosition(sample);
        }

    public:
        ActiveBotAfkEvent Update(
            const ActiveBotAfkSample& sample,
            std::uint64_t tick)
        {
            if (!sample.active)
            {
                Reset();
                return {};
            }

            if (!initialized_)
            {
                initialized_ = true;
                BaselinePosition(sample);
            }

            if (!sample.safeIdle)
            {
                ResetSafeIdle(sample);
                return {};
            }

            if (!safeIdleActive_ || tick < safeIdleSinceTick_)
            {
                safeIdleActive_ = true;
                safeIdleSinceTick_ = tick;
                BaselinePosition(sample);
                return {};
            }

            if (Distance3D(
                    anchorX_, anchorY_, anchorZ_,
                    sample.x, sample.y, sample.z) >= MeaningfulMovement)
            {
                safeIdleSinceTick_ = tick;
                BaselinePosition(sample);
                return {};
            }

            const std::uint64_t age = tick - safeIdleSinceTick_;
            if (age < TriggerTicks)
                return {};

            if (lastRequestTick_ != 0 &&
                tick >= lastRequestTick_ &&
                tick - lastRequestTick_ < RetryCooldownTicks)
            {
                return {};
            }

            lastRequestTick_ = tick;
            ++requests_;
            return {true, age, requests_};
        }

        void Reset()
        {
            initialized_ = false;
            safeIdleActive_ = false;
            anchorX_ = 0.0f;
            anchorY_ = 0.0f;
            anchorZ_ = 0.0f;
            safeIdleSinceTick_ = 0;
            lastRequestTick_ = 0;
            requests_ = 0;
        }

        bool SafeIdleActive() const { return safeIdleActive_; }

        std::uint64_t SafeIdleAgeTicks(std::uint64_t tick) const
        {
            if (!safeIdleActive_ || tick < safeIdleSinceTick_)
                return 0;
            return tick - safeIdleSinceTick_;
        }

        int Requests() const { return requests_; }

        static constexpr std::uint64_t TriggerAfterTicks()
        {
            return TriggerTicks;
        }

        static constexpr std::uint64_t RetryAfterTicks()
        {
            return RetryCooldownTicks;
        }
    };
}
