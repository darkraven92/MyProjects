#pragma once

#include <string_view>

namespace Bot
{
    enum class CombatInputProbeState { Ready, Wait, Blocked, Unknown };

    class CombatInputProbePolicy
    {
    public:
        static CombatInputProbeState Classify(std::string_view reason)
        {
            if (reason=="ready") return CombatInputProbeState::Ready;
            if (reason=="waiting_cast" || reason=="waiting_cast_or_gcd")
                return CombatInputProbeState::Wait;
            if (reason=="blocked_modal_frame" || reason=="blocked_editbox" ||
                reason=="blocked_keyboard_handler" ||
                reason=="blocked_spell_targeting" ||
                reason=="blocked_player_dead" || reason=="blocked_target_invalid" ||
                reason=="blocked_execution_guard")
                return CombatInputProbeState::Blocked;
            return CombatInputProbeState::Unknown;
        }

        static const char* Name(CombatInputProbeState state)
        {
            switch (state)
            {
                case CombatInputProbeState::Ready: return "ready";
                case CombatInputProbeState::Wait: return "wait";
                case CombatInputProbeState::Blocked: return "blocked";
                case CombatInputProbeState::Unknown: return "unknown";
            }
            return "unknown";
        }
    };
}
