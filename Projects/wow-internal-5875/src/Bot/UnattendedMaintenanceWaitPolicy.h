#pragma once

#include <cstdint>

namespace Bot
{
    enum class MaintenanceWaitDecision { Wait, RetryVendor, MaintenanceBlocked, Resume };

    // Ordinary failed full-bag trips release on bag proof alone. The caller
    // may additionally gate release for an exhausted automatic service-search
    // episode; that separate latch must not change ordinary bag-wait semantics.
    class UnattendedMaintenanceWaitPolicy
    {
        bool active_ = false;
        unsigned retries_ = 0, spaceObservations_ = 0;
        std::uint64_t retryAt_ = 0;
    public:
        static constexpr unsigned MaximumRetries = 2;
        void FailedTrip(std::uint64_t retryAt)
        {
            active_ = true;
            retryAt_ = retryAt;
            spaceObservations_ = 0;
        }
        bool Active() const { return active_; }
        unsigned Retries() const { return retries_; }
        unsigned SpaceObservations() const { return spaceObservations_; }
        std::uint64_t RetryAt() const { return retryAt_; }
        void InvalidateSpaceProof() { spaceObservations_ = 0; }
        void Reset() { *this = {}; }
        MaintenanceWaitDecision Observe(std::uint64_t tick, bool freshRead,
            bool bagsKnown, int freeSlots, int safeThreshold,
            bool ownerSafe, bool automaticEnabled, bool releaseAllowed = true)
        {
            if (!active_) return MaintenanceWaitDecision::Wait;
            if (!ownerSafe)
            {
                spaceObservations_ = 0;
                return MaintenanceWaitDecision::Wait;
            }
            if (freshRead)
            {
                spaceObservations_ = releaseAllowed && bagsKnown && freeSlots > safeThreshold
                    ? spaceObservations_ + 1 : 0;
                if (spaceObservations_ >= 2) return MaintenanceWaitDecision::Resume;
            }
            // A fresh free-space observation must be confirmed, not interrupted
            // by a new trip. Unknown/full snapshots never authorize Grind.
            if (spaceObservations_) return MaintenanceWaitDecision::Wait;
            if (!automaticEnabled || retries_ >= MaximumRetries)
                return MaintenanceWaitDecision::MaintenanceBlocked;
            if (!freshRead || tick < retryAt_) return MaintenanceWaitDecision::Wait;
            ++retries_;
            return MaintenanceWaitDecision::RetryVendor;
        }
    };
}
