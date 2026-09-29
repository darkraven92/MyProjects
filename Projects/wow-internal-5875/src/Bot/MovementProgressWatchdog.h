#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class MovementStallSeverity
    {
        None,
        Suspected,
        Hard
    };

    struct MovementProgressWatchdogConfig
    {
        float meaningfulNetMovement = 0.65f;
        float meaningfulGoalGain = 0.75f;
        std::uint64_t suspectedStallTicks = 8;
        std::uint64_t hardStallTicks = 16;
    };

    struct MovementProgressObservation
    {
        bool valid = false;
        bool progressed = false;
        MovementStallSeverity severity = MovementStallSeverity::None;
        float netMovement = 0.0f;
        float goalGain = 0.0f;
        std::uint64_t stalledTicks = 0;
    };

    class MovementProgressWatchdog
    {
    private:
        MovementProgressWatchdogConfig config_{};
        bool initialized_ = false;
        float anchorX_ = 0.0f;
        float anchorY_ = 0.0f;
        float anchorZ_ = 0.0f;
        float anchorGoalDistance_ = 0.0f;
        std::uint64_t lastProgressTick_ = 0;

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

        bool IsFiniteSample(
            float x,
            float y,
            float z,
            float goalDistance) const
        {
            return
                std::isfinite(x) &&
                std::isfinite(y) &&
                std::isfinite(z) &&
                std::isfinite(goalDistance) &&
                goalDistance >= 0.0f;
        }

    public:
        explicit MovementProgressWatchdog(
            MovementProgressWatchdogConfig config = {})
            : config_(config)
        {
            if (config_.meaningfulNetMovement <= 0.0f)
                config_.meaningfulNetMovement = 0.65f;
            if (config_.meaningfulGoalGain <= 0.0f)
                config_.meaningfulGoalGain = 0.75f;
            if (config_.suspectedStallTicks == 0)
                config_.suspectedStallTicks = 8;
            if (config_.hardStallTicks < config_.suspectedStallTicks)
                config_.hardStallTicks = config_.suspectedStallTicks;
        }

        void Reset(
            float x,
            float y,
            float z,
            float goalDistance,
            std::uint64_t tick)
        {
            initialized_ = IsFiniteSample(x, y, z, goalDistance);
            anchorX_ = x;
            anchorY_ = y;
            anchorZ_ = z;
            anchorGoalDistance_ = goalDistance;
            lastProgressTick_ = tick;
        }

        void Suspend(
            float x,
            float y,
            float z,
            float goalDistance,
            std::uint64_t tick)
        {
            Reset(x, y, z, goalDistance, tick);
        }

        MovementProgressObservation Update(
            float x,
            float y,
            float z,
            float goalDistance,
            std::uint64_t tick,
            bool movementExpected)
        {
            MovementProgressObservation result{};

            if (!IsFiniteSample(x, y, z, goalDistance))
                return result;

            result.valid = true;

            if (!initialized_ || !movementExpected)
            {
                Reset(x, y, z, goalDistance, tick);
                return result;
            }

            result.netMovement = Distance3D(
                anchorX_, anchorY_, anchorZ_, x, y, z);
            result.goalGain = anchorGoalDistance_ - goalDistance;

            if (
                result.netMovement >= config_.meaningfulNetMovement ||
                result.goalGain >= config_.meaningfulGoalGain)
            {
                result.progressed = true;
                Reset(x, y, z, goalDistance, tick);
                return result;
            }

            if (tick >= lastProgressTick_)
                result.stalledTicks = tick - lastProgressTick_;

            if (result.stalledTicks >= config_.hardStallTicks)
                result.severity = MovementStallSeverity::Hard;
            else if (result.stalledTicks >= config_.suspectedStallTicks)
                result.severity = MovementStallSeverity::Suspected;

            return result;
        }

        void BeginRecoveryWindow(
            float x,
            float y,
            float z,
            float goalDistance,
            std::uint64_t tick)
        {
            // Start a fresh bounded observation window after a recovery command.
            // This does not imply success; the caller must observe real progress
            // before clearing its consecutive recovery counter.
            Reset(x, y, z, goalDistance, tick);
        }
    };
}
