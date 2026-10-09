#pragma once

#include "PullSafetyDiagnosticPolicy.h"

#include "../Debug/Logger.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>

namespace Bot
{
    struct PullSafetyCandidateTelemetry
    {
        static void Emit(
            std::span<const PullSafetyUnit> units, float healthPercent,
            const PullSafetySelection& selection, const char* mode)
        {
            const auto candidates = PullSafetyDiagnosticPolicy::Evaluate(
                units, healthPercent, selection);
            if (!PullSafetyDiagnosticPolicy::ShouldEmit(
                    selection, candidates))
                return;

            for (const auto& candidate : candidates)
            {
                const auto& unit = units[candidate.index];
                std::ostringstream line;
                line << std::fixed << std::setprecision(3)
                    << "PULL SAFETY CANDIDATE mode=" << mode
                    << " guid=0x" << std::uppercase << std::hex
                    << std::setw(16) << std::setfill('0') << unit.guid
                    << std::dec << std::setfill(' ')
                    << " entry=" << unit.entry
                    << " distance=" << unit.playerDistance
                    << " predictedAdds=" <<
                        candidate.assessment.predictedAdds
                    << " nearestHostileDistance=";
                if (std::isfinite(
                        candidate.assessment.nearestHostileDistance))
                    line << candidate.assessment.nearestHostileDistance;
                else
                    line << "none";
                line << " decision=" <<
                    PullSafetyDiagnosticPolicy::DecisionName(
                        candidate.decision)
                    << " reason=" <<
                    PullSafetyDiagnosticPolicy::ReasonName(candidate);
                Debug::Logger::Info(line.str());
            }
        }
    };
}
