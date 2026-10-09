#pragma once

#include "CollectItemFromMobExecutor.h"
#include "IObjectiveExecutor.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "SummonedQuestObjectivePolicy.h"
#include "UseItemOnUnitExecutor.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace Bot
{
    enum class SummonedQuestState
    {
        Idle,
        RoutingToUseLocation,
        Positioning,
        AwaitingSpawn,
        CombatAndLoot,
        AwaitingProgress,
        ReadyForTurnIn,
        Failed
    };

    // A generic item-at-location summon objective. Its combat/loot leg is
    // delegated to the existing CollectItemFromMobExecutor; this controller
    // never implements attacks or assumes a kill satisfies an item objective.
    class SummonedQuestObjectiveExecutor final : public IObjectiveExecutor
    {
    private:
        SummonedQuestState state_ = SummonedQuestState::Idle;
        std::string failureReason_{};
        const QuestProfile* profile_ = nullptr;
        QuestProfile collectProfile_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        std::unique_ptr<CollectItemFromMobExecutor> collect_{};
        ObjectiveDefensiveCombatGuard defense_{};
        std::uint64_t startTick_ = 0;
        std::uint64_t stateTick_ = 0;
        std::uint64_t lastUseTick_ = 0;
        std::uint64_t nextUseTick_ = 0;
        int useAttempts_ = 0;
        int stableSamples_ = 0;
        float lastX_ = 0.0f;
        float lastY_ = 0.0f;
        float lastZ_ = 0.0f;

        static const char* Name(SummonedQuestState state)
        {
            switch (state)
            {
                case SummonedQuestState::Idle: return "Idle";
                case SummonedQuestState::RoutingToUseLocation: return "RoutingToUseLocation";
                case SummonedQuestState::Positioning: return "Positioning";
                case SummonedQuestState::AwaitingSpawn: return "AwaitingSpawn";
                case SummonedQuestState::CombatAndLoot: return "CombatAndLoot";
                case SummonedQuestState::AwaitingProgress: return "AwaitingProgress";
                case SummonedQuestState::ReadyForTurnIn: return "ReadyForTurnIn";
                case SummonedQuestState::Failed: return "Failed";
            }
            return "Unknown";
        }

        void SetState(SummonedQuestState next, std::uint64_t tick)
        {
            if (state_ == next)
                return;
            state_ = next;
            stateTick_ = tick;
            Debug::Logger::Info(
                "QUEST 16C OBJECTIVE questId=" +
                std::to_string(profile_ == nullptr ? 0 : profile_->questId) +
                " state=" + Name(next) +
                " expectedEntry=" +
                std::to_string(profile_ == nullptr ? 0 : profile_->objective.targetEntry));
        }

        void Fail(const std::string& reason, std::uint64_t tick,
            CombatController* combat = nullptr)
        {
            failureReason_ = reason;
            if (collect_ != nullptr && combat != nullptr)
                combat->ClearPlannerQuestTarget();
            navigator_.reset();
            collect_.reset();
            Debug::Logger::Info(
                "QUEST 16C OBJECTIVE questId=" +
                std::to_string(profile_ == nullptr ? 0 : profile_->questId) +
                " result=failed reason=" + reason +
                " useAttempts=" + std::to_string(useAttempts_));
            SetState(SummonedQuestState::Failed, tick);
        }

        const PlannerQuestLogEntry* FindLiveQuest(
            const QuestPlannerSnapshot& snapshot) const
        {
            if (profile_ == nullptr)
                return nullptr;
            for (const auto& entry : snapshot.quests)
            {
                const auto* mapped = ValleyOfTrialsProfiles::Find(
                    entry, snapshot.classToken);
                if (mapped != nullptr && mapped->questId == profile_->questId)
                    return &entry;
            }
            return nullptr;
        }

        float DistanceToUseLocation(float x, float y, float z) const
        {
            return std::hypot(
                x - profile_->destination.x,
                y - profile_->destination.y,
                z - profile_->destination.z);
        }

        const Objects::UnitState* FindExpectedSpawn(
            const Objects::WorldState& world) const
        {
            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!SummonedQuestObjectivePolicy::ExpectedSpawn(
                        unit.entryId, profile_->objective.targetEntry,
                        unit.valid && unit.guid != 0, unit.isPet,
                        unit.health,
                        DistanceToUseLocation(unit.x, unit.y, unit.z)))
                    continue;
                if (best == nullptr || unit.distance < best->distance ||
                    (unit.distance == best->distance && unit.guid < best->guid))
                    best = &unit;
            }
            return best;
        }

        bool StartCollect(
            const Objects::WorldState& world,
            const Objects::UnitState& spawn,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (navigator_ != nullptr)
                navigator_->PauseForCombat(
                    world.player, "expected summoned quest unit acquired");
            navigator_.reset();
            collectProfile_ = *profile_;
            collectProfile_.objective.type = QuestObjectiveType::CollectItemFromMob;
            collectProfile_.questUseItemId = 0;
            collect_ = std::make_unique<CollectItemFromMobExecutor>();
            Debug::Logger::Info(
                "QUEST 16C SPAWN questId=" + std::to_string(profile_->questId) +
                " entry=" + std::to_string(spawn.entryId) +
                " guid=" + std::to_string(spawn.guid) +
                " distance=" + std::to_string(spawn.distance));
            if (!collect_->Start(collectProfile_, world, combat, tick))
            {
                collect_.reset();
                Fail("collect_executor_start_failed", tick);
                return false;
            }
            SetState(SummonedQuestState::CombatAndLoot, tick);
            return true;
        }

    public:
        bool Supports(const QuestProfile& profile) const override
        {
            return SummonedQuestObjectivePolicy::Supports(profile);
        }

        bool Start(
            const QuestProfile& profile,
            const Objects::WorldState& world,
            CombatController&,
            std::uint64_t tick) override
        {
            if (state_ != SummonedQuestState::Idle || !Supports(profile) ||
                !world.player.valid || world.player.health == 0)
                return false;
            profile_ = &profile;
            startTick_ = tick;
            stateTick_ = tick;
            useAttempts_ = 0;
            nextUseTick_ = tick;
            stableSamples_ = 0;
            defense_.Reset(world, "summoned quest " + std::to_string(profile.questId));
            navigator_ = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            const Navigation::NavPoint destination{
                profile.destination.x, profile.destination.y, profile.destination.z};
            if (!navigator_->Start(world.player, tick, destination,
                    profile.destination.mapId,
                    profile.destination.arrivalDistance,
                    profile.destination.label))
            {
                Fail("route_start_failed", tick);
                return false;
            }
            SetState(SummonedQuestState::RoutingToUseLocation, tick);
            return true;
        }

        void Update(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) override
        {
            if (state_ == SummonedQuestState::Idle ||
                state_ == SummonedQuestState::ReadyForTurnIn ||
                state_ == SummonedQuestState::Failed)
                return;

            const auto* liveQuest = FindLiveQuest(snapshot);
            if (liveQuest == nullptr)
            {
                Fail("live_quest_identity_lost", tick, &combat);
                return;
            }
            if (liveQuest->complete)
            {
                if (collect_ != nullptr)
                    combat.ClearPlannerQuestTarget();
                navigator_.reset();
                collect_.reset();
                Debug::Logger::Info(
                    "QUEST 16C PROGRESS questId=" +
                    std::to_string(profile_->questId) + " completed=yes source=live_quest_log");
                SetState(SummonedQuestState::ReadyForTurnIn, tick);
                return;
            }
            if (SelectedObjectiveComplete(*liveQuest, *profile_) &&
                state_ != SummonedQuestState::CombatAndLoot &&
                state_ != SummonedQuestState::AwaitingProgress)
            {
                navigator_.reset();
                SetState(SummonedQuestState::AwaitingProgress, tick);
                return;
            }
            if (world.player.health == 0 ||
                SummonedQuestObjectivePolicy::ObjectiveTimedOut(tick, startTick_))
            {
                Fail(world.player.health == 0 ? "player_dead" : "objective_timeout",
                    tick, &combat);
                return;
            }

            if (state_ == SummonedQuestState::CombatAndLoot)
            {
                if (collect_ == nullptr)
                {
                    Fail("collect_executor_missing", tick, &combat);
                    return;
                }
                collect_->Update(snapshot, world, combat, tick);
                if (collect_->State() == ObjectiveExecutorState::Failed)
                    Fail("combat_or_loot_failed", tick, &combat);
                else if (collect_->State() == ObjectiveExecutorState::ReadyForTurnIn)
                {
                    collect_.reset();
                    SetState(SummonedQuestState::AwaitingProgress, tick);
                }
                return;
            }

            if (state_ == SummonedQuestState::AwaitingProgress)
            {
                if (tick - stateTick_ >= SummonedQuestObjectivePolicy::SpawnWaitTicks)
                    Fail("live_quest_completion_not_observed", tick, &combat);
                return;
            }

            // An existing valid objective creature can be acquired without
            // using another horn. Only the profiled creature near the profiled
            // use area is eligible; unrelated nearby mobs are ignored.
            if (const auto* spawn = FindExpectedSpawn(world); spawn != nullptr)
            {
                // Do not steal an unrelated defensive fight merely because
                // the expected creature became visible at the same time.
                if (combat.LockedGuid() == 0 ||
                    combat.LockedGuid() == spawn->guid)
                {
                    StartCollect(world, *spawn, combat, tick);
                    return;
                }
            }

            const auto defenseResult = defense_.Update(
                world, combat, navigator_.get(), 0, tick);
            if (defenseResult == ObjectiveDefenseUpdate::Failed)
            {
                Fail("defensive_combat_failed", tick, &combat);
                return;
            }
            if (defenseResult == ObjectiveDefenseUpdate::OwnsControl)
                return;

            if (state_ == SummonedQuestState::RoutingToUseLocation)
            {
                navigator_->Update(world.player, tick);
                if (navigator_->Failed())
                {
                    Fail("route_to_use_location_failed", tick);
                    return;
                }
                if (navigator_->Arrived())
                {
                    if (!ClickToMoveController::MoveTo(
                            world.player, world.player.x, world.player.y,
                            world.player.z, 0.25f))
                    {
                        Fail("stop_at_use_location_failed", tick, &combat);
                        return;
                    }
                    navigator_.reset();
                    lastX_ = world.player.x;
                    lastY_ = world.player.y;
                    lastZ_ = world.player.z;
                    stableSamples_ = 0;
                    SetState(SummonedQuestState::Positioning, tick);
                }
                return;
            }

            if (state_ == SummonedQuestState::AwaitingSpawn)
            {
                if (!SummonedQuestObjectivePolicy::SpawnWaitExpired(tick, lastUseTick_))
                    return;
                if (useAttempts_ >= SummonedQuestObjectivePolicy::MaximumUseAttempts)
                {
                    Fail("spawn_not_observed_after_bounded_uses", tick);
                    return;
                }
                stableSamples_ = 0;
                nextUseTick_ = std::max(
                    nextUseTick_,
                    tick + SummonedQuestObjectivePolicy::RetryDelayTicks);
                SetState(SummonedQuestState::Positioning, tick);
                return;
            }

            if (state_ != SummonedQuestState::Positioning)
                return;
            const float displacement = std::hypot(
                world.player.x - lastX_, world.player.y - lastY_,
                world.player.z - lastZ_);
            stableSamples_ = SummonedQuestObjectivePolicy::StableSample(displacement)
                ? stableSamples_ + 1 : 0;
            lastX_ = world.player.x;
            lastY_ = world.player.y;
            lastZ_ = world.player.z;

            const float useDistance = DistanceToUseLocation(
                world.player.x, world.player.y, world.player.z);
            const bool combatIdle = combat.LockedGuid() == 0 &&
                (combat.State() == CombatState::Idle ||
                 combat.State() == CombatState::AcquiringTarget);
            if (!SummonedQuestObjectivePolicy::MayUse(
                    true, !SelectedObjectiveComplete(*liveQuest, *profile_),
                    world.player.health > 0, combatIdle, true,
                    useDistance, stableSamples_, useAttempts_, tick, nextUseTick_))
            {
                if (useDistance > SummonedQuestObjectivePolicy::UseRadius)
                    Fail("outside_verified_use_radius", tick);
                return;
            }

            std::string useResult;
            if (!UseItemOnUnitExecutor::UseQuestItemById(
                    profile_->questUseItemId, useResult))
            {
                Fail("quest_item_dispatch_failed", tick);
                return;
            }
            Debug::Logger::Info(
                "QUEST 16C ITEM questId=" + std::to_string(profile_->questId) +
                " itemId=" + std::to_string(profile_->questUseItemId) +
                " present=" + (useResult.rfind("used|", 0) == 0 ? "yes" : "no") +
                " action=" + useResult);
            if (useResult.rfind("used|", 0) != 0)
            {
                Fail("required_quest_item_absent", tick);
                return;
            }
            ++useAttempts_;
            lastUseTick_ = tick;
            nextUseTick_ = tick + profile_->questUseMinimumIntervalTicks;
            SetState(SummonedQuestState::AwaitingSpawn, tick);
        }

        bool OwnsControl() const override
        {
            return state_ != SummonedQuestState::Idle &&
                state_ != SummonedQuestState::Failed;
        }

        ObjectiveExecutorState State() const override
        {
            if (state_ == SummonedQuestState::ReadyForTurnIn)
                return ObjectiveExecutorState::ReadyForTurnIn;
            if (state_ == SummonedQuestState::Failed)
                return ObjectiveExecutorState::Failed;
            if (state_ == SummonedQuestState::Idle)
                return ObjectiveExecutorState::Idle;
            return state_ == SummonedQuestState::RoutingToUseLocation
                ? ObjectiveExecutorState::Navigating
                : ObjectiveExecutorState::Executing;
        }

        const char* StateName() const override
        {
            return Name(state_);
        }

        const char* FailureReason() const override
        {
            return failureReason_.empty() ? nullptr : failureReason_.c_str();
        }
    };
}
