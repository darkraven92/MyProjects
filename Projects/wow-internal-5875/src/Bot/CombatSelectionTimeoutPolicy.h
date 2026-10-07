#pragma once

namespace Bot
{
    enum class CombatSelectionTimeoutAction
    {
        SystemFail, AbandonOptional, FailMandatoryOwner
    };

    struct CombatSelectionTimeoutEvidence
    {
        bool optionalGrind=false, mandatoryObjective=false;
        bool known=false, hostileEngaged=true;
        bool attackKnown=false, attackActive=false;
    };

    inline CombatSelectionTimeoutAction DecideSelectionTimeout(
        const CombatSelectionTimeoutEvidence& e)
    {
        // No Attack was started for this pending lock. Never release an active
        // attacker, unknown client state, or an unowned objective as optional.
        if (!e.known || e.hostileEngaged || !e.attackKnown || e.attackActive)
            return CombatSelectionTimeoutAction::SystemFail;
        if (e.optionalGrind) return CombatSelectionTimeoutAction::AbandonOptional;
        if (e.mandatoryObjective) return CombatSelectionTimeoutAction::FailMandatoryOwner;
        return CombatSelectionTimeoutAction::SystemFail;
    }
}
