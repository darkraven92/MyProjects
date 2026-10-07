#pragma once

#include <cstdint>

namespace Bot
{
    // Observation only. A missing SWIMMING bit is not positive ground contact.
    enum class WaterObservationClass
    {
        Unknown,
        DryOrNonSwimming,
        SwimmingStateUnknown
    };

    inline const char* WaterObservationClassName(WaterObservationClass value)
    {
        switch (value)
        {
            case WaterObservationClass::Unknown: return "Unknown";
            case WaterObservationClass::DryOrNonSwimming: return "DryOrNonSwimming";
            case WaterObservationClass::SwimmingStateUnknown: return "SwimmingStateUnknown";
        }
        return "Unknown";
    }

    struct WaterEvidenceSnapshot5875
    {
        bool movementKnown=false;
        std::uint32_t movementFlags=0;
        // No coherent, fresh live mirror-timer or liquid-contact reader is
        // source verified yet. These stay unknown rather than guessed.
        bool breathKnown=false;
        bool fatigueKnown=false;
        bool submergedKnown=false;
        bool surfaceKnown=false;
        bool groundContactKnown=false;
    };

    class WaterEvidenceTracker5875
    {
        WaterObservationClass confirmed_=WaterObservationClass::Unknown;
        WaterObservationClass pending_=WaterObservationClass::Unknown;
        unsigned pendingSamples_=0;
        bool emitted_=false;
        std::uint64_t lastEmitMs_=0;

    public:
        static constexpr std::uint32_t SwimmingMask=0x00200000u;
        static constexpr std::uint64_t PeriodicMs=30000;

        WaterObservationClass Update(const WaterEvidenceSnapshot5875& evidence)
        {
            if (!evidence.movementKnown)
            {
                confirmed_=pending_=WaterObservationClass::Unknown;
                pendingSamples_=0;
                return confirmed_;
            }
            const auto desired=(evidence.movementFlags&SwimmingMask)
                ? WaterObservationClass::SwimmingStateUnknown
                : WaterObservationClass::DryOrNonSwimming;
            if (desired==confirmed_)
            {
                pending_=WaterObservationClass::Unknown;
                pendingSamples_=0;
                return confirmed_;
            }
            if (desired!=pending_)
            {
                pending_=desired;
                pendingSamples_=1;
                return WaterObservationClass::Unknown;
            }
            if (++pendingSamples_>=2)
            {
                confirmed_=desired;
                pending_=WaterObservationClass::Unknown;
                pendingSamples_=0;
                return confirmed_;
            }
            return WaterObservationClass::Unknown;
        }

        bool ShouldEmit(WaterObservationClass state, std::uint64_t nowMs)
        {
            if (!emitted_ || state!=lastEmitted_ ||
                nowMs-lastEmitMs_>=PeriodicMs)
            {
                emitted_=true;
                lastEmitted_=state;
                lastEmitMs_=nowMs;
                return true;
            }
            return false;
        }

    private:
        WaterObservationClass lastEmitted_=WaterObservationClass::Unknown;
    };
}
