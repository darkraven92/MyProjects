#pragma once

#include <cstdint>

namespace Bot
{
    enum class DisconnectDiagnosticEvent
    {
        None,
        Healthy,
        SnapshotLost,
        SnapshotStillUnavailable,
        SnapshotRecovered
    };

    // Wall-clock cadence: snapshot failures must not stop the diagnostic clock.
    class DisconnectDiagnosticPolicy
    {
    private:
        static constexpr std::uint64_t HealthyIntervalMs = 60000;
        static constexpr std::uint64_t EarlyIncidentIntervalMs = 5000;
        static constexpr std::uint64_t LateIncidentIntervalMs = 30000;

        bool initialized_ = false;
        bool haveSuccessfulSnapshot_ = false;
        bool snapshotUnavailable_ = false;
        std::uint64_t lastSuccessfulSnapshotMs_ = 0;
        std::uint64_t incidentSinceMs_ = 0;
        std::uint64_t lastLogMs_ = 0;

    public:
        DisconnectDiagnosticEvent Update(bool snapshotValid, std::uint64_t nowMs)
        {
            if (!initialized_ || nowMs < lastLogMs_)
            {
                initialized_ = true;
                haveSuccessfulSnapshot_ = snapshotValid;
                snapshotUnavailable_ = !snapshotValid;
                lastSuccessfulSnapshotMs_ = nowMs;
                incidentSinceMs_ = nowMs;
                lastLogMs_ = nowMs;
                return snapshotValid
                    ? DisconnectDiagnosticEvent::Healthy
                    : DisconnectDiagnosticEvent::SnapshotLost;
            }

            if (snapshotValid)
            {
                haveSuccessfulSnapshot_ = true;
                lastSuccessfulSnapshotMs_ = nowMs;
                if (snapshotUnavailable_)
                {
                    snapshotUnavailable_ = false;
                    lastLogMs_ = nowMs;
                    return DisconnectDiagnosticEvent::SnapshotRecovered;
                }
                if (nowMs - lastLogMs_ >= HealthyIntervalMs)
                {
                    lastLogMs_ = nowMs;
                    return DisconnectDiagnosticEvent::Healthy;
                }
                return DisconnectDiagnosticEvent::None;
            }

            if (!snapshotUnavailable_)
            {
                snapshotUnavailable_ = true;
                incidentSinceMs_ = nowMs;
                lastLogMs_ = nowMs;
                return DisconnectDiagnosticEvent::SnapshotLost;
            }

            const std::uint64_t interval = nowMs - incidentSinceMs_ < LateIncidentIntervalMs
                ? EarlyIncidentIntervalMs
                : LateIncidentIntervalMs;
            if (nowMs - lastLogMs_ >= interval)
            {
                lastLogMs_ = nowMs;
                return DisconnectDiagnosticEvent::SnapshotStillUnavailable;
            }
            return DisconnectDiagnosticEvent::None;
        }

        bool HaveSuccessfulSnapshot() const { return haveSuccessfulSnapshot_; }

        std::uint64_t SnapshotAgeMs(std::uint64_t nowMs) const
        {
            if (!haveSuccessfulSnapshot_ || nowMs < lastSuccessfulSnapshotMs_)
                return 0;
            return nowMs - lastSuccessfulSnapshotMs_;
        }

        std::uint64_t IncidentAgeMs(std::uint64_t nowMs) const
        {
            if (!snapshotUnavailable_ || nowMs < incidentSinceMs_)
                return 0;
            return nowMs - incidentSinceMs_;
        }
    };
}
