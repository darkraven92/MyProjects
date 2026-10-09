#pragma once

#include "ClickToMoveController.h"
#include "CollectItemFromMobExecutor.h"
#include "GameObjectApproachPolicy.h"
#include "IObjectiveExecutor.h"
#include "InteractGameObjectExecutor.h"
#include "MultiLocationItemUsePolicy.h"
#include "ObjectiveDefensiveCombatGuard.h"
#include "UseItemOnUnitExecutor.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"

#include <cmath>
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace Bot
{
    enum class MultiLocationItemUseState
    {
        Idle,
        CheckingResource,
        CollectingResource,
        SelectingApproach,
        RoutingToObject,
        Positioning,
        AwaitingProgress,
        ReadyForTurnIn,
        Failed
    };

    // One live leaderboard row at a time. The planner rematerializes the next
    // incomplete row after ReadyForTurnIn, so restart/resume is driven by the
    // quest log rather than an executor-owned nest index.
    class MultiLocationItemUseExecutor final : public IObjectiveExecutor
    {
    private:
        MultiLocationItemUseState state_ = MultiLocationItemUseState::Idle;
        std::string failureReason_{};
        const QuestProfile* profile_ = nullptr;
        QuestProfile collectProfile_{};
        std::unique_ptr<CollectItemFromMobExecutor> collect_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> navigator_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> approachProbe_{};
        std::array<GameObjectApproachPoint,
            GameObjectApproachPolicy::MaximumCandidates> approachCandidates_{};
        GameObjectApproachPoint approachObject_{};
        Objects::PlayerState approachOrigin_{};
        std::uint64_t approachProbeTick_ = 0;
        std::size_t approachCandidateIndex_ = 0;
        bool approachSourceLive_ = false;
        bool approachRefined_ = false;
        Navigation::DirectedPolyTransition failedApproachTransition_{};
        static constexpr std::uint64_t MaximumApproachProbeTicks = 80;
        ObjectiveDefensiveCombatGuard defense_{};
        std::uint64_t startTick_ = 0;
        std::uint64_t stateTick_ = 0;
        std::uint64_t nextUseTick_ = 0;
        int useAttempts_ = 0;
        int stableSamples_ = 0;
        float lastX_ = 0.0f;
        float lastY_ = 0.0f;
        float lastZ_ = 0.0f;

        static const char* Name(MultiLocationItemUseState state)
        {
            switch (state)
            {
                case MultiLocationItemUseState::Idle: return "Idle";
                case MultiLocationItemUseState::CheckingResource: return "CheckingResource";
                case MultiLocationItemUseState::CollectingResource: return "CollectingResource";
                case MultiLocationItemUseState::SelectingApproach: return "SelectingApproach";
                case MultiLocationItemUseState::RoutingToObject: return "RoutingToObject";
                case MultiLocationItemUseState::Positioning: return "Positioning";
                case MultiLocationItemUseState::AwaitingProgress: return "AwaitingProgress";
                case MultiLocationItemUseState::ReadyForTurnIn: return "ReadyForTurnIn";
                case MultiLocationItemUseState::Failed: return "Failed";
            }
            return "Unknown";
        }

        void SetState(MultiLocationItemUseState state, std::uint64_t tick)
        {
            if (state_ == state)
                return;
            state_ = state;
            stateTick_ = tick;
            Debug::Logger::Info(
                "QUEST 16D OBJECTIVE questId=" +
                std::to_string(profile_ == nullptr ? 0 : profile_->questId) +
                " index=" + std::to_string(profile_ == nullptr ? -1 :
                    profile_->activeLeaderboardIndex) +
                " entry=" + std::to_string(profile_ == nullptr ? 0 :
                    profile_->objective.objectEntry) +
                " state=" + Name(state));
        }

        const PlannerQuestLogEntry* FindLiveQuest(
            const QuestPlannerSnapshot& snapshot) const
        {
            if (profile_ == nullptr)
                return nullptr;
            for (const auto& entry : snapshot.quests)
            {
                const auto* matched = ValleyOfTrialsProfiles::Find(
                    entry, snapshot.classToken);
                if (matched != nullptr && matched->questId == profile_->questId)
                    return &entry;
            }
            return nullptr;
        }

        void StopNavigation(const Objects::PlayerState& player)
        {
            approachProbe_.reset();
            if (navigator_ != nullptr)
            {
                navigator_->PauseForCombat(player, "item-at-gameobject handoff");
                navigator_.reset();
            }
        }

        void Fail(const char* reason, const Objects::WorldState& world,
            CombatController& combat, std::uint64_t tick)
        {
            failureReason_ = reason;
            StopNavigation(world.player);
            if (collect_ != nullptr)
                combat.ClearPlannerQuestTarget();
            collect_.reset();
            Debug::Logger::Info(
                "QUEST 16D OBJECTIVE questId=" +
                std::to_string(profile_ == nullptr ? 0 : profile_->questId) +
                " result=failed reason=" + reason +
                " useAttempts=" + std::to_string(useAttempts_));
            SetState(MultiLocationItemUseState::Failed, tick);
        }

        bool StartCollect(const Objects::WorldState& world,
            CombatController& combat, std::uint64_t tick)
        {
            const int index = MultiLocationItemUsePolicy::NearestResourceSource(
                *profile_, world.player.x, world.player.y);
            if (index < 0)
            {
                Fail("no_valid_resource_source", world, combat, tick);
                return false;
            }
            StopNavigation(world.player);
            const auto& source = profile_->questResourceSources[
                static_cast<std::size_t>(index)];
            collectProfile_ = *profile_;
            collectProfile_.objective = {
                QuestObjectiveType::CollectItemFromMob,
                source.creatureEntry, profile_->questUseItemId,
                0, 1, source.creatureName};
            collectProfile_.destination = source.destination;
            collectProfile_.activeLeaderboardIndex = -1;
            collect_ = std::make_unique<CollectItemFromMobExecutor>();
            Debug::Logger::Info(
                "QUEST 16D RESOURCE questId=" +
                std::to_string(profile_->questId) +
                " itemId=" + std::to_string(profile_->questUseItemId) +
                " present=no state=collecting sourceEntry=" +
                std::to_string(source.creatureEntry));
            if (!collect_->Start(collectProfile_, world, combat, tick))
            {
                Fail("resource_collector_start_failed", world, combat, tick);
                return false;
            }
            SetState(MultiLocationItemUseState::CollectingResource, tick);
            return true;
        }

        bool StartRoute(const Objects::WorldState& world,
            CombatController& combat, std::uint64_t tick)
        {
            const auto& destination = profile_->destination;
            InteractGameObjectExecutor::ObservedGameObject live{};
            const bool observed =
                InteractGameObjectExecutor::ObserveMatchingObject(
                    profile_->objective.objectEntry, world.player, live) &&
                std::hypot(live.x - destination.x, live.y - destination.y,
                    live.z - destination.z) <=
                    MultiLocationItemUsePolicy::AnchorIdentityDistance;
            approachObject_ = observed
                ? GameObjectApproachPoint{live.x, live.y, live.z}
                : GameObjectApproachPoint{
                    destination.x, destination.y, destination.z};
            if (!GameObjectApproachPolicy::Finite(approachObject_))
            {
                Fail("invalid_gameobject_anchor", world, combat, tick);
                return false;
            }
            approachSourceLive_ = observed;
            if (observed && MultiLocationItemUsePolicy::MatchesGameObject(
                    *profile_, live.entry, live.x, live.y, live.z,
                    live.distance))
            {
                StopNavigation(world.player);
                if (!ClickToMoveController::MoveTo(world.player,
                        world.player.x, world.player.y, world.player.z, 0.25f))
                {
                    Fail("stop_at_object_failed", world, combat, tick);
                    return false;
                }
                stableSamples_ = 0;
                lastX_ = world.player.x;
                lastY_ = world.player.y;
                lastZ_ = world.player.z;
                SetState(MultiLocationItemUseState::Positioning, tick);
                return true;
            }
            approachOrigin_ = world.player;
            approachCandidates_ = GameObjectApproachPolicy::Generate(
                {world.player.x, world.player.y, world.player.z},
                approachObject_);
            approachCandidateIndex_ = 0;
            approachProbe_.reset();
            navigator_.reset();
            Debug::Logger::Info(
                "QUEST GO APPROACH entry=" +
                std::to_string(profile_->objective.objectEntry) +
                " source=" + (observed ? "live" : "static") +
                " candidateCount=" +
                std::to_string(approachCandidates_.size()));
            SetState(MultiLocationItemUseState::SelectingApproach, tick);
            return true;
        }

        void UpdateApproach(const Objects::WorldState& world,
            CombatController& combat, std::uint64_t tick)
        {
            if (approachCandidateIndex_ >= approachCandidates_.size())
            {
                Debug::Logger::Info(
                    "QUEST GO APPROACH FAIL entry=" +
                    std::to_string(profile_->objective.objectEntry) +
                    " reason=no_reachable_interaction_point");
                Fail("all_gameobject_approaches_failed", world, combat, tick);
                return;
            }
            const std::size_t index = approachCandidateIndex_;
            const auto candidate = approachCandidates_[index];
            if (approachProbe_ == nullptr)
            {
                approachProbe_ = std::make_unique<
                    Navigation::GenericNavMeshPathFollower>();
                approachProbeTick_ = tick;
                const Navigation::GenericNavMeshStartOptions options{
                    false, true, failedApproachTransition_};
                // Bounded route/expanded, planning only. A proven directed
                // failure is carried to the existing 13D.4 alternative query.
                if (!approachProbe_->Start(approachOrigin_, tick,
                        {candidate.x, candidate.y, candidate.z},
                        profile_->destination.mapId,
                        GameObjectApproachPolicy::ArrivalDistance,
                        "gameobject interaction approach probe", true,
                        options))
                {
                    approachProbe_.reset();
                    ++approachCandidateIndex_;
                }
                return;
            }
            approachProbe_->Update(approachOrigin_, tick);
            const auto result = approachProbe_->PlanningOnlyResult();
            if (result.status == Navigation::RouteCostProbeStatus::Pending &&
                tick - approachProbeTick_ < MaximumApproachProbeTicks)
                return;
            const auto projectedNav =
                approachProbe_->PlanningOnlyProjectedDestination();
            const bool accepted = result.status ==
                    Navigation::RouteCostProbeStatus::Reachable &&
                GameObjectApproachPolicy::AcceptProjection(
                    approachObject_, candidate,
                    {projectedNav.x, projectedNav.y, projectedNav.z},
                    approachProbe_->PlanningOnlyReachedDestination()) &&
                !approachProbe_->PlanningOnlyCorridorContainsTransition(
                    failedApproachTransition_);
            const char* routeResult = result.status ==
                    Navigation::RouteCostProbeStatus::Reachable
                ? "reachable" : result.status ==
                    Navigation::RouteCostProbeStatus::Unreachable
                ? "unreachable" : "timeout";
            Debug::Logger::Info(
                "QUEST GO APPROACH CANDIDATE entry=" +
                std::to_string(profile_->objective.objectEntry) +
                " index=" + std::to_string(index) +
                " distanceToObject=" + std::to_string(
                    GameObjectApproachPolicy::CandidateRadius) +
                " routeResult=" + routeResult +
                " terrainValid=" + (accepted ? "yes" : "no"));
            approachProbe_.reset();
            ++approachCandidateIndex_;
            if (!accepted)
                return;
            navigator_ = std::make_unique<
                Navigation::GenericNavMeshPathFollower>();
            if (!navigator_->Start(world.player, tick,
                    {candidate.x, candidate.y, candidate.z},
                    profile_->destination.mapId,
                    GameObjectApproachPolicy::ArrivalDistance,
                    "gameobject reachable interaction approach", true,
                    {true, false, failedApproachTransition_}))
            {
                navigator_.reset();
                return;
            }
            Debug::Logger::Info(
                "QUEST GO APPROACH SELECT entry=" +
                std::to_string(profile_->objective.objectEntry) +
                " index=" + std::to_string(index) +
                " distanceToObject=" + std::to_string(
                    GameObjectApproachPolicy::CandidateRadius) +
                " routeLength=" + std::to_string(result.pathLength));
            SetState(MultiLocationItemUseState::RoutingToObject, tick);
        }

        void Complete(const Objects::WorldState& world,
            CombatController& combat, std::uint64_t tick)
        {
            StopNavigation(world.player);
            if (collect_ != nullptr)
                combat.ClearPlannerQuestTarget();
            collect_.reset();
            Debug::Logger::Info(
                "QUEST 16D PROGRESS questId=" +
                std::to_string(profile_->questId) +
                " objectiveIndex=" +
                std::to_string(profile_->activeLeaderboardIndex) +
                " before=no after=yes source=live_quest_log");
            SetState(MultiLocationItemUseState::ReadyForTurnIn, tick);
        }

    public:
        bool Supports(const QuestProfile& profile) const override
        {
            return MultiLocationItemUsePolicy::Supports(profile);
        }

        bool Start(const QuestProfile& profile,
            const Objects::WorldState& world, CombatController&,
            std::uint64_t tick) override
        {
            if (state_ != MultiLocationItemUseState::Idle ||
                !Supports(profile) || !world.player.valid ||
                world.player.health == 0)
                return false;
            profile_ = &profile;
            startTick_ = tick;
            nextUseTick_ = tick;
            approachRefined_ = false;
            failedApproachTransition_ = {};
            lastX_ = world.player.x;
            lastY_ = world.player.y;
            lastZ_ = world.player.z;
            defense_.Reset(world, "item-at-gameobject quest " +
                std::to_string(profile.questId));
            SetState(MultiLocationItemUseState::CheckingResource, tick);
            return true;
        }

        void Update(const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world, CombatController& combat,
            std::uint64_t tick) override
        {
            if (state_ == MultiLocationItemUseState::Idle ||
                state_ == MultiLocationItemUseState::ReadyForTurnIn ||
                state_ == MultiLocationItemUseState::Failed)
                return;
            const auto* live = FindLiveQuest(snapshot);
            if (live == nullptr ||
                profile_->activeLeaderboardIndex >= live->objectiveCount ||
                static_cast<std::size_t>(profile_->activeLeaderboardIndex) >=
                    live->objectiveComplete.size())
            {
                Fail("live_objective_identity_unavailable", world, combat, tick);
                return;
            }
            if (MultiLocationItemUsePolicy::ProgressConfirmed(*live, *profile_))
            {
                Complete(world, combat, tick);
                return;
            }
            if (world.player.health == 0 || !world.player.valid ||
                tick - startTick_ >= MultiLocationItemUsePolicy::MaximumObjectiveTicks)
            {
                Fail(world.player.health == 0 ? "player_dead" :
                    "objective_timeout", world, combat, tick);
                return;
            }

            std::uint32_t itemCount = 0;
            if (!UseItemOnUnitExecutor::CountQuestItemById(
                    profile_->questUseItemId, itemCount))
            {
                // Item identity/count unavailable is not authoritative zero.
                if (tick - stateTick_ >= 40)
                    Fail("item_inventory_unavailable", world, combat, tick);
                return;
            }

            if (state_ == MultiLocationItemUseState::CollectingResource)
            {
                if (itemCount > 0 &&
                    collect_->ReleaseAfterResourceAcquired(world.player, combat))
                {
                    collect_.reset();
                    Debug::Logger::Info(
                        "QUEST 16D RESOURCE questId=" +
                        std::to_string(profile_->questId) +
                        " itemId=" + std::to_string(profile_->questUseItemId) +
                        " present=yes count=" + std::to_string(itemCount) +
                        " state=ready");
                    StartRoute(world, combat, tick);
                    return;
                }
                collect_->Update(snapshot, world, combat, tick);
                if (collect_->State() == ObjectiveExecutorState::Failed)
                    Fail("resource_collection_failed", world, combat, tick);
                return;
            }

            if (itemCount == 0 &&
                state_ != MultiLocationItemUseState::AwaitingProgress)
            {
                StartCollect(world, combat, tick);
                return;
            }

            const auto defenseResult = defense_.Update(
                world, combat, navigator_.get(), 0, tick);
            if (defenseResult == ObjectiveDefenseUpdate::Failed)
            {
                Fail("defensive_combat_failed", world, combat, tick);
                return;
            }
            if (defenseResult == ObjectiveDefenseUpdate::OwnsControl)
                return;

            if (state_ == MultiLocationItemUseState::CheckingResource)
            {
                StartRoute(world, combat, tick);
                return;
            }

            if (state_ == MultiLocationItemUseState::SelectingApproach)
            {
                UpdateApproach(world, combat, tick);
                return;
            }

            if (state_ == MultiLocationItemUseState::RoutingToObject)
            {
                InteractGameObjectExecutor::ObservedGameObject live{};
                if (!approachSourceLive_ && !approachRefined_ &&
                    InteractGameObjectExecutor::ObserveMatchingObject(
                        profile_->objective.objectEntry, world.player, live) &&
                    std::hypot(live.x - profile_->destination.x,
                        live.y - profile_->destination.y,
                        live.z - profile_->destination.z) <=
                        MultiLocationItemUsePolicy::AnchorIdentityDistance &&
                    GameObjectApproachPolicy::ShouldRefine(
                        true, false, approachObject_,
                        {live.x, live.y, live.z}))
                {
                    Debug::Logger::Info(
                        "QUEST GO APPROACH REFINE entry=" +
                        std::to_string(profile_->objective.objectEntry) +
                        " oldSource=static newSource=live");
                    approachRefined_ = true;
                    StopNavigation(world.player);
                    StartRoute(world, combat, tick);
                    return;
                }
                navigator_->Update(world.player, tick);
                if (navigator_->Failed())
                {
                    const auto evidence = navigator_->FailureEvidence();
                    if (evidence.failedTransition.Valid())
                        failedApproachTransition_ = evidence.failedTransition;
                    else if (evidence.learnedTransition.Valid())
                        failedApproachTransition_ = evidence.learnedTransition;
                    StopNavigation(world.player);
                    // The failed leg may have made real progress. Subsequent
                    // probes must start from the current physical position,
                    // while retaining the bounded candidate index.
                    approachOrigin_ = world.player;
                    approachCandidates_ = GameObjectApproachPolicy::Generate(
                        {world.player.x, world.player.y, world.player.z},
                        approachObject_);
                    SetState(MultiLocationItemUseState::SelectingApproach, tick);
                    return;
                }
                if (navigator_->Arrived())
                {
                    if (!ClickToMoveController::MoveTo(world.player,
                            world.player.x, world.player.y, world.player.z, 0.25f))
                    {
                        Fail("stop_at_object_failed", world, combat, tick);
                        return;
                    }
                    navigator_.reset();
                    stableSamples_ = 0;
                    lastX_ = world.player.x;
                    lastY_ = world.player.y;
                    lastZ_ = world.player.z;
                    SetState(MultiLocationItemUseState::Positioning, tick);
                }
                return;
            }

            if (state_ == MultiLocationItemUseState::AwaitingProgress)
            {
                if (tick - stateTick_ < MultiLocationItemUsePolicy::ProgressWaitTicks)
                    return;
                if (useAttempts_ >= MultiLocationItemUsePolicy::MaximumUseAttempts)
                {
                    Fail("live_row_progress_not_observed", world, combat, tick);
                    return;
                }
                stableSamples_ = 0;
                SetState(MultiLocationItemUseState::Positioning, tick);
                return;
            }

            if (state_ != MultiLocationItemUseState::Positioning)
                return;
            InteractGameObjectExecutor::ObservedGameObject object{};
            if (!InteractGameObjectExecutor::ObserveMatchingObject(
                    profile_->objective.objectEntry, world.player, object))
            {
                if (tick - stateTick_ >= 40)
                {
                    approachOrigin_ = world.player;
                    SetState(MultiLocationItemUseState::SelectingApproach, tick);
                }
                return;
            }
            if (!approachSourceLive_ && !approachRefined_ &&
                std::hypot(object.x - profile_->destination.x,
                    object.y - profile_->destination.y,
                    object.z - profile_->destination.z) <=
                    MultiLocationItemUsePolicy::AnchorIdentityDistance &&
                GameObjectApproachPolicy::ShouldRefine(
                    true, false, approachObject_,
                    {object.x, object.y, object.z}))
            {
                Debug::Logger::Info(
                    "QUEST GO APPROACH REFINE entry=" +
                    std::to_string(profile_->objective.objectEntry) +
                    " oldSource=static newSource=live");
                approachRefined_ = true;
                StartRoute(world, combat, tick);
                return;
            }
            if (!MultiLocationItemUsePolicy::MatchesGameObject(*profile_,
                    object.entry, object.x, object.y, object.z,
                    object.distance))
            {
                if (tick - stateTick_ >= 40)
                {
                    approachOrigin_ = world.player;
                    SetState(MultiLocationItemUseState::SelectingApproach, tick);
                }
                return;
            }

            const float movement = std::hypot(
                world.player.x - lastX_, world.player.y - lastY_,
                world.player.z - lastZ_);
            stableSamples_ = movement <=
                MultiLocationItemUsePolicy::StableDisplacement
                ? stableSamples_ + 1 : 0;
            lastX_ = world.player.x;
            lastY_ = world.player.y;
            lastZ_ = world.player.z;
            const bool combatIdle = combat.LockedGuid() == 0 &&
                (combat.State() == CombatState::Idle ||
                 combat.State() == CombatState::AcquiringTarget);
            if (!MultiLocationItemUsePolicy::MayIssueUse(
                    true, true, world.player.health > 0, combatIdle,
                    itemCount > 0, true, stableSamples_, useAttempts_,
                    tick, nextUseTick_))
                return;

            std::string useResult;
            if (!UseItemOnUnitExecutor::UseQuestItemById(
                    profile_->questUseItemId, useResult))
            {
                Fail("item_use_dispatch_failed", world, combat, tick);
                return;
            }
            if (useResult.rfind("used|", 0) != 0)
            {
                SetState(MultiLocationItemUseState::CheckingResource, tick);
                return;
            }
            // Local quest rows bind an item spell to a specific GO entry.
            // Revalidate exact entry/GUID/range before the existing GO click;
            // runtime must still confirm that this dispatch targets the GO.
            // Neither dispatch is interpreted as quest credit.
            const bool objectDispatch =
                InteractGameObjectExecutor::InteractMatchingObject(
                    object, world.player);
            ++useAttempts_;
            nextUseTick_ = tick + profile_->questUseMinimumIntervalTicks;
            Debug::Logger::Info(
                "QUEST 16D ITEM USE questId=" +
                std::to_string(profile_->questId) +
                " itemId=" + std::to_string(profile_->questUseItemId) +
                " objectiveIndex=" +
                std::to_string(profile_->activeLeaderboardIndex) +
                " issued=" + (objectDispatch ? "yes" : "item_only") +
                " attempt=" + std::to_string(useAttempts_));
            SetState(MultiLocationItemUseState::AwaitingProgress, tick);
        }

        bool OwnsControl() const override
        {
            return state_ != MultiLocationItemUseState::Idle &&
                state_ != MultiLocationItemUseState::Failed;
        }

        ObjectiveExecutorState State() const override
        {
            if (state_ == MultiLocationItemUseState::Idle)
                return ObjectiveExecutorState::Idle;
            if (state_ == MultiLocationItemUseState::Failed)
                return ObjectiveExecutorState::Failed;
            if (state_ == MultiLocationItemUseState::ReadyForTurnIn)
                return ObjectiveExecutorState::ReadyForTurnIn;
            if (state_ == MultiLocationItemUseState::RoutingToObject ||
                state_ == MultiLocationItemUseState::SelectingApproach)
                return ObjectiveExecutorState::Navigating;
            return ObjectiveExecutorState::Executing;
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
