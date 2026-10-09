#pragma once
#include "EquipmentUpgradeController.h"
#include "ClassTrainerController.h"
#include "IdleActivityPolicy.h"

#include "CombatController.h"
#include "GenericQuestTurnInExecutor.h"
#include "GenericQuestDiscoveryController.h"
#include "MovementController.h"
#include "ObjectiveExecutionDirector.h"
#include "PersistentQuestState.h"
#include "QuestPlanner.h"
#include "QuestDeferPolicy.h"
#include "QuestClassificationPolicy.h"
#include "QuestPlannerStateReader.h"
#include "ValleyOfTrialsProfiles.h"
#include "VanillaQuestDatabase.h"
#include "RegionalQuestRelocationController.h"
#include "QuestMaintenanceController.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <set>
#include <string>

namespace Bot
{
    class QuestPlannerRuntimeController
    {
    private:
        bool initialized_ =
            false;

        // Zero retains the existing multi-quest mode. A positive value is
        // an explicit single-quest diagnostic scope, never a hardcoded route.
        int focusQuestId_ = 0;

        bool readerValid_ =
            false;

        bool haveSnapshot_ =
            false;

        bool deathSuspended_ = false;
        bool resumeAfterDeathPending_ = false;
        int suspendedQuestId_ = 0;
        QuestDeferPolicy difficulty_{};
        std::uint64_t plannerTick_ = 0;
        std::uint64_t diagnosticGeneration_ = 0;
        std::uint64_t lastObjectiveOwnedTick_ = 0;
        std::uint64_t lastQuestLockedGuid_ = 0;
        std::uint64_t lastQuestLockTick_ = 0;
        int lastQuestLockQuestId_ = 0;
        std::uint64_t lastPullEvidenceTick_ = 0;
        bool objectiveFailureEvidenceRecorded_ = false;
        std::optional<std::uint64_t> objectiveAttemptStartedTick_{};

        QuestPlannerSnapshot snapshot_{};

        QuestPlannerPlan plan_{};

        ObjectiveExecutionDirector
            objectiveDirector_{};

        bool startAttempted_ =
            false;

        bool readyForTurnInLogged_ =
            false;

        // Phase 11D.2.6: failed objectives must not become permanent idle
        // ownership. Retry quickly a few times, then keep a bounded slower
        // retry cadence while the same local quest remains active.
        std::uint64_t objectiveRetryAfterTick_ = 0;
        int objectiveRetryCount_ = 0;
        int objectiveRetryQuestId_ = 0;

        static constexpr std::uint64_t ObjectiveFastRetryDelayTicks = 8;
        static constexpr std::uint64_t ObjectiveSlowRetryDelayTicks = 24;
        static constexpr int ObjectiveFastRetryLimit = 3;

        GenericQuestTurnInExecutor
            turnInExecutor_{};

        const QuestProfile* turnInProfile_ =
            nullptr;

        bool turnInStartAttempted_ =
            false;

        GenericQuestDiscoveryController
            discoveryController_{};

        bool discoveryStarted_ =
            false;

        RegionalQuestRelocationController relocation_{};
        QuestMaintenanceController maintenance_{};
        EquipmentUpgradeController equipment_{};
        ClassTrainerController trainer_{};
        IdleActivityPolicy idleActivity_{};
        float idleX_=0, idleY_=0, idleZ_=0;
        bool idleReported_=false;
        std::uint64_t nextRelocationDecisionTick_ = 0;

        bool valleyDiscoverySweepComplete_ =
            false;

        // Phase 13B.1: once the accepted wave has no unfinished objective,
        // turn in every completed member before opening discovery for the
        // next follow-up wave.
        bool batchTurnInMode_ =
            false;

        // Phase 13D.5.1: the first discovery pass of every pickup wave must
        // be a fresh local-hub reconciliation. The persistent giver ledger is
        // an optimization, not authority: a giver that was empty earlier can
        // gain an offer after a level-up/turn-in, and older Phase 13B.1 builds
        // could incorrectly cache givers after accepting only one quest.
        bool freshPickupWaveAudit_ = true;

        // Phase 13D.7.1: distinguish a DLL/process restart in the middle of
        // an already accepted wave from a genuinely new pickup wave.  The
        // persisted giver ledger is the restart frontier; same-session batch
        // turn-in still explicitly opens a fresh reconciliation below.
        bool startupPickupPolicyResolved_ = false;

        std::uint64_t discoveryRetryAfterTick_ = 0;
        int discoveryRetryCount_ = 0;
        std::uint64_t nextDiscoveryAuditTick_ = 0;
        int lastDiscoveryLevel_ = 0;
        static constexpr std::uint64_t DiscoveryRetryDelayTicks = 12;

        // Phase 13C.2 bounded work quarantine. A repeatedly failing objective
        // is temporarily removed from wave selection so one bad executor/path
        // cannot deadlock the whole character.
        std::set<int> temporarilyBlockedQuestIds_{};
        std::uint64_t blockedWorkReleaseTick_ = 0;
        static constexpr int MaximumObjectiveRetriesPerWave = 6;
        static constexpr std::uint64_t BlockedObjectiveRetryDelayTicks = 160;

        // Phase 13C.2 high-level C++ exception containment + diagnostic
        // context. Native access violations are not guaranteed to become C++
        // exceptions; the activity/context markers still narrow the last
        // subsystem entered before such a process crash.
        std::string runtimeActivity_ = "Idle";
        std::uint64_t runtimeFaultBackoffUntilTick_ = 0;
        int containedRuntimeFaultCount_ = 0;
        static constexpr std::uint64_t RuntimeFaultBackoffTicks = 40;

        // Phase 12B.8: long-lived quest/giver audit ledger for this injected
        // bot session. A fresh discovery sweep must not walk back through every
        // already-verified giver after each turn-in or after a bounded retry.
        std::set<int> completedQuestIds_{};
        std::set<std::uint32_t> checkedGiverEntriesLedger_{};

        // Phase 12B.9: completion/audit history survives WoW and DLL restarts.
        // Completed quest IDs persist permanently per player GUID. Giver audit
        // entries persist only for the current level and are invalidated on
        // level-up so newly level-gated offers can be discovered.
        PersistentQuestState persistentQuestState_{};

        void LogDifficultyEvidence(int questId, const char* source,
            const QuestDeferPolicy::ObjectiveFailureObservation* failure = nullptr) const
        {
            const auto* record = difficulty_.Find(questId);
            if (record == nullptr)
                return;
            Debug::Logger::Info(
                "QUEST DIFFICULTY EVIDENCE quest=" + std::to_string(questId) +
                " source=" + source +
                " deathCount=" + std::to_string(record->deaths) +
                " noSafeCount=" + std::to_string(record->noSafe) +
                " objectiveFailures=" +
                    std::to_string(record->objectiveFailures) +
                (failure == nullptr ? "" :
                    " reason=repeated_objective_failure previousCount=" +
                    std::to_string(failure->previousCount) +
                    " newCount=" + std::to_string(failure->newCount) +
                    " failureTick=" + std::to_string(plannerTick_) +
                    " attemptStartTick=" +
                    (objectiveAttemptStartedTick_
                        ? std::to_string(*objectiveAttemptStartedTick_)
                        : std::string("unknown")) +
                    " failureGapTicks=" +
                    std::to_string(failure->failureGapTicks) +
                    " idleGapTicks=" +
                    std::to_string(failure->idleGapTicks) +
                    " duplicateAttempt=" +
                    (failure->duplicateAttempt ? "yes" : "no")));
        }

        void LogDefer(int questId) const
        {
            const auto* record = difficulty_.Find(questId);
            if (record == nullptr || !record->deferred)
                return;
            Debug::Logger::Info(
                "QUEST DEFER quest=" + std::to_string(questId) +
                " reason=" + QuestDeferReasonName(record->reason) +
                " deathCount=" + std::to_string(record->deaths) +
                " noSafeCount=" + std::to_string(record->noSafe) +
                " objectiveFailures=" +
                    std::to_string(record->objectiveFailures) +
                " revisitAfter=" +
                    std::to_string(record->deferTick +
                        QuestDeferPolicy::RevisitCooldownTicks) +
                " focusMode=" + (focusQuestId_ > 0 ? "yes" : "no"));
        }

        bool ObservePullDifficulty(CombatController& combat,
            std::uint64_t tick)
        {
            const auto* profile = objectiveDirector_.ActiveProfile();
            if (profile == nullptr || !combat.PlannerQuestTargetActive() ||
                combat.DesiredQuestEntry() == 0 ||
                combat.LastPullEvaluationQuestEntry() !=
                    combat.DesiredQuestEntry())
                return false;
            const auto evaluationTick = combat.LastPullEvaluationTick();
            if (evaluationTick == 0 || evaluationTick <= lastPullEvidenceTick_ ||
                evaluationTick > tick || tick - evaluationTick > 1)
                return false;
            lastPullEvidenceTick_ = evaluationTick;
            if (combat.LastPullDecision() == PullSafetyDecision::Voluntary)
            {
                if (difficulty_.ObserveSafeTarget(profile->questId))
                    Debug::Logger::Info(
                        "QUEST DIFFICULTY EVIDENCE quest=" +
                        std::to_string(profile->questId) +
                        " source=safe_target_available noSafeCount=0");
                return false;
            }
            if (combat.LastPullDecision() != PullSafetyDecision::NoSafeCandidate)
                return false;
            const auto* before = difficulty_.Find(profile->questId);
            const int previousCount = before == nullptr ? 0 : before->noSafe;
            const bool deferred = difficulty_.ObserveNoSafe(profile->questId,
                evaluationTick, snapshot_.playerLevel,
                static_cast<int>(completedQuestIds_.size()));
            const auto* after = difficulty_.Find(profile->questId);
            if (after != nullptr && after->noSafe != previousCount)
            {
                LogDifficultyEvidence(profile->questId, "no_safe_candidate");
                if (after->noSafe == 1)
                    Debug::Logger::Info(
                        "QUEST TEMPORARILY BLOCKED quest=" +
                        std::to_string(profile->questId) +
                        " reason=no_safe_candidate");
            }
            if (!deferred)
                return false;
            LogDefer(profile->questId);
            combat.ClearPlannerQuestTarget();
            objectiveDirector_.ReleaseActive();
            startAttempted_ = false;
            objectiveFailureEvidenceRecorded_ = false;
            objectiveRetryAfterTick_ = 0;
            objectiveRetryCount_ = 0;
            objectiveRetryQuestId_ = 0;
            RefreshPlanner();
            return true;
        }

        void PersistLedger(const char* reason)
        {
            if (!haveSnapshot_)
                return;
            persistentQuestState_.SaveIfChanged(
                snapshot_.playerLevel,
                completedQuestIds_,
                checkedGiverEntriesLedger_,
                reason);
        }

        void SetRuntimeActivity(const char* activity)
        {
            runtimeActivity_ = activity == nullptr ? "Unknown" : activity;
        }

        void LogRuntimeFailureContext(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const std::string& reason)
        {
            const int primaryQuestId =
                plan_.primary == nullptr ? 0 : plan_.primary->questId;
            const char* primaryTitle =
                plan_.primary == nullptr || plan_.primary->title == nullptr
                    ? ""
                    : plan_.primary->title;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUESTDB 13C.2: CONTAINED RUNTIME FAULT");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info(
                "tick=" + std::to_string(tick) +
                " activity=" + runtimeActivity_ +
                " plannerState=" + std::string(StateName()) +
                " action=" + std::string(QuestPlanner::ActionName(plan_.action)));
            Debug::Logger::Info(
                "questId=" + std::to_string(primaryQuestId) +
                " title=\"" + std::string(primaryTitle) + "\"" +
                " objectiveIndex=" + std::to_string(plan_.activeObjectiveIndex) +
                " executorState=" + std::string(objectiveDirector_.StateName()) +
                " turnInQuestId=" +
                std::to_string(turnInProfile_ == nullptr ? 0 : turnInProfile_->questId));
            Debug::Logger::Info(
                "player=(" + std::to_string(world.player.x) + "," +
                std::to_string(world.player.y) + "," +
                std::to_string(world.player.z) + ") hp=" +
                std::to_string(world.player.health) + "/" +
                std::to_string(world.player.maxHealth) + " power=" +
                std::to_string(world.player.power) + "/" +
                std::to_string(world.player.maxPower));
            Debug::Logger::Info(
                "wave active=" + std::to_string(plan_.waveActiveQuestCount) +
                " incomplete=" + std::to_string(plan_.waveIncompleteCount) +
                " ready=" + std::to_string(plan_.waveReadyForTurnInCount) +
                " blocked=" + std::to_string(plan_.waveBlockedCount) +
                " containedFaults=" + std::to_string(containedRuntimeFaultCount_ + 1));
            Debug::Logger::Info("================================");
        }

        void ContainRuntimeFault(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            const std::string& reason)
        {
            LogRuntimeFailureContext(world, tick, reason);
            ++containedRuntimeFaultCount_;
            idleActivity_=IdleActivityPolicy{};
            if (relocation_.Active() || maintenance_.Active() || trainer_.Active())
            {
                MovementController::HoldPosition(world.player);
                relocation_.Cancel();
                maintenance_.Cancel();
                trainer_.Cancel();
            }

            if (plan_.primary != nullptr && plan_.primary->questId > 0)
            {
                temporarilyBlockedQuestIds_.insert(plan_.primary->questId);
                blockedWorkReleaseTick_ = tick + BlockedObjectiveRetryDelayTicks;
            }

            combat.ClearPlannerQuestTarget();
            objectiveDirector_.ReleaseActive();
            objectiveFailureEvidenceRecorded_ = false;
            turnInExecutor_ = GenericQuestTurnInExecutor{};
            turnInProfile_ = nullptr;
            turnInStartAttempted_ = false;
            discoveryController_ = GenericQuestDiscoveryController{};
            discoveryStarted_ = false;
            discoveryRetryAfterTick_ = 0;
            discoveryRetryCount_ = 0;
            startAttempted_ = false;
            readyForTurnInLogged_ = false;
            objectiveRetryAfterTick_ = 0;
            objectiveRetryCount_ = 0;
            objectiveRetryQuestId_ = 0;
            runtimeFaultBackoffUntilTick_ = tick + RuntimeFaultBackoffTicks;
            runtimeActivity_ = "ContainedFaultBackoff";
        }

        void MergeDiscoveryLedger()
        {
            const auto before = checkedGiverEntriesLedger_.size();
            const auto& checked = discoveryController_.CheckedGiverEntries();
            checkedGiverEntriesLedger_.insert(checked.begin(), checked.end());
            if (checkedGiverEntriesLedger_.size() != before)
                PersistLedger("giver-audit checkpoint");
        }

        void RecordCompletedQuest(const QuestProfile& profile)
        {
            if (difficulty_.ObserveProgress(profile.questId))
                Debug::Logger::Info(
                    "QUEST DEFER CLEARED quest=" +
                    std::to_string(profile.questId) +
                    " reason=verified_turnin_and_log_removal");
            completedQuestIds_.insert(profile.questId);
            if (focusQuestId_ > 0)
                Debug::Logger::Info(
                    "QUEST 16A COMPLETE questId=" +
                    std::to_string(profile.questId) +
                    " result=reward_action_and_log_removal_verified");

            // The NPC receiving a completed quest is the most likely place for
            // the next chain step. Re-audit that one giver only; keep every
            // other verified-empty giver cached.
            if (profile.turnInEntry != 0)
                checkedGiverEntriesLedger_.erase(profile.turnInEntry);

            // Phase 13B: a zone-wide catalogue means a future chain giver may
            // have been checked before its prerequisite was complete. VMaNGOS
            // chain metadata is therefore used as a soft invalidation hint:
            // it never fabricates availability, it only makes the real live
            // quest dialog eligible to be checked again.
            int chainGiversInvalidated = 0;
            for (const auto& candidate : ValleyOfTrialsProfiles::All())
            {
                bool followsCompleted =
                    candidate.previousQuestId == profile.questId ||
                    candidate.previousQuestId == -profile.questId ||
                    candidate.breadcrumbForQuestId == profile.questId ||
                    (profile.nextInChainQuestId != 0 &&
                     candidate.questId == profile.nextInChainQuestId);
                for(const auto& clause:candidate.prerequisiteAlternatives)
                    followsCompleted |= std::find(clause.allOf.begin(),clause.allOf.end(),profile.questId)!=clause.allOf.end();

                if (!followsCompleted || candidate.giverEntry == 0)
                    continue;

                chainGiversInvalidated += static_cast<int>(
                    checkedGiverEntriesLedger_.erase(candidate.giverEntry));
                QuestEligibilityContext context;
                context.level=snapshot_.playerLevel;
                context.classMask=QuestEligibilityPolicy::ClassMask(snapshot_.classToken);
                context.raceMask=QuestAcquisitionPolicy::RaceMask(snapshot_.raceToken);
                context.completed=completedQuestIds_;
                context.activeHistoryComplete=snapshot_.valid;
                for(const auto& live:snapshot_.quests)
                {
                    const auto* active=ValleyOfTrialsProfiles::Find(live,snapshot_.classToken);
                    if(active) context.activeQuestIds.insert(active->questId);
                    else context.activeHistoryComplete=false;
                }
                const auto eligibility=QuestAcquisitionPolicy::Evaluate(candidate,
                    ValleyOfTrialsProfiles::Graph().Find(candidate.questId),context);
                Debug::Logger::Info("QUEST CHAIN ADVANCE completedQuest="+std::to_string(profile.questId)+
                    " candidateQuest="+std::to_string(candidate.questId)+" eligibility="+QuestAcquisitionPolicy::Name(eligibility)+
                    " action=reaudit_only_live_offer_required");
            }

            Debug::Logger::Info(
                "QUESTDB 13B: COMPLETION LEDGER questId=" +
                std::to_string(profile.questId) +
                " completedCount=" + std::to_string(completedQuestIds_.size()) +
                " cachedGivers=" + std::to_string(checkedGiverEntriesLedger_.size()) +
                " invalidatedTurnInEntry=" + std::to_string(profile.turnInEntry) +
                " chainGiversInvalidated=" + std::to_string(chainGiversInvalidated));

            PersistLedger("turn-in pass");
        }

        bool SnapshotContains(
            const QuestProfile& profile) const
        {
            // Failed identity resolution is not proof of removal.
            if(QuestAcquisitionPolicy::TitleStillPresent(profile,snapshot_)) return true;
            for (const auto& entry : snapshot_.quests)
            {
                const auto* matched =
                    ValleyOfTrialsProfiles::Find(
                        entry,
                        snapshot_.classToken,
                        &completedQuestIds_
                    );

                if (
                    matched != nullptr &&
                    matched->questId ==
                        profile.questId)
                {
                    return true;
                }
            }

            return false;
        }

        const PlannerQuestLogEntry* FindSnapshotEntry(
            const QuestProfile& profile) const
        {
            for (const auto& entry : snapshot_.quests)
            {
                const auto* matched = ValleyOfTrialsProfiles::Find(
                    entry,
                    snapshot_.classToken,
                    &completedQuestIds_);
                if (matched != nullptr && matched->questId == profile.questId)
                    return &entry;
            }
            return nullptr;
        }

        void LogRuntimePlan()
        {
            if (focusQuestId_ > 0)
            {
                const QuestProfile* focusedProfile = nullptr;
                for (const auto& profile : ValleyOfTrialsProfiles::All())
                    if (profile.questId == focusQuestId_)
                        focusedProfile = &profile;

                const PlannerQuestLogEntry* liveMatch = nullptr;
                int titleMatches = 0;
                if (focusedProfile != nullptr)
                    for (const auto& entry : snapshot_.quests)
                        if (entry.title == focusedProfile->title)
                        {
                            liveMatch = &entry;
                            ++titleMatches;
                        }

                const bool identityResolved =
                    titleMatches == 1 && liveMatch != nullptr &&
                    ValleyOfTrialsProfiles::Find(
                        *liveMatch, snapshot_.classToken,
                        &completedQuestIds_) == focusedProfile;
                Debug::Logger::Info(
                    "QUEST 16B STATE questId=" +
                    std::to_string(focusQuestId_) +
                    " title=\"" +
                    (focusedProfile != nullptr ? focusedProfile->title : "unknown") +
                    "\" liveTitleMatch=" +
                    (titleMatches > 0 ? "yes" : "no") +
                    " liveComplete=" +
                    (liveMatch != nullptr && titleMatches == 1
                        ? (liveMatch->complete ? "yes" : "no") : "unknown") +
                    " objectiveRows=" +
                    std::to_string(liveMatch != nullptr && titleMatches == 1
                        ? liveMatch->objectiveCount : -1) +
                    " identityResolved=" +
                    (identityResolved ? "yes" : "no") +
                    " plannerAction=" + QuestPlanner::ActionName(plan_.action) +
                    " reason=" +
                    (titleMatches == 0 ? "title_absent" :
                     titleMatches > 1 ? "duplicate_live_title" :
                     !identityResolved ? "ambiguous_profile_identity" :
                     liveMatch->complete ? "active_complete" :
                     "active_incomplete"));
                Debug::Logger::Info(
                    "QUEST 16A STATE questId=" +
                    std::to_string(focusQuestId_) +
                    " action=" + QuestPlanner::ActionName(plan_.action) +
                    " objectiveIndex=" +
                    std::to_string(plan_.activeObjectiveIndex) +
                    " reason=live_quest_planner_snapshot");
                if (plan_.primary != nullptr)
                {
                    const QuestProfile selected = MaterializeObjectiveStep(
                        *plan_.primary, plan_.activeObjectiveIndex);
                    if (selected.objective.type ==
                        QuestObjectiveType::UseQuestItemAtLocation)
                        Debug::Logger::Info(
                            "QUEST 16C SELECT questId=" +
                            std::to_string(selected.questId) +
                            " title=\"" + selected.title +
                            "\" action=" + QuestPlanner::ActionName(plan_.action));
                    if (selected.objective.type ==
                        QuestObjectiveType::UseItemAtGameObject)
                        Debug::Logger::Info(
                            "QUEST 16D SELECT questId=" +
                            std::to_string(selected.questId) +
                            " title=\"" + selected.title +
                            "\" action=" + QuestPlanner::ActionName(plan_.action) +
                            " objectiveIndex=" +
                            std::to_string(selected.activeLeaderboardIndex));
                    Debug::Logger::Info(
                        "QUEST 16B SELECT questId=" +
                        std::to_string(selected.questId) +
                        " title=\"" + selected.title +
                        "\" objectiveType=" +
                        QuestPlanner::ObjectiveTypeName(selected.objective.type) +
                        " profileSource=" +
                        (selected.databaseDerived ? "QuestDB" : "hand_authored"));
                }
            }
            Debug::Logger::Info("================================");
            Debug::Logger::Info("QUEST PLANNER 13B: RUNTIME PLAN");
            Debug::Logger::Info(
                std::string("Action: ") + QuestPlanner::ActionName(plan_.action));

            if (plan_.primary != nullptr)
            {
                const QuestProfile materialized =
                    MaterializeObjectiveStep(
                        *plan_.primary,
                        plan_.activeObjectiveIndex);

                Debug::Logger::Info(
                    "Primary questId=" +
                    std::to_string(plan_.primary->questId) +
                    " title=\"" + plan_.primary->title +
                    "\" objective=" +
                    QuestPlanner::ObjectiveTypeName(materialized.objective.type) +
                    " step=" + std::to_string(plan_.activeObjectiveIndex + 1) +
                    " leaderboard=" +
                    std::to_string(materialized.activeLeaderboardIndex + 1) +
                    " hubExit=" + std::string(plan_.primary->hubExit ? "yes" : "no"));
            }

            Debug::Logger::Info(
                "QUESTDB 13C.2: wave active=" + std::to_string(plan_.waveActiveQuestCount) +
                " incomplete=" + std::to_string(plan_.waveIncompleteCount) +
                " readyForTurnIn=" + std::to_string(plan_.waveReadyForTurnInCount) +
                " quarantined=" + std::to_string(plan_.waveBlockedCount));
            Debug::Logger::Info(
                "Execution policy: Phase 13B.1 closes a local pickup wave first, executes "
                "all supported active objectives before any turn-in, then batch-turns the "
                "completed wave in before opening discovery for follow-up quests.");
            Debug::Logger::Info("================================");
        }

        void RefreshPlanner()
        {
            SetRuntimeActivity("RefreshPlanner");
            QuestPlannerSnapshot next{};

            if (!QuestPlannerStateReader::
                    Read(
                        next
                    ))
            {
                Debug::Logger::Info(
                    "QUEST PLANNER 11B: live snapshot read failed."
                );

                return;
            }

            const bool changed =
                !haveSnapshot_ ||
                !QuestPlannerStateReader::
                    Same(
                        snapshot_,
                        next
                    );

            std::set<int> presentQuestIds;
            for (const auto& entry : next.quests)
            {
                const auto* profile = ValleyOfTrialsProfiles::Find(
                    entry, next.classToken, &completedQuestIds_);
                if (profile == nullptr || !profile->automatable)
                    continue;
                presentQuestIds.insert(profile->questId);
                if (entry.complete)
                {
                    if (difficulty_.ObserveProgress(profile->questId))
                        Debug::Logger::Info(
                            "QUEST DEFER CLEARED quest=" +
                            std::to_string(profile->questId) +
                            " reason=live_quest_complete");
                    continue;
                }
                if (!haveSnapshot_)
                    continue;
                for (const auto& previous : snapshot_.quests)
                {
                    const auto* previousProfile = ValleyOfTrialsProfiles::Find(
                        previous, snapshot_.classToken, &completedQuestIds_);
                    if (previousProfile == nullptr ||
                        previousProfile->questId != profile->questId)
                        continue;
                    bool progressed = !previous.complete && entry.complete;
                    const auto count = std::min(previous.objectiveComplete.size(),
                        entry.objectiveComplete.size());
                    for (std::size_t index = 0; index < count; ++index)
                        progressed = progressed ||
                            (!previous.objectiveComplete[index] &&
                             entry.objectiveComplete[index]);
                    if (progressed && difficulty_.ObserveProgress(profile->questId))
                        Debug::Logger::Info(
                            "QUEST DIFFICULTY PROGRESS RESET quest=" +
                            std::to_string(profile->questId) +
                            " source=live_objective_completion");
                    break;
                }
            }
            // A temporarily ambiguous profile identity is not authoritative
            // evidence that the quest was removed. Preserve its session
            // defer record while the same live title remains visible.
            for (int deferredId : difficulty_.DeferredIds())
                if (presentQuestIds.find(deferredId) == presentQuestIds.end())
                    for (const auto& profile : ValleyOfTrialsProfiles::All())
                        if (profile.questId == deferredId)
                        {
                            for (const auto& entry : next.quests)
                                if (entry.title == profile.title)
                                {
                                    presentQuestIds.insert(deferredId);
                                    break;
                                }
                            break;
                        }
            const auto deferredBeforeReconcile = difficulty_.DeferredIds();
            difficulty_.ReconcilePresent(presentQuestIds);
            for (int questId : deferredBeforeReconcile)
                if (presentQuestIds.find(questId) == presentQuestIds.end())
                    Debug::Logger::Info(
                        "QUEST DEFER CLEARED quest=" +
                        std::to_string(questId) +
                        " reason=removed_from_live_quest_log");

            snapshot_ =
                next;

            haveSnapshot_ =
                true;

            const QuestRevisitBoundary revisitBoundary{
                objectiveDirector_.OwnsControl(),
                startAttempted_,
                objectiveRetryAfterTick_ != 0,
                discoveryStarted_ || discoveryRetryAfterTick_ != 0 || relocation_.Active() || maintenance_.Active() || trainer_.Active(),
                turnInProfile_ != nullptr || turnInStartAttempted_};
            const auto deferredBeforeRevisit = difficulty_.DeferredIds();
            if (CanEvaluateQuestRevisit(revisitBoundary))
            for (int questId : deferredBeforeRevisit)
            {
                if (!difficulty_.RevisitEligible(questId, plannerTick_,
                        snapshot_.playerLevel,
                        static_cast<int>(completedQuestIds_.size())))
                    continue;
                const auto* record = difficulty_.Find(questId);
                const char* trigger = snapshot_.playerLevel > record->deferLevel
                    ? "level_increase"
                    : static_cast<int>(completedQuestIds_.size()) >
                        record->deferCompletedCount
                        ? "other_quest_completed" : "cooldown_elapsed";
                Debug::Logger::Info(
                    "QUEST REVISIT ELIGIBLE quest=" +
                    std::to_string(questId) + " trigger=" + trigger +
                    " deferred=yes deferredAt=" +
                    std::to_string(record->deferTick) +
                    " previousReason=" + QuestDeferReasonName(record->reason));
                if (difficulty_.Revisit(questId, plannerTick_,
                        snapshot_.playerLevel,
                        static_cast<int>(completedQuestIds_.size())))
                    Debug::Logger::Info(
                        "QUEST REVISIT quest=" + std::to_string(questId) +
                        " result=normal_eligibility_restored");
            }

            const auto deferredIds = difficulty_.DeferredIds();

            // Snapshot/progress reconciliation above remains live. Do not
            // rewrite selection under another executor, retry, discovery or
            // turn-in owner. Reuse precisely the verified revisit boundary.
            if (!QuestPlanner::MayReplaceSelection(revisitBoundary, deathSuspended_))
                return;

            const auto nextPlan =
                QuestPlanner::Evaluate(
                    snapshot_,
                    &temporarilyBlockedQuestIds_,
                    &completedQuestIds_,
                    focusQuestId_,
                    &deferredIds
                );

            const int previousQuestId =
                plan_.primary == nullptr
                    ? 0
                    : plan_.primary->questId;

            const int nextQuestId =
                nextPlan.primary == nullptr
                    ? 0
                    : nextPlan.primary->questId;

            const bool planChanged =
                changed ||
                plan_.candidateEvaluations != nextPlan.candidateEvaluations ||
                plan_.action !=
                    nextPlan.action ||
                previousQuestId !=
                    nextQuestId;

            plan_ =
                nextPlan;

            if (planChanged)
            {
                for (const auto& candidate : plan_.candidateEvaluations)
                {
                    const bool selected = nextQuestId != 0 && candidate.questId == nextQuestId;
                    Debug::Logger::Info("QUEST GRAPH ELIGIBILITY quest=" + std::to_string(candidate.questId) +
                        " result=" + QuestEligibilityName(candidate.eligibility) + " source=live_quest_snapshot");
                    Debug::Logger::Info("QUEST PLANNER CANDIDATE quest=" + std::to_string(candidate.questId) +
                        " eligibility=" + QuestEligibilityName(candidate.eligibility) +
                        " support=" + QuestClassificationPolicy::SupportName(candidate.support) +
                        " deferred=" + (candidate.deferred ? "yes" : "no") +
                        " blocked=" + (candidate.blocked ? "yes" : "no") +
                        " decision=" + (selected ? "selected" : "skip"));
                    if (!selected)
                        Debug::Logger::Info("QUEST PLANNER SKIP quest=" + std::to_string(candidate.questId) +
                            " reason=" + QuestEligibilityName(candidate.eligibility));
                }
                Debug::Logger::Info("QUEST PLANNER SELECT quest=" + std::to_string(nextQuestId) +
                    " action=" + QuestPlanner::ActionName(plan_.action) + " reason=" + plan_.reason);
                for (int questId : plan_.nonExecutableQuestIds)
                {
                    Debug::Logger::Info(
                        "QUEST EXECUTION DISPATCH quest=" +
                        std::to_string(questId) +
                        " result=non_executable reason=no_supported_objective_executor");
                    if (focusQuestId_ == 0)
                        Debug::Logger::Info(
                            "QUEST MULTIQUEST SKIP quest=" +
                            std::to_string(questId) +
                            " reason=non_executable nextQuest=" +
                            std::to_string(nextQuestId));
                }
                if (plan_.action == QuestPlannerAction::UnsupportedActiveQuest)
                    Debug::Logger::Info(
                        "QUEST EXECUTION HOLD quest=" +
                        std::to_string(focusQuestId_) +
                        " action=UnsupportedActiveQuest reason=" + plan_.reason);
                if (!deferredIds.empty())
                    for (int questId : deferredIds)
                        Debug::Logger::Info(
                            "QUEST SKIP DEFERRED quest=" +
                            std::to_string(questId) +
                            " reason=" + QuestDeferReasonName(
                                difficulty_.Find(questId)->reason) +
                            " nextQuest=" + std::to_string(nextQuestId));
                LogRuntimePlan();
            }
        }

    public:
        ~QuestPlannerRuntimeController()
        {
            if (initialized_)
                Debug::Logger::Event("QUEST RUNTIME STOP generation=" +
                    std::to_string(diagnosticGeneration_) +
                    " plannerTick=" + std::to_string(plannerTick_) +
                    " reason=controller_destroyed sessionLocalDifficulty=discarded");
        }

        // Called before CombatController clears its lock on the death tick.
        // This is evidence capture only; DeathRecovery remains the owner.
        void ObserveDeathAtOwnershipBoundary(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (deathSuspended_)
                return;
            const auto* activeProfile = objectiveDirector_.ActiveProfile();
            const int questId = activeProfile == nullptr
                ? 0 : activeProfile->questId;
            bool lockedQuestTarget = false;
            const auto lockedGuid = combat.LockedGuid();
            if (lockedGuid != 0 && combat.PlannerQuestTargetActive() &&
                combat.DesiredQuestEntry() != 0)
                for (const auto& unit : world.units)
                    if (unit.guid == lockedGuid &&
                        unit.entryId == combat.DesiredQuestEntry())
                        lockedQuestTarget = true;
            if (!lockedQuestTarget && lockedGuid != 0 &&
                lockedGuid == lastQuestLockedGuid_ &&
                lastQuestLockQuestId_ == questId &&
                tick >= lastQuestLockTick_ &&
                tick - lastQuestLockTick_ <= 2)
                lockedQuestTarget = true;
            QuestDeathContext deathContext{};
            deathContext.questObjectiveOwned =
                objectiveDirector_.OwnsControl() && activeProfile != nullptr;
            deathContext.recentlyActive = lastObjectiveOwnedTick_ != 0 &&
                tick >= lastObjectiveOwnedTick_ &&
                tick - lastObjectiveOwnedTick_ <= 2;
            deathContext.lockedQuestTarget = lockedQuestTarget;
            const auto combatState = combat.State();
            const bool offensiveOrDefensiveCombat =
                combatState == CombatState::WarriorChargeFacing ||
                combatState == CombatState::WarriorOpening ||
                combatState == CombatState::Chasing ||
                combatState == CombatState::Fighting;
            deathContext.objectiveActionActive = lockedGuid == 0 &&
                !offensiveOrDefensiveCombat &&
                (objectiveDirector_.State() == ObjectiveExecutorState::Navigating ||
                 objectiveDirector_.State() == ObjectiveExecutorState::Executing);
            bool unrelatedDirectAggressor = false;
            if (world.activePlayerGuid != 0)
                for (const auto& unit : world.units)
                    if (unit.valid && unit.health > 0 &&
                        unit.targetGuid == world.activePlayerGuid &&
                        unit.guid != lockedGuid)
                    {
                        unrelatedDirectAggressor = true;
                        break;
                    }
            deathContext.unrelatedDefensiveCombat =
                (lockedGuid != 0 && !lockedQuestTarget) ||
                (offensiveOrDefensiveCombat && !lockedQuestTarget) ||
                unrelatedDirectAggressor;
            deathContext.otherOwner = discoveryStarted_ ||
                turnInProfile_ != nullptr;
            const bool attributed = ShouldAttributeQuestDeath(deathContext);
            if (attributed)
            {
                const bool deferred = difficulty_.ObserveDeath(questId,
                    true, tick, snapshot_.playerLevel,
                    static_cast<int>(completedQuestIds_.size()));
                LogDifficultyEvidence(questId, "quest_objective_death");
                if (deferred)
                    LogDefer(questId);
            }
        }

        void ReleaseNavigationForLivingWater()
        {
            objectiveDirector_.ReleaseActive();
            // These additional optional owners may exist in a newer local
            // QuestPlanner; the published baseline does not require them.
            auto cancelOptionalOwners = []<class Controller>(Controller& owner)
            {
                if constexpr (requires(Controller& value) { value.relocation_.Cancel(); })
                    owner.relocation_.Cancel();
                if constexpr (requires(Controller& value) { value.maintenance_.Cancel(); })
                    owner.maintenance_.Cancel();
                if constexpr (requires(Controller& value) { value.trainer_.Cancel(); })
                    owner.trainer_.Cancel();
            };
            cancelOptionalOwners(*this);
            turnInExecutor_ = GenericQuestTurnInExecutor{};
            turnInProfile_ = nullptr;
            turnInStartAttempted_ = false;
            discoveryController_ = GenericQuestDiscoveryController{};
            discoveryStarted_ = false;
            startAttempted_ = false;
            readyForTurnInLogged_ = false;
            objectiveRetryAfterTick_ = 0;
            haveSnapshot_ = false;
        }

        void SuspendForDeathRecovery(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (deathSuspended_)
                return;
            suspendedQuestId_ = plan_.primary == nullptr
                ? 0 : plan_.primary->questId;
            combat.ClearPlannerQuestTarget();
            objectiveDirector_.ReleaseActive();
            objectiveFailureEvidenceRecorded_ = false;
            turnInExecutor_ = GenericQuestTurnInExecutor{};
            turnInProfile_ = nullptr;
            turnInStartAttempted_ = false;
            discoveryController_ = GenericQuestDiscoveryController{};
            discoveryStarted_ = false;
            relocation_.Cancel();
            maintenance_.Cancel();
            trainer_.Cancel();
            discoveryRetryAfterTick_ = 0;
            startAttempted_ = false;
            readyForTurnInLogged_ = false;
            objectiveRetryAfterTick_ = 0;
            haveSnapshot_ = false;
            deathSuspended_ = true;
            idleActivity_=IdleActivityPolicy{};
            lastObjectiveOwnedTick_ = 0;
            lastQuestLockedGuid_ = 0;
            lastQuestLockTick_ = 0;
            lastQuestLockQuestId_ = 0;
            resumeAfterDeathPending_ = false;
            MovementController::HoldPosition(world.player);
            Debug::Logger::Info(
                "QUEST 16I SUSPEND questId=" +
                std::to_string(suspendedQuestId_) +
                " reason=death_recovery_owned tick=" +
                std::to_string(tick) +
                " objectiveRetriesPreserved=" +
                std::to_string(objectiveRetryCount_));
        }

        void ResumeAfterDeathRecovery(std::uint64_t tick)
        {
            if (!deathSuspended_)
                return;
            deathSuspended_ = false;
            haveSnapshot_ = false;
            resumeAfterDeathPending_ = true;
            Debug::Logger::Info(
                "QUEST 16I RESUME PENDING questId=" +
                std::to_string(suspendedQuestId_) +
                " tick=" + std::to_string(tick) +
                " reason=await_fresh_live_quest_snapshot");
        }

        void ConfigureFocusQuest(int questId)
        {
            if (!initialized_)
                focusQuestId_ = questId;
        }

        void SetVendorAutomationEnabled(bool enabled) { maintenance_.SetEnabled(enabled); }

        void Initialize()
        {
            if (initialized_)
            {
                return;
            }

            initialized_ =
                true;

            static std::uint64_t nextGeneration = 0;
            diagnosticGeneration_ = ++nextGeneration;
            Debug::Logger::Event("QUEST RUNTIME INIT generation=" +
                std::to_string(diagnosticGeneration_) +
                " plannerTick=" + std::to_string(plannerTick_) +
                " difficultyLifetime=controller_session");

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST PLANNER PHASE 13C.2: RUNTIME STABILITY + QUEST IDENTITY"
            );

            Debug::Logger::Info(
                "Executors: KillMob/CollectItemFromMob, CollectWorldItem, InteractGameObject, UseItemOnUnit."
            );

            Debug::Logger::Info(
                "Live target handoff -> existing CombatController "
                "-> native loot -> quest completion flag."
            );

            Debug::Logger::Info(
                focusQuestId_ > 0
                    ? "Single-quest test discovery is enabled; autonomous pickup remains local-hub scoped."
                    : (focusQuestId_ < 0
                        ? "Invalid single-quest test configuration; quest ownership is disabled."
                        : "Durotar-wide QuestDB discovery is enabled. Autonomous pickup is local-hub scoped; unsupported objective patterns stay catalogued but are not auto-accepted."));
            if (focusQuestId_ > 0)
                Debug::Logger::Info(
                    "QUEST 16A FOCUS questId=" +
                    std::to_string(focusQuestId_) +
                    " mode=single_quest_test");

            auto& questDatabase = VanillaQuestDatabase::Instance();
            std::size_t executable = 0, unsupported = 0, ambiguous = 0, missing = 0;
            std::size_t overrides = 0, conflicts = 0;
            std::size_t enriched=0, grouped=0, conditions=0, areaObjectives=0;
            for (const auto& profile : ValleyOfTrialsProfiles::All())
            {
                enriched += profile.sourceMetadata.has_value();
                grouped += profile.exclusiveGroup && *profile.exclusiveGroup!=0;
                conditions += profile.requiredCondition && *profile.requiredCondition!=0;
                areaObjectives += !profile.areaTriggerIds.empty();
                const auto classification = QuestClassificationPolicy::Classify(profile);
                switch (classification.support)
                {
                    case QuestRuntimeSupport::KnownExecutable: ++executable; break;
                    case QuestRuntimeSupport::KnownSemanticButUnsupported: ++unsupported; break;
                    case QuestRuntimeSupport::Ambiguous: ++ambiguous; break;
                    case QuestRuntimeSupport::MissingData: ++missing; break;
                }
                overrides += profile.handwrittenOverride;
                conflicts += profile.metadataConflicts.size();
                Debug::Logger::Info("QUEST CLASSIFICATION quest=" + std::to_string(profile.questId) +
                    " semantic=" + QuestPlanner::ObjectiveTypeName(classification.semantic) +
                    " support=" + QuestClassificationPolicy::SupportName(classification.support) +
                    " confidence=" + (classification.confidence == QuestClassificationConfidence::ExplicitProfile
                        ? "explicit_profile" : classification.confidence == QuestClassificationConfidence::StructuredData
                            ? "structured_data" : "unresolved") +
                    " source=" + (profile.databaseDerived ? "QuestDB" : "handwritten") +
                    " executor=" + QuestObjectiveDispatchPolicy::ExecutorName(classification.executor) +
                    " reason=" + classification.reason);
                for (const auto& conflict : profile.metadataConflicts)
                    Debug::Logger::Info("QUEST CLASSIFICATION CONFLICT quest=" + std::to_string(profile.questId) +
                        " field=" + conflict.field + " handwritten=" + conflict.handwritten +
                        " generated=" + conflict.generated + " resolution=" + conflict.resolution);
            }
            Debug::Logger::Info("QUEST CLASSIFICATION SUMMARY total=" +
                std::to_string(ValleyOfTrialsProfiles::All().size()) +
                " executable=" + std::to_string(executable) + " unsupported=" + std::to_string(unsupported) +
                " ambiguous=" + std::to_string(ambiguous) + " missing=" + std::to_string(missing) +
                " handwrittenOverride=" + std::to_string(overrides) + " conflicts=" + std::to_string(conflicts));
            const auto& graph = ValleyOfTrialsProfiles::Graph();
            Debug::Logger::Info("QUESTDB METADATA SUMMARY quests="+std::to_string(enriched)+
                " relationships="+std::to_string(graph.Edges().size())+" exclusiveGroups="+std::to_string(grouped)+
                " conditions="+std::to_string(conditions)+" areaObjectives="+std::to_string(areaObjectives)+
                " history=partial");
            Debug::Logger::Info("QUEST CATALOGUE SUMMARY total="+std::to_string(ValleyOfTrialsProfiles::All().size())+
                " executable="+std::to_string(executable)+" unsupported="+std::to_string(unsupported)+
                " ambiguous="+std::to_string(ambiguous)+" missing="+std::to_string(missing)+
                " source="+questDatabase.LoadedPath());
            for (const auto type : {QuestObjectiveType::TalkToNpc, QuestObjectiveType::ExploreOrAreaTrigger})
                Debug::Logger::Info("QUEST EXECUTOR SUPPORT semantic="+std::string(QuestPlanner::ObjectiveTypeName(type))+
                    " executor="+QuestObjectiveDispatchPolicy::ExecutorName(QuestObjectiveDispatchPolicy::Executor(type))+
                    " result=requires_source_verified_metadata");
            Debug::Logger::Info("QUEST GRAPH SUMMARY nodes=" + std::to_string(graph.Nodes().size()) +
                " edges=" + std::to_string(graph.Edges().size()) + " issues=" + std::to_string(graph.Issues().size()) +
                " completedHistory=partial source=merged_catalogue");
            for (const auto& issue : graph.Issues())
                Debug::Logger::Info("QUEST GRAPH VALIDATION quest=" + std::to_string(issue.questId) +
                    " issue=" + std::to_string(static_cast<int>(issue.kind)));
            if (questDatabase.EnsureLoaded())
            {
                Debug::Logger::Info(
                    "QUESTDB 12B: runtime catalog loaded: " + questDatabase.LoadedPath());
                Debug::Logger::Info(
                    "QUESTDB 13B: database profiles=" +
                    std::to_string(questDatabase.Profiles().size()) +
                    " merged profiles=" +
                    std::to_string(ValleyOfTrialsProfiles::All().size()));

                std::size_t automatable = 0;
                std::size_t deferred = 0;
                for (const auto& profile : ValleyOfTrialsProfiles::All())
                {
                    if (profile.automatable)
                        ++automatable;
                    else
                        ++deferred;
                }
                Debug::Logger::Info(
                    "QUESTDB 13B: support matrix automatable=" +
                    std::to_string(automatable) +
                    " deferred=" + std::to_string(deferred));
            }
            else
            {
                Debug::Logger::Info(
                    "QUESTDB 12B: runtime catalog unavailable; using hand-authored fallback. Reason: " +
                    questDatabase.LastError());
            }

            if (focusQuestId_ > 0)
            {
                const auto& profiles = ValleyOfTrialsProfiles::All();
                const auto found = std::find_if(
                    profiles.begin(), profiles.end(),
                    [&](const QuestProfile& profile) {
                        return profile.questId == focusQuestId_;
                    });
                if (found == profiles.end())
                {
                    Debug::Logger::Info(
                        "QUEST 16A FOCUS disabled reason=unknown_or_unsupported_profile");
                    focusQuestId_ = -1;
                }
                else if (found->objective.type ==
                    QuestObjectiveType::UseQuestItemAtLocation)
                {
                    Debug::Logger::Info(
                        "QUEST 16C SELECT questId=" +
                        std::to_string(found->questId) +
                        " title=\"" + found->title +
                        "\" profileSource=" +
                        (found->databaseDerived ? "QuestDB" : "hand_authored") +
                        " result=focused_profile_ready");
                }
                else if (found->objective.type ==
                    QuestObjectiveType::UseItemAtGameObject)
                {
                    Debug::Logger::Info(
                        "QUEST 16D SELECT questId=" +
                        std::to_string(found->questId) +
                        " title=\"" + found->title +
                        "\" profileSource=" +
                        (found->databaseDerived ? "QuestDB" : "hand_authored") +
                        " result=focused_profile_ready");
                }
            }

            std::string profileValidationError;
            if (!ValleyOfTrialsProfiles::Validate(profileValidationError))
            {
                Debug::Logger::Info(
                    "QUESTDB 13C.2: merged profile structural validation FAILED: " + profileValidationError);
            }
            else
            {
                Debug::Logger::Info(
                    "QUESTDB 13C.2: profile identity validation PASS; duplicate titles are legal and resolved from objective/giver/chain context.");
                Debug::Logger::Info(
                    "QUESTDB 13C.2: duplicate-title groups=" +
                    std::to_string(ValleyOfTrialsProfiles::DuplicateTitleGroupCount()));
            }

            readerValid_ =
                QuestPlannerStateReader::
                    Validate();

            Debug::Logger::Info(
                "================================"
            );
        }

        void UpdateImpl(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (focusQuestId_ < 0)
                return; // invalid explicit focus never expands to all quests
            if (!initialized_)
            {
                Initialize();
            }

            if (!readerValid_)
            {
                return;
            }

            if (
                !haveSnapshot_ ||
                (tick % 20) == 0)
            {
                RefreshPlanner();
            }

            if (!haveSnapshot_)
            {
                return;
            }

            if (resumeAfterDeathPending_)
            {
                resumeAfterDeathPending_ = false;
                Debug::Logger::Info(
                    "QUEST RESUME questId=" +
                    std::to_string(plan_.primary == nullptr
                        ? suspendedQuestId_ : plan_.primary->questId) +
                    " liveState=" + QuestPlanner::ActionName(plan_.action) +
                    " objectiveIndex=" +
                    std::to_string(plan_.activeObjectiveIndex) +
                    " source=fresh_quest_snapshot");
            }

            // Phase 12B.9: load/refresh persistent state once the live player
            // GUID and level are available. Persistence failure never blocks
            // gameplay; the in-memory 12B.8 ledger remains the fallback.
            const bool persistentStateReady = persistentQuestState_.EnsureLoaded(
                snapshot_.playerLevel,
                completedQuestIds_,
                checkedGiverEntriesLedger_);

            if (!startupPickupPolicyResolved_ && persistentStateReady)
            {
                startupPickupPolicyResolved_ = true;

                const bool activeAcceptedWave =
                    plan_.waveActiveQuestCount > 0;
                const bool havePersistentAuditFrontier =
                    !checkedGiverEntriesLedger_.empty();

                if (freshPickupWaveAudit_ &&
                    activeAcceptedWave &&
                    havePersistentAuditFrontier)
                {
                    freshPickupWaveAudit_ = false;
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "QUESTDB 13D.7.1: RESTART ACTIVE WAVE RESUME");
                    Debug::Logger::Info(
                        "Active accepted quests=" +
                        std::to_string(plan_.waveActiveQuestCount) +
                        " persistedCheckedGivers=" +
                        std::to_string(checkedGiverEntriesLedger_.size()) + ".");
                    Debug::Logger::Info(
                        "Policy: resume giver discovery from the persisted audit frontier; do not reinterpret DLL injection as a new pickup wave.");
                    Debug::Logger::Info("================================");
                }
                else
                {
                    Debug::Logger::Info(
                        "QUESTDB 13D.7.1: STARTUP PICKUP POLICY freshReconciliation=" +
                        std::string(freshPickupWaveAudit_ ? "yes" : "no") +
                        " activeAcceptedQuests=" +
                        std::to_string(plan_.waveActiveQuestCount) +
                        " persistedCheckedGivers=" +
                        std::to_string(checkedGiverEntriesLedger_.size()));
                }
            }

            if (trainer_.Active())
            {
                SetRuntimeActivity("ClassTrainer.Update");
                trainer_.Update(world,combat,tick);
                if(!trainer_.Active()) RefreshPlanner();
                return;
            }

            if (relocation_.Active())
            {
                SetRuntimeActivity("RegionalRelocation.Update");
                relocation_.Update(world,combat,tick);
                if (!relocation_.Active())
                {
                    if (relocation_.Arrived())
                    {
                        valleyDiscoverySweepComplete_=false;
                        freshPickupWaveAudit_=true;
                        checkedGiverEntriesLedger_.clear();
                        nextDiscoveryAuditTick_=0;
                    }
                    nextRelocationDecisionTick_=tick+DiscoveryRetryDelayTicks;
                    RefreshPlanner();
                }
                return;
            }

            if (maintenance_.Active())
            {
                SetRuntimeActivity("QuestMaintenance.Update");
                maintenance_.Update(world,combat,tick);
                if (!maintenance_.Active()) RefreshPlanner();
                return;
            }

            if (!temporarilyBlockedQuestIds_.empty() &&
                blockedWorkReleaseTick_ != 0 &&
                tick >= blockedWorkReleaseTick_)
            {
                Debug::Logger::Info(
                    "QUESTDB 13C.2: objective quarantine expired; reopening " +
                    std::to_string(temporarilyBlockedQuestIds_.size()) +
                    " quest(s) and replanning from live state.");
                temporarilyBlockedQuestIds_.clear();
                blockedWorkReleaseTick_ = 0;
                RefreshPlanner();
            }

            // =============================================
            // Phase 11D.2 Valley hub-completion discovery
            // =============================================

            if (discoveryStarted_)
            {
                SetRuntimeActivity("Discovery.Update");
                discoveryController_.Update(
                    snapshot_,
                    world,
                    combat,
                    tick
                );

                // Persist each newly verified giver immediately instead of
                // waiting for the whole sweep to finish. A later navigation
                // failure or process restart therefore resumes from the audit
                // frontier instead of giver #1.
                MergeDiscoveryLedger();

                if (discoveryController_.Done())
                {
                    MergeDiscoveryLedger();
                    discoveryStarted_ = false;
                    valleyDiscoverySweepComplete_ = true;
                    discoveryRetryAfterTick_ = 0;
                    discoveryRetryCount_ = 0;
                    nextDiscoveryAuditTick_=tick+QuestAcquisitionPolicy::ReauditTicks;
                    lastDiscoveryLevel_=snapshot_.playerLevel;
                    Debug::Logger::Info("QUEST DISCOVERY BACKOFF quest=0 reason=local_sweep_exhausted untilTick="+
                        std::to_string(nextDiscoveryAuditTick_));
                    discoveryController_ = GenericQuestDiscoveryController{};

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 12B.5: DATABASE GIVER AUDIT VERIFIED COMPLETE");
                    Debug::Logger::Info(
                        "Visible offers plus configured profiled giver-audit seeds are exhausted. Re-evaluating active Valley quests before any zone-exit objective.");
                    Debug::Logger::Info("================================");

                    RefreshPlanner();
                }
                else if (discoveryController_.Failed())
                {
                    MergeDiscoveryLedger();
                    discoveryStarted_ = false;
                    valleyDiscoverySweepComplete_ = false;
                    discoveryController_ = GenericQuestDiscoveryController{};
                    discoveryRetryAfterTick_ = tick + DiscoveryRetryDelayTicks;
                    ++discoveryRetryCount_;
                    Debug::Logger::Info("QUEST DISCOVERY BACKOFF quest=0 reason=bounded_discovery_failure untilTick="+
                        std::to_string(discoveryRetryAfterTick_));

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info("QUESTDB 12B.5: GIVER AUDIT RETRY SCHEDULED");
                    Debug::Logger::Info(
                        "retry=" + std::to_string(discoveryRetryCount_) +
                        " delayTicks=" + std::to_string(DiscoveryRetryDelayTicks));
                    Debug::Logger::Info(
                        "Hub-exit travel remains blocked while the current hub giver audit is unresolved.");
                    Debug::Logger::Info("================================");
                }

                return;
            }

            const bool zoneExitPending =
                plan_.primary != nullptr &&
                plan_.primary->hubExit;

            const QuestRevisitBoundary acquisitionBoundary{objectiveDirector_.OwnsControl(),startAttempted_,
                objectiveRetryAfterTick_!=0,discoveryStarted_ || discoveryRetryAfterTick_!=0,
                turnInProfile_!=nullptr || turnInStartAttempted_};
            if (CanEvaluateQuestRevisit(acquisitionBoundary))
            {
                equipment_.Update(world,combat,tick);
                // Finish the bounded inventory transaction before issuing a new
                // navigation/UI owner. Defensive/death preemption remains above.
                if(equipment_.Pending()) return;
            }
            if (plan_.action!=QuestPlannerAction::TurnIn && plan_.action!=QuestPlannerAction::DiscoverPickup &&
                CanEvaluateQuestRevisit(acquisitionBoundary))
            {
                trainer_.ObserveLevel(world.player.level,snapshot_.classToken);
                if(trainer_.TryStart(world,combat,tick,snapshot_.classToken)) return;
            }
            if (plan_.action!=QuestPlannerAction::TurnIn &&
                CanEvaluateQuestRevisit(acquisitionBoundary) && maintenance_.TryStart(world,combat,tick))
                return;
            if (focusQuestId_==0 && valleyDiscoverySweepComplete_ &&
                RegionalQuestRelocationPolicy::IdleAction(plan_.action) &&
                CanEvaluateQuestRevisit(acquisitionBoundary) && tick>=nextRelocationDecisionTick_ &&
                combat.LockedGuid()==0 && !combat.HasDeferredCorpseLootPending() &&
                (combat.State()==CombatState::AcquiringTarget || combat.State()==CombatState::PostKillDelay ||
                 combat.State()==CombatState::Idle))
            {
                QuestEligibilityContext context;
                context.level=snapshot_.playerLevel;
                context.raceMask=QuestAcquisitionPolicy::RaceMask(snapshot_.raceToken);
                context.classMask=QuestEligibilityPolicy::ClassMask(snapshot_.classToken);
                context.completed=completedQuestIds_;
                context.activeHistoryComplete=snapshot_.valid;
                for(const auto& live:snapshot_.quests)
                {
                    const auto* p=ValleyOfTrialsProfiles::Find(live,snapshot_.classToken,&completedQuestIds_);
                    if(p) context.activeQuestIds.insert(p->questId); else context.activeHistoryComplete=false;
                }
                // Existing runtime deployment is Kalimdor-only, as are
                // discovery/vendor. No cross-map path or map inference added.
                const auto candidates=RegionalQuestRelocationPolicy::Candidates(
                    ValleyOfTrialsProfiles::All(),ValleyOfTrialsProfiles::Graph(),context,snapshot_,
                    1,world.player.x,world.player.y,world.player.z,tick,relocation_.Backoff());
                nextRelocationDecisionTick_=tick+QuestAcquisitionPolicy::ReauditTicks;
                if(!candidates.empty())
                {
                    if(!relocation_.Start(candidates.front(),world,tick))
                        nextRelocationDecisionTick_=tick+DiscoveryRetryDelayTicks;
                    return;
                }
                Debug::Logger::Info("QUEST REGIONAL HOLD reason=no_source_backed_eligible_destination");
            }
            const bool acquisitionIdle=plan_.action==QuestPlannerAction::DiscoverPickup ||
                (focusQuestId_==0 && (plan_.action==QuestPlannerAction::UnsupportedActiveQuest ||
                    plan_.action==QuestPlannerAction::Deferred));
            if(QuestAcquisitionPolicy::ReauditDue(valleyDiscoverySweepComplete_,acquisitionIdle,
                CanEvaluateQuestRevisit(acquisitionBoundary),tick,nextDiscoveryAuditTick_,lastDiscoveryLevel_,snapshot_.playerLevel))
            {
                valleyDiscoverySweepComplete_=false;
                freshPickupWaveAudit_=true;
                checkedGiverEntriesLedger_.clear();
                nextDiscoveryAuditTick_=0;
                PersistLedger("bounded giver re-audit eligibility");
                Debug::Logger::Info("QUEST DISCOVERY BACKOFF quest=0 reason=expired_at_safe_boundary action=reevaluate");
            }

            if (discoveryRetryAfterTick_ != 0)
            {
                if (tick < discoveryRetryAfterTick_)
                    return;

                discoveryRetryAfterTick_ = 0;
                Debug::Logger::Info(
                    "QUESTDB 13A: RETRYING HUB GIVER AUDIT");
            }

            const bool shouldDiscover =
                !objectiveDirector_.OwnsControl() && !startAttempted_ &&
                objectiveRetryAfterTick_==0 && turnInProfile_==nullptr && !turnInStartAttempted_ &&
                QuestFocusPolicy::NeedsPickupAudit(
                    focusQuestId_, plan_.action, plan_.primary != nullptr) &&
                !valleyDiscoverySweepComplete_ &&
                !batchTurnInMode_ &&
                (
                    plan_.action == QuestPlannerAction::DiscoverPickup ||
                    plan_.action == QuestPlannerAction::ExecuteObjective ||
                    plan_.action == QuestPlannerAction::TurnIn ||
                    zoneExitPending || acquisitionIdle
                );

            if (shouldDiscover)
            {
                if (zoneExitPending)
                {
                    Debug::Logger::Info(
                        "QUESTDB 13A: HUB EXIT BLOCKED - database giver audit must finish before quest " +
                        std::to_string(plan_.primary == nullptr ? 0 : plan_.primary->questId) +
                        " may execute.");
                }

                SetRuntimeActivity("Discovery.Start");

                const std::set<std::uint32_t> emptyPrecheckedGivers{};
                const auto& discoveryPrecheckedGivers =
                    freshPickupWaveAudit_
                        ? emptyPrecheckedGivers
                        : checkedGiverEntriesLedger_;

                if (freshPickupWaveAudit_)
                {
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "QUESTDB 13D.5.1: FRESH PICKUP-WAVE HUB RECONCILIATION");
                    Debug::Logger::Info(
                        "Persistent checked-giver cache is ignored for this first pass so currently available supported quests cannot be hidden by an older audit.");
                    Debug::Logger::Info("================================");
                }

                if (discoveryController_.Start(
                        snapshot_,
                        world,
                        tick,
                        completedQuestIds_,
                        discoveryPrecheckedGivers,
                        focusQuestId_))
                {
                    discoveryStarted_ = true;
                    freshPickupWaveAudit_ = false;
                }
                else
                {
                    discoveryRetryAfterTick_=tick+DiscoveryRetryDelayTicks;
                    Debug::Logger::Info("QUEST DISCOVERY BACKOFF quest=0 reason=start_failed untilTick="+
                        std::to_string(discoveryRetryAfterTick_));
                    Debug::Logger::Info(
                        "QUEST PLANNER 13A: failed to start hub-completion discovery."
                    );
                }

                return;
            }

            /*
             * If no quest is active after a verified visible-hub sweep, keep
             * planner ownership. Never fall back to the legacy single-quest
             * pickup FSM or guess completed-history state.
             */
            if (
                valleyDiscoverySweepComplete_ &&
                plan_.action == QuestPlannerAction::DiscoverPickup)
            {
                return;
            }

            // =============================================
            // Phase 11C planner-owned generic quest turn-in
            // =============================================

            if (turnInProfile_ != nullptr)
            {
                if (!SnapshotContains(*turnInProfile_))
                {
                    turnInExecutor_.MarkQuestRemoved();
                }

                SetRuntimeActivity("TurnIn.Update");
                turnInExecutor_.Update(
                    world,
                    combat,
                    tick
                );

                if (turnInExecutor_.Done())
                {
                    objectiveDirector_.ReleaseActive();

                    const QuestProfile* completedProfile = turnInProfile_;
                    if (completedProfile != nullptr)
                    {
                        RecordCompletedQuest(*completedProfile);
                        if (completedProfile->objective.type ==
                            QuestObjectiveType::UseQuestItemAtLocation)
                            Debug::Logger::Info(
                                "QUEST 16C COMPLETE questId=" +
                                std::to_string(completedProfile->questId) +
                                " verifiedRemoved=yes");
                        if (completedProfile->objective.type ==
                            QuestObjectiveType::UseItemAtGameObject)
                            Debug::Logger::Info(
                                "QUEST 16D COMPLETE questId=" +
                                std::to_string(completedProfile->questId) +
                                " verifiedRemoved=yes");
                        if (focusQuestId_ == completedProfile->questId)
                            Debug::Logger::Info(
                                "QUEST 16B COMPLETE questId=" +
                                std::to_string(completedProfile->questId) +
                                " reason=verified_quest_log_removal");
                    }

                    Debug::Logger::Info(
                        "================================"
                    );
                    Debug::Logger::Info(
                        "QUEST PLANNER 11D.2: TURN-IN COMPLETE; CONTINUING ZONE"
                    );
                    Debug::Logger::Info(
                        "Quest removal was verified. Phase 12B.8 keeps the completed/giver ledger and re-audits only unresolved entries; the turn-in NPC is invalidated for possible chain continuation."
                    );
                    Debug::Logger::Info(
                        "================================"
                    );

                    turnInProfile_ = nullptr;
                    turnInStartAttempted_ = false;
                    readyForTurnInLogged_ = false;
                    startAttempted_ = false;
                    objectiveRetryAfterTick_ = 0;
                    objectiveRetryCount_ = 0;
                    objectiveRetryQuestId_ = 0;
                    turnInExecutor_ = GenericQuestTurnInExecutor{};
                    discoveryRetryAfterTick_ = 0;
                    discoveryRetryCount_ = 0;

                    // Keep the completed-wave turn-in phase closed to pickup
                    // discovery until every already-complete active quest has
                    // been handed in.  Follow-ups are therefore collected as
                    // one new wave rather than interleaved between turn-ins.
                    batchTurnInMode_ = true;
                    valleyDiscoverySweepComplete_ = true;
                    RefreshPlanner();

                    if (plan_.action == QuestPlannerAction::TurnIn)
                    {
                        Debug::Logger::Info(
                            "QUESTDB 13B.1: BATCH TURN-IN CONTINUES - another completed quest remains in the accepted wave.");
                    }
                    else
                    {
                        batchTurnInMode_ = false;
                        valleyDiscoverySweepComplete_ = false;
                        freshPickupWaveAudit_ = true;
                        Debug::Logger::Info(
                            "QUESTDB 13B.1: QUEST WAVE TURN-IN COMPLETE -> OPEN NEXT PICKUP WAVE");
                        Debug::Logger::Info(
                            "QUESTDB 13D.5.1: next pickup wave will re-audit the local hub from live quest dialogs.");
                    }
                }

                return;
            }

            if (
                plan_.action == QuestPlannerAction::TurnIn &&
                plan_.primary != nullptr &&
                !turnInStartAttempted_)
            {
                if (!batchTurnInMode_)
                {
                    batchTurnInMode_ = true;
                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "QUESTDB 13B.1: QUEST WAVE OBJECTIVES COMPLETE -> BATCH TURN-IN START");
                    Debug::Logger::Info(
                        "All supported active quests were allowed to finish before the first turn-in.");
                    Debug::Logger::Info("================================");
                }

                turnInStartAttempted_ = true;
                turnInProfile_ = plan_.primary;

                if (turnInProfile_->objective.type ==
                    QuestObjectiveType::UseQuestItemAtLocation)
                    Debug::Logger::Info(
                        "QUEST 16C TURNIN questId=" +
                        std::to_string(turnInProfile_->questId) +
                        " entry=" + std::to_string(turnInProfile_->turnInEntry) +
                        " state=generic_executor_start");
                if (turnInProfile_->objective.type ==
                    QuestObjectiveType::UseItemAtGameObject)
                    Debug::Logger::Info(
                        "QUEST 16D TURNIN questId=" +
                        std::to_string(turnInProfile_->questId) +
                        " entry=" +
                        std::to_string(turnInProfile_->turnInEntry) +
                        " state=generic_executor_start");

                SetRuntimeActivity("TurnIn.Start");
                if (focusQuestId_ == turnInProfile_->questId)
                    Debug::Logger::Info(
                        "QUEST 16B TURNIN questId=" +
                        std::to_string(turnInProfile_->questId) +
                        " npcEntry=" +
                        std::to_string(turnInProfile_->turnInEntry) +
                        " result=started");
                if (!turnInExecutor_.Start(
                        *turnInProfile_,
                        tick))
                {
                    Debug::Logger::Info(
                        "QUEST PLANNER 11C: failed to start generic turn-in executor."
                    );
                }

                return;
            }

            if (objectiveDirector_.OwnsControl())
            {
                SetRuntimeActivity("Objective.Update");
                lastObjectiveOwnedTick_ = tick;
                objectiveDirector_.Update(
                    snapshot_,
                    world,
                    combat,
                    tick);

                if (combat.PlannerQuestTargetActive() &&
                    combat.DesiredQuestEntry() != 0 &&
                    combat.LockedGuid() != 0)
                    for (const auto& unit : world.units)
                        if (unit.guid == combat.LockedGuid() &&
                            unit.entryId == combat.DesiredQuestEntry())
                        {
                            lastQuestLockedGuid_ = unit.guid;
                            lastQuestLockTick_ = tick;
                            lastQuestLockQuestId_ =
                                objectiveDirector_.ActiveProfile() == nullptr
                                    ? 0 : objectiveDirector_.ActiveProfile()->questId;
                            break;
                        }

                if (ObservePullDifficulty(combat, tick))
                    return;

                if (objectiveDirector_.ReadyForTurnIn())
                {
                    const auto* completedProfile =
                        objectiveDirector_.ActiveProfile();
                    const auto* liveEntry =
                        completedProfile == nullptr
                            ? nullptr
                            : FindSnapshotEntry(*completedProfile);

                    /*
                     * Phase 13A: ReadyForTurnIn from an executor now means
                     * "the selected objective step is complete". Only hand
                     * ownership to the turn-in FSM when the live quest itself
                     * is complete. Otherwise release the step and let the
                     * planner select the next unfinished leaderboard row.
                     */
                    if (completedProfile != nullptr &&
                        completedProfile->activeLeaderboardIndex >= 0 &&
                        liveEntry != nullptr &&
                        !liveEntry->complete)
                    {
                        const bool multiLocation =
                            completedProfile->objective.type ==
                                QuestObjectiveType::UseItemAtGameObject;
                        const int completedIndex =
                            completedProfile->activeLeaderboardIndex;
                        const int completedQuestId = completedProfile->questId;
                        Debug::Logger::Info("================================");
                        Debug::Logger::Info(
                            "QUESTDB 13A: MULTI-OBJECTIVE STEP COMPLETE");
                        Debug::Logger::Info(
                            "QuestId=" + std::to_string(completedProfile->questId) +
                            " step=" +
                            std::to_string(completedProfile->activeObjectiveIndex + 1) +
                            " leaderboard=" +
                            std::to_string(completedProfile->activeLeaderboardIndex + 1));
                        Debug::Logger::Info(
                            "Quest still active; releasing executor and selecting the next unfinished objective.");
                        Debug::Logger::Info("================================");

                        objectiveDirector_.ReleaseActive();
                        startAttempted_ = false;
                        readyForTurnInLogged_ = false;
                        objectiveRetryAfterTick_ = 0;
                        objectiveRetryCount_ = 0;
                        RefreshPlanner();
                        if (multiLocation)
                            Debug::Logger::Info(
                                "QUEST 16D ADVANCE questId=" +
                                std::to_string(completedQuestId) +
                                " from=" + std::to_string(completedIndex) +
                                " to=" +
                                std::to_string(plan_.activeObjectiveIndex) +
                                " source=live_quest_log");
                        return;
                    }

                    if (!readyForTurnInLogged_)
                    {
                        readyForTurnInLogged_ = true;
                        Debug::Logger::Info("================================");
                        Debug::Logger::Info(
                            "QUEST PLANNER 13A: QUEST READY FOR TURN-IN");
                        Debug::Logger::Info(
                            "All required objective rows are complete; handing ownership to Phase 11C generic turn-in.");
                        Debug::Logger::Info("================================");
                    }

                    if (completedProfile != nullptr &&
                        !turnInStartAttempted_)
                    {
                        turnInStartAttempted_ = true;
                        turnInProfile_ = completedProfile;

                        if (!turnInExecutor_.Start(*turnInProfile_, tick))
                        {
                            Debug::Logger::Info(
                                "QUEST PLANNER 11C: failed to start turn-in after objective completion.");
                        }
                    }
                }

                return;
            }

            if (objectiveDirector_.Failed())
            {
                const auto* failedProfile = objectiveDirector_.ActiveProfile();
                const int failedQuestId =
                    failedProfile != nullptr
                        ? failedProfile->questId
                        : (plan_.primary != nullptr ? plan_.primary->questId : 0);

                if (!objectiveFailureEvidenceRecorded_)
                {
                    objectiveFailureEvidenceRecorded_ = true;
                    const char* failureReason = objectiveDirector_.FailureReason();
                    bool unrelatedCombat = false;
                    if (combat.LockedGuid() != 0)
                    {
                        unrelatedCombat = true;
                        for (const auto& unit : world.units)
                            if (unit.guid == combat.LockedGuid() &&
                                combat.PlannerQuestTargetActive() &&
                                unit.entryId == combat.DesiredQuestEntry())
                            {
                                unrelatedCombat = false;
                                break;
                            }
                    }
                    if (MeaningfulObjectiveFailure(failureReason) &&
                        !unrelatedCombat)
                    {
                        QuestDeferPolicy::ObjectiveFailureObservation evidence{};
                        const bool deferred = difficulty_.ObserveObjectiveFailure(
                            failedQuestId, true, tick, snapshot_.playerLevel,
                            static_cast<int>(completedQuestIds_.size()),
                            objectiveAttemptStartedTick_, &evidence);
                        if (evidence.expired)
                            Debug::Logger::Info(
                                "QUEST DIFFICULTY EVIDENCE RESET quest=" +
                                std::to_string(failedQuestId) +
                                " reason=idle_window_expired previousCount=" +
                                std::to_string(evidence.previousCount) +
                                " idleGapTicks=" +
                                std::to_string(evidence.idleGapTicks));
                        LogDifficultyEvidence(failedQuestId,
                            failureReason == nullptr ? "unknown" : failureReason,
                            &evidence);
                        if (deferred)
                        {
                            LogDefer(failedQuestId);
                            objectiveDirector_.ReleaseActive();
                            startAttempted_ = false;
                            objectiveRetryAfterTick_ = 0;
                            objectiveRetryCount_ = 0;
                            objectiveRetryQuestId_ = 0;
                            RefreshPlanner();
                            return;
                        }
                    }
                }

                if (objectiveRetryQuestId_ != failedQuestId)
                {
                    objectiveRetryQuestId_ = failedQuestId;
                    objectiveRetryCount_ = 0;
                    objectiveRetryAfterTick_ = 0;
                }

                if (objectiveRetryAfterTick_ == 0)
                {
                    const std::uint64_t delay =
                        objectiveRetryCount_ < ObjectiveFastRetryLimit
                            ? ObjectiveFastRetryDelayTicks
                            : ObjectiveSlowRetryDelayTicks;

                    objectiveRetryAfterTick_ = tick + delay;

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "QUEST PLANNER 11D.2.6: OBJECTIVE RETRY SCHEDULED");
                    Debug::Logger::Info(
                        "QuestId=" + std::to_string(failedQuestId) +
                        " retry=" + std::to_string(objectiveRetryCount_ + 1) +
                        " delayTicks=" + std::to_string(delay));
                    Debug::Logger::Info(
                        "Failed local work keeps planner ownership, but no longer becomes a permanent idle hold.");
                    Debug::Logger::Info("================================");
                    return;
                }

                if (tick < objectiveRetryAfterTick_)
                    return;

                ++objectiveRetryCount_;
                objectiveRetryAfterTick_ = 0;

                if (objectiveRetryCount_ >= MaximumObjectiveRetriesPerWave)
                {
                    temporarilyBlockedQuestIds_.insert(failedQuestId);
                    blockedWorkReleaseTick_ = tick + BlockedObjectiveRetryDelayTicks;

                    Debug::Logger::Info("================================");
                    Debug::Logger::Info(
                        "QUESTDB 13C.2: OBJECTIVE QUARANTINED");
                    Debug::Logger::Info(
                        "QuestId=" + std::to_string(failedQuestId) +
                        " retries=" + std::to_string(objectiveRetryCount_) +
                        " cooldownTicks=" + std::to_string(BlockedObjectiveRetryDelayTicks) +
                        "; other quest-wave work may continue.");
                    Debug::Logger::Info("================================");

                    objectiveDirector_.ReleaseActive();
                    objectiveFailureEvidenceRecorded_ = false;
                    startAttempted_ = false;
                    readyForTurnInLogged_ = false;
                    objectiveRetryQuestId_ = 0;
                    RefreshPlanner();
                    return;
                }

                Debug::Logger::Info("================================");
                Debug::Logger::Info(
                    "QUEST PLANNER 11D.2.6: RETRYING FAILED OBJECTIVE");
                Debug::Logger::Info(
                    "QuestId=" + std::to_string(failedQuestId) +
                    " retry=" + std::to_string(objectiveRetryCount_));
                Debug::Logger::Info("================================");

                objectiveDirector_.ReleaseActive();
                objectiveFailureEvidenceRecorded_ = false;
                startAttempted_ = false;
                readyForTurnInLogged_ = false;
                RefreshPlanner();
                return;
            }

            if (startAttempted_)
                return;

            if (objectiveRetryAfterTick_ != 0 &&
                tick < objectiveRetryAfterTick_)
                return;

            if (
                plan_.action != QuestPlannerAction::ExecuteObjective ||
                plan_.primary == nullptr)
            {
                return;
            }

            if (
                plan_.primary->hubExit &&
                !valleyDiscoverySweepComplete_)
            {
                return;
            }

            /*
             * Phase 12B: zero-objective report/talk quests are database-
             * classified as TravelReport. The existing Phase 11C generic
             * turn-in navigator already owns exactly this interaction flow,
             * so route them there instead of requiring a fake executor.
             */
            if (QuestObjectiveDispatchPolicy::Executor(*plan_.primary) == QuestExecutorKind::TurnIn)
            {
                startAttempted_ = true;
                turnInStartAttempted_ = true;
                turnInProfile_ = plan_.primary;

                Debug::Logger::Info("================================");
                Debug::Logger::Info("QUESTDB 12B: TRAVEL REPORT -> GENERIC TURN-IN");
                Debug::Logger::Info(
                    "QuestId=" + std::to_string(turnInProfile_->questId) +
                    " turnInEntry=" + std::to_string(turnInProfile_->turnInEntry));
                Debug::Logger::Info("================================");

                if (!turnInExecutor_.Start(*turnInProfile_, tick))
                {
                    Debug::Logger::Info(
                        "QUESTDB 12B: failed to start generic turn-in for TravelReport.");
                    turnInProfile_ = nullptr;
                    turnInStartAttempted_ = false;
                    startAttempted_ = false;
                }
                return;
            }

            if (objectiveRetryQuestId_ != plan_.primary->questId)
            {
                objectiveRetryQuestId_ = plan_.primary->questId;
                objectiveRetryCount_ = 0;
                objectiveRetryAfterTick_ = 0;
            }

            objectiveFailureEvidenceRecorded_ = false;

            SetRuntimeActivity("Objective.Start");
            if (focusQuestId_ == plan_.primary->questId)
                Debug::Logger::Info(
                    "QUEST 16B OBJECTIVE questId=" +
                    std::to_string(plan_.primary->questId) +
                    " objectiveIndex=" +
                    std::to_string(plan_.activeObjectiveIndex) +
                    " result=start_attempt");
            if (!objectiveDirector_.Start(
                    *plan_.primary,
                    plan_.activeObjectiveIndex,
                    world,
                    combat,
                    tick))
            {
                const QuestProfile materialized =
                    MaterializeObjectiveStep(
                        *plan_.primary,
                        plan_.activeObjectiveIndex);
                Debug::Logger::Info(
                    "QUEST PLANNER 13A: no generic executor is available for objective type " +
                    std::string(QuestPlanner::ObjectiveTypeName(materialized.objective.type)) +
                    " questId=" + std::to_string(plan_.primary->questId) +
                    " objectiveIndex=" + std::to_string(plan_.activeObjectiveIndex) + ".");
                ++objectiveRetryCount_;
                if (objectiveRetryCount_ >= MaximumObjectiveRetriesPerWave)
                {
                    temporarilyBlockedQuestIds_.insert(plan_.primary->questId);
                    blockedWorkReleaseTick_ = tick + BlockedObjectiveRetryDelayTicks;
                    Debug::Logger::Info(
                        "QUEST EXECUTION DISPATCH quest=" +
                        std::to_string(plan_.primary->questId) +
                        " result=blocked reason=executor_start_failed retries=" +
                        std::to_string(objectiveRetryCount_));
                    objectiveRetryQuestId_ = 0;
                    objectiveRetryAfterTick_ = 0;
                    RefreshPlanner();
                }
                else
                {
                    objectiveRetryAfterTick_ =
                        tick + ObjectiveFastRetryDelayTicks;
                    Debug::Logger::Info(
                        "QUEST EXECUTION DISPATCH quest=" +
                        std::to_string(plan_.primary->questId) +
                        " result=failed reason=executor_start_failed retry=" +
                        std::to_string(objectiveRetryCount_));
                }
            }
            else
            {
                startAttempted_ = true;
                objectiveAttemptStartedTick_ = tick;
                objectiveRetryAfterTick_ = 0;
                Debug::Logger::Info(
                    "QUEST EXECUTION DISPATCH quest=" +
                    std::to_string(plan_.primary->questId) +
                    " objective=" + QuestPlanner::ObjectiveTypeName(
                        MaterializeObjectiveStep(*plan_.primary,
                            plan_.activeObjectiveIndex).objective.type) +
                    " support=" + QuestPlanner::SupportName(
                        plan_.primary->support) +
                    " result=started executor=" +
                    objectiveDirector_.StateName());
            }
        }

        void Update(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) noexcept
        {
            if (deathSuspended_)
                return;
            plannerTick_ = tick;
            if (runtimeFaultBackoffUntilTick_ != 0)
            {
                if (tick < runtimeFaultBackoffUntilTick_)
                    return;

                Debug::Logger::Info(
                    "QUESTDB 13C.2: contained-fault backoff expired; replanning from live state.");
                runtimeFaultBackoffUntilTick_ = 0;
            }

            try
            {
                std::uint64_t failedCombatGuid=0;
                std::uint32_t failedCombatEntry=0;
                const char* failedCombatReason=nullptr;
                if (combat.ConsumeOwnerTargetFailure(failedCombatGuid,
                        failedCombatEntry,failedCombatReason))
                {
                    if (!objectiveDirector_.FailActiveFromCombat(failedCombatReason))
                    {
                        ContainRuntimeFault(world,combat,tick,
                            "mandatory combat failure without active objective");
                        return;
                    }
                    combat.ClearPlannerQuestTarget();
                    Debug::Logger::Info("COMBAT OWNER HANDOFF from=Combat to=QuestPlanner"
                        " guid="+std::to_string(failedCombatGuid)+
                        " entry="+std::to_string(failedCombatEntry)+
                        " reason="+failedCombatReason+
                        " decision=objective_failure_not_kill");
                }
                SetRuntimeActivity("Update");
                UpdateImpl(world, combat, tick);
                SetRuntimeActivity(StateName());
            }
            catch (const std::exception& ex)
            {
                ContainRuntimeFault(
                    world,
                    combat,
                    tick,
                    std::string("std::exception: ") + ex.what());
            }
            catch (...)
            {
                ContainRuntimeFault(
                    world,
                    combat,
                    tick,
                    "unknown C++ exception escaped a QuestPlanner subsystem");
            }
        }

        // WorldMonitor calls AFTER all owner updates, including legacy owners.
        void ObserveIdle(const Objects::WorldState& world,CombatController& combat,
                         std::uint64_t tick,bool externalOwner)
        {
            // SharedAfkController consumes only this safe-idle classification.
            // Actual qualifying activity is verified from the client input clock.
            IdleSample idle;
            idle.meaningfulActivity=std::hypot(world.player.x-idleX_,world.player.y-idleY_)>=0.5f ||
                std::abs(world.player.z-idleZ_)>=0.5f;
            if(idle.meaningfulActivity) { idleX_=world.player.x; idleY_=world.player.y; idleZ_=world.player.z; }
            idle.reason=runtimeFaultBackoffUntilTick_ ? IdleReason::FaultedSubsystem :
                plan_.action==QuestPlannerAction::None ? IdleReason::NoEligibleWork : IdleReason::EvidenceWait;
            idle.healthyWorld=!deathSuspended_ && !externalOwner && readerValid_ && haveSnapshot_ && snapshot_.valid && world.player.valid &&
                world.player.maxHealth>0 && world.player.health>1 &&
                RecoveryController::HealthPercent(world.player)>=RecoveryController::ExitThresholdPercent();
            idle.plannerOwner=OwnsControl();
            idle.combat=combat.State()!=CombatState::Idle || combat.LockedGuid()!=0 ||
                ObjectiveDefensiveCombatGuard::HasDirectAggressor(world) || combat.Recovery().IsActive();
            idle.looting=combat.HasDeferredCorpseLootPending();
            idle.vendor=maintenance_.Active(); idle.trainer=trainer_.Active();
            const bool safeIdle=IdleActivityPolicy::Safe(idle);
            if(safeIdle && !idleReported_)
                Debug::Logger::Info("IDLE STATE reason=NoEligibleWork owner=None elapsedTicks=0");
            idleReported_=safeIdle;
            (void)tick;
        }

        bool SafeIdleForAfk() const
        {
            return readerValid_ && haveSnapshot_ && snapshot_.valid &&
                plan_.action==QuestPlannerAction::None &&
                !OwnsControl() && !runtimeFaultBackoffUntilTick_ &&
                !objectiveDirector_.Failed();
        }

        bool OwnsControl() const
        {
            return
                equipment_.Pending() ||
                relocation_.Active() ||
                maintenance_.Active() ||
                trainer_.Active() ||
                discoveryController_.OwnsControl() ||
                discoveryStarted_ ||
                discoveryRetryAfterTick_ != 0 ||
                turnInProfile_ != nullptr ||
                turnInStartAttempted_ ||
                objectiveDirector_.OwnsControl() ||
                (plan_.action == QuestPlannerAction::ExecuteObjective &&
                 plan_.primary != nullptr) ||
                // Includes the turn-in -> next pickup-wave boundary, before
                // discoveryStarted_ is set on the following update. Releasing
                // this tick lets CombatController start its legacy pickup FSM.
                plan_.action == QuestPlannerAction::DiscoverPickup ||
                (!temporarilyBlockedQuestIds_.empty() &&
                 plan_.action == QuestPlannerAction::TemporarilyBlocked) ||
                plan_.action == QuestPlannerAction::UnsupportedActiveQuest ||
                plan_.action == QuestPlannerAction::Deferred;
        }

        const char* StateName() const
        {
            if (equipment_.Pending()) return "EquipmentVerification";
            if (relocation_.Active()) return "RegionalRelocation";
            if (maintenance_.Active()) return "QuestMaintenance";
            if (trainer_.Active()) return "ClassTrainer";
            if (discoveryStarted_)
            {
                return discoveryController_.StateName();
            }

            if (discoveryRetryAfterTick_ != 0)
            {
                return "PlannerDiscoveryRetryBackoff";
            }

            if (!temporarilyBlockedQuestIds_.empty() &&
                plan_.action == QuestPlannerAction::TemporarilyBlocked)
            {
                return "QuestWaveQuarantineBackoff";
            }

            if (plan_.action == QuestPlannerAction::Deferred)
                return "QuestDifficultyDeferred";

            if (plan_.action == QuestPlannerAction::UnsupportedActiveQuest)
                return "QuestObjectiveUnsupported";

            if (
                turnInProfile_ != nullptr ||
                turnInStartAttempted_)
            {
                return turnInExecutor_.StateName();
            }

            if (
                valleyDiscoverySweepComplete_ &&
                plan_.action == QuestPlannerAction::DiscoverPickup)
            {
                return "ValleyHubSweepComplete";
            }

            if (objectiveDirector_.Failed() && objectiveRetryAfterTick_ != 0)
                return "PlannerObjectiveRetryBackoff";

            if (
                startAttempted_ &&
                plan_.action == QuestPlannerAction::ExecuteObjective &&
                plan_.primary != nullptr &&
                !objectiveDirector_.OwnsControl())
            {
                return "PlannerObjectiveHold";
            }

            if (plan_.action == QuestPlannerAction::ExecuteObjective &&
                plan_.primary != nullptr &&
                !objectiveDirector_.OwnsControl())
                return "PlannerObjectiveDispatchPending";

            return
                objectiveDirector_.StateName();
        }
    };
}
