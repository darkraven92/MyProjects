#pragma once

#include "QuestPlannerTypes.h"

#include "../Objects/WorldState.h"

#include <cstdint>

namespace Bot
{
    class CombatController;

    enum class ObjectiveExecutorState
    {
        Idle,
        Navigating,
        Executing,
        ReadyForTurnIn,
        Failed
    };

    class IObjectiveExecutor
    {
    public:
        virtual ~IObjectiveExecutor() = default;

        virtual bool Supports(
            const QuestProfile& profile) const = 0;

        virtual bool Start(
            const QuestProfile& profile,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) = 0;

        virtual void Update(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) = 0;

        virtual bool OwnsControl() const = 0;

        virtual ObjectiveExecutorState State() const = 0;

        virtual const char* StateName() const = 0;

        // Optional normalized failure evidence. Executors without a stable
        // reason fail closed for planner difficulty accounting.
        virtual const char* FailureReason() const { return nullptr; }
    };
}
