#pragma once

#include "CollectItemFromMobExecutor.h"
#include "CollectWorldItemExecutor.h"
#include "IObjectiveExecutor.h"
#include "InteractGameObjectExecutor.h"
#include "UseItemOnUnitExecutor.h"

#include "../Debug/Logger.h"

#include <memory>
#include <string>

namespace Bot
{
    class ObjectiveExecutionDirector
    {
    private:
        std::unique_ptr<IObjectiveExecutor> active_{};
        const QuestProfile* catalogueProfile_ = nullptr;
        QuestProfile resolvedProfile_{};
        bool haveResolvedProfile_ = false;

    public:
        bool Start(
            const QuestProfile& profile,
            int objectiveIndex,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (active_ != nullptr)
                return false;

            resolvedProfile_ = MaterializeObjectiveStep(profile, objectiveIndex);
            haveResolvedProfile_ = true;
            catalogueProfile_ = &profile;

            std::unique_ptr<IObjectiveExecutor> candidate{};

            {
                auto executor = std::make_unique<CollectItemFromMobExecutor>();
                if (executor->Supports(resolvedProfile_))
                    candidate = std::move(executor);
            }

            if (candidate == nullptr)
            {
                auto executor = std::make_unique<CollectWorldItemExecutor>();
                if (executor->Supports(resolvedProfile_))
                    candidate = std::move(executor);
            }

            if (candidate == nullptr)
            {
                auto executor = std::make_unique<UseItemOnUnitExecutor>();
                if (executor->Supports(resolvedProfile_))
                    candidate = std::move(executor);
            }

            if (candidate == nullptr)
            {
                auto executor = std::make_unique<InteractGameObjectExecutor>();
                if (executor->Supports(resolvedProfile_))
                    candidate = std::move(executor);
            }

            if (candidate == nullptr)
            {
                Debug::Logger::Info(
                    "OBJECTIVE DIRECTOR: no executor supports objective type for quest " +
                    std::to_string(profile.questId) +
                    " objectiveIndex=" + std::to_string(objectiveIndex));
                haveResolvedProfile_ = false;
                catalogueProfile_ = nullptr;
                return false;
            }

            active_ = std::move(candidate);

            Debug::Logger::Info(
                "QUESTDB 13A: OBJECTIVE STEP MATERIALIZED questId=" +
                std::to_string(profile.questId) +
                " step=" + std::to_string(resolvedProfile_.activeObjectiveIndex + 1) +
                " leaderboard=" + std::to_string(resolvedProfile_.activeLeaderboardIndex + 1) +
                " type=" + std::to_string(static_cast<int>(resolvedProfile_.objective.type)));

            if (!active_->Start(resolvedProfile_, world, combat, tick))
            {
                active_.reset();
                haveResolvedProfile_ = false;
                catalogueProfile_ = nullptr;
                return false;
            }

            return true;
        }

        void Update(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (active_ != nullptr)
                active_->Update(snapshot, world, combat, tick);
        }

        bool OwnsControl() const
        {
            return active_ != nullptr && active_->OwnsControl();
        }

        bool ReadyForTurnIn() const
        {
            return active_ != nullptr &&
                   active_->State() == ObjectiveExecutorState::ReadyForTurnIn;
        }

        bool Failed() const
        {
            return active_ != nullptr &&
                   active_->State() == ObjectiveExecutorState::Failed;
        }

        void ReleaseActive()
        {
            active_.reset();
            catalogueProfile_ = nullptr;
            haveResolvedProfile_ = false;
            // resolvedProfile_ remains value storage, but an inactive director
            // must never expose stale quest ownership after retries, combat
            // preemption or contained runtime faults.
        }

        const QuestProfile* ActiveProfile() const
        {
            if (active_ == nullptr)
                return nullptr;
            return haveResolvedProfile_ ? &resolvedProfile_ : catalogueProfile_;
        }

        const QuestProfile* CatalogueProfile() const
        {
            return catalogueProfile_;
        }

        int ActiveObjectiveIndex() const
        {
            return haveResolvedProfile_ ? resolvedProfile_.activeObjectiveIndex : -1;
        }

        const char* StateName() const
        {
            return active_ == nullptr ? "Idle" : active_->StateName();
        }
    };
}
