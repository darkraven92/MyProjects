#pragma once

#include <cstddef>

namespace Navigation
{
    // At 250 ms monitor cadence, 704 one-tile steps would take >=176 seconds
    // before addTile work, exceeding the existing 3-minute death no-progress
    // budget. Two tiles keep each update bounded while allowing that route
    // to complete before the deadline under the measured ~36-second load.
    // The cursor belongs to one
    // provider initialization episode and is never shared across threads.
    class IncrementalTileInitializationPolicy
    {
    public:
        static constexpr std::size_t MaxTilesPerStep = 2;
        enum class State { Idle, Pending, Ready, Failed, Cancelled };

        void Begin(std::size_t tileCount)
        {
            tileCount_ = tileCount;
            next_ = 0;
            processedThisStep_ = 0;
            state_ = State::Pending;
        }

        void BeginStep() { processedThisStep_ = 0; }

        bool Next(std::size_t& index)
        {
            if (state_ != State::Pending || next_ >= tileCount_ ||
                processedThisStep_ >= MaxTilesPerStep)
                return false;
            index = next_++;
            ++processedThisStep_;
            return true;
        }

        void Cancel()
        {
            state_ = State::Cancelled;
            tileCount_ = 0;
            next_ = 0;
            processedThisStep_ = 0;
        }

        void Fail() { state_ = State::Failed; }
        void MarkReady()
        {
            if (Complete())
                state_ = State::Ready;
        }

        bool Complete() const { return state_ == State::Pending && next_ == tileCount_; }
        bool Active() const { return state_ == State::Pending; }
        State CurrentState() const { return state_; }
        std::size_t Total() const { return tileCount_; }
        std::size_t Processed() const { return next_; }

    private:
        std::size_t tileCount_ = 0;
        std::size_t next_ = 0;
        std::size_t processedThisStep_ = 0;
        State state_ = State::Idle;
    };
}
