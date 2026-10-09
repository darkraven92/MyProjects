#pragma once

#include "UnattendedMaintenanceWaitPolicy.h"
#include "AutonomousMaintenancePolicy.h"
#include "VendorState.h"
#include <cstdint>

namespace Bot
{
    // Absolute automatic-maintenance attempt budget, independent of navigation
    // progress, controller substates and native AFK time. 480 ticks is the
    // existing maintenance recovery/cooldown scale (~120 s nominal).
    class AutomaticVendorEpisodePolicy
    {
        bool active_ = false, waiting_ = false, serviceProofRequired_ = false;
        std::uint64_t started_ = 0, attemptStarted_ = 0;
        unsigned attempts_ = 0, candidates_ = 0, changes_ = 0, failures_ = 0;
        std::uint32_t candidate_ = 0;
    public:
        static constexpr std::uint64_t MaximumAttemptTicks = 480;
        static constexpr unsigned MaximumAttempts =
            1 + UnattendedMaintenanceWaitPolicy::MaximumRetries;
        static bool RequirementsSatisfied(bool bagsKnown, int freeSlots, int threshold,
            const MaintenanceSnapshot& snapshot, const MaintenanceNeed& requested)
        {
            if (!bagsKnown || freeSlots <= threshold) return false;
            // New, unrelated service needs must not extend the original bag
            // lock. Known lower-bound supplies suffice at existing thresholds.
            if (!requested.Any()) return true;
            return snapshot.valid &&
                (!requested.repair || (snapshot.durabilityKnown &&
                    !AutonomousMaintenancePolicy::Evaluate(snapshot).repair)) &&
                (!requested.food || snapshot.foodCount >= AutonomousMaintenancePolicy::FoodTripThreshold) &&
                (!requested.drink || snapshot.drinkCount >= AutonomousMaintenancePolicy::DrinkTripThreshold);
        }
        static constexpr bool RejectCandidateOnExpiry(VendorState state)
        {
            switch (state)
            {
                case VendorState::SearchingVendor:
                case VendorState::NavigatingVendor:
                case VendorState::NavigatingVendorAnchor:
                case VendorState::DirectVendorApproach:
                case VendorState::WaitingForMerchant:
                case VendorState::Selling:
                case VendorState::Maintaining:
                    return true;
                default:
                    // Completed service followed by a long return journey is
                    // not merchant failure. Planning has no selected actor yet.
                    return false;
            }
        }
        bool Begin(std::uint64_t tick)
        {
            if (active_) return false; // Reset/Start is not a new attempt.
            active_ = true;
            started_ = attemptStarted_ = tick;
            attempts_ = 1;
            return true;
        }
        bool Active() const { return active_; }
        bool Waiting() const { return waiting_; }
        bool ServiceProofRequired() const { return serviceProofRequired_; }
        bool Expired(std::uint64_t tick) const
        {
            return active_ && (waiting_ || tick < attemptStarted_ ||
                tick - attemptStarted_ >= MaximumAttemptTicks);
        }
        bool Exhaust(std::uint64_t tick)
        {
            if (waiting_ || !Expired(tick)) return false;
            waiting_ = serviceProofRequired_ = true;
            return true;
        }
        bool CanRetry() const { return !active_ || attempts_ < MaximumAttempts; }
        // Only the maintenance wait's explicit, cooldown-qualified retry may
        // call this. Candidate/fallback/intent changes have no budget API.
        bool Retry(std::uint64_t tick)
        {
            if (!active_) return Begin(tick);
            if (!CanRetry()) return false;
            ++attempts_;
            attemptStarted_ = tick;
            waiting_ = false;
            return true;
        }
        void Select(std::uint32_t entry)
        {
            if (!active_) return;
            ++candidates_;
            if (candidate_ && candidate_ != entry) ++changes_;
            candidate_ = entry;
        }
        void Failure() { if (active_) ++failures_; }
        std::uint64_t Age(std::uint64_t tick) const { return tick >= started_ ? tick-started_ : 0; }
        unsigned Attempts() const { return attempts_; }
        unsigned Candidates() const { return candidates_; }
        unsigned Changes() const { return changes_; }
        unsigned Failures() const { return failures_; }
        std::uint32_t Candidate() const { return candidate_; }
        void Complete() { *this = {}; }
    };
}
