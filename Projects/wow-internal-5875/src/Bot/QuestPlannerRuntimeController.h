#pragma once

#include "CombatController.h"
#include "GenericQuestTurnInExecutor.h"
#include "GenericQuestDiscoveryController.h"
#include "ObjectiveExecutionDirector.h"
#include "PersistentQuestState.h"
#include "QuestPlanner.h"
#include "QuestPlannerStateReader.h"
#include "ValleyOfTrialsProfiles.h"
#include "VanillaQuestDatabase.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <set>
#include <string>

namespace Bot
{
    class QuestPlannerRuntimeController
    {
    private:
        bool initialized_ =
            false;

        bool readerValid_ =
            false;

        bool haveSnapshot_ =
            false;

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

            if (plan_.primary != nullptr && plan_.primary->questId > 0)
            {
                temporarilyBlockedQuestIds_.insert(plan_.primary->questId);
                blockedWorkReleaseTick_ = tick + BlockedObjectiveRetryDelayTicks;
            }

            combat.ClearPlannerQuestTarget();
            objectiveDirector_.ReleaseActive();
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
            completedQuestIds_.insert(profile.questId);

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
                const bool followsCompleted =
                    candidate.previousQuestId == profile.questId ||
                    candidate.previousQuestId == -profile.questId ||
                    candidate.breadcrumbForQuestId == profile.questId ||
                    (profile.nextInChainQuestId != 0 &&
                     candidate.questId == profile.nextInChainQuestId);

                if (!followsCompleted || candidate.giverEntry == 0)
                    continue;

                chainGiversInvalidated += static_cast<int>(
                    checkedGiverEntriesLedger_.erase(candidate.giverEntry));
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

            snapshot_ =
                next;

            haveSnapshot_ =
                true;

            const auto nextPlan =
                QuestPlanner::Evaluate(
                    snapshot_,
                    &temporarilyBlockedQuestIds_,
                    &completedQuestIds_
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
                plan_.action !=
                    nextPlan.action ||
                previousQuestId !=
                    nextQuestId;

            plan_ =
                nextPlan;

            if (planChanged)
            {
                LogRuntimePlan();
            }
        }

    public:
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

        void Initialize()
        {
            if (initialized_)
            {
                return;
            }

            initialized_ =
                true;

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
                "Durotar-wide QuestDB discovery is enabled. Autonomous pickup is local-hub scoped; "
                "unsupported objective patterns stay catalogued but are not auto-accepted."
            );

            auto& questDatabase = VanillaQuestDatabase::Instance();
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
                if (discoveryController_.WaitingForQuestLog())
                {
                    const auto* activatedProfile =
                        discoveryController_.ActivatedProfile(snapshot_);

                    if (activatedProfile != nullptr)
                    {
                        Debug::Logger::Info(
                            "QUESTDB 12B.5: pickup verified from live quest log; actualQuestId=" +
                            std::to_string(activatedProfile->questId) +
                            " title=\"" + activatedProfile->title + "\".");
                        discoveryController_.MarkKnownQuestActive(
                            *activatedProfile,
                            tick);
                    }
                }

                SetRuntimeActivity("Discovery.Update");
                discoveryController_.Update(
                    snapshot_,
                    world,
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

            if (discoveryRetryAfterTick_ != 0)
            {
                if (tick < discoveryRetryAfterTick_)
                    return;

                discoveryRetryAfterTick_ = 0;
                Debug::Logger::Info(
                    "QUESTDB 13A: RETRYING HUB GIVER AUDIT");
            }

            const bool shouldDiscover =
                !valleyDiscoverySweepComplete_ &&
                !batchTurnInMode_ &&
                (
                    plan_.action == QuestPlannerAction::DiscoverPickup ||
                    plan_.action == QuestPlannerAction::ExecuteObjective ||
                    plan_.action == QuestPlannerAction::TurnIn ||
                    zoneExitPending
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
                        discoveryPrecheckedGivers))
                {
                    discoveryStarted_ = true;
                    freshPickupWaveAudit_ = false;
                }
                else
                {
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
                    tick
                );

                if (turnInExecutor_.Done())
                {
                    objectiveDirector_.ReleaseActive();

                    const QuestProfile* completedProfile = turnInProfile_;
                    if (completedProfile != nullptr)
                        RecordCompletedQuest(*completedProfile);

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

                SetRuntimeActivity("TurnIn.Start");
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
                objectiveDirector_.Update(
                    snapshot_,
                    world,
                    combat,
                    tick);

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
                startAttempted_ = false;
                readyForTurnInLogged_ = false;
                RefreshPlanner();
                return;
            }

            if (startAttempted_)
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
            if (plan_.primary->objective.type == QuestObjectiveType::TravelReport)
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

            startAttempted_ =
                true;

            SetRuntimeActivity("Objective.Start");
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
            }
        }

        void Update(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) noexcept
        {
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
                discoveryController_.OwnsControl() ||
                discoveryStarted_ ||
                discoveryRetryAfterTick_ != 0 ||
                turnInProfile_ != nullptr ||
                turnInStartAttempted_ ||
                objectiveDirector_.OwnsControl() ||
                (startAttempted_ &&
                 plan_.action == QuestPlannerAction::ExecuteObjective &&
                 plan_.primary != nullptr) ||
                (valleyDiscoverySweepComplete_ && plan_.action == QuestPlannerAction::DiscoverPickup) ||
                (!temporarilyBlockedQuestIds_.empty() &&
                 plan_.action == QuestPlannerAction::UnsupportedActiveQuest);
        }

        const char* StateName() const
        {
            if (discoveryStarted_)
            {
                return discoveryController_.StateName();
            }

            if (discoveryRetryAfterTick_ != 0)
            {
                return "PlannerDiscoveryRetryBackoff";
            }

            if (!temporarilyBlockedQuestIds_.empty() &&
                plan_.action == QuestPlannerAction::UnsupportedActiveQuest)
            {
                return "QuestWaveQuarantineBackoff";
            }

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

            return
                objectiveDirector_.StateName();
        }
    };
}
