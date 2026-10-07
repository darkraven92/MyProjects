#pragma once

namespace Bot
{
    enum class LivingWaterBlockEvent
    {
        None,
        Entered,
        ExitedNonSwimming,
        ExitedDeathOwnership
    };

    // A verified SWIMMING bit blocks immediately. Unknown reads never clear a
    // block; three consecutive verified non-swimming reads permit manual exit.
    // This is deliberately not a DryGround classifier.
    class LivingWaterBlockPolicy
    {
        bool blocked_ = false;
        unsigned nonSwimmingSamples_ = 0;
    public:
        static constexpr unsigned ExitSamples = 3;

        bool Blocked() const { return blocked_; }

        LivingWaterBlockEvent Observe(bool deathOwns, bool swimmingKnown,
                                      bool swimming)
        {
            if (deathOwns)
            {
                const bool wasBlocked = blocked_;
                blocked_ = false;
                nonSwimmingSamples_ = 0;
                return wasBlocked ? LivingWaterBlockEvent::ExitedDeathOwnership
                                  : LivingWaterBlockEvent::None;
            }
            if (!swimmingKnown)
            {
                nonSwimmingSamples_ = 0;
                return LivingWaterBlockEvent::None;
            }
            if (swimming)
            {
                nonSwimmingSamples_ = 0;
                if (blocked_) return LivingWaterBlockEvent::None;
                blocked_ = true;
                return LivingWaterBlockEvent::Entered;
            }
            if (!blocked_) return LivingWaterBlockEvent::None;
            if (++nonSwimmingSamples_ < ExitSamples)
                return LivingWaterBlockEvent::None;
            blocked_ = false;
            nonSwimmingSamples_ = 0;
            return LivingWaterBlockEvent::ExitedNonSwimming;
        }
    };
}
