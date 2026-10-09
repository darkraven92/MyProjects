#pragma once

namespace Bot
{
    enum class MaintenanceOutcome
    {
        InProgress, Satisfied, StillRequired, TemporarilyUnavailable, TerminalFailure
    };
    struct MaintenanceOutcomePolicy
    {
        static bool Unmet(bool previousUnmet, bool bagRequired, bool bagConfirmed,
                          bool repairRequired, bool repairConfirmed,
                          bool foodRequired, bool foodConfirmed,
                          bool drinkRequired, bool drinkConfirmed)
        {
            return previousUnmet || (bagRequired && !bagConfirmed) ||
                (repairRequired && !repairConfirmed) || (foodRequired && !foodConfirmed) ||
                (drinkRequired && !drinkConfirmed);
        }
        static const char* Name(MaintenanceOutcome outcome)
        {
            switch (outcome)
            {
                case MaintenanceOutcome::InProgress: return "in_progress";
                case MaintenanceOutcome::Satisfied: return "maintenance_satisfied";
                case MaintenanceOutcome::StillRequired: return "maintenance_still_required";
                case MaintenanceOutcome::TemporarilyUnavailable: return "maintenance_temporarily_unavailable";
                case MaintenanceOutcome::TerminalFailure: return "maintenance_terminal_failure";
            }
            return "unknown";
        }
    };
}
