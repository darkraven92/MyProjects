#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace Bot
{
    enum class QuestObjectiveType
    {
        TalkToNpc,
        KillMob,
        CollectItemFromMob,
        CollectWorldItem,
        UseItemOnUnit,
        UseQuestItemAtLocation,
        UseItemAtGameObject,
        InteractGameObject,
        TravelReport,
        Unknown,
        ExploreOrAreaTrigger
    };

    enum class QuestRouteGroup
    {
        None,
        ValleyExterior,
        BurningBladeCoven,
        SenjinRoad,
        SenjinVillage,
        SenjinCoast,
        EchoIsles,
        RazorHillRoad
    };

    enum class QuestExecutionSupport
    {
        VerifiedExistingSubsystem,
        ProfiledPlannerOnly
    };

    enum class QuestRuntimeSupport
    {
        KnownExecutable, KnownSemanticButUnsupported, Ambiguous, MissingData
    };
    enum class QuestClassificationConfidence { ExplicitProfile, StructuredData, Unresolved };
    enum class QuestEligibility
    {
        Eligible, TooLowLevel, TooHighLevel, RaceMismatch, ClassMismatch, FactionMismatch,
        MissingPrerequisite, PrerequisiteUnknown, ExclusiveConflict, AlreadyActive,
        ReadyForTurnIn, Completed, Deferred, TemporarilyBlocked, Unsupported,
        NonExecutable, AmbiguousMetadata, UnknownRestrictions
    };
    struct QuestCandidateEvaluation
    {
        int questId = 0;
        QuestEligibility eligibility = QuestEligibility::AmbiguousMetadata;
        QuestRuntimeSupport support = QuestRuntimeSupport::MissingData;
        bool deferred = false;
        bool blocked = false;
        bool operator==(const QuestCandidateEvaluation&) const = default;
    };

    enum class QuestPlannerAction
    {
        None,
        TurnIn,
        ExecuteObjective,
        DiscoverPickup,
        TemporarilyBlocked,
        Deferred,
        UnsupportedActiveQuest
    };

    struct QuestObjectiveProfile
    {
        QuestObjectiveType type =
            QuestObjectiveType::Unknown;

        std::uint32_t targetEntry =
            0;

        std::uint32_t itemId =
            0;

        std::uint32_t objectEntry =
            0;

        int requiredCount =
            0;

        const char* targetName =
            "";
    };

    struct ObjectiveDestination
    {
        bool valid =
            false;

        std::uint32_t mapId =
            1;

        float x =
            0.0f;

        float y =
            0.0f;

        float z =
            0.0f;

        /*
         * For hostile mob objectives we deliberately stop
         * outside melee range and let CombatController +
         * ChaseController own the final approach.
         */
        float arrivalDistance =
            28.0f;

        const char* label =
            "";
    };

    /*
     * Phase 13A: one Vanilla quest may expose several independent
     * leaderboard rows. Keep every database objective as a separate step
     * while preserving the legacy QuestProfile::objective fields for all
     * already-proven single-objective executors.
     */
    struct QuestObjectiveStep
    {
        int slot = 0;

        // 0-based GetQuestLogLeaderBoard index. The exporter mirrors the
        // 1.12 client order: creature/GO rows first, then item rows.
        int leaderboardIndex = -1;

        QuestObjectiveProfile objective{};
        ObjectiveDestination destination{};
        std::vector<ObjectiveDestination> searchDestinations{};

        int gameObjectType = -1;
        std::uint32_t gameObjectLootId = 0;
        std::string gossipCreditText{};
    };

    struct QuestResourceSource
    {
        std::uint32_t creatureEntry = 0;
        const char* creatureName = "";
        ObjectiveDestination destination{};
    };

    struct QuestMetadataConflict
    {
        std::string field;
        std::string handwritten;
        std::string generated;
        const char* resolution = "handwritten_override";
    };

    struct QuestSourceMetadata
    {
        std::optional<int> method, flags, specialFlags, exclusiveGroup;
        std::optional<int> requiredSkill, requiredSkillValue, startScript, completeScript;
        std::optional<int> reputationObjective, timeLimit, rewardOrRequiredMoney, breadcrumb;
    };
    struct QuestPrerequisiteClause
    {
        bool requiresActive = false;
        bool resolved = false;
        std::vector<int> allOf{};
    };
    struct QuestAreaTrigger
    {
        int id = 0;
        int build = 0;
        ObjectiveDestination center{};
        float radius = 0;
        bool clientGeometryVerified = false;
    };

    struct QuestProfile
    {
        int questId =
            0;

        const char* title =
            "";

        std::uint32_t giverEntry =
            0;

        std::uint32_t turnInEntry =
            0;

        int minimumLevel =
            1;

        int priority =
            0;

        bool optional =
            false;

        /*
         * Empty = any supported class.
         * "WARRIOR" = Orc Warrior class quest in the
         * first Phase 11A catalogue.
         */
        const char* classToken =
            "";

        /*
         * Some Vanilla quests share the exact same title.
         * Quest 790 and 804 are both named "Sarkoth".
         *
         * Vanilla's quest log does not expose quest IDs through
         * the normal Lua API, so profiles can be disambiguated with
         * the number of objective leaderboard rows.
         *
         * -1 = do not care.
         */
        int expectedObjectiveCount =
            -1;

        QuestRouteGroup routeGroup =
            QuestRouteGroup::None;

        QuestExecutionSupport support =
            QuestExecutionSupport::
                ProfiledPlannerOnly;

        /*
         * Legacy/current objective view. Phase 13A materializes the selected
         * multi-objective step into these fields before handing ownership to
         * an existing executor, so old executors remain modular.
         */
        QuestObjectiveProfile objective{};

        ObjectiveDestination destination{};

        std::vector<ObjectiveDestination> searchDestinations{};

        ObjectiveDestination giverDestination{};
        std::vector<ObjectiveDestination> giverDestinations{};
        std::vector<ObjectiveDestination> turnInDestinations{};
        ObjectiveDestination turnInDestination{};

        bool databaseDerived = false;
        int gameObjectType = -1;
        std::uint32_t gameObjectLootId = 0;

        /* Phase 13A multi-objective + multi-hub metadata. */
        std::vector<QuestObjectiveStep> objectives{};

        // Materialized runtime-only selector fields. Stored catalogue profiles
        // leave both at -1.
        int activeObjectiveIndex = -1;
        int activeLeaderboardIndex = -1;

        // 0 means the hub is immediately eligible. A non-zero value requires
        // that quest to exist in the persistent completion ledger before this
        // profile may be discovered from a giver seed.
        int hubUnlockQuestId = 0;

        // Travel/report quests such as 805 and 823 are allowed to be picked up
        // during a hub sweep, but they may not execute until all local giver
        // seeds for the current unlocked hub have been audited.
        bool hubExit = false;

        /* Phase 13B zone-wide metadata. These are advisory chain hints from
         * VMaNGOS. Live quest dialogs remain authoritative because old
         * characters may have incomplete local completion history. */
        int questLevel = 0;
        int previousQuestId = 0;
        int nextInChainQuestId = 0;
        int breadcrumbForQuestId = 0;

        // The full Durotar catalogue may contain objective patterns that do
        // not yet have a generic executor. Keep them visible in data, but do
        // not let autonomous discovery accept them and deadlock the planner.
        bool automatable = true;
        const char* supportNote = "supported";

        // Opt-in for bounded, same-map nearest-anchor selection at objective
        // start. Appended to preserve existing aggregate profile initializers.
        bool preferNearestObjectiveAnchor = false;

        // A source item used at a profiled location to summon the creature
        // named by objective.targetEntry. objective.itemId remains the actual
        // loot requirement; the two item identities must not be conflated.
        std::uint32_t questUseItemId = 0;

        // Optional minimum gap between issued uses, in WorldMonitor ticks.
        // Profile data may account for a locally verified item cooldown;
        // this never increases navigation or recovery budgets.
        std::uint64_t questUseMinimumIntervalTicks = 0;

        // Optional, bounded sources for an item used by a location/GO step.
        // These are profile data; the live bag and quest log decide progress.
        std::vector<QuestResourceSource> questResourceSources{};

        // Static catalogue diagnostics, never difficulty/defer state.
        bool semanticAmbiguous = false;
        bool handwrittenOverride = false;
        std::vector<QuestMetadataConflict> metadataConflicts{};

        // Appended TSV metadata. nullopt means absent in a legacy catalogue,
        // not an unrestricted zero mask. Faction/exclusivity are not present
        // in the current SQLite schema and remain unknown.
        std::optional<std::uint32_t> requiredRaceMask{};
        std::optional<std::uint32_t> requiredClassMask{};
        std::optional<std::uint32_t> requiredFactionMask{};
        std::optional<int> maximumLevel{};
        std::optional<int> zoneOrSort{};
        std::optional<int> requiredCondition{};
        std::optional<int> exclusiveGroup{};
        std::optional<int> nextQuestId{};
        std::optional<QuestSourceMetadata> sourceMetadata{};
        std::optional<int> sourceObjectiveCount{};
        bool relationshipMetadataKnown = false;
        std::vector<QuestPrerequisiteClause> prerequisiteAlternatives{};
        std::vector<int> exclusivePeers{};
        bool giverIsGameObject = false;
        bool turnInIsGameObject = false;
        std::vector<int> areaTriggerIds{};
        std::vector<QuestAreaTrigger> areaTriggers{};
        std::string gossipCreditText{};
    };

    struct PlannerQuestLogEntry
    {
        std::string title{};

        bool complete =
            false;

        int objectiveCount =
            0;

        // Completion state for each GetQuestLogLeaderBoard row in live client
        // order. This is enough to advance between database objective steps
        // without parsing localized objective text.
        std::vector<bool> objectiveComplete{};
    };

    inline bool SelectedObjectiveComplete(
        const PlannerQuestLogEntry& entry,
        const QuestProfile& profile)
    {
        if (entry.complete)
            return true;

        if (profile.activeLeaderboardIndex >= 0 &&
            static_cast<std::size_t>(profile.activeLeaderboardIndex) <
                entry.objectiveComplete.size())
        {
            return entry.objectiveComplete[
                static_cast<std::size_t>(profile.activeLeaderboardIndex)];
        }

        // Legacy single-objective profiles never set an active leaderboard
        // row. Preserve their old whole-quest completion semantics.
        return profile.activeLeaderboardIndex < 0
            ? entry.complete
            : false;
    }

    inline QuestProfile MaterializeObjectiveStep(
        const QuestProfile& base,
        int objectiveIndex)
    {
        QuestProfile result = base;
        result.activeObjectiveIndex = objectiveIndex;
        result.activeLeaderboardIndex = -1;

        if (objectiveIndex < 0 ||
            static_cast<std::size_t>(objectiveIndex) >= base.objectives.size())
        {
            return result;
        }

        const auto& step = base.objectives[
            static_cast<std::size_t>(objectiveIndex)];

        result.objective = step.objective;
        result.destination = step.destination;
        result.searchDestinations = step.searchDestinations;
        result.gameObjectType = step.gameObjectType;
        result.gameObjectLootId = step.gameObjectLootId;
        result.gossipCreditText = step.gossipCreditText;
        result.activeLeaderboardIndex = step.leaderboardIndex;
        return result;
    }

    struct QuestPlannerSnapshot
    {
        bool valid =
            false;

        std::string classToken{};

        int playerLevel =
            0;

        int rawQuestLogEntries =
            0;

        std::vector<PlannerQuestLogEntry>
            quests{};
        std::string raceToken{};
    };

    struct QuestPlannerPlan
    {
        QuestPlannerAction action =
            QuestPlannerAction::None;

        const QuestProfile* primary =
            nullptr;

        // -1 for completed/turn-in/legacy profiles. For a database-backed
        // multi-objective quest this identifies the next unfinished step.
        int activeObjectiveIndex = -1;

        std::vector<const QuestProfile*>
            routeBundle{};

        std::vector<std::string>
            unknownActiveTitles{};

        // Recognized profiles whose current objective has no runtime
        // executor. This is not difficulty/defer evidence.
        std::vector<int> nonExecutableQuestIds{};
        std::vector<QuestCandidateEvaluation> candidateEvaluations{};

        std::string reason{};

        // Phase 13C.2 wave diagnostics. These counts describe the live
        // supported quest log after the temporary objective-quarantine set
        // has been applied.
        int waveActiveQuestCount = 0;
        int waveIncompleteCount = 0;
        int waveReadyForTurnInCount = 0;
        int waveBlockedCount = 0;
        int waveDeferredCount = 0;

        bool readOnly =
            true;
    };
}
