#pragma once

#include <cstdint>
#include <limits>

namespace Bot
{
    enum class AfkDiagnosticEvent
    {
        None,
        Entered,
        Cleared
    };

    struct AfkDiagnosticTransition
    {
        AfkDiagnosticEvent event = AfkDiagnosticEvent::None;
        bool initialObservation = false;
        std::uint64_t observedDurationMs = 0;
    };

    // Tracks the unverified PLAYER_FLAGS candidate, not authoritative AFK state.
    class AfkDiagnosticTracker
    {
    private:
        bool everKnown_ = false;
        bool stale_ = true;
        bool candidateAfk_ = false;
        std::uint64_t enteredAtMs_ = 0;
        std::uint64_t lastObservedAtMs_ = 0;
        std::uint64_t observedDurationMs_ = 0;

        static std::uint64_t Age(std::uint64_t now, std::uint64_t then)
        {
            return now >= then ? now - then : 0;
        }

        static std::uint64_t AddClamped(std::uint64_t a, std::uint64_t b)
        {
            const auto maximum = std::numeric_limits<std::uint64_t>::max();
            return b > maximum - a ? maximum : a + b;
        }

    public:
        AfkDiagnosticTransition Observe(
            bool known,
            bool candidateAfk,
            std::uint64_t nowMs)
        {
            if (!known)
            {
                stale_ = true;
                return {};
            }

            const bool initialObservation = !everKnown_ || stale_;
            if (!everKnown_)
            {
                everKnown_ = true;
                stale_ = false;
                candidateAfk_ = candidateAfk;
                lastObservedAtMs_ = nowMs;
                if (candidateAfk)
                {
                    enteredAtMs_ = nowMs;
                    return {AfkDiagnosticEvent::Entered, true, 0};
                }
                return {};
            }

            // Count only intervals bounded by two positive AFK observations.
            // Unknown intervals cannot establish continuous AFK duration.
            if (candidateAfk_ && candidateAfk && !stale_)
                observedDurationMs_ = AddClamped(
                    observedDurationMs_, Age(nowMs, lastObservedAtMs_));

            if (nowMs > lastObservedAtMs_)
                lastObservedAtMs_ = nowMs;
            stale_ = false;
            if (candidateAfk_ == candidateAfk)
                return {};

            candidateAfk_ = candidateAfk;
            if (candidateAfk)
            {
                enteredAtMs_ = nowMs;
                observedDurationMs_ = 0;
                return {AfkDiagnosticEvent::Entered, initialObservation, 0};
            }

            const auto clearedDurationMs = observedDurationMs_;
            observedDurationMs_ = 0;
            return {AfkDiagnosticEvent::Cleared,
                    initialObservation, clearedDurationMs};
        }

        bool EverKnown() const { return everKnown_; }
        bool Stale() const { return stale_; }
        bool CandidateAfk() const { return candidateAfk_; }
        std::uint64_t EnteredAtMs() const { return enteredAtMs_; }
        std::uint64_t LastObservedAtMs() const { return lastObservedAtMs_; }
        std::uint64_t ObservedDurationMs() const { return observedDurationMs_; }
        std::uint64_t LastObservedAgeMs(std::uint64_t nowMs) const
        {
            return everKnown_ ? Age(nowMs, lastObservedAtMs_) : 0;
        }
    };
}
