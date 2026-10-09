#pragma once

#include "QuestPlannerTypes.h"

#include <limits>

namespace Bot
{
    enum class QuestFocusConfigurationKind
    {
        Multiquest,
        Focused,
        Invalid
    };

    struct QuestFocusConfiguration
    {
        QuestFocusConfigurationKind kind = QuestFocusConfigurationKind::Multiquest;
        int questId = 0;
        bool configured = false;
    };

    // Zero preserves the existing multi-quest planner. A positive ID scopes
    // an explicitly requested diagnostic run to one profile. Negative means
    // invalid configuration and must never silently enable all quests.
    struct QuestFocusPolicy
    {
        static constexpr QuestFocusConfiguration ParseConfiguration(
            const char* raw)
        {
            if (raw == nullptr)
                return {};
            if (*raw == '\0')
                return {QuestFocusConfigurationKind::Invalid, -1, true};

            int value = 0;
            for (const char* cursor = raw; *cursor != '\0'; ++cursor)
            {
                if (*cursor < '0' || *cursor > '9')
                    return {QuestFocusConfigurationKind::Invalid, -1, true};
                const int digit = *cursor - '0';
                if (value > (std::numeric_limits<int>::max() - digit) / 10)
                    return {QuestFocusConfigurationKind::Invalid, -1, true};
                value = value * 10 + digit;
            }
            return value == 0
                ? QuestFocusConfiguration{QuestFocusConfigurationKind::Multiquest,
                    0, true}
                : QuestFocusConfiguration{QuestFocusConfigurationKind::Focused,
                    value, true};
        }

        static constexpr const char* ConfigurationName(
            QuestFocusConfigurationKind kind)
        {
            switch (kind)
            {
                case QuestFocusConfigurationKind::Multiquest: return "multiquest";
                case QuestFocusConfigurationKind::Focused: return "focused";
                case QuestFocusConfigurationKind::Invalid: return "invalid_disabled";
            }
            return "invalid_disabled";
        }

        static constexpr bool Allows(int focusQuestId, int questId)
        {
            return questId > 0 &&
                (focusQuestId == 0 || focusQuestId == questId);
        }

        // A focused quest already present in the live log owns its objective
        // or turn-in directly. Unrelated giver offers are not audited first.
        static constexpr bool NeedsPickupAudit(
            int focusQuestId, QuestPlannerAction action, bool hasFocusedProfile)
        {
            return !(focusQuestId > 0 && hasFocusedProfile &&
                (action == QuestPlannerAction::ExecuteObjective ||
                 action == QuestPlannerAction::TurnIn));
        }
    };
}
