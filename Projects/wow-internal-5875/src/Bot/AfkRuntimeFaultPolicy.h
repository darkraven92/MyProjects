#pragma once

namespace Bot
{
    // Tactical recovery debt is liveness evidence, not an input/terminal fault.
    enum class AfkRuntimeFaultReason { None, TerminalCombat, TerminalGrind };

    struct AfkRuntimeFaultEvidence
    {
        bool combatFailed=false;
        bool grindFailed=false;
        int recoveryDebt=0;
    };

    struct AfkRuntimeFaultAssessment
    {
        bool terminal=false;
        AfkRuntimeFaultReason reason=AfkRuntimeFaultReason::None;
        int recoveryDebt=0;
    };

    struct AfkRuntimeFaultPolicy
    {
        static AfkRuntimeFaultAssessment Assess(const AfkRuntimeFaultEvidence& evidence)
        {
            return {evidence.combatFailed || evidence.grindFailed,
                evidence.combatFailed ? AfkRuntimeFaultReason::TerminalCombat :
                evidence.grindFailed ? AfkRuntimeFaultReason::TerminalGrind :
                AfkRuntimeFaultReason::None,
                evidence.recoveryDebt};
        }
        static const char* ReasonName(AfkRuntimeFaultReason reason)
        {
            switch (reason)
            {
            case AfkRuntimeFaultReason::TerminalCombat: return "terminal_combat_fault";
            case AfkRuntimeFaultReason::TerminalGrind: return "terminal_grind_fault";
            case AfkRuntimeFaultReason::None: return "fault_or_unknown_subsystem";
            }
            return "fault_or_unknown_subsystem";
        }
    };
}
