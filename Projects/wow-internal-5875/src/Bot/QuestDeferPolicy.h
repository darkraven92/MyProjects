#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string_view>

namespace Bot
{
    enum class QuestDeferReason
    {
        None,
        RepeatedDeaths,
        PersistentNoSafeTarget,
        RepeatedObjectiveFailure
    };

    inline const char* QuestDeferReasonName(QuestDeferReason reason)
    {
        switch (reason)
        {
            case QuestDeferReason::RepeatedDeaths: return "repeated_deaths";
            case QuestDeferReason::PersistentNoSafeTarget: return "persistent_no_safe_target";
            case QuestDeferReason::RepeatedObjectiveFailure: return "repeated_objective_failure";
            default: return "none";
        }
    }

    struct QuestDeathContext
    {
        bool questObjectiveOwned = false;
        bool recentlyActive = false;
        bool lockedQuestTarget = false;
        bool objectiveActionActive = false;
        bool unrelatedDefensiveCombat = false;
        bool otherOwner = false;
    };

    struct QuestRevisitBoundary
    {
        bool objectiveOwned = false;
        bool objectiveStartPending = false;
        bool objectiveRetryPending = false;
        bool discoveryOwned = false;
        bool turnInOwned = false;
    };

    inline bool CanEvaluateQuestRevisit(const QuestRevisitBoundary& boundary)
    {
        return !boundary.objectiveOwned &&
            !boundary.objectiveStartPending &&
            !boundary.objectiveRetryPending &&
            !boundary.discoveryOwned &&
            !boundary.turnInOwned;
    }

    inline bool ShouldAttributeQuestDeath(const QuestDeathContext& context)
    {
        return context.questObjectiveOwned && context.recentlyActive &&
            !context.unrelatedDefensiveCombat && !context.otherOwner &&
            (context.lockedQuestTarget || context.objectiveActionActive);
    }

    inline bool MeaningfulObjectiveFailure(const char* reason)
    {
        if (reason == nullptr)
            return false;
        const std::string_view value(reason);
        return value == "generic NavMesh follower failed." ||
            value == "generic NavMesh route failed to start." ||
            value == "objective execution timeout reached." ||
            value == "all_gameobject_approaches_failed" ||
            value == "live_row_progress_not_observed" ||
            value == "item_use_dispatch_failed" ||
            value == "route_to_use_location_failed" ||
            value == "quest_item_dispatch_failed" ||
            value == "spawn_not_observed_after_bounded_uses" ||
            value == "live_quest_completion_not_observed" ||
            value == "objective_timeout" ||
            value == "CollectWorldItem objective timeout reached." ||
            value == "InteractGameObject objective timeout reached." ||
            value == "InteractGameObject exact seed and bounded reachable search envelope made no progress." ||
            value == "NavMesh could not reach the InteractGameObject seed or any bounded search-envelope approach." ||
            value == "quest did not become complete after bounded exact gameobject interactions." ||
            value == "native gameobject right-click dispatch failed." ||
            value == "UseItemOnUnit objective timeout reached." ||
            value == "NavMesh route to the UseItemOnUnit search seed failed." ||
            value == "FrameScript quest-item use failed.";
    }

    // Session-local, event-driven evidence. No observation is counted merely
    // because the WorldMonitor called the planner again on the next tick.
    class QuestDeferPolicy
    {
    public:
        struct ObjectiveFailureObservation
        {
            int previousCount = 0;
            int newCount = 0;
            std::uint64_t failureGapTicks = 0;
            std::uint64_t idleGapTicks = 0;
            bool expired = false;
            bool duplicateAttempt = false;
        };

        static constexpr int DeathThreshold = 2;
        static constexpr int NoSafeThreshold = 3;
        static constexpr int ObjectiveFailureThreshold = 3;
        static constexpr std::uint64_t NoSafeSampleTicks = 20;
        static constexpr std::uint64_t NoSafeEvidenceWindowTicks = 160;
        static constexpr std::uint64_t DeathEvidenceWindowTicks = 1200;
        static constexpr std::uint64_t FailureEvidenceWindowTicks = 320;
        static constexpr std::uint64_t RevisitCooldownTicks = 160;
        static constexpr std::uint64_t TimeRevisitTicks = 320;

        struct Record
        {
            int deaths = 0;
            int noSafe = 0;
            int objectiveFailures = 0;
            bool deferred = false;
            QuestDeferReason reason = QuestDeferReason::None;
            std::uint64_t lastNoSafeTick = 0;
            std::uint64_t lastDeathTick = 0;
            std::uint64_t lastFailureTick = 0;
            std::optional<std::uint64_t> lastCountedAttemptStartTick{};
            bool safeTargetReturned = false;
            std::uint64_t deferTick = 0;
            int deferLevel = 0;
            int deferCompletedCount = 0;
        };

    private:
        std::map<int, Record> records_{};

        static bool MaybeDefer(Record& record, std::uint64_t tick,
            int level, int completedCount)
        {
            if (record.deferred)
                return false;
            if (record.deaths >= DeathThreshold)
                record.reason = QuestDeferReason::RepeatedDeaths;
            else if (record.noSafe >= NoSafeThreshold)
                record.reason = QuestDeferReason::PersistentNoSafeTarget;
            else if (record.objectiveFailures >= ObjectiveFailureThreshold)
                record.reason = QuestDeferReason::RepeatedObjectiveFailure;
            else
                return false;
            record.deferred = true;
            record.safeTargetReturned = false;
            record.deferTick = tick;
            record.deferLevel = level;
            record.deferCompletedCount = completedCount;
            return true;
        }

    public:
        const Record* Find(int questId) const
        {
            const auto it = records_.find(questId);
            return it == records_.end() ? nullptr : &it->second;
        }

        bool IsDeferred(int questId) const
        {
            const auto* record = Find(questId);
            return record != nullptr && record->deferred;
        }

        bool ObserveDeath(int questId, bool attributed, std::uint64_t tick,
            int level, int completedCount)
        {
            if (questId <= 0 || !attributed)
                return false;
            auto& record = records_[questId];
            if (record.lastDeathTick != 0 &&
                tick - record.lastDeathTick > DeathEvidenceWindowTicks)
                record.deaths = 0;
            record.lastDeathTick = tick;
            record.deaths = std::min(record.deaths + 1, DeathThreshold);
            return MaybeDefer(record, tick, level, completedCount);
        }

        bool ObserveNoSafe(int questId, std::uint64_t tick,
            int level, int completedCount)
        {
            if (questId <= 0)
                return false;
            auto& record = records_[questId];
            if (record.lastNoSafeTick != 0 &&
                tick - record.lastNoSafeTick > NoSafeEvidenceWindowTicks)
                record.noSafe = 0;
            if (record.lastNoSafeTick != 0 &&
                tick - record.lastNoSafeTick < NoSafeSampleTicks)
                return false;
            record.lastNoSafeTick = tick;
            record.noSafe = std::min(record.noSafe + 1, NoSafeThreshold);
            return MaybeDefer(record, tick, level, completedCount);
        }

        bool ObserveObjectiveFailure(int questId, bool meaningful,
            std::uint64_t tick, int level, int completedCount,
            std::optional<std::uint64_t> attemptStartTick = std::nullopt,
            ObjectiveFailureObservation* observation = nullptr)
        {
            if (questId <= 0 || !meaningful)
                return false;
            auto& record = records_[questId];
            ObjectiveFailureObservation result{};
            result.previousCount = record.objectiveFailures;
            result.newCount = result.previousCount;
            if (attemptStartTick && record.lastCountedAttemptStartTick ==
                    attemptStartTick)
            {
                result.duplicateAttempt = true;
                if (observation != nullptr)
                    *observation = result;
                return false;
            }

            if (record.objectiveFailures > 0)
            {
                result.failureGapTicks = tick >= record.lastFailureTick
                    ? tick - record.lastFailureTick
                    : UINT64_MAX;
                // A completed route attempt can exceed the evidence window.
                // Measure inactivity until its start, not time spent actively
                // executing the attempt. Missing/invalid start evidence uses
                // the original failure-to-failure window.
                const bool validAttempt = attemptStartTick &&
                    *attemptStartTick >= record.lastFailureTick &&
                    *attemptStartTick <= tick;
                result.idleGapTicks = validAttempt
                    ? *attemptStartTick - record.lastFailureTick
                    : result.failureGapTicks;
                result.expired = result.idleGapTicks >
                    FailureEvidenceWindowTicks;
            }
            if (result.expired)
                record.objectiveFailures = 0;
            record.lastFailureTick = tick;
            record.objectiveFailures = std::min(record.objectiveFailures + 1,
                ObjectiveFailureThreshold);
            record.lastCountedAttemptStartTick = attemptStartTick;
            result.newCount = record.objectiveFailures;
            if (observation != nullptr)
                *observation = result;
            return MaybeDefer(record, tick, level, completedCount);
        }

        bool ObserveSafeTarget(int questId)
        {
            auto it = records_.find(questId);
            if (it == records_.end() || it->second.noSafe == 0)
                return false;
            it->second.noSafe = 0;
            it->second.lastNoSafeTick = 0;
            it->second.safeTargetReturned = true;
            return true;
        }

        bool ObserveProgress(int questId)
        {
            return questId > 0 && records_.erase(questId) != 0;
        }

        bool RevisitEligible(int questId, std::uint64_t tick, int level,
            int completedCount) const
        {
            const auto* record = Find(questId);
            if (record == nullptr || !record->deferred ||
                tick < record->deferTick + RevisitCooldownTicks)
                return false;
            return level > record->deferLevel ||
                completedCount > record->deferCompletedCount ||
                record->safeTargetReturned ||
                tick >= record->deferTick + TimeRevisitTicks;
        }

        bool Revisit(int questId, std::uint64_t tick, int level,
            int completedCount)
        {
            if (!RevisitEligible(questId, tick, level, completedCount))
                return false;
            auto& record = records_[questId];
            record = Record{}; // fresh evidence required before another defer
            return true;
        }

        void ReconcilePresent(const std::set<int>& presentQuestIds)
        {
            for (auto it = records_.begin(); it != records_.end();)
                if (presentQuestIds.find(it->first) == presentQuestIds.end())
                    it = records_.erase(it);
                else
                    ++it;
        }

        std::set<int> DeferredIds() const
        {
            std::set<int> ids;
            for (const auto& [id, record] : records_)
                if (record.deferred)
                    ids.insert(id);
            return ids;
        }
    };
}
