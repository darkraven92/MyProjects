#pragma once

#include "DetourNavigationProvider.h"
#include "EpisodeBadTransitionPolicy.h"
#include "CorridorReplanHysteresisPolicy.h"
#include "NavigationHazardMemory.h"
#include "NavigationInitTelemetryPolicy.h"
#include "RouteCostProbePolicy.h"
#include "LongPathDiagnosticPolicy.h"
#include "CompleteLongStagePolicy.h"
#include "LocalRecoveryExhaustionPolicy.h"
#include "SurfaceRecoveryEpisodePolicy.h"
#include "SteeringSelectionPolicy.h"
#include "LocalPortalSteeringPolicy.h"
#include "IssuedSteeringCommandPolicy.h"
#include "PathValidationDiagnosticPolicy.h"

#include "../Bot/ClickToMoveController.h"
#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace Navigation
{
    enum class GenericNavMeshFollowState
    {
        Idle,
        Planning,
        Moving,
        Arrived,
        Failed,
        Planned
    };

    struct GenericNavMeshStartOptions
    {
        bool allowFullMapFallback = true;
        bool planningOnly = false;
        // Optional proven episode-local edge supplied by a strategic owner.
        // Start still clears every other follower state and 13D.4 performs
        // the actual alternative query; this is not a global blacklist.
        DirectedPolyTransition initialAvoidedTransition{};
        std::uint64_t initialTransitionMeshGeneration = 0;
    };

    struct NavigationFailureEvidence
    {
        NavigationPlanFailure reason = NavigationPlanFailure::None;
        NavigationInitTier tier = NavigationInitTier::None;
        NavigationInitTier queryTier = NavigationInitTier::None;
        std::string queryError{};
        PathValidationDetail validation{};
        bool destinationProjected = false;
        NavPoint failurePosition{};
        bool failurePositionKnown = false;
        NavPoint lastSafePosition{};
        bool lastSafePositionKnown = false;
        DirectedPolyTransition failedTransition{};
        DirectedPolyTransition learnedTransition{};
        std::uint64_t corridorFingerprint = 0;
        std::uint64_t meshGeneration = 0;
    };

    class GenericNavMeshPathFollower
    {
    private:
        static constexpr float CornerArrivalDistance =
            3.0f;

        static constexpr float NearCornerRecoveryDistance =
            5.0f;

        static constexpr float CtmPrecision =
            0.75f;

        static constexpr float ProgressThreshold =
            0.75f;

        // Phase 12B.1 adds an independent motion watchdog. The older
        // corner-distance watchdog can miss cases where CTM keeps being
        // reissued while the character is physically pinned on geometry.
        static constexpr float MotionProgressThreshold =
            0.35f;

        static constexpr std::uint64_t HardStallTicks =
            12;

        static constexpr float HardStallNearPortalDistance =
            7.0f;

        // Phase 12B.2: one replan is not enough when Detour returns the same
        // corridor around the same piece of local geometry. Keep recovery
        // state across replans and require meaningful route progress before
        // clearing it. A tiny lateral CTM probe is used only after repeated
        // verified zero-motion stalls, then control immediately returns to
        // Detour from the new live position.
        static constexpr float RecoveryResetProgressDistance =
            4.0f;

        // Phase 12B.4: once the live destination is close, a long dense-portal
        // corridor becomes counterproductive. Use one bounded direct final
        // approach, but fall back to the existing NavMesh recovery if the
        // direct CTM cannot make physical progress.
        static constexpr float FinalApproachEnterDistance =
            18.0f;

        static constexpr float FinalApproachMinimumProgress =
            0.80f;

        static constexpr std::uint64_t FinalApproachSettleTicks =
            10;

        static constexpr float FinalApproachMinimumStandOff =
            1.50f;

        static constexpr float EscapeProbeLateralDistance =
            3.0f;

        static constexpr float EscapeProbeForwardBias =
            0.75f;

        static constexpr float EscapeProbeSuccessDistance =
            0.80f;

        static constexpr std::uint64_t EscapeProbeSettleTicks =
            7;

        static constexpr int MaximumHardStallEpisodes =
            3;

        // Planner navigation must never continue deep into the recovery band.
        // CombatController already blocks new pulls below 70%; use the same
        // floor as a second-line fail-safe. Phase 11B.5 normally preempts on
        // the first observed HP loss before this threshold is reached.
        static constexpr float MinimumSafetyHealthPercent =
            70.0f;

        static constexpr std::uint64_t StuckTicks =
            24;

        static constexpr std::uint64_t ReissueTicks =
            40;

        static constexpr int MaximumReplans =
            4;

        static constexpr int InitialRouteTileMargin =
            2;

        static constexpr int ExpandedRouteTileMargin =
            4;

        static constexpr float MaximumSegment =
            300.0f;

        static constexpr float MaximumVerticalSegment =
            30.0f;

        static constexpr float MaximumPathLength =
            2000.0f;

        // Reserve 300 units for reconstruction/steering variation rather
        // than targeting the generic single-stage limit itself.
        static constexpr float CompleteLongStageLength = 1700.0f;
        static constexpr int MaximumCompleteLongStages = 8;

        // Phase 13C.1 robust local navigation. Corridor steering CTM legs are
        // raycast against the currently loaded Detour surface before they are
        // issued.  When a direct segment is blocked, moveAlongSurface is used
        // to generate a connected local detour instead of projecting an
        // arbitrary point that might lie across world geometry.
        static constexpr float SurfaceRecoveryMinimumStep = 1.25f;
        static constexpr float SurfaceRecoveryMaximumStep = 12.0f;
        static constexpr float SurfaceRecoveryMaximumVerticalDelta = 4.0f;
        static constexpr float SurfaceRecoveryMaximumFinalDistanceLoss = 10.0f;
        static constexpr float SurfaceRecoveryArrivalDistance = 1.50f;
        static constexpr float SurfaceRecoveryProgressThreshold = 0.35f;
        static constexpr std::uint64_t SurfaceRecoveryStallTicks = 18;
        static constexpr int MaximumSurfaceRecoveryAttempts = 4;
        static constexpr std::size_t SurfaceLookaheadPoints = 4;
        static constexpr float SurfaceLookaheadMaximumDistance = 24.0f;
        static constexpr float SurfaceLookaheadMaximumVerticalDelta = 4.5f;

        // Phase 14I.1: Detour portal samples can be valid mathematically while
        // sitting too close to a world wall for WoW's character collision
        // capsule. Keep normal steering points inset from nearby walls and,
        // after a real no-motion stall, perform one bounded wall-normal escape
        // before falling back to the older replan/escape machinery.
        static constexpr float WallSteeringProbeRadius = 3.0f;
        static constexpr float WallSteeringMinimumClearance =
            SteeringSelectionPolicy::MinimumWallClearance;
        static constexpr float WallSteeringDesiredClearance = 1.35f;
        static constexpr float WallSteeringMaximumInset = 1.75f;
        static constexpr float WallTrapProbeRadius = 3.0f;
        static constexpr float WallTrapTriggerClearance = 0.90f;
        static constexpr float WallTrapDesiredClearance = 1.60f;
        static constexpr float WallTrapMinimumClearanceGain = 0.45f;
        static constexpr float WallTrapMaximumVerticalDelta = 3.0f;

        // Phase 13D.6: ascending ramps/stairs need dense portal steering.
        // A surface raycast can legitimately cross several connected stair
        // polygons, but handing the resulting far/raised point straight to
        // WoW CTM can make the client press into the stair riser. Keep the
        // normal lookahead on flat/downhill ground, while limiting how much
        // upward elevation a single skipped segment may contain.
        static constexpr float StairLookaheadMaximumRise = 1.25f;
        static constexpr float StairFinalDirectMaximumRise = 1.75f;

        // Phase 13D.6: a Detour partial path is useful when it is a bounded
        // staging corridor toward a nearby destination. Previously every
        // DT_PARTIAL_RESULT was rejected immediately, which could leave quest
        // discovery idle on small local mesh seams (stairs, platforms, hut
        // entrances). Follow only partial corridors that make meaningful
        // progress and leave a small residual gap, then replan from the live
        // endpoint.
        static constexpr float PartialStageMaximumResidualDistance = 45.0f;
        static constexpr float PartialStageMaximumVerticalResidual = 10.0f;
        static constexpr float PartialStageMinimumProgress = 3.0f;
        static constexpr float PartialStageRepeatProgress = 1.0f;
        static constexpr int MaximumPartialStageAttempts = 3;

        // Phase 13D.7: a node-pool-exhausted query can return a long,
        // connected and strongly improving prefix. Its residual is not a
        // local seam and must not be judged by the 45-yard local staging cap.
        // Phase 13D.7.1: ALL_CROSSINGS points are portal samples. A raw
        // Z-rise threshold alone can reject a perfectly walkable long ramp or
        // staircase when two portal samples are horizontally far apart.
        // Keep the old small-rise fast path, but classify larger rises by
        // local slope. The historical cliff failure (~10.5 yd rise over
        // ~4 yd horizontal) is still rejected, while gradual ascents are
        // allowed to stage.
        static constexpr float PartialStageMaximumAscentPerSegment = 6.0f;
        static constexpr float PartialStageMaximumAscentSlope = 1.10f;
        static constexpr float PartialStageMinimumSlopeRun = 0.50f;
        static constexpr float PartialStageMinimumPhysicalProgress = 1.0f;
        static constexpr float LastSafePromotionDistance = 2.0f;
        static constexpr float LastSafeBacktrackArrivalDistance = 1.5f;
        static constexpr float LastSafeBacktrackProgressThreshold = 0.35f;
        static constexpr std::uint64_t LastSafeBacktrackStallTicks = 18;
        static constexpr int MaximumLastSafeBacktracks = 2;

        // Phase 13D.1-13D.4: keep Detour polygon identity through the
        // follower and quarantine repeatedly failing corridor transitions.
        // Two independent physical failures are required before the target
        // polygon is temporarily excluded from subsequent route queries.
        static constexpr int BlockedTransitionFailureThreshold = 2;
        static constexpr std::uint64_t BlockedTransitionTtlTicks = 600;
        static constexpr std::size_t MaximumBlockedTransitions = 8;

        // Planning-time terrain alternates are bounded inside one existing
        // plan attempt; they do not refill the follower's replan budget.
        static constexpr int MaximumTerrainAlternativeQueries = 2;

        // Phase 14G.3.1: exhausting both local escape sides is stronger
        // evidence than a normal single stall. Quarantine that transition and
        // allow a bounded last-safe/backtrack replan sequence before failing.
        static constexpr int MaximumEscapeExhaustionRecoveries = 2;

        // Phase 12B.10: DT_STRAIGHTPATH_ALL_CROSSINGS can expose a portal
        // point whose elevation belongs to the upper side of a steep local
        // transition while the character is still several yards below it.
        // Sending that portal directly to WoW CTM can pin the character even
        // though Detour's polygon corridor itself is valid. Detect only
        // strongly implausible player->portal elevation jumps and walk toward
        // them through short ground-projected steps before normal CTM/recovery
        // is allowed to take over again.
        static constexpr float VerticalPortalGuardMinimumDelta =
            TerrainTransitionPolicy::MinimumVerticalDelta;

        static constexpr float VerticalPortalGuardMaximumHorizontal =
            TerrainTransitionPolicy::MaximumHorizontal;

        static constexpr float VerticalPortalGuardMinimumSlopeRatio =
            TerrainTransitionPolicy::MinimumRiseRun;

        static constexpr float VerticalPortalProbeHorizontalExtent =
            1.25f;

        static constexpr float VerticalPortalProbeVerticalExtent =
            3.50f;

        static constexpr float VerticalPortalMaximumProjectedDelta =
            3.50f;

        static constexpr float VerticalPortalMinimumStepProgress =
            0.60f;

        static constexpr float VerticalPortalRecoverySuccessDistance =
            0.70f;

        static constexpr std::uint64_t VerticalPortalRecoverySettleTicks =
            7;

        // Phase 12B.13: a lateral barrier-bypass target is intentionally
        // several yards away. Treating the first ~0.7 yd of movement as a
        // completed recovery immediately replans back into the same blocked
        // cliff corridor. A bypass leg must therefore remain owned until the
        // character reaches the projected target (or stops making progress).
        static constexpr float VerticalBarrierBypassArrivalDistance =
            1.25f;

        static constexpr float VerticalBarrierBypassProgressThreshold =
            0.30f;

        static constexpr std::uint64_t VerticalBarrierBypassStallTicks =
            18;

        // Phase 12B.14: ProjectGroundNear() only proves that the bypass
        // endpoint lies on ground mesh. It does not prove that WoW can CTM
        // there in one straight line. Route lateral bypass legs through
        // Detour as a short local path and follow its dense portal points.
        static constexpr float VerticalBarrierBypassRouteCornerArrivalDistance =
            1.50f;

        static constexpr float VerticalBarrierBypassRouteMaximumSegment =
            12.0f;

        static constexpr float VerticalBarrierBypassRouteMaximumVerticalSegment =
            4.50f;

        static constexpr float VerticalBarrierBypassRouteMaximumLength =
            30.0f;

        // Phase 12B.15: repeated micro-recovery against the same steep portal
        // means the local 3-6 yd fan is too small to get around the world
        // geometry. Search a wider, bounded lateral/backward ring for a
        // temporary Detour anchor. This remains geometry-driven: no quest,
        // zone, NPC or hand-authored waypoint is involved.
        static constexpr int CliffDetourAttemptsBeforeSearch =
            2;

        static constexpr float CliffDetourMinimumVerticalDelta =
            8.0f;

        static constexpr float CliffDetourProbeHorizontalExtent =
            2.0f;

        static constexpr float CliffDetourProbeVerticalExtent =
            4.0f;

        static constexpr float CliffDetourMaximumProjectedDelta =
            4.0f;

        static constexpr float CliffDetourMinimumAnchorDistance =
            8.0f;

        static constexpr float CliffDetourMaximumFinalDistanceLoss =
            20.0f;

        static constexpr float CliffDetourRouteMaximumSegment =
            28.0f;

        static constexpr float CliffDetourRouteMaximumVerticalSegment =
            4.50f;

        static constexpr float CliffDetourRouteMaximumLength =
            140.0f;

        static constexpr float CliffDetourSafeOnwardPrefixLength =
            45.0f;

        static constexpr float CliffDetourSparseRouteDirectLimit =
            8.0f;

        static constexpr int MaximumVerticalPortalRecoveryAttempts =
            8;

        // Phase 12B.12: the 8-attempt guard is an episode budget, not a
        // lifetime budget. Real movement through a long ramp/barrier must be
        // allowed to earn a fresh bounded episode, just like Phase 12B.3 does
        // for replans. A separate lifetime cap still prevents an endlessly
        // wandering recovery loop.
        static constexpr float VerticalPortalRecoveryAnchorResetDistance =
            6.0f;

        static constexpr float VerticalPortalRecoveryBudgetResetFinalProgress =
            4.0f;

        static constexpr int MaximumVerticalPortalRecoveryTotalAttempts =
            64;

        // Phase 12B.11: if the straight same-surface probe cannot find a
        // walkable step, search a small deterministic fan around the live
        // player position. This is a local geometry bypass, not a quest
        // waypoint: candidates are projected to the currently loaded ground
        // mesh and ranked by progress toward the real destination.
        static constexpr float VerticalBypassProbeHorizontalExtent =
            1.25f;

        static constexpr float VerticalBypassProbeVerticalExtent =
            3.00f;

        static constexpr float VerticalBypassMaximumProjectedDelta =
            3.00f;

        static constexpr float VerticalBypassMinimumStepProgress =
            0.90f;

        static constexpr float VerticalBypassMaximumFinalDistanceLoss =
            3.00f;

        DetourNavigationProvider provider_{};
        bool fullMapFallbackAttempted_ = false;
        NavigationInitTier currentInitTier_ = NavigationInitTier::Route;
        NavigationPlanFailure lastPlanFailure_ = NavigationPlanFailure::None;
        PathValidationDetail lastValidationDetail_{};
        bool lastDestinationProjected_ = false;
        NavigationInitTier lastQueryTier_ = NavigationInitTier::None;
        std::string lastQueryError_{};
        GenericNavMeshStartOptions startOptions_{};
        Objects::PlayerState planningOriginPlayer_{};
        std::string initializationDirectory_{};
        bool initializationPending_ = false;
        bool retainIntentForInitialFallback_ = false;
        std::size_t initializationProgressLogBucket_ = 0;

        GenericNavMeshFollowState state_ =
            GenericNavMeshFollowState::Idle;

        NavPoint destination_{};

        std::uint32_t mapId_ =
            1;

        bool healthSafetyEnabled_ =
            true;

        float finalArrivalDistance_ =
            20.0f;

        std::string destinationLabel_{};
        // Observational only: one ID per Start, retained through local replans
        // and surface/backtrack recovery. Planning-only probes are not owners.
        inline static std::atomic<std::uint64_t> nextIntentId_{0};
        std::uint64_t intentId_=0, intentTick_=0, intentStarted_=0;
        std::string intentOwner_{};
        NavPoint intentDestination_{}, intentPosition_{};
        bool intentReleased_=false;

        void ReleaseIntent(const char* result)
        {
            if(intentId_==0 || intentReleased_) return;
            intentReleased_=true;
            std::ostringstream log;
            log<<"MOVEMENT INTENT RELEASE intent="<<intentId_<<" owner={"<<intentOwner_
               <<"} purpose={"<<destinationLabel_<<"} tick="<<intentTick_
               <<" elapsedTicks="<<(intentTick_>=intentStarted_?intentTick_-intentStarted_:0)
               <<" destination="<<intentDestination_.x<<","<<intentDestination_.y<<","<<intentDestination_.z
               <<" position="<<intentPosition_.x<<","<<intentPosition_.y<<","<<intentPosition_.z
               <<" result="<<result<<" reason="<<NavigationInitTelemetryPolicy::ReasonName(lastPlanFailure_)
               <<" commands="<<commands_<<" replans="<<totalReplans_;
            Debug::Logger::Info(log.str());
        }
        std::uint64_t lastCorpsePortalDiagnosticFingerprint_ = 0;

        std::vector<NavPoint> points_{};

        std::size_t pointIndex_ =
            0;

        int commands_ =
            0;

        // Phase 12B.3 separates the per-progress-episode replan budget from
        // the lifetime diagnostic count. A long route that genuinely advances
        // must earn a fresh recovery budget instead of failing because of old
        // replans from an earlier obstacle.
        int replans_ =
            0;

        int totalReplans_ =
            0;

        std::uint64_t lastCommandTick_ =
            0;

        std::uint64_t lastProgressTick_ =
            0;

        std::uint64_t lastStatusTick_ =
            0;

        float bestCornerDistance_ =
            0.0f;

        float plannedPathLength_ =
            0.0f;
        NavPoint plannedProjectedDestination_{};

        // Read-only probe evidence: a valid partial/staged prefix does not
        // establish reachability of the requested destination.
        bool planningOnlyReachedDestination_ = false;

        bool motionWatchdogInitialized_ =
            false;

        float lastMotionX_ =
            0.0f;

        float lastMotionY_ =
            0.0f;

        std::uint64_t lastMotionTick_ =
            0;

        int hardStallEpisodes_ =
            0;

        float hardStallBestFinalDistance_ =
            1.0e30f;

        bool escapeProbeActive_ =
            false;

        int escapeProbeSide_ =
            0;

        int escapeExhaustionRecoveries_ =
            0;

        float escapeProbeStartX_ =
            0.0f;

        float escapeProbeStartY_ =
            0.0f;

        std::uint64_t escapeProbeIssuedTick_ =
            0;

        bool finalApproachActive_ =
            false;

        bool surfaceRecoveryActive_ = false;
        float surfaceRecoveryStartX_ = 0.0f;
        float surfaceRecoveryStartY_ = 0.0f;
        NavPoint surfaceRecoveryTarget_{};
        float surfaceRecoveryBestTargetDistance_ = 0.0f;
        std::uint64_t surfaceRecoveryProgressTick_ = 0;
        int surfaceRecoveryAttempts_ = 0;
        std::uint64_t surfaceEpisodeSequence_ = 0;
        std::uint64_t surfaceEpisodeId_ = 0;
        std::uint64_t routeGeneration_ = 0;
        int surfaceEpisodeRouteRefreshes_ = 0;
        bool surfaceEpisodeActive_ = false;
        bool surfaceEpisodeForwardPortal_ = false;
        float surfaceEpisodeInitialDistance_ = 0.0f;
        float surfaceRecoveryStartFinalDistance_ = 0.0f;
        std::size_t surfaceRecoveryStartCorridor_ = 0;
        std::uint64_t surfaceRecoveryStartPortal_ = 0;
        std::uint64_t surfaceRecoveryProblemFrom_ = 0;
        std::uint64_t surfaceRecoveryProblemTo_ = 0;
        std::vector<NavPoint> surfaceEpisodeTargets_{};

        std::uint64_t lastPathFingerprint_ = 0;
        std::uint64_t lastSteeringLogTick_ = 0;
        std::uint64_t lastSteeringLogFingerprint_ = 0;
        std::size_t lastSteeringLogCandidate_ = static_cast<std::size_t>(-1);
        std::string lastSteeringLogDecision_{};
        bool steeringAtCorridorEnd_ = false;
        int repeatedCorridorPlans_ = 0;
        CorridorFailureRecord failedCorridor_{};
        NavPoint issuedOrdinarySteeringTarget_{};
        bool issuedOrdinarySteeringTargetValid_ = false;
        IssuedSteeringCommand issuedNavCommand_{};
        std::uint64_t navCommandSequence_ = 0;

        // Phase 13D.6 bounded partial-corridor staging state.
        bool partialStageActive_ = false;
        int partialStageAttempts_ = 0;
        float partialStageBestResidualDistance_ = 1.0e30f;
        float partialStageStartFinalDistance_ = 1.0e30f;
        std::uint64_t partialCorridorFingerprint_ = 0;
        float partialCorridorStartX_ = 0.0f;
        float partialCorridorStartY_ = 0.0f;

        // destination_ remains the caller's final objective throughout.
        bool completeLongStageActive_ = false;
        bool completeLongStageNeedsFinalPlan_ = false;
        int completeLongStageCount_ = 0;
        NavPoint activeStageDestination_{};
        NavPoint completeLongStageStart_{};
        NavPoint completeLongStageCompletionPosition_{};
        float completeLongStageStartFinalDistance_ = 0.0f;
        std::chrono::steady_clock::time_point completeLongStageStarted_{};

        struct LastSafeNavState
        {
            std::uint64_t polyRef = 0;
            NavPoint position{};
            std::uint64_t timestamp = 0;
            bool valid = false;
        };

        LastSafeNavState lastSafeNav_{};
        LastSafeNavState lastSafeCandidate_{};
        bool lastSafeBacktrackActive_ = false;
        NavPoint lastSafeBacktrackTarget_{};
        float lastSafeBacktrackBestDistance_ = 0.0f;
        std::uint64_t lastSafeBacktrackProgressTick_ = 0;
        int lastSafeBacktrackAttempts_ = 0;
        std::size_t lastStairLogPoint_ = static_cast<std::size_t>(-1);

        // Phase 13D.1: retain both the full Detour corridor and the polygon
        // entered at each straight-path steering point.
        std::vector<std::uint64_t> corridorPolys_{};
        std::vector<std::uint64_t> pointPolyRefs_{};
        std::uint64_t pathStartPoly_ = 0;
        std::uint64_t pathEndPoly_ = 0;

        struct BlockedTransition
        {
            std::uint64_t fromPoly = 0;
            std::uint64_t toPoly = 0;
            int failures = 0;
            std::uint64_t expiresAtTick = 0;
        };

        std::vector<BlockedTransition> blockedTransitions_{};
        EpisodeBadTransitionPolicy badVerticalTransitions_{};
        bool verticalTransitionAvoidancePending_ = false;
        bool boundaryVerticalAvoidanceAttempted_ = false;

        // Phase 14I.0: last live player position is retained so every stall
        // path can feed the persistent spatial hazard learner without
        // widening all legacy recovery method signatures.
        NavPoint lastObservedPlayerPosition_{};
        bool lastObservedPlayerPositionValid_ = false;

        bool verticalPortalRecoveryActive_ =
            false;

        float verticalPortalRecoveryStartX_ =
            0.0f;

        float verticalPortalRecoveryStartY_ =
            0.0f;

        std::uint64_t verticalPortalRecoveryIssuedTick_ =
            0;

        bool verticalPortalRecoveryBypassActive_ =
            false;

        float verticalPortalRecoveryTargetX_ =
            0.0f;

        float verticalPortalRecoveryTargetY_ =
            0.0f;

        float verticalPortalRecoveryInitialTargetDistance_ =
            0.0f;

        float verticalPortalRecoveryBestTargetDistance_ =
            0.0f;

        std::vector<NavPoint> verticalPortalBypassRoutePoints_{};

        std::size_t verticalPortalBypassRouteIndex_ =
            0;

        float verticalPortalBypassBestCornerDistance_ =
            0.0f;

        std::uint64_t verticalPortalRecoveryProgressTick_ =
            0;

        int verticalPortalRecoveryAttempts_ =
            0;

        int verticalPortalRecoveryTotalAttempts_ =
            0;

        float verticalPortalRecoveryBudgetBestFinalDistance_ =
            1.0e30f;

        bool verticalPortalRecoveryAnchorValid_ =
            false;

        float verticalPortalRecoveryAnchorX_ =
            0.0f;

        float verticalPortalRecoveryAnchorY_ =
            0.0f;

        bool finalApproachSuppressed_ =
            false;

        float finalApproachStartX_ =
            0.0f;

        float finalApproachStartY_ =
            0.0f;

        float finalApproachStartDistance_ =
            0.0f;

        std::uint64_t finalApproachIssuedTick_ =
            0;

        int finalApproachCommands_ =
            0;

        bool pausedForCombat_ =
            false;

        static constexpr float MaximumCachedResumeDistance =
            25.0f;

        static constexpr std::size_t CachedResumeLookBehind =
            2;

        static constexpr std::size_t CachedResumeLookAhead =
            6;

        static float Distance2D(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx =
                bx - ax;

            const float dy =
                by - ay;

            return
                std::sqrt(
                    dx * dx +
                    dy * dy
                );
        }

        static float Distance3D(
            const NavPoint& a,
            const NavPoint& b)
        {
            const float dx =
                b.x - a.x;

            const float dy =
                b.y - a.y;

            const float dz =
                b.z - a.z;

            return
                std::sqrt(
                    dx * dx +
                    dy * dy +
                    dz * dz
                );
        }

        static float HealthPercent(
            const Objects::PlayerState& player)
        {
            if (player.maxHealth == 0)
            {
                return 0.0f;
            }

            return
                static_cast<float>(
                    player.health
                ) *
                100.0f /
                static_cast<float>(
                    player.maxHealth
                );
        }

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(3)
                << value;

            return
                stream.str();
        }

        static const char* StateNameInternal(
            GenericNavMeshFollowState state)
        {
            switch (state)
            {
                case GenericNavMeshFollowState::Idle:
                    return "Idle";
                case GenericNavMeshFollowState::Planning:
                    return "Planning";
                case GenericNavMeshFollowState::Planned:
                    return "Planned";
                case GenericNavMeshFollowState::Moving:
                    return "Moving";
                case GenericNavMeshFollowState::Arrived:
                    return "Arrived";
                case GenericNavMeshFollowState::Failed:
                    return "Failed";
                default:
                    return "Unknown";
            }
        }

        void SetState(
            GenericNavMeshFollowState next)
        {
            if (state_ == next)
            {
                return;
            }

            // A failed route-scope plan is not terminal while another
            // initialization tier is still available. Keep the same intent
            // owned until a usable plan or a genuinely terminal failure.
            if (next == GenericNavMeshFollowState::Failed &&
                retainIntentForInitialFallback_)
            {
                IssuedSteeringCommandPolicy::Invalidate(issuedNavCommand_);
                Debug::Logger::Info(
                    "NAV INITIAL PLAN REJECT tier=" +
                    std::string(NavigationInitTelemetryPolicy::TierName(
                        currentInitTier_)) +
                    " reason=" +
                    NavigationInitTelemetryPolicy::ReasonName(lastPlanFailure_) +
                    " intentRetained=yes");
                return;
            }

            if (next == GenericNavMeshFollowState::Failed)
                lastPlanFailure_ = NavigationInitTelemetryPolicy::TerminalFailure(
                    lastPlanFailure_);
            if(next==GenericNavMeshFollowState::Failed || next==GenericNavMeshFollowState::Arrived)
                ReleaseIntent(StateNameInternal(next));

            if (next == GenericNavMeshFollowState::Failed &&
                completeLongStageCount_ > 0)
            {
                Debug::Logger::Info(
                    "NAV 14N.4.1 COMPLETE LONG STAGE FAILED"
                    " reason=underlying_navigation_failure"
                    " planReason=" + std::string(
                        NavigationInitTelemetryPolicy::ReasonName(lastPlanFailure_)) +
                    " stageIndex=" + std::to_string(completeLongStageCount_) +
                    " stageActive=" +
                    (completeLongStageActive_ ? "yes" : "no"));
            }

            Debug::Logger::Info(
                std::string(
                    "GenericNavMeshPathFollower state: "
                ) +
                StateNameInternal(
                    state_
                ) +
                " -> " +
                StateNameInternal(
                    next
                )
            );

            state_ =
                next;
            if (next != GenericNavMeshFollowState::Moving)
                IssuedSteeringCommandPolicy::Invalidate(issuedNavCommand_);
        }

        static NavPoint PlayerPoint(
            const Objects::PlayerState& player)
        {
            return
                NavPoint{
                    player.x,
                    player.y,
                    player.z
                };
        }

        bool IssueNavMovement(const Objects::PlayerState& player,
            NavPoint target, float precision, NavCommandSource source,
            DirectedPolyTransition transition = {},
            const std::source_location origin = std::source_location::current())
        {
            IssuedSteeringCommandPolicy::Invalidate(issuedNavCommand_);
            if (!provider_.WithCurrentTopology([&] {
                    return Bot::ClickToMoveController::MoveTo(player,target.x,target.y,
                        target.z,precision,origin);
                }))
                return false;
            const auto& dispatched=Bot::ClickToMoveController::LastCommand();
            if (dispatched.writer!=origin.function_name() ||
                std::fabs(dispatched.x-target.x)>0.01f ||
                std::fabs(dispatched.y-target.y)>0.01f ||
                std::fabs(dispatched.z-target.z)>0.01f)
                return true; // Issued, but no safe provenance for attribution.
            IssuedSteeringCommandPolicy::Record(issuedNavCommand_,
                lastPathFingerprint_,routeGeneration_,intentId_,
                ++navCommandSequence_,dispatched.serial,source,
                {target.x,target.y,target.z},
                {player.x,player.y,player.z},transition);
            if (source==NavCommandSource::OrdinarySteering ||
                source==NavCommandSource::FallbackNearer)
                Debug::Logger::Info("NAV COMMAND PROVENANCE intent="+
                    std::to_string(intentId_)+
                    " commandSequence="+
                        std::to_string(issuedNavCommand_.commandSequence)+
                    " corridorFingerprint="+
                        std::to_string(lastPathFingerprint_)+
                    " source="+IssuedSteeringCommandPolicy::SourceName(source)+
                    " target=("+Float(target.x)+","+Float(target.y)+","+
                        Float(target.z)+")"+
                    " fromPoly="+HexPoly(issuedNavCommand_.transition.from)+
                    " toPoly="+HexPoly(issuedNavCommand_.transition.to)+
                    " transitionKnown="+
                        (issuedNavCommand_.transition.Valid()?"yes":"no"));
            return true;
        }

        void StopAtCurrentPosition(
            const Objects::PlayerState& player)
        {
            IssuedSteeringCommandPolicy::Invalidate(issuedNavCommand_);
            if (!RouteCostProbePolicy::MayIssueMovement(
                    startOptions_.planningOnly))
                return;
            Bot::ClickToMoveController::MoveTo(player,player.x,player.y,
                player.z,0.25f);
        }

        static std::string HexPoly(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase << value;
            return stream.str();
        }

        void RememberPlayerPosition(const Objects::PlayerState& player)
        {
            lastObservedPlayerPosition_ = NavPoint{
                player.x, player.y, player.z};
            lastObservedPlayerPositionValid_ =
                std::isfinite(player.x) &&
                std::isfinite(player.y) &&
                std::isfinite(player.z);
        }

        void LearnPersistentHazard(
            std::uint64_t tick,
            int severity,
            const char* reason)
        {
            if (!lastObservedPlayerPositionValid_)
                return;

            NavigationHazardMemory::Instance().RecordFailure(
                mapId_,
                lastObservedPlayerPosition_,
                tick,
                severity,
                reason);
        }

        std::vector<std::uint64_t> ResolvePersistentHazardPolygons(
            const Objects::PlayerState& player)
        {
            std::vector<std::uint64_t> result;
            const auto centers =
                startOptions_.planningOnly
                    ? NavigationHazardMemory::Instance().HardCellCentersForProbe(mapId_)
                    : NavigationHazardMemory::Instance().HardCellCenters(mapId_);
            if (centers.empty())
                return result;

            const float minX = std::min(player.x, destination_.x) - 80.0f;
            const float maxX = std::max(player.x, destination_.x) + 80.0f;
            const float minY = std::min(player.y, destination_.y) - 80.0f;
            const float maxY = std::max(player.y, destination_.y) + 80.0f;

            static constexpr float offset = 3.0f;
            static constexpr float offsets[5][2] =
            {
                { 0.0f, 0.0f },
                { offset, 0.0f },
                { -offset, 0.0f },
                { 0.0f, offset },
                { 0.0f, -offset }
            };

            for (const auto& center : centers)
            {
                if (center.x < minX || center.x > maxX ||
                    center.y < minY || center.y > maxY)
                {
                    continue;
                }

                for (const auto& sampleOffset : offsets)
                {
                    NavPoint sample{
                        center.x + sampleOffset[0],
                        center.y + sampleOffset[1],
                        center.z};
                    NavPoint projected{};
                    std::uint64_t polyRef = 0;
                    if (!provider_.ProjectToNavMesh(
                            sample, projected, polyRef, 4.0f, 7.0f) ||
                        polyRef == 0)
                    {
                        continue;
                    }

                    if (std::find(result.begin(), result.end(), polyRef) ==
                        result.end())
                    {
                        result.push_back(polyRef);
                    }
                }
            }

            return result;
        }

        void PruneBlockedTransitions(std::uint64_t tick)
        {
            blockedTransitions_.erase(
                std::remove_if(
                    blockedTransitions_.begin(),
                    blockedTransitions_.end(),
                    [tick](const BlockedTransition& item)
                    {
                        return item.expiresAtTick != 0 &&
                            tick >= item.expiresAtTick;
                    }),
                blockedTransitions_.end());
        }

        bool CurrentCorridorTransition(
            std::uint64_t& fromPoly,
            std::uint64_t& toPoly) const
        {
            fromPoly = 0;
            toPoly = 0;

            if (pointIndex_ >= pointPolyRefs_.size())
                return false;

            toPoly = pointPolyRefs_[pointIndex_];
            if (toPoly == 0)
                toPoly = pathEndPoly_;

            if (toPoly == 0 || toPoly == pathEndPoly_)
                return false;

            for (std::size_t i = 0; i < corridorPolys_.size(); ++i)
            {
                if (corridorPolys_[i] != toPoly)
                    continue;

                if (i == 0)
                    fromPoly = pathStartPoly_;
                else
                    fromPoly = corridorPolys_[i - 1];
                break;
            }

            if (fromPoly == 0 && pointIndex_ > 0 &&
                pointIndex_ - 1 < pointPolyRefs_.size())
            {
                fromPoly = pointPolyRefs_[pointIndex_ - 1];
            }

            if (fromPoly == 0 || fromPoly == toPoly)
                return false;

            return true;
        }

        static const char* RecoveryQualityName(SurfaceRecoveryQuality quality)
        {
            switch(quality)
            {
                case SurfaceRecoveryQuality::Progress: return "progress";
                case SurfaceRecoveryQuality::Neutral: return "neutral";
                case SurfaceRecoveryQuality::Regression: return "regression";
            }
            return "neutral";
        }

        void BeginSurfaceRecoveryAttempt(const Objects::PlayerState& player,
            const NavPoint& target, float finalDistance,
            DirectedPolyTransition provenFailure = {})
        {
            if(!surfaceEpisodeActive_)
            {
                surfaceEpisodeActive_=true;
                surfaceEpisodeId_=++surfaceEpisodeSequence_;
                surfaceEpisodeInitialDistance_=finalDistance;
                surfaceEpisodeRouteRefreshes_=0;
                surfaceEpisodeForwardPortal_=false;
                surfaceEpisodeTargets_.clear();
            }
            surfaceRecoveryStartFinalDistance_=finalDistance;
            surfaceRecoveryStartCorridor_=pointIndex_+1;
            surfaceRecoveryStartPortal_=pointIndex_<pointPolyRefs_.size()
                ? pointPolyRefs_[pointIndex_] : 0;
            surfaceRecoveryProblemFrom_=provenFailure.from;
            surfaceRecoveryProblemTo_=provenFailure.to;
            if (!provenFailure.Valid())
                ResolveIssuedLocalTransition(player,surfaceRecoveryProblemFrom_,
                    surfaceRecoveryProblemTo_);
            surfaceEpisodeTargets_.push_back(target);
            float clearance=0;
            NavPoint wall{};
            const bool known=provider_.FindWallDistance(
                PlayerPoint(player),WallSteeringProbeRadius,clearance,wall);
            Debug::Logger::Info("NAV RECOVERY EPISODE intent="+std::to_string(intentId_)+
                " episode="+std::to_string(surfaceEpisodeId_)+
                " attempt="+std::to_string(surfaceRecoveryAttempts_)+"/"+
                    std::to_string(MaximumSurfaceRecoveryAttempts)+
                " routeGeneration="+std::to_string(routeGeneration_)+
                " routeRefreshes="+std::to_string(surfaceEpisodeRouteRefreshes_)+
                " start=("+Float(player.x)+","+Float(player.y)+","+Float(player.z)+")"+
                " target=("+Float(target.x)+","+Float(target.y)+","+Float(target.z)+")"+
                " problemPoly="+HexPoly(surfaceRecoveryProblemFrom_)+
                " problemNextPoly="+HexPoly(surfaceRecoveryProblemTo_)+
                " corridorBefore="+std::to_string(surfaceRecoveryStartCorridor_)+
                " portalBefore="+HexPoly(surfaceRecoveryStartPortal_)+
                " destinationBefore="+Float(finalDistance)+
                " clearanceBefore="+(known?Float(clearance):"unknown")+
                " result=started");
        }

        bool RegisterVerifiedSurfaceTransitionFailure(
            const Objects::PlayerState& player,std::uint64_t tick,const char* reason)
        {
            std::uint64_t issuedFrom=0,issuedTo=0,currentFrom=0,currentTo=0;
            if(!ResolveIssuedLocalTransition(player,issuedFrom,issuedTo) ||
                !CurrentCorridorTransition(currentFrom,currentTo) ||
                issuedFrom!=currentFrom || issuedTo!=currentTo)
            {
                Debug::Logger::Info("NAV RECOVERY ESCALATE intent="+
                    std::to_string(intentId_)+" episode="+
                    std::to_string(surfaceEpisodeId_)+
                    " decision=transition_unresolved reason=no_verified_local_directed_pair");
                return false;
            }
            return RegisterCurrentTransitionFailure(tick,reason);
        }

        DirectedPolyTransition VerticalCandidateTransition(
            std::size_t candidateIndex) const
        {
            if (candidateIndex >= pointPolyRefs_.size())
                return {};
            const std::uint64_t to = pointPolyRefs_[candidateIndex];
            if (to == 0)
                return {};

            DirectedPolyTransition result{};
            int occurrences = 0;
            for (std::size_t i = 0; i < corridorPolys_.size(); ++i)
            {
                if (corridorPolys_[i] != to)
                    continue;
                ++occurrences;
                result = {i > 0 ? corridorPolys_[i - 1] : pathStartPoly_, to};
            }
            return occurrences == 1 && result.Valid()
                ? result : DirectedPolyTransition{};
        }

        bool RegisterCurrentTransitionFailure(
            std::uint64_t tick,
            const char* reason)
        {
            PruneBlockedTransitions(tick);
            LearnPersistentHazard(tick, 1, reason);

            std::uint64_t fromPoly = 0;
            std::uint64_t toPoly = 0;
            if (!CurrentCorridorTransition(fromPoly, toPoly))
            {
                Debug::Logger::Info(
                    std::string("NAVMESH 13D.2: STALL TRANSITION unresolved reason=") +
                    reason + " portal=" + std::to_string(pointIndex_ + 1) +
                    "/" + std::to_string(points_.size()));
                return false;
            }

            for (BlockedTransition& item : blockedTransitions_)
            {
                if (item.fromPoly == fromPoly && item.toPoly == toPoly)
                {
                    ++item.failures;
                    item.expiresAtTick = tick + BlockedTransitionTtlTicks;

                    Debug::Logger::Info(
                        "NAVMESH 13D.2: TRANSITION FAILURE from=" +
                        HexPoly(fromPoly) + " to=" + HexPoly(toPoly) +
                        " failures=" + std::to_string(item.failures) +
                        "/" + std::to_string(BlockedTransitionFailureThreshold) +
                        " reason=" + reason);

                    if (item.failures == BlockedTransitionFailureThreshold)
                    {
                        Debug::Logger::Info(
                            "NAVMESH 13D.3: BLOCKED TRANSITION ACTIVATED from=" +
                            HexPoly(fromPoly) + " to=" + HexPoly(toPoly) +
                            "; route queries will temporarily exclude target polygon " +
                            HexPoly(toPoly) + ".");
                    }

                    return item.failures >= BlockedTransitionFailureThreshold;
                }
            }

            if (blockedTransitions_.size() >= MaximumBlockedTransitions)
            {
                blockedTransitions_.erase(blockedTransitions_.begin());
            }

            blockedTransitions_.push_back(BlockedTransition{
                fromPoly,
                toPoly,
                1,
                tick + BlockedTransitionTtlTicks
            });

            Debug::Logger::Info(
                "NAVMESH 13D.2: TRANSITION FAILURE from=" +
                HexPoly(fromPoly) + " to=" + HexPoly(toPoly) +
                " failures=1/" +
                std::to_string(BlockedTransitionFailureThreshold) +
                " reason=" + reason);

            return false;
        }

        bool QuarantineCurrentTransition(
            std::uint64_t tick,
            const char* reason)
        {
            PruneBlockedTransitions(tick);
            LearnPersistentHazard(tick, 2, reason);

            std::uint64_t fromPoly = 0;
            std::uint64_t toPoly = 0;
            if (!CurrentCorridorTransition(fromPoly, toPoly))
            {
                Debug::Logger::Info(
                    std::string("NAVMESH 14G.3.1: transition quarantine unavailable reason=") +
                    reason);
                return false;
            }

            for (BlockedTransition& item : blockedTransitions_)
            {
                if (item.fromPoly == fromPoly && item.toPoly == toPoly)
                {
                    item.failures = std::max(
                        item.failures, BlockedTransitionFailureThreshold);
                    item.expiresAtTick = tick + BlockedTransitionTtlTicks;
                    Debug::Logger::Info(
                        "NAVMESH 14G.3.1: QUARANTINED BLOCKED TRANSITION from=" +
                        HexPoly(fromPoly) + " to=" + HexPoly(toPoly) +
                        " reason=" + reason);
                    return true;
                }
            }

            if (blockedTransitions_.size() >= MaximumBlockedTransitions)
                blockedTransitions_.erase(blockedTransitions_.begin());

            blockedTransitions_.push_back(BlockedTransition{
                fromPoly,
                toPoly,
                BlockedTransitionFailureThreshold,
                tick + BlockedTransitionTtlTicks
            });

            Debug::Logger::Info(
                "NAVMESH 14G.3.1: QUARANTINED BLOCKED TRANSITION from=" +
                HexPoly(fromPoly) + " to=" + HexPoly(toPoly) +
                " reason=" + reason);
            return true;
        }

        std::vector<std::uint64_t> ActiveBlockedPolygons(std::uint64_t tick)
        {
            PruneBlockedTransitions(tick);

            std::vector<std::uint64_t> result;
            result.reserve(blockedTransitions_.size());

            for (const BlockedTransition& item : blockedTransitions_)
            {
                if (item.failures < BlockedTransitionFailureThreshold ||
                    item.toPoly == 0 || item.toPoly == pathEndPoly_)
                {
                    continue;
                }

                if (std::find(result.begin(), result.end(), item.toPoly) == result.end())
                    result.push_back(item.toPoly);
            }

            return result;
        }

        static std::uint64_t FingerprintPath(
            const std::vector<NavPoint>& points)
        {
            // FNV-1a over the first few half-yard-quantized steering points.
            // This is intentionally geometric rather than quest-specific: it
            // detects replans that keep returning the same local corridor.
            std::uint64_t hash = 1469598103934665603ull;
            const std::size_t count = std::min<std::size_t>(points.size(), 8);

            auto mix = [&hash](std::int64_t value)
            {
                const std::uint64_t u = static_cast<std::uint64_t>(value);
                for (int shift = 0; shift < 64; shift += 8)
                {
                    hash ^= (u >> shift) & 0xFFull;
                    hash *= 1099511628211ull;
                }
            };

            for (std::size_t i = 0; i < count; ++i)
            {
                mix(static_cast<std::int64_t>(std::llround(points[i].x * 2.0f)));
                mix(static_cast<std::int64_t>(std::llround(points[i].y * 2.0f)));
                mix(static_cast<std::int64_t>(std::llround(points[i].z * 2.0f)));
            }

            return hash;
        }

        static std::uint64_t FingerprintCorridor(
            const std::vector<std::uint64_t>& corridor)
        {
            std::uint64_t hash = 1469598103934665603ull;
            for (const std::uint64_t ref : corridor)
            {
                for (int shift = 0; shift < 64; shift += 8)
                {
                    hash ^= (ref >> shift) & 0xFFull;
                    hash *= 1099511628211ull;
                }
            }
            hash ^= static_cast<std::uint64_t>(corridor.size());
            hash *= 1099511628211ull;
            return corridor.empty() ? 0 : hash;
        }

        // pointIndex_ can name a far lookahead portal. Resolve the first
        // corridor edge actually ahead of the live player and on the issued
        // ordinary CTM leg instead of treating that far portal's incoming
        // edge as the local obstruction.
        bool ResolveIssuedLocalTransition(
            const Objects::PlayerState& player,
            std::uint64_t& fromPoly,
            std::uint64_t& toPoly) const
        {
            fromPoly = 0;
            toPoly = 0;
            if (!issuedOrdinarySteeringTargetValid_ ||
                pointIndex_ >= pointPolyRefs_.size())
                return false;

            NavPoint projectedPlayer{};
            NavPoint projectedTarget{};
            std::uint64_t playerPoly = 0;
            std::uint64_t targetPoly = 0;
            if (!provider_.ProjectToNavMesh(
                    PlayerPoint(player), projectedPlayer, playerPoly) ||
                !provider_.ProjectToNavMesh(
                    issuedOrdinarySteeringTarget_, projectedTarget, targetPoly))
                return false;

            const auto candidate = std::find(
                corridorPolys_.begin(), corridorPolys_.end(),
                pointPolyRefs_[pointIndex_]);
            if (candidate == corridorPolys_.end())
                return false;
            const std::size_t candidateIndex = static_cast<std::size_t>(
                candidate - corridorPolys_.begin());

            std::size_t playerIndex = corridorPolys_.size();
            for (std::size_t i = 0; i < candidateIndex; ++i)
            {
                if (corridorPolys_[i] == playerPoly)
                    playerIndex = i;
            }
            if (playerIndex >= candidateIndex ||
                std::find(corridorPolys_.begin() + playerIndex + 1,
                    corridorPolys_.begin() + candidateIndex + 1,
                    targetPoly) ==
                    corridorPolys_.begin() + candidateIndex + 1)
                return false;

            fromPoly = corridorPolys_[playerIndex];
            toPoly = corridorPolys_[playerIndex + 1];
            return fromPoly != 0 && toPoly != 0 && fromPoly != toPoly;
        }

        // The nearest forward steering point is already inside the normal
        // lookahead window. A different first edge is a different local exit;
        // a failed edge merely appearing later in the route is not a match.
        static bool ResolveCandidateLocalTransition(
            const Objects::PlayerState& player,
            const NavPathResult& path,
            std::uint64_t& fromPoly,
            std::uint64_t& toPoly)
        {
            fromPoly = 0;
            toPoly = 0;
            if (path.corridorPolys.size() < 2 || path.points.size() < 2 ||
                path.pointPolys.size() != path.points.size() ||
                path.corridorPolys.front() != path.startPoly)
                return false;

            std::size_t firstCandidate = 1;
            while (firstCandidate + 1 < path.points.size() &&
                   Distance2D(player.x, player.y,
                       path.points[firstCandidate].x,
                       path.points[firstCandidate].y) <= CornerArrivalDistance)
                ++firstCandidate;

            if (firstCandidate + 1 == path.points.size() &&
                Distance2D(player.x, player.y,
                    path.points[firstCandidate].x,
                    path.points[firstCandidate].y) <= CornerArrivalDistance)
                return false;

            const auto entered = std::find(
                path.corridorPolys.begin() + 1, path.corridorPolys.end(),
                path.pointPolys[firstCandidate]);
            if (entered == path.corridorPolys.end())
                return false;

            fromPoly = path.corridorPolys[0];
            toPoly = path.corridorPolys[1];
            return fromPoly != 0 && toPoly != 0 && fromPoly != toPoly;
        }

        void RegisterFailedCorridor(
            const Objects::PlayerState& player,
            const char* reason,
            bool ordinarySteeringStalled,
            DirectedPolyTransition provenSteeringFailure = {})
        {
            if (lastPathFingerprint_ == 0)
                return;

            std::uint64_t fromPoly = 0;
            std::uint64_t toPoly = 0;
            bool issuedKnown = false;
            if (ordinarySteeringStalled)
            {
                const auto& dispatched=Bot::ClickToMoveController::LastCommand();
                const auto attribution=IssuedSteeringCommandPolicy::ForHardStall(
                    issuedNavCommand_,dispatched.serial,lastPathFingerprint_,
                    routeGeneration_,intentId_,
                    {dispatched.x,dispatched.y,dispatched.z},
                    state_==GenericNavMeshFollowState::Moving);
                fromPoly=attribution.transition.from;
                toPoly=attribution.transition.to;
                issuedKnown=attribution.transition.Valid();
                Debug::Logger::Info("HARD STALL ATTRIBUTION commandSequence="+
                    std::to_string(issuedNavCommand_.commandSequence)+
                    " source="+IssuedSteeringCommandPolicy::SourceName(
                        issuedNavCommand_.source)+
                    " corridorFingerprint="+
                        std::to_string(issuedNavCommand_.corridorFingerprint)+
                    " target=("+Float(issuedNavCommand_.target.x)+","+
                        Float(issuedNavCommand_.target.y)+","+
                        Float(issuedNavCommand_.target.z)+")"+
                    " fromPoly="+HexPoly(fromPoly)+
                    " toPoly="+HexPoly(toPoly)+
                    " transitionKnown="+(issuedKnown?"yes":"no")+
                    " reason="+IssuedSteeringCommandPolicy::ReasonName(
                        attribution.reason));
            }
            if (provenSteeringFailure.Valid())
            {
                fromPoly = provenSteeringFailure.from;
                toPoly = provenSteeringFailure.to;
            }
            const bool transitionKnown = issuedKnown ||
                provenSteeringFailure.Valid();
            const CorridorFailureRecord previous = failedCorridor_;
            CorridorReplanHysteresisPolicy::RecordFailure(
                failedCorridor_, lastPathFingerprint_, player.x, player.y,
                RecoveryResetProgressDistance,
                transitionKnown, fromPoly, toPoly,
                !surfaceEpisodeActive_ ||
                    SurfaceRecoveryEpisodePolicy::Assess(
                        surfaceEpisodeInitialDistance_,Distance2D(player.x,player.y,
                            destination_.x,destination_.y),
                        RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_)==
                        SurfaceRecoveryQuality::Progress);
            if (!previous.active ||
                previous.fingerprint != failedCorridor_.fingerprint ||
                previous.anchorX != failedCorridor_.anchorX ||
                previous.anchorY != failedCorridor_.anchorY ||
                previous.transitionKnown != failedCorridor_.transitionKnown)
            {
                Debug::Logger::Info(
                    std::string("NAV 14N.2 HYSTERESIS: failedFingerprint=") +
                    std::to_string(failedCorridor_.fingerprint) +
                    " failedFromPoly=" + HexPoly(failedCorridor_.failedFromPoly) +
                    " failedToPoly=" + HexPoly(failedCorridor_.failedToPoly) +
                    " transitionKnown=" +
                    (failedCorridor_.transitionKnown ? "yes" : "no") +
                    " decision=record_failure reason=" + reason);
            }
        }

        bool BuildWallSafeSteeringPoint(
            const Objects::PlayerState& player,
            const NavPoint& point,
            NavPoint& adjusted,
            float& originalClearance,
            float& adjustedClearance,
            bool& originalClearanceKnown,
            bool& adjustedClearanceKnown)
        {
            adjusted = point;
            originalClearance = WallSteeringProbeRadius;
            adjustedClearance = originalClearance;
            originalClearanceKnown = false;
            adjustedClearanceKnown = false;

            NavPoint wallPoint{};
            if (!provider_.FindWallDistance(
                    point,
                    WallSteeringProbeRadius,
                    originalClearance,
                    wallPoint))
            {
                return false;
            }
            originalClearanceKnown = true;
            adjustedClearance = originalClearance;
            if (originalClearance >= WallSteeringMinimumClearance)
                return false;

            float awayX = point.x - wallPoint.x;
            float awayY = point.y - wallPoint.y;
            float awayLength = std::sqrt(awayX * awayX + awayY * awayY);

            if (!std::isfinite(awayLength) || awayLength < 0.05f)
            {
                awayX = player.x - wallPoint.x;
                awayY = player.y - wallPoint.y;
                awayLength = std::sqrt(awayX * awayX + awayY * awayY);
            }

            if (!std::isfinite(awayLength) || awayLength < 0.05f)
                return false;

            awayX /= awayLength;
            awayY /= awayLength;

            const float inset = std::min(
                WallSteeringMaximumInset,
                std::max(
                    0.0f,
                    WallSteeringDesiredClearance - originalClearance + 0.15f));

            if (inset <= 0.05f)
                return false;

            const NavPoint desired{
                point.x + awayX * inset,
                point.y + awayY * inset,
                point.z
            };

            NavPoint projected{};
            if (!provider_.ProjectGroundNear(desired, 1.5f, 3.0f, projected))
                return false;

            NavPoint projectedWall{};
            if (!provider_.FindWallDistance(
                    projected,
                    WallSteeringProbeRadius,
                    adjustedClearance,
                    projectedWall))
            {
                return false;
            }
            adjustedClearanceKnown = true;

            if (!SteeringSelectionPolicy::MeetsMinimumClearance(
                    adjustedClearance))
            {
                return false;
            }

            adjusted = projected;
            return true;
        }

        bool IssueWallTrapRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const NavPoint& toward)
        {
            if (surfaceRecoveryAttempts_ >= MaximumSurfaceRecoveryAttempts)
                return false;

            float currentClearance = WallTrapProbeRadius;
            NavPoint wallPoint{};
            if (!provider_.FindWallDistance(
                    PlayerPoint(player),
                    WallTrapProbeRadius,
                    currentClearance,
                    wallPoint) ||
                currentClearance >= WallTrapTriggerClearance)
            {
                return false;
            }

            float awayX = player.x - wallPoint.x;
            float awayY = player.y - wallPoint.y;
            float awayLength = std::sqrt(awayX * awayX + awayY * awayY);
            if (!std::isfinite(awayLength) || awayLength < 0.05f)
                return false;

            const float baseAngle = std::atan2(awayY, awayX);
            const float angleOffsets[] =
            {
                0.0f,
                0.610865238f, -0.610865238f,
                1.047197551f, -1.047197551f
            };
            const float probeDistances[] = { 2.75f, 4.00f, 5.50f };

            const float currentFinalDistance = Distance2D(
                player.x, player.y, destination_.x, destination_.y);
            const float currentCornerDistance = Distance2D(
                player.x, player.y, toward.x, toward.y);

            bool found = false;
            float bestScore = -1.0e30f;
            float bestClearance = currentClearance;
            NavPoint best{};

            for (const float probeDistance : probeDistances)
            {
                for (const float offset : angleOffsets)
                {
                    const float angle = baseAngle + offset;
                    const NavPoint desired{
                        player.x + std::cos(angle) * probeDistance,
                        player.y + std::sin(angle) * probeDistance,
                        player.z
                    };

                    NavPoint projected{};
                    if (!provider_.ProjectGroundNear(
                            desired, 1.75f, 3.25f, projected))
                    {
                        continue;
                    }

                    const float step = Distance2D(
                        player.x, player.y, projected.x, projected.y);
                    const float vertical = std::fabs(projected.z - player.z);
                    if (!std::isfinite(step) || !std::isfinite(vertical) ||
                        step < SurfaceRecoveryMinimumStep ||
                        step > SurfaceRecoveryMaximumStep ||
                        vertical > WallTrapMaximumVerticalDelta)
                    {
                        continue;
                    }

                    float candidateClearance = WallTrapProbeRadius;
                    NavPoint candidateWall{};
                    if (!provider_.FindWallDistance(
                            projected,
                            WallTrapProbeRadius,
                            candidateClearance,
                            candidateWall) ||
                        candidateClearance <
                            currentClearance + WallTrapMinimumClearanceGain)
                    {
                        continue;
                    }

                    float reachableFraction = 0.0f;
                    NavPoint reached{};
                    if (!provider_.IsSurfaceSegmentReachable(
                            PlayerPoint(player),
                            projected,
                            reachableFraction,
                            reached) ||
                        reachableFraction < 0.985f)
                    {
                        continue;
                    }

                    const float nextFinalDistance = Distance2D(
                        projected.x, projected.y, destination_.x, destination_.y);
                    const float nextCornerDistance = Distance2D(
                        projected.x, projected.y, toward.x, toward.y);
                    const float finalGain = currentFinalDistance - nextFinalDistance;
                    const float cornerGain = currentCornerDistance - nextCornerDistance;
                    const float clearanceGain = candidateClearance - currentClearance;

                    if (finalGain < -SurfaceRecoveryMaximumFinalDistanceLoss)
                        continue;

                    const float score =
                        clearanceGain * 8.0f +
                        finalGain * 1.5f +
                        cornerGain * 0.5f +
                        step * 0.10f -
                        vertical * 0.50f;

                    if (!found || score > bestScore)
                    {
                        found = true;
                        bestScore = score;
                        best = projected;
                        bestClearance = candidateClearance;
                    }
                }
            }

            if (!found)
            {
                Debug::Logger::Info(
                    "NAVMESH 14I.1: WALL TRAP detected but no validated "
                    "clearance escape was available; continuing bounded legacy recovery.");
                return false;
            }

            if (!IssueNavMovement(player,best,0.50f,
                    NavCommandSource::WallRecovery))
            {
                return false;
            }

            LearnPersistentHazard(tick, 2, "wall-proximity hard stall");
            ++commands_;
            ++surfaceRecoveryAttempts_;
            surfaceRecoveryActive_ = true;
            surfaceRecoveryStartX_ = player.x;
            surfaceRecoveryStartY_ = player.y;
            surfaceRecoveryTarget_ = best;
            BeginSurfaceRecoveryAttempt(player,best,Distance2D(
                player.x,player.y,destination_.x,destination_.y));
            surfaceRecoveryBestTargetDistance_ = Distance2D(
                player.x, player.y, best.x, best.y);
            surfaceRecoveryProgressTick_ = tick;
            lastCommandTick_ = tick;

            Debug::Logger::Info(
                "NAVMESH 14I.1: WALL TRAP RECOVERY issued clearance=" +
                Float(currentClearance) + " -> " + Float(bestClearance) +
                " target=(" + Float(best.x) + "," + Float(best.y) + "," +
                Float(best.z) + ") attempt=" +
                std::to_string(surfaceRecoveryAttempts_) + "/" +
                std::to_string(MaximumSurfaceRecoveryAttempts));
            return true;
        }

        bool IssueSurfaceRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const NavPoint& toward,
            const char* reason,
            DirectedPolyTransition provenSteeringFailure = {})
        {
            if (!RouteCostProbePolicy::MayIssueMovement(
                    startOptions_.planningOnly))
                return false;
            if (surfaceRecoveryAttempts_ >= MaximumSurfaceRecoveryAttempts)
                return false;

            const float dx = toward.x - player.x;
            const float dy = toward.y - player.y;
            const float headingLength = std::sqrt(dx * dx + dy * dy);
            if (!std::isfinite(headingLength) || headingLength < 0.10f)
                return false;

            const float baseAngle = std::atan2(dy, dx);
            const float angleOffsets[] =
            {
                0.610865238f, -0.610865238f,   // +/-35 deg
                1.047197551f, -1.047197551f,   // +/-60 deg
                1.570796327f, -1.570796327f,   // +/-90 deg
                2.094395102f, -2.094395102f,   // +/-120 deg
                3.141592654f                    // backtrack
            };
            const float distances[] = { 6.0f, 9.0f, 12.0f };

            const float currentFinalDistance = Distance2D(
                player.x, player.y, destination_.x, destination_.y);
            const float currentCornerDistance = Distance2D(
                player.x, player.y, toward.x, toward.y);

            bool found = false;
            bool repeatedLocalTarget = false;
            float bestScore = -1.0e30f;
            NavPoint best{};

            for (const float distance : distances)
            {
                for (const float offset : angleOffsets)
                {
                    const float angle = baseAngle + offset;
                    const NavPoint desired
                    {
                        player.x + std::cos(angle) * distance,
                        player.y + std::sin(angle) * distance,
                        player.z
                    };

                    NavPoint reached{};
                    if (!provider_.MoveAlongSurface(
                            PlayerPoint(player), desired, reached))
                    {
                        continue;
                    }

                    const float step = Distance2D(
                        player.x, player.y, reached.x, reached.y);
                    const float vertical = std::fabs(reached.z - player.z);
                    if (!std::isfinite(step) || !std::isfinite(vertical) ||
                        step < SurfaceRecoveryMinimumStep ||
                        step > SurfaceRecoveryMaximumStep ||
                        vertical > SurfaceRecoveryMaximumVerticalDelta)
                    {
                        continue;
                    }

                    const float nextFinalDistance = Distance2D(
                        reached.x, reached.y, destination_.x, destination_.y);
                    const float nextCornerDistance = Distance2D(
                        reached.x, reached.y, toward.x, toward.y);
                    const float finalGain = currentFinalDistance - nextFinalDistance;
                    const float cornerGain = currentCornerDistance - nextCornerDistance;

                    if (finalGain < -SurfaceRecoveryMaximumFinalDistanceLoss)
                        continue;

                    float fraction = 0.0f;
                    NavPoint rayReached{};
                    if (!provider_.IsSurfaceSegmentReachable(
                            PlayerPoint(player), reached, fraction, rayReached) ||
                        fraction < 0.985f)
                    {
                        continue;
                    }

                    const bool repeated=std::any_of(surfaceEpisodeTargets_.begin(),
                        surfaceEpisodeTargets_.end(),[&](const NavPoint& previous)
                        {
                            return SurfaceRecoveryEpisodePolicy::SameLocalTarget(
                                previous.x,previous.y,reached.x,reached.y,
                                SurfaceRecoveryArrivalDistance);
                        });
                    if(repeated && surfaceEpisodeActive_ &&
                        SurfaceRecoveryEpisodePolicy::Assess(
                            surfaceEpisodeInitialDistance_,currentFinalDistance,
                            RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_)!=
                            SurfaceRecoveryQuality::Progress)
                    { repeatedLocalTarget=true; continue; }

                    const float score =
                        finalGain * 3.0f +
                        cornerGain * 1.5f +
                        step * 0.15f -
                        vertical * 0.5f;

                    if (!found || score > bestScore)
                    {
                        found = true;
                        bestScore = score;
                        best = reached;
                    }
                }
            }

            if (!found)
            {
                if(repeatedLocalTarget)
                {
                    RegisterVerifiedSurfaceTransitionFailure(player,tick,
                        "repeated surface recovery target without route progress");
                    surfaceRecoveryAttempts_=MaximumSurfaceRecoveryAttempts;
                    Debug::Logger::Info("NAV RECOVERY ESCALATE intent="+
                        std::to_string(intentId_)+" episode="+
                        std::to_string(surfaceEpisodeId_)+
                        " decision=backtrack_or_fail reason=no_distinct_connected_recovery_target");
                }
                Debug::Logger::Info(
                    std::string("NAVMESH 13C.1: SURFACE RECOVERY unavailable reason=") +
                    reason);
                return false;
            }

            if (!IssueNavMovement(player,best,0.50f,
                    NavCommandSource::SurfaceRecovery))
            {
                return false;
            }

            if(surfaceEpisodeActive_ &&
                SurfaceRecoveryEpisodePolicy::Assess(
                    surfaceEpisodeInitialDistance_,currentFinalDistance,
                    RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_)!=
                    SurfaceRecoveryQuality::Progress)
            {
                const bool blocked=RegisterVerifiedSurfaceTransitionFailure(
                    player,tick,"repeated surface recovery without forward route progress");
                if(blocked)
                    Debug::Logger::Info("NAV RECOVERY ESCALATE intent="+
                        std::to_string(intentId_)+" episode="+
                        std::to_string(surfaceEpisodeId_)+
                        " decision=block_transition reason=bounded_directed_failure_evidence");
            }
            RegisterFailedCorridor(player, reason, false,
                provenSteeringFailure);
            ++commands_;
            ++surfaceRecoveryAttempts_;
            surfaceRecoveryActive_ = true;
            surfaceRecoveryStartX_ = player.x;
            surfaceRecoveryStartY_ = player.y;
            surfaceRecoveryTarget_ = best;
            BeginSurfaceRecoveryAttempt(player,best,currentFinalDistance,
                provenSteeringFailure);
            surfaceRecoveryBestTargetDistance_ = Distance2D(
                player.x, player.y, best.x, best.y);
            surfaceRecoveryProgressTick_ = tick;
            lastCommandTick_ = tick;

            Debug::Logger::Info(
                std::string("NAVMESH 13C.1: SURFACE RECOVERY issued reason=") +
                reason +
                " attempt=" + std::to_string(surfaceRecoveryAttempts_) +
                "/" + std::to_string(MaximumSurfaceRecoveryAttempts) +
                " target=(" + Float(best.x) + "," + Float(best.y) + "," +
                Float(best.z) + ") finalDistance=" +
                Float(currentFinalDistance));
            Debug::Logger::Info(
                std::string("NAVMESH 13D.7: MOVE ALONG SURFACE RECOVERY reason=") +
                reason + " target=(" + Float(best.x) + "," +
                Float(best.y) + "," + Float(best.z) + ")");

            return true;
        }

        bool UpdateSurfaceRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!surfaceRecoveryActive_)
                return false;

            const float targetDistance = Distance2D(
                player.x,
                player.y,
                surfaceRecoveryTarget_.x,
                surfaceRecoveryTarget_.y);

            if (targetDistance <= SurfaceRecoveryArrivalDistance)
            {
                const float moved = Distance2D(
                    player.x,
                    player.y,
                    surfaceRecoveryStartX_,
                    surfaceRecoveryStartY_);
                const float after=Distance2D(player.x,player.y,
                    destination_.x,destination_.y);
                const auto quality=SurfaceRecoveryEpisodePolicy::Assess(
                    surfaceEpisodeInitialDistance_,after,
                    RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_);
                float clearance=0;
                NavPoint wall{};
                const bool known=provider_.FindWallDistance(
                    PlayerPoint(player),WallSteeringProbeRadius,clearance,wall);
                Debug::Logger::Info("NAV RECOVERY EPISODE intent="+
                    std::to_string(intentId_)+" episode="+
                    std::to_string(surfaceEpisodeId_)+" attempt="+
                    std::to_string(surfaceRecoveryAttempts_)+
                    " routeGeneration="+std::to_string(routeGeneration_)+
                    " routeRefreshes="+std::to_string(surfaceEpisodeRouteRefreshes_)+
                    " end=("+Float(player.x)+","+Float(player.y)+","+Float(player.z)+")"+
                    " destinationBefore="+Float(surfaceRecoveryStartFinalDistance_)+
                    " destinationAfter="+Float(after)+
                    " progress="+Float(surfaceRecoveryStartFinalDistance_-after)+
                    " displacement="+Float(moved)+
                    " corridorBefore="+std::to_string(surfaceRecoveryStartCorridor_)+
                    " corridorAfter="+std::to_string(pointIndex_+1)+
                    " portalBefore="+HexPoly(surfaceRecoveryStartPortal_)+
                    " portalAfter="+HexPoly(pointIndex_<pointPolyRefs_.size()
                        ?pointPolyRefs_[pointIndex_]:0)+
                    " problemPoly="+HexPoly(surfaceRecoveryProblemFrom_)+
                    " problemNextPoly="+HexPoly(surfaceRecoveryProblemTo_)+
                    " clearanceAfter="+(known?Float(clearance):"unknown")+
                    " result="+RecoveryQualityName(quality));
                if(quality!=SurfaceRecoveryQuality::Progress)
                    Debug::Logger::Info("NAV RECOVERY RESET intent="+
                        std::to_string(intentId_)+" episode="+
                        std::to_string(surfaceEpisodeId_)+
                        " earnedProgress=no reason=target_reached_without_verified_forward_route");

                Debug::Logger::Info(
                    "NAVMESH 13C.1: SURFACE RECOVERY TARGET REACHED moved=" +
                    Float(moved) + " quality="+RecoveryQualityName(quality)+
                    " -> route refresh; episode budget retained until verified corridor progress.");

                surfaceRecoveryActive_ = false;
                ResetMotionWatchdog(player, tick);
                Replan(player, tick);
                return true;
            }

            if (targetDistance + SurfaceRecoveryProgressThreshold <
                surfaceRecoveryBestTargetDistance_)
            {
                surfaceRecoveryBestTargetDistance_ = targetDistance;
                surfaceRecoveryProgressTick_ = tick;
                return true;
            }

            if (tick >= surfaceRecoveryProgressTick_ &&
                tick - surfaceRecoveryProgressTick_ >= SurfaceRecoveryStallTicks)
            {
                const float after=Distance2D(player.x,player.y,
                    destination_.x,destination_.y);
                Debug::Logger::Info("NAV RECOVERY EPISODE intent="+
                    std::to_string(intentId_)+" episode="+
                    std::to_string(surfaceEpisodeId_)+" attempt="+
                    std::to_string(surfaceRecoveryAttempts_)+
                    " destinationBefore="+Float(surfaceRecoveryStartFinalDistance_)+
                    " destinationAfter="+Float(after)+
                    " progress="+Float(surfaceRecoveryStartFinalDistance_-after)+
                    " displacement="+Float(Distance2D(player.x,player.y,
                        surfaceRecoveryStartX_,surfaceRecoveryStartY_))+
                    " result=stalled");
                Debug::Logger::Info(
                    "NAVMESH 13C.1: SURFACE RECOVERY STALLED remaining=" +
                    Float(targetDistance) +
                    "; falling back to a fresh Detour replan.");
                RegisterCurrentTransitionFailure(
                    tick,
                    "surface recovery stalled");
                surfaceRecoveryActive_ = false;
                ResetMotionWatchdog(player, tick);
                Replan(player, tick);
                return true;
            }

            return true;
        }

        bool ValidatePath(
            const NavPathResult& path,
            std::string& error,
            float& length,
            NavigationPlanFailure* failure = nullptr,
            PathValidationDetail* detail = nullptr) const
        {
            if (failure)
                *failure = NavigationPlanFailure::None;
            if (detail)
                *detail = {};
            length =
                0.0f;

            if (!path.success)
            {
                if (failure)
                    *failure = NavigationPlanFailure::NoPath;
                error =
                    "Detour did not return a successful path.";
                return false;
            }

            // Phase 13D.6 validates bounded partial corridors in PlanFrom(),
            // where the live player and final destination are available. The
            // generic segment/length checks below still apply unchanged.

            if (path.points.size() < 2)
            {
                if (detail) detail->reason =
                    PathValidationSubreason::TooFewSteeringPoints;
                if (failure)
                    *failure = NavigationPlanFailure::PathValidationFailed;
                error =
                    "Detour returned fewer than two steering points.";
                return false;
            }

            for (
                std::size_t i = 1;
                i < path.points.size();
                ++i)
            {
                const float segment =
                    Distance3D(
                        path.points[i - 1],
                        path.points[i]
                    );

                const float vertical =
                    std::fabs(
                        path.points[i].z -
                        path.points[i - 1].z
                    );

                if (
                    !std::isfinite(
                        segment
                    ) ||
                    segment >
                        MaximumSegment)
                {
                    if (detail)
                    {
                        detail->reason=PathValidationSubreason::ImplausibleSegment;
                        detail->pointIndex=i;
                        detail->segmentLength=segment;
                    }
                    if (failure)
                        *failure = NavigationPlanFailure::PathValidationFailed;
                    error =
                        "implausible segment " +
                        std::to_string(i) +
                        ": " +
                        Float(segment);

                    return false;
                }

                if (
                    !std::isfinite(
                        vertical
                    ) ||
                    vertical >
                        MaximumVerticalSegment)
                {
                    if (detail)
                    {
                        detail->reason=
                            PathValidationSubreason::ImplausibleVerticalSegment;
                        detail->pointIndex=i;
                        detail->verticalDelta=vertical;
                    }
                    if (failure)
                        *failure = NavigationPlanFailure::PathValidationFailed;
                    error =
                        "implausible vertical segment " +
                        std::to_string(i) +
                        ": " +
                        Float(vertical);

                    return false;
                }

                length +=
                    segment;

                if (LongPathDiagnosticPolicy::ExceedsSafetyLength(
                        length, MaximumPathLength))
                {
                    if (detail)
                    {
                        detail->reason=
                            PathValidationSubreason::SafetyLengthExceeded;
                        detail->pointIndex=i;
                        detail->segmentLength=length;
                    }
                    if (failure)
                        *failure = NavigationPlanFailure::PathLengthExceeded;
                    error =
                        "path exceeds generic Phase 11B "
                        "safety length.";

                    return false;
                }
            }

            error.clear();

            return true;
        }

        void LogLongPathDiagnostic(const Objects::PlayerState& player,
                                   const NavPathResult& path,
                                   float rejectedPrefixLength) const
        {
            const auto measured =
                LongPathDiagnosticPolicy::MeasureStraightPath(path.points);
            const bool polygonBufferFilled =
                LongPathDiagnosticPolicy::PolygonBufferFilled(
                    path.polygonCount, path.maximumPolygons);
            const bool straightBufferFilled =
                path.maximumStraightPoints > 0 &&
                path.points.size() >=
                    static_cast<std::size_t>(path.maximumStraightPoints);
            const char* capacityHit =
                path.findPathBufferTooSmall ||
                path.findStraightPathBufferTooSmall
                    ? "yes"
                    : polygonBufferFilled || straightBufferFilled
                        ? "possible" : "no";

            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3)
                << "NAV 14N.4 LONG PATH DIAGNOSTIC: mode="
                << NavigationInitTelemetryPolicy::TierName(currentInitTier_)
                << " directDistance="
                << Distance3D(PlayerPoint(player), destination_)
                << " directDistanceMetric=3d"
                << " polygonCount=" << path.polygonCount
                << " maxPolygons=" << path.maximumPolygons
                << " straightPathPointCount=" << path.points.size()
                << " maxStraightPoints=" << path.maximumStraightPoints
                << " reachedEnd="
                << (LongPathDiagnosticPolicy::ReachedDestination(
                        path.lastPoly, path.endPoly) ? "yes" : "no")
                << " partial=" << (path.partial ? "yes" : "no")
                << " detourPartialResult="
                << (path.findPathPartialResult ? "yes" : "no")
                << " outOfNodes="
                << (path.findPathOutOfNodes ? "yes" : "no")
                << " capacityHit=" << capacityHit
                << " polygonBufferFilled="
                << (polygonBufferFilled ? "yes" : "no")
                << " straightBufferFilled="
                << (straightBufferFilled ? "yes" : "no")
                << " corridorLength=unknown"
                << " straightPathLength=";
            if (measured.known)
                stream << measured.straightPathLength;
            else
                stream << "unknown";
            stream << " safetyLimit=" << MaximumPathLength
                << " rejectionMetric=validated_straight_path_prefix_3d"
                << " rejectionValue=" << rejectedPrefixLength
                << " rejectionLimit=" << MaximumPathLength
                << " startPoly=" << HexPoly(path.startPoly)
                << " lastPoly=" << HexPoly(path.lastPoly)
                << " endPoly=" << HexPoly(path.endPoly)
                << " findPathStatus=0x" << std::hex << path.findPathStatus
                << " findStraightPathStatus=0x" << path.findStraightPathStatus
                << std::dec
                << " queryNodePoolSize=" << path.queryNodePoolSize
                << " loadedTiles=" << path.loadedTiles;
            Debug::Logger::Info(stream.str());
        }

        void LogPath(
            const NavPathResult& path,
            float length,
            bool replan) const
        {
            if (!replan &&
                destinationLabel_.find("death recovery corpse route") !=
                    std::string::npos)
            {
                std::ostringstream prefixPolys;
                const std::size_t polyCount =
                    std::min<std::size_t>(path.corridorPolys.size(), 12);
                for (std::size_t i = 0; i < polyCount; ++i)
                {
                    if (i != 0) prefixPolys << ',';
                    prefixPolys << HexPoly(path.corridorPolys[i]);
                }
                std::ostringstream prefixPoints;
                const std::size_t pointCount =
                    std::min<std::size_t>(path.points.size(), 8);
                for (std::size_t i = 0; i < pointCount; ++i)
                {
                    if (i != 0) prefixPoints << ';';
                    prefixPoints << '(' << Float(path.points[i].x) << ','
                                 << Float(path.points[i].y) << ','
                                 << Float(path.points[i].z) << ')';
                }
                Debug::Logger::Info(
                    "NAV 15B.0 CORPSE ROUTE: mode=" +
                    std::string(NavigationInitTelemetryPolicy::TierName(currentInitTier_)) +
                    " destination=(" + Float(destination_.x) + "," +
                    Float(destination_.y) + "," + Float(destination_.z) + ")" +
                    " pathLength=" + Float(length) +
                    " corridorFingerprint=" +
                    std::to_string(FingerprintPath(path.points)) +
                    " polygonCount=" + std::to_string(path.polygonCount) +
                    " startPoly=" + HexPoly(path.startPoly) +
                    " endPoly=" + HexPoly(path.endPoly) +
                    " prefixPolys=" + prefixPolys.str() +
                    " prefixPoints=" + prefixPoints.str());
            }
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                replan
                    ? "NAVMESH 11B: REPLAN COMPLETE"
                    : "NAVMESH 11B: INITIAL PLAN COMPLETE"
            );

            Debug::Logger::Info(
                "Destination: " +
                destinationLabel_
            );

            Debug::Logger::Info(
                "Loaded tiles: " +
                std::to_string(
                    path.loadedTiles
                )
            );

            Debug::Logger::Info(
                "Polygon path count: " +
                std::to_string(
                    path.polygonCount
                )
            );

            Debug::Logger::Info(
                "Dense portal steering points: " +
                std::to_string(
                    path.points.size()
                )
            );

            Debug::Logger::Info(
                "NAVMESH 13D.1: corridor refs=" +
                std::to_string(path.corridorPolys.size()) +
                " steering refs=" +
                std::to_string(path.pointPolys.size()) +
                " start=" + HexPoly(path.startPoly) +
                " end=" + HexPoly(path.endPoly));

            if (path.avoidanceActive)
            {
                Debug::Logger::Info(
                    "NAVMESH 13D.4: ALTERNATIVE CORRIDOR QUERY requested=" +
                    std::to_string(path.avoidanceRequestedCount) +
                    " applied=" +
                    std::to_string(path.avoidanceAppliedCount));
            }

            Debug::Logger::Info(
                "Path length: " +
                Float(
                    length
                )
            );

            Debug::Logger::Info(
                "================================"
            );
        }

        void ResetMotionWatchdog(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            motionWatchdogInitialized_ = true;
            lastMotionX_ = player.x;
            lastMotionY_ = player.y;
            lastMotionTick_ = tick;
        }

        void ResetEscalatingRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            float finalDistance)
        {
            // Local escape/backtrack completion is not destination progress.
            // Preserve episode counters and bad-transition evidence unless the
            // same progress condition used by ordinary following is satisfied.
            const auto quality=SurfaceRecoveryEpisodePolicy::Assess(
                surfaceEpisodeInitialDistance_,finalDistance,
                RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_);
            const bool earned=surfaceEpisodeActive_
                ? quality==SurfaceRecoveryQuality::Progress
                : LocalRecoveryExhaustionPolicy::EarnedProgressReset(
                    finalDistance,hardStallBestFinalDistance_,RecoveryResetProgressDistance);
            if (!earned)
            {
                ResetMotionWatchdog(player, tick);
                if(surfaceEpisodeActive_)
                    Debug::Logger::Info("NAV RECOVERY RESET intent="+
                        std::to_string(intentId_)+" episode="+
                        std::to_string(surfaceEpisodeId_)+
                        " earnedProgress=no reason=forward_portal_and_destination_progress_required");
                Debug::Logger::Info(
                    "NAV STUCK RECOVERY action=refresh_position budgetReset=no reason="+
                    std::string(surfaceEpisodeActive_
                        ?"no_verified_corridor_progress":"no_destination_progress"));
                return;
            }
            NavigationHazardMemory::Instance().ObserveTraversalSuccess(
                mapId_, PlayerPoint(player), tick);
            if(surfaceEpisodeActive_)
            {
                Debug::Logger::Info("NAV RECOVERY RESET intent="+
                    std::to_string(intentId_)+" episode="+
                    std::to_string(surfaceEpisodeId_)+
                    " earnedProgress=yes reason=ordinary_portal_crossed_and_destination_gain");
                surfaceEpisodeActive_=false;
                surfaceEpisodeTargets_.clear();
                surfaceEpisodeForwardPortal_=false;
                surfaceEpisodeRouteRefreshes_=0;
            }
            hardStallEpisodes_ = 0;
            hardStallBestFinalDistance_ = finalDistance;

            if (replans_ > 0)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.3: meaningful progress earned fresh replan budget; "
                    "episodeReplans=" + std::to_string(replans_) +
                    " lifetimeReplans=" + std::to_string(totalReplans_)
                );
            }

            replans_ = 0;
            badVerticalTransitions_.ClearStallEvidence();
            boundaryVerticalAvoidanceAttempted_ = false;
            escapeProbeActive_ = false;
            escapeProbeSide_ = 0;
            escapeProbeIssuedTick_ = 0;
            escapeExhaustionRecoveries_ = 0;
            surfaceRecoveryActive_ = false;
            surfaceRecoveryAttempts_ = 0;
            surfaceEpisodeActive_ = false;
            surfaceEpisodeTargets_.clear();
            surfaceEpisodeForwardPortal_ = false;
            surfaceEpisodeRouteRefreshes_ = 0;
            surfaceRecoveryProgressTick_ = 0;
            repeatedCorridorPlans_ = 0;
            finalApproachActive_ = false;
            finalApproachSuppressed_ = false;
            finalApproachIssuedTick_ = 0;
            ResetMotionWatchdog(player, tick);
        }

        bool IsUnsafeVerticalPortal(
            const Objects::PlayerState& player,
            const NavPoint& point,
            float& horizontal,
            float& vertical) const
        {
            const auto geometry = TerrainTransitionPolicy::Assess(
                player, point);
            horizontal = geometry.horizontal;
            vertical = geometry.vertical;
            return geometry.rejected;
        }

        bool ValidateVerticalBarrierBypassRoute(
            const NavPathResult& path,
            float& routeLength) const
        {
            routeLength = 0.0f;

            if (
                !path.success ||
                path.partial ||
                path.points.size() < 2)
            {
                return false;
            }

            for (
                std::size_t index = 1;
                index < path.points.size();
                ++index)
            {
                const float segment = Distance3D(
                    path.points[index - 1],
                    path.points[index]
                );

                const float vertical = std::fabs(
                    path.points[index].z -
                    path.points[index - 1].z
                );

                if (
                    !std::isfinite(segment) ||
                    !std::isfinite(vertical) ||
                    segment > VerticalBarrierBypassRouteMaximumSegment ||
                    vertical > VerticalBarrierBypassRouteMaximumVerticalSegment)
                {
                    return false;
                }

                routeLength += segment;

                if (
                    !std::isfinite(routeLength) ||
                    routeLength > VerticalBarrierBypassRouteMaximumLength)
                {
                    return false;
                }
            }

            return true;
        }

        bool FindVerticalBarrierBypassTarget(
            const Objects::PlayerState& player,
            const NavPoint& portal,
            NavPoint& target,
            std::vector<NavPoint>& routePoints,
            float& routeLength,
            float& targetFinalGain,
            float& targetPortalGain)
        {
            target = NavPoint{};
            routePoints.clear();
            routeLength = 0.0f;
            targetFinalGain = 0.0f;
            targetPortalGain = 0.0f;

            const float destinationDx = destination_.x - player.x;
            const float destinationDy = destination_.y - player.y;
            const float destinationLength = std::sqrt(
                destinationDx * destinationDx +
                destinationDy * destinationDy
            );

            if (
                !std::isfinite(destinationLength) ||
                destinationLength < 0.10f)
            {
                return false;
            }

            const float baseAngle = std::atan2(
                destinationDy,
                destinationDx
            );

            // Phase 12B.14 keeps the 12B.11 fan search, but a projected
            // endpoint is accepted only if Detour can build a short,
            // non-partial, locally sane route from the live player position
            // to that endpoint. This prevents a valid point on the far side
            // of a cliff from being sent directly to CTM.
            const float angleOffsets[] =
            {
                0.0f,
                0.436332313f, -0.436332313f,
                0.785398163f, -0.785398163f,
                1.047197551f, -1.047197551f,
                1.570796327f, -1.570796327f,
                2.094395102f, -2.094395102f,
                3.141592654f
            };

            const float probeDistances[] =
            {
                4.50f,
                6.00f,
                3.00f
            };

            const float currentFinalDistance = Distance2D(
                player.x,
                player.y,
                destination_.x,
                destination_.y
            );

            const float currentPortalDistance = Distance2D(
                player.x,
                player.y,
                portal.x,
                portal.y
            );

            bool found = false;
            float bestScore = -1.0e30f;

            for (const float probeDistance : probeDistances)
            {
                for (const float angleOffset : angleOffsets)
                {
                    const float angle = baseAngle + angleOffset;

                    const NavPoint probe
                    {
                        player.x + std::cos(angle) * probeDistance,
                        player.y + std::sin(angle) * probeDistance,
                        player.z
                    };

                    NavPoint projected{};

                    if (!provider_.ProjectGroundNear(
                            probe,
                            VerticalBypassProbeHorizontalExtent,
                            VerticalBypassProbeVerticalExtent,
                            projected))
                    {
                        continue;
                    }

                    const float stepDistance = Distance2D(
                        player.x,
                        player.y,
                        projected.x,
                        projected.y
                    );

                    const float verticalDelta = std::fabs(
                        projected.z - player.z
                    );

                    if (
                        !std::isfinite(stepDistance) ||
                        !std::isfinite(verticalDelta) ||
                        stepDistance < VerticalBypassMinimumStepProgress ||
                        verticalDelta > VerticalBypassMaximumProjectedDelta)
                    {
                        continue;
                    }

                    const float candidateFinalDistance = Distance2D(
                        projected.x,
                        projected.y,
                        destination_.x,
                        destination_.y
                    );

                    const float candidatePortalDistance = Distance2D(
                        projected.x,
                        projected.y,
                        portal.x,
                        portal.y
                    );

                    const float finalGain =
                        currentFinalDistance - candidateFinalDistance;

                    const float portalGain =
                        currentPortalDistance - candidatePortalDistance;

                    if (
                        !std::isfinite(finalGain) ||
                        !std::isfinite(portalGain) ||
                        finalGain < -VerticalBypassMaximumFinalDistanceLoss)
                    {
                        continue;
                    }

                    NavPathResult localPath{};
                    if (!provider_.FindPath(
                            PlayerPoint(player),
                            projected,
                            localPath))
                    {
                        continue;
                    }

                    float localRouteLength = 0.0f;
                    if (!ValidateVerticalBarrierBypassRoute(
                            localPath,
                            localRouteLength))
                    {
                        continue;
                    }

                    // Phase 12B.15: if a several-yard candidate produces
                    // only start+end points, Detour has not given CTM any
                    // intermediate steering around world geometry. The cliff
                    // screenshot/runtime behaviour showed that such a sparse
                    // route can still point straight through an obstacle.
                    if (
                        stepDistance > CliffDetourSparseRouteDirectLimit &&
                        localPath.points.size() < 3)
                    {
                        continue;
                    }

                    // Prefer real destination progress, but penalize a large
                    // Detour detour relative to the direct probe. This still
                    // permits going around a local wall when needed.
                    const float routeInflation = std::max(
                        0.0f,
                        localRouteLength - stepDistance
                    );

                    const float score =
                        finalGain * 4.0f +
                        portalGain * 0.35f -
                        verticalDelta * 0.30f +
                        stepDistance * 0.05f -
                        routeInflation * 0.20f;

                    if (
                        !found ||
                        score > bestScore)
                    {
                        found = true;
                        bestScore = score;
                        target = projected;
                        routePoints = localPath.points;
                        routeLength = localRouteLength;
                        targetFinalGain = finalGain;
                        targetPortalGain = portalGain;
                    }
                }
            }

            return found;
        }

        bool ValidateCliffDetourRoute(
            const NavPathResult& path,
            float& routeLength) const
        {
            routeLength = 0.0f;

            if (
                !path.success ||
                path.partial ||
                path.points.size() < 2)
            {
                return false;
            }

            for (
                std::size_t index = 1;
                index < path.points.size();
                ++index)
            {
                const float horizontal = Distance2D(
                    path.points[index - 1].x,
                    path.points[index - 1].y,
                    path.points[index].x,
                    path.points[index].y
                );

                const float segment = Distance3D(
                    path.points[index - 1],
                    path.points[index]
                );

                const float vertical = std::fabs(
                    path.points[index].z -
                    path.points[index - 1].z
                );

                if (
                    !std::isfinite(horizontal) ||
                    !std::isfinite(segment) ||
                    !std::isfinite(vertical) ||
                    segment > CliffDetourRouteMaximumSegment ||
                    vertical > CliffDetourRouteMaximumVerticalSegment)
                {
                    return false;
                }

                // Keep the temporary route away from the exact failure class:
                // a large elevation jump over very little horizontal travel.
                if (
                    vertical >= VerticalPortalGuardMinimumDelta &&
                    horizontal <= VerticalPortalGuardMaximumHorizontal &&
                    vertical /
                        std::max(horizontal, 0.25f) >=
                            VerticalPortalGuardMinimumSlopeRatio)
                {
                    return false;
                }

                routeLength += segment;

                if (
                    !std::isfinite(routeLength) ||
                    routeLength > CliffDetourRouteMaximumLength)
                {
                    return false;
                }
            }

            return true;
        }

        bool HasSafeCliffDetourOnwardPrefix(
            const NavPathResult& path) const
        {
            if (
                !path.success ||
                path.partial ||
                path.points.size() < 2)
            {
                return false;
            }

            float inspected = 0.0f;

            for (
                std::size_t index = 1;
                index < path.points.size();
                ++index)
            {
                const NavPoint& previous = path.points[index - 1];
                const NavPoint& current = path.points[index];

                const float horizontal = Distance2D(
                    previous.x,
                    previous.y,
                    current.x,
                    current.y
                );

                const float vertical = std::fabs(
                    current.z - previous.z
                );

                const float segment = Distance3D(previous, current);

                if (
                    !std::isfinite(horizontal) ||
                    !std::isfinite(vertical) ||
                    !std::isfinite(segment))
                {
                    return false;
                }

                if (
                    vertical >= VerticalPortalGuardMinimumDelta &&
                    horizontal <= VerticalPortalGuardMaximumHorizontal &&
                    vertical /
                        std::max(horizontal, 0.25f) >=
                            VerticalPortalGuardMinimumSlopeRatio)
                {
                    return false;
                }

                inspected += segment;

                if (inspected >= CliffDetourSafeOnwardPrefixLength)
                {
                    break;
                }
            }

            return true;
        }

        static float PathLength(const NavPathResult& path)
        {
            float length = 0.0f;

            for (
                std::size_t index = 1;
                index < path.points.size();
                ++index)
            {
                const float segment = Distance3D(
                    path.points[index - 1],
                    path.points[index]
                );

                if (!std::isfinite(segment))
                {
                    return 1.0e30f;
                }

                length += segment;
            }

            return length;
        }

        bool FindCliffDetourAnchor(
            const Objects::PlayerState& player,
            const NavPoint& blockedPortal,
            NavPoint& target,
            std::vector<NavPoint>& routePoints,
            float& routeLength,
            float& targetFinalGain,
            float& targetPortalClearance)
        {
            target = NavPoint{};
            routePoints.clear();
            routeLength = 0.0f;
            targetFinalGain = 0.0f;
            targetPortalClearance = 0.0f;

            const float blockedDx = blockedPortal.x - player.x;
            const float blockedDy = blockedPortal.y - player.y;
            const float blockedLength = std::sqrt(
                blockedDx * blockedDx +
                blockedDy * blockedDy
            );

            if (
                !std::isfinite(blockedLength) ||
                blockedLength < 0.25f)
            {
                return false;
            }

            const float blockedAngle = std::atan2(
                blockedDy,
                blockedDx
            );

            // Deliberately avoid the blocked forward ray. Start perpendicular
            // to it, then widen toward backward headings so a long cliff face
            // can be rounded instead of climbed.
            const float angleOffsets[] =
            {
                1.570796327f, -1.570796327f,
                2.094395102f, -2.094395102f,
                1.047197551f, -1.047197551f,
                2.617993878f, -2.617993878f,
                3.141592654f
            };

            const float probeDistances[] =
            {
                12.0f,
                18.0f,
                24.0f,
                30.0f,
                36.0f
            };

            const float currentFinalDistance = Distance2D(
                player.x,
                player.y,
                destination_.x,
                destination_.y
            );

            const float currentPortalDistance = Distance2D(
                player.x,
                player.y,
                blockedPortal.x,
                blockedPortal.y
            );

            bool found = false;
            float bestScore = -1.0e30f;

            for (const float probeDistance : probeDistances)
            {
                for (const float angleOffset : angleOffsets)
                {
                    const float angle = blockedAngle + angleOffset;

                    const NavPoint probe
                    {
                        player.x + std::cos(angle) * probeDistance,
                        player.y + std::sin(angle) * probeDistance,
                        player.z
                    };

                    NavPoint projected{};

                    if (!provider_.ProjectGroundNear(
                            probe,
                            CliffDetourProbeHorizontalExtent,
                            CliffDetourProbeVerticalExtent,
                            projected))
                    {
                        continue;
                    }

                    const float anchorDistance = Distance2D(
                        player.x,
                        player.y,
                        projected.x,
                        projected.y
                    );

                    const float verticalDelta = std::fabs(
                        projected.z - player.z
                    );

                    if (
                        !std::isfinite(anchorDistance) ||
                        !std::isfinite(verticalDelta) ||
                        anchorDistance < CliffDetourMinimumAnchorDistance ||
                        verticalDelta > CliffDetourMaximumProjectedDelta)
                    {
                        continue;
                    }

                    const float candidateFinalDistance = Distance2D(
                        projected.x,
                        projected.y,
                        destination_.x,
                        destination_.y
                    );

                    const float finalGain =
                        currentFinalDistance - candidateFinalDistance;

                    if (
                        !std::isfinite(finalGain) ||
                        finalGain < -CliffDetourMaximumFinalDistanceLoss)
                    {
                        continue;
                    }

                    NavPathResult localPath{};
                    if (!provider_.FindPath(
                            PlayerPoint(player),
                            projected,
                            localPath))
                    {
                        continue;
                    }

                    float localLength = 0.0f;
                    if (!ValidateCliffDetourRoute(
                            localPath,
                            localLength))
                    {
                        continue;
                    }

                    // A long start/end-only path gives CTM no steering around
                    // a wall. Reject it rather than trusting a coarse polygon
                    // that spans non-traversable world geometry.
                    if (
                        anchorDistance > CliffDetourSparseRouteDirectLimit &&
                        localPath.points.size() < 3)
                    {
                        continue;
                    }

                    NavPathResult onwardPath{};
                    if (!provider_.FindPath(
                            projected,
                            destination_,
                            onwardPath))
                    {
                        continue;
                    }

                    if (!HasSafeCliffDetourOnwardPrefix(onwardPath))
                    {
                        continue;
                    }

                    const float onwardLength = PathLength(onwardPath);
                    if (!std::isfinite(onwardLength))
                    {
                        continue;
                    }

                    const float candidatePortalDistance = Distance2D(
                        projected.x,
                        projected.y,
                        blockedPortal.x,
                        blockedPortal.y
                    );

                    const float portalClearance =
                        candidatePortalDistance - currentPortalDistance;

                    // Prefer an anchor that can be reached through a sane
                    // local route and whose onward path does not immediately
                    // recreate the same steep transition. Some temporary loss
                    // toward the final destination is intentionally allowed.
                    const float score =
                        finalGain * 2.0f +
                        portalClearance * 0.45f -
                        localLength * 0.20f -
                        onwardLength * 0.01f +
                        static_cast<float>(localPath.points.size()) * 0.10f;

                    if (
                        !found ||
                        score > bestScore)
                    {
                        found = true;
                        bestScore = score;
                        target = projected;
                        routePoints = localPath.points;
                        routeLength = localLength;
                        targetFinalGain = finalGain;
                        targetPortalClearance = portalClearance;
                    }
                }
            }

            return found;
        }

        bool IssueVerticalBarrierBypassRouteCorner(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const char* reason)
        {
            while (
                verticalPortalBypassRouteIndex_ <
                    verticalPortalBypassRoutePoints_.size())
            {
                const NavPoint& point =
                    verticalPortalBypassRoutePoints_[
                        verticalPortalBypassRouteIndex_
                    ];

                const float distance = Distance2D(
                    player.x,
                    player.y,
                    point.x,
                    point.y
                );

                if (
                    distance <= VerticalBarrierBypassRouteCornerArrivalDistance &&
                    verticalPortalBypassRouteIndex_ + 1 <
                        verticalPortalBypassRoutePoints_.size())
                {
                    ++verticalPortalBypassRouteIndex_;
                    continue;
                }

                Debug::Logger::Info(
                    "NAVMESH 12B.14: BYPASS ROUTE CTM point=" +
                    std::to_string(verticalPortalBypassRouteIndex_ + 1) +
                    "/" +
                    std::to_string(verticalPortalBypassRoutePoints_.size()) +
                    " reason=" + reason +
                    " distance=" + Float(distance) +
                    " target=(" + Float(point.x) + "," +
                    Float(point.y) + "," + Float(point.z) + ")"
                );

                if (!IssueNavMovement(player,point,0.50f,
                        NavCommandSource::VerticalRecovery))
                {
                    return false;
                }

                ++commands_;
                verticalPortalBypassBestCornerDistance_ = distance;
                verticalPortalRecoveryProgressTick_ = tick;
                lastCommandTick_ = tick;
                lastProgressTick_ = tick;
                ResetMotionWatchdog(player, tick);
                return true;
            }

            return false;
        }

        bool IssueVerticalPortalRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const NavPoint& point,
            float horizontal,
            float vertical,
            std::size_t candidateIndex)
        {
            const float currentFinalDistance = Distance2D(
                player.x,
                player.y,
                destination_.x,
                destination_.y
            );

            const float anchorTravel =
                verticalPortalRecoveryAnchorValid_
                    ? Distance2D(
                        player.x,
                        player.y,
                        verticalPortalRecoveryAnchorX_,
                        verticalPortalRecoveryAnchorY_
                    )
                    : 0.0f;

            const float finalGain =
                verticalPortalRecoveryAnchorValid_ &&
                std::isfinite(verticalPortalRecoveryBudgetBestFinalDistance_)
                    ? verticalPortalRecoveryBudgetBestFinalDistance_ -
                        currentFinalDistance
                    : 0.0f;

            const bool resetEpisode =
                !verticalPortalRecoveryAnchorValid_ ||
                anchorTravel >= VerticalPortalRecoveryAnchorResetDistance ||
                finalGain >= VerticalPortalRecoveryBudgetResetFinalProgress;

            if (resetEpisode)
            {
                if (
                    verticalPortalRecoveryAnchorValid_ &&
                    verticalPortalRecoveryAttempts_ > 0)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.12: meaningful vertical recovery progress "
                        "earned fresh attempt budget; attemptsUsed=" +
                        std::to_string(verticalPortalRecoveryAttempts_) +
                        " totalAttempts=" +
                        std::to_string(verticalPortalRecoveryTotalAttempts_) +
                        " anchorTravel=" + Float(anchorTravel) +
                        " finalGain=" + Float(finalGain)
                    );
                }

                verticalPortalRecoveryAnchorValid_ = true;
                verticalPortalRecoveryAnchorX_ = player.x;
                verticalPortalRecoveryAnchorY_ = player.y;
                verticalPortalRecoveryAttempts_ = 0;
                verticalPortalRecoveryBudgetBestFinalDistance_ =
                    currentFinalDistance;
                badVerticalTransitions_.ClearObservation();
            }

            Debug::Logger::Info(
                "NAVMESH 12B.10: UNSAFE VERTICAL PORTAL player=(" +
                Float(player.x) + "," + Float(player.y) + "," +
                Float(player.z) + ") portal=(" +
                Float(point.x) + "," + Float(point.y) + "," +
                Float(point.z) + ") horizontal=" +
                Float(horizontal) + " vertical=" + Float(vertical) +
                " ratio=" + Float(vertical / horizontal)
            );

            const DirectedPolyTransition portalEdge =
                VerticalCandidateTransition(candidateIndex);
            if ((verticalPortalRecoveryAttempts_ >=
                     MaximumVerticalPortalRecoveryAttempts ||
                 verticalPortalRecoveryTotalAttempts_ >=
                     MaximumVerticalPortalRecoveryTotalAttempts) &&
                badVerticalTransitions_.ExhaustionProvesEdge(
                    portalEdge, verticalPortalRecoveryAttempts_,
                    MaximumVerticalPortalRecoveryAttempts) &&
                badVerticalTransitions_.Learn(portalEdge))
            {
                verticalTransitionAvoidancePending_ = true;
                Debug::Logger::Info(
                    "NAV 15B.1 TRANSITION LEARNED fromPoly=" +
                    HexPoly(portalEdge.from) + " toPoly=" +
                    HexPoly(portalEdge.to) +
                    " reason=vertical_recovery_exhausted" +
                    " stalledObservations=" + std::to_string(
                        badVerticalTransitions_.StalledObservations()) +
                    " verticalAttempts=" + std::to_string(
                        verticalPortalRecoveryAttempts_) +
                    " corridorFingerprint=" +
                    std::to_string(lastPathFingerprint_) +
                    " occurrence=" + std::to_string(
                        badVerticalTransitions_.ConsecutiveIssued()) +
                    " avoidedTransitions=" + std::to_string(
                        badVerticalTransitions_.Size()));
                StopAtCurrentPosition(player);
                return true; // Replan on the next Update, never recursively here.
            }

            if (
                verticalPortalRecoveryTotalAttempts_ >=
                    MaximumVerticalPortalRecoveryTotalAttempts)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.12: VERTICAL PORTAL RECOVERY LIFETIME "
                    "SAFETY LIMIT reached; totalAttempts=" +
                    std::to_string(verticalPortalRecoveryTotalAttempts_) +
                    "; returning control to normal stuck recovery."
                );
                return false;
            }

            if (
                verticalPortalRecoveryAttempts_ >=
                    MaximumVerticalPortalRecoveryAttempts)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.10: VERTICAL PORTAL RECOVERY EXHAUSTED "
                    "for current progress episode; returning control to "
                    "normal stuck recovery."
                );
                return false;
            }

            const float dx = point.x - player.x;
            const float dy = point.y - player.y;
            const float length = std::sqrt(dx * dx + dy * dy);

            if (!std::isfinite(length) || length < 0.10f)
            {
                return false;
            }

            const float ux = dx / length;
            const float uy = dy / length;

            // Prefer the furthest bounded step. If the lower surface ends
            // before that distance, progressively shorten the probe.
            const float probeDistances[] =
            {
                3.00f,
                2.00f,
                1.25f
            };

            NavPoint best{};
            bool haveBest = false;
            float bestProgress = 0.0f;

            for (const float probeDistance : probeDistances)
            {
                const float boundedDistance = std::min(
                    probeDistance,
                    std::max(0.0f, horizontal - 0.35f)
                );

                if (boundedDistance < VerticalPortalMinimumStepProgress)
                {
                    continue;
                }

                const NavPoint probe
                {
                    player.x + ux * boundedDistance,
                    player.y + uy * boundedDistance,
                    player.z
                };

                NavPoint projected{};

                if (!provider_.ProjectGroundNear(
                        probe,
                        VerticalPortalProbeHorizontalExtent,
                        VerticalPortalProbeVerticalExtent,
                        projected))
                {
                    continue;
                }

                const float projectedProgress = Distance2D(
                    player.x,
                    player.y,
                    projected.x,
                    projected.y
                );

                const float projectedVertical = std::fabs(
                    projected.z - player.z
                );

                const float forwardDot =
                    (projected.x - player.x) * ux +
                    (projected.y - player.y) * uy;

                if (
                    !std::isfinite(projectedProgress) ||
                    !std::isfinite(projectedVertical) ||
                    projectedProgress < VerticalPortalMinimumStepProgress ||
                    projectedVertical > VerticalPortalMaximumProjectedDelta ||
                    forwardDot < VerticalPortalMinimumStepProgress)
                {
                    continue;
                }

                if (
                    !haveBest ||
                    projectedProgress > bestProgress)
                {
                    best = projected;
                    bestProgress = projectedProgress;
                    haveBest = true;
                }
            }

            bool bypassTarget = false;
            bool cliffDetourTarget = false;
            float bypassFinalGain = 0.0f;
            float bypassPortalGain = 0.0f;
            float bypassRouteLength = 0.0f;
            std::vector<NavPoint> bypassRoutePoints{};

            // Phase 12B.15 escalates from micro-stepping to a wider Detour
            // anchor once repeated attempts prove that this is a real cliff
            // face rather than a short ramp. A successful macro candidate
            // intentionally overrides a forward micro-step.
            if (
                verticalPortalRecoveryAttempts_ >=
                    CliffDetourAttemptsBeforeSearch &&
                vertical >= CliffDetourMinimumVerticalDelta)
            {
                NavPoint cliffAnchor{};
                float cliffFinalGain = 0.0f;
                float cliffPortalClearance = 0.0f;
                float cliffRouteLength = 0.0f;
                std::vector<NavPoint> cliffRoutePoints{};

                if (FindCliffDetourAnchor(
                        player,
                        point,
                        cliffAnchor,
                        cliffRoutePoints,
                        cliffRouteLength,
                        cliffFinalGain,
                        cliffPortalClearance))
                {
                    best = cliffAnchor;
                    bestProgress = Distance2D(
                        player.x,
                        player.y,
                        best.x,
                        best.y
                    );
                    haveBest = true;
                    bypassTarget = true;
                    cliffDetourTarget = true;
                    bypassRoutePoints = cliffRoutePoints;
                    bypassRouteLength = cliffRouteLength;
                    bypassFinalGain = cliffFinalGain;
                    bypassPortalGain = cliffPortalClearance;

                    Debug::Logger::Info(
                        "NAVMESH 12B.15: CLIFF DETOUR ANCHOR target=(" +
                        Float(best.x) + "," + Float(best.y) + "," +
                        Float(best.z) + ") distance=" +
                        Float(bestProgress) + " localPoints=" +
                        std::to_string(bypassRoutePoints.size()) +
                        " localLength=" + Float(bypassRouteLength) +
                        " finalGain=" + Float(bypassFinalGain) +
                        " portalClearance=" + Float(bypassPortalGain)
                    );
                }
                else
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.15: no safe macro cliff-detour anchor "
                        "was found; falling back to bounded local recovery."
                    );
                }
            }

            if (!cliffDetourTarget && !haveBest)
            {
                NavPoint bypass{};

                if (FindVerticalBarrierBypassTarget(
                        player,
                        point,
                        bypass,
                        bypassRoutePoints,
                        bypassRouteLength,
                        bypassFinalGain,
                        bypassPortalGain))
                {
                    best = bypass;
                    bestProgress = Distance2D(
                        player.x,
                        player.y,
                        best.x,
                        best.y
                    );
                    haveBest = true;
                    bypassTarget = true;

                    Debug::Logger::Info(
                        "NAVMESH 12B.11: VERTICAL BARRIER BYPASS target=(" +
                        Float(best.x) + "," + Float(best.y) + "," +
                        Float(best.z) + ") step2D=" +
                        Float(bestProgress) + " stepZ=" +
                        Float(best.z - player.z) + " finalGain=" +
                        Float(bypassFinalGain) + " portalGain=" +
                        Float(bypassPortalGain)
                    );

                    Debug::Logger::Info(
                        "NAVMESH 12B.14: BYPASS LOCAL ROUTE points=" +
                        std::to_string(bypassRoutePoints.size()) +
                        " length=" + Float(bypassRouteLength) +
                        "; direct CTM to the projected endpoint is disabled."
                    );
                }
            }

            if (!haveBest)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.11: no bounded same-surface or lateral "
                    "bypass projection was found; returning control to "
                    "normal CTM/recovery."
                );
                return false;
            }

            if (destinationLabel_.find("death recovery corpse route") !=
                    std::string::npos &&
                (resetEpisode ||
                 lastCorpsePortalDiagnosticFingerprint_ != lastPathFingerprint_))
            {
                lastCorpsePortalDiagnosticFingerprint_ = lastPathFingerprint_;
                NavPoint projectedPlayer{};
                NavPoint projectedPortal{};
                NavPoint projectedRecovery{};
                std::uint64_t playerNearestPoly = 0;
                std::uint64_t portalNearestPoly = 0;
                std::uint64_t recoveryNearestPoly = 0;
                const bool playerProjectionKnown = provider_.ProjectToNavMesh(
                    PlayerPoint(player), projectedPlayer, playerNearestPoly);
                const bool portalProjectionKnown = provider_.ProjectToNavMesh(
                    point, projectedPortal, portalNearestPoly);
                const bool recoveryProjectionKnown = provider_.ProjectToNavMesh(
                    best, projectedRecovery, recoveryNearestPoly);
                const std::uint64_t candidatePoly =
                    candidateIndex < pointPolyRefs_.size()
                        ? pointPolyRefs_[candidateIndex] : 0;
                std::uint64_t nextCorridorPoly = 0;
                int playerPolyOccurrences = 0;
                std::uint64_t candidatePredecessorPoly = 0;
                int candidatePolyOccurrences = 0;
                for (std::size_t i = 0; i < corridorPolys_.size(); ++i)
                {
                    if (playerProjectionKnown &&
                        corridorPolys_[i] == playerNearestPoly)
                    {
                        ++playerPolyOccurrences;
                        nextCorridorPoly = i + 1 < corridorPolys_.size()
                            ? corridorPolys_[i + 1] : 0;
                    }
                    if (corridorPolys_[i] != candidatePoly || candidatePoly == 0)
                        continue;
                    ++candidatePolyOccurrences;
                    candidatePredecessorPoly = i > 0 ? corridorPolys_[i - 1] : 0;
                }
                if (candidatePolyOccurrences != 1)
                    candidatePredecessorPoly = 0;
                if (playerPolyOccurrences != 1)
                    nextCorridorPoly = 0;
                float remainingStraightLength = 0.0f;
                for (std::size_t i = candidateIndex + 1; i < points_.size(); ++i)
                {
                    const float dx = points_[i].x - points_[i - 1].x;
                    const float dy = points_[i].y - points_[i - 1].y;
                    const float dz = points_[i].z - points_[i - 1].z;
                    remainingStraightLength += std::sqrt(dx * dx + dy * dy + dz * dz);
                }
                const NavPoint currentPoint = pointIndex_ < points_.size()
                    ? points_[pointIndex_] : NavPoint{};
                Debug::Logger::Info(
                    "NAV 15B.0 CORPSE VERTICAL PORTAL: mode=" +
                    std::string(cliffDetourTarget ? "cliff-detour" :
                        (bypassTarget ? "bypass" : "forward")) +
                    " corridorFingerprint=" + std::to_string(lastPathFingerprint_) +
                    " destination=(" + Float(destination_.x) + "," +
                    Float(destination_.y) + "," + Float(destination_.z) + ")" +
                    " player=(" + Float(player.x) + "," + Float(player.y) +
                    "," + Float(player.z) + ")" +
                    " playerNearestPoly=" + HexPoly(playerNearestPoly) +
                    " nextCorridorPoly=" + HexPoly(nextCorridorPoly) +
                    " playerPolyOnCorridor=" +
                    (playerPolyOccurrences == 1 ? "yes" : "no") +
                    " playerProjectionKnown=" + (playerProjectionKnown ? "yes" : "no") +
                    " playerProjected=(" + Float(projectedPlayer.x) + "," +
                    Float(projectedPlayer.y) + "," + Float(projectedPlayer.z) + ")" +
                    " pointIndex=" + std::to_string(pointIndex_) +
                    " candidateIndex=" + std::to_string(candidateIndex) +
                    " candidatePredecessorPoly=" + HexPoly(candidatePredecessorPoly) +
                    " candidatePoly=" + HexPoly(candidatePoly) +
                    " currentPoint=(" + Float(currentPoint.x) + "," +
                    Float(currentPoint.y) + "," + Float(currentPoint.z) + ")" +
                    " portal=(" + Float(point.x) + "," + Float(point.y) +
                    "," + Float(point.z) + ")" +
                    " portalNearestPoly=" + HexPoly(portalNearestPoly) +
                    " portalProjectionKnown=" + (portalProjectionKnown ? "yes" : "no") +
                    " portalProjected=(" + Float(projectedPortal.x) + "," +
                    Float(projectedPortal.y) + "," + Float(projectedPortal.z) + ")" +
                    " recoveryTarget=(" + Float(best.x) + "," +
                    Float(best.y) + "," + Float(best.z) + ")" +
                    " recoveryNearestPoly=" + HexPoly(recoveryNearestPoly) +
                    " recoveryProjectionKnown=" + (recoveryProjectionKnown ? "yes" : "no") +
                    " recoveryProjected=(" + Float(projectedRecovery.x) + "," +
                    Float(projectedRecovery.y) + "," + Float(projectedRecovery.z) + ")" +
                    " delta2D=" + Float(horizontal) +
                    " deltaZ=" + Float(point.z - player.z) +
                    " remainingStraightLength=" + Float(remainingStraightLength));
            }

            ++verticalPortalRecoveryAttempts_;
            ++verticalPortalRecoveryTotalAttempts_;

            Debug::Logger::Info(
                "NAVMESH 12B.10: VERTICAL PORTAL RECOVERY attempt=" +
                std::to_string(verticalPortalRecoveryAttempts_) +
                "/" +
                std::to_string(MaximumVerticalPortalRecoveryAttempts) +
                " total=" +
                std::to_string(verticalPortalRecoveryTotalAttempts_) +
                "/" +
                std::to_string(MaximumVerticalPortalRecoveryTotalAttempts) +
                " target=(" + Float(best.x) + "," +
                Float(best.y) + "," + Float(best.z) + ")" +
                " step2D=" + Float(bestProgress) +
                " stepZ=" + Float(best.z - player.z) +
                (cliffDetourTarget
                    ? " mode=cliff-detour"
                    : (bypassTarget ? " mode=bypass" : " mode=forward"))
            );

            verticalPortalRecoveryActive_ = true;
            verticalPortalRecoveryBypassActive_ = bypassTarget;
            verticalPortalRecoveryStartX_ = player.x;
            verticalPortalRecoveryStartY_ = player.y;
            verticalPortalRecoveryTargetX_ = best.x;
            verticalPortalRecoveryTargetY_ = best.y;
            verticalPortalRecoveryInitialTargetDistance_ = bestProgress;
            verticalPortalRecoveryBestTargetDistance_ = bestProgress;
            verticalPortalRecoveryIssuedTick_ = tick;
            verticalPortalRecoveryProgressTick_ = tick;

            if (bypassTarget)
            {
                verticalPortalBypassRoutePoints_ = bypassRoutePoints;
                verticalPortalBypassRouteIndex_ = 1;
                verticalPortalBypassBestCornerDistance_ = 1.0e30f;

                if (cliffDetourTarget)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.15: CLIFF DETOUR ROUTE START points=" +
                        std::to_string(verticalPortalBypassRoutePoints_.size()) +
                        " length=" + Float(bypassRouteLength) +
                        "; following temporary Detour anchor route before "
                        "resuming the global destination."
                    );
                }

                if (!IssueVerticalBarrierBypassRouteCorner(
                        player,
                        tick,
                        "initial"))
                {
                    badVerticalTransitions_.ObserveDispatch(portalEdge, false);
                    Debug::Logger::Info(
                        "NAVMESH 12B.14: bypass local route could not issue "
                        "its first CTM point; returning control to normal recovery."
                    );
                    verticalPortalRecoveryActive_ = false;
                    verticalPortalRecoveryBypassActive_ = false;
                    verticalPortalBypassRoutePoints_.clear();
                    verticalPortalBypassRouteIndex_ = 0;
                    return false;
                }

                badVerticalTransitions_.ObserveDispatch(portalEdge, true);
                return true;
            }

            verticalPortalBypassRoutePoints_.clear();
            verticalPortalBypassRouteIndex_ = 0;

            if (!IssueNavMovement(player,best,0.50f,
                    NavCommandSource::VerticalRecovery))
            {
                badVerticalTransitions_.ObserveDispatch(portalEdge, false);
                Debug::Logger::Info(
                    "NAVMESH 12B.10: projected recovery CTM was rejected; "
                    "returning control to normal CTM/recovery."
                );
                verticalPortalRecoveryActive_ = false;
                return false;
            }

            badVerticalTransitions_.ObserveDispatch(portalEdge, true);
            ++commands_;
            lastCommandTick_ = tick;
            lastProgressTick_ = tick;
            ResetMotionWatchdog(player, tick);
            return true;
        }

        bool UpdateVerticalPortalRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!verticalPortalRecoveryActive_)
            {
                return false;
            }

            const float moved = Distance2D(
                player.x,
                player.y,
                verticalPortalRecoveryStartX_,
                verticalPortalRecoveryStartY_
            );

            if (verticalPortalRecoveryBypassActive_)
            {
                const float finalTargetDistance = Distance2D(
                    player.x,
                    player.y,
                    verticalPortalRecoveryTargetX_,
                    verticalPortalRecoveryTargetY_
                );

                if (
                    moved >= VerticalPortalRecoverySuccessDistance &&
                    finalTargetDistance <= VerticalBarrierBypassArrivalDistance)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.14: NAVMESH-ROUTED BARRIER BYPASS COMPLETE "
                        "moved=" + Float(moved) +
                        " remaining=" + Float(finalTargetDistance) +
                        " localPoints=" +
                        std::to_string(verticalPortalBypassRoutePoints_.size())
                    );

                    verticalPortalRecoveryActive_ = false;
                    verticalPortalRecoveryBypassActive_ = false;
                    verticalPortalBypassRoutePoints_.clear();
                    verticalPortalBypassRouteIndex_ = 0;

                    const float finalDistance = Distance2D(
                        player.x,
                        player.y,
                        destination_.x,
                        destination_.y
                    );

                    Debug::Logger::Info(
                        "NAVMESH 12B.14: BYPASS COMPLETE -> ROUTE REFRESH "
                        "from live position; finalDistance=" +
                        Float(finalDistance)
                    );

                    ResetEscalatingRecovery(
                        player,
                        tick,
                        finalDistance
                    );

                    if (!PlanFrom(
                            player,
                            tick,
                            true,
                            "vertical_bypass_refresh"))
                    {
                        SetState(GenericNavMeshFollowState::Failed);
                        StopAtCurrentPosition(player);
                    }

                    return true;
                }

                if (
                    verticalPortalBypassRouteIndex_ >=
                        verticalPortalBypassRoutePoints_.size())
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.14: bypass local route ended before "
                        "the projected endpoint was reached; replanning."
                    );

                    verticalPortalRecoveryActive_ = false;
                    verticalPortalRecoveryBypassActive_ = false;
                    verticalPortalBypassRoutePoints_.clear();
                    verticalPortalBypassRouteIndex_ = 0;
                    ResetMotionWatchdog(player, tick);
                    Replan(player, tick);
                    return true;
                }

                const NavPoint& routePoint =
                    verticalPortalBypassRoutePoints_[
                        verticalPortalBypassRouteIndex_
                    ];

                const float cornerDistance = Distance2D(
                    player.x,
                    player.y,
                    routePoint.x,
                    routePoint.y
                );

                if (
                    std::isfinite(cornerDistance) &&
                    cornerDistance + VerticalBarrierBypassProgressThreshold <
                        verticalPortalBypassBestCornerDistance_)
                {
                    verticalPortalBypassBestCornerDistance_ = cornerDistance;
                    verticalPortalRecoveryProgressTick_ = tick;
                }

                if (
                    cornerDistance <= VerticalBarrierBypassRouteCornerArrivalDistance)
                {
                    if (
                        verticalPortalBypassRouteIndex_ + 1 <
                            verticalPortalBypassRoutePoints_.size())
                    {
                        ++verticalPortalBypassRouteIndex_;

                        if (!IssueVerticalBarrierBypassRouteCorner(
                                player,
                                tick,
                                "next local portal"))
                        {
                            Debug::Logger::Info(
                                "NAVMESH 12B.14: failed to issue the next "
                                "local bypass portal; replanning."
                            );

                            verticalPortalRecoveryActive_ = false;
                            verticalPortalRecoveryBypassActive_ = false;
                            verticalPortalBypassRoutePoints_.clear();
                            verticalPortalBypassRouteIndex_ = 0;
                            ResetMotionWatchdog(player, tick);
                            Replan(player, tick);
                        }

                        return true;
                    }

                    // We reached the final local portal. The projected
                    // endpoint should now be within the completion radius on
                    // the next update; retain ownership rather than falling
                    // back into the global cliff corridor prematurely.
                    verticalPortalRecoveryProgressTick_ = tick;
                    return true;
                }

                if (
                    tick >= verticalPortalRecoveryProgressTick_ &&
                    tick - verticalPortalRecoveryProgressTick_ >=
                        VerticalBarrierBypassStallTicks)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.14: NAVMESH-ROUTED BYPASS STALLED "
                        "point=" +
                        std::to_string(verticalPortalBypassRouteIndex_ + 1) +
                        "/" +
                        std::to_string(verticalPortalBypassRoutePoints_.size()) +
                        " cornerDistance=" + Float(cornerDistance) +
                        " finalTargetDistance=" + Float(finalTargetDistance) +
                        " moved=" + Float(moved) +
                        "; falling back to existing replan/escape recovery."
                    );

                    verticalPortalRecoveryActive_ = false;
                    verticalPortalRecoveryBypassActive_ = false;
                    verticalPortalBypassRoutePoints_.clear();
                    verticalPortalBypassRouteIndex_ = 0;
                    const DirectedPolyTransition stalledEdge =
                        badVerticalTransitions_.ObserveStalled();
                    if (stalledEdge.Valid())
                        Debug::Logger::Info(
                            "NAV 15B.1 VERTICAL STALL fromPoly=" +
                            HexPoly(stalledEdge.from) + " toPoly=" +
                            HexPoly(stalledEdge.to) +
                            " stalledObservations=" + std::to_string(
                                badVerticalTransitions_.StalledObservations()));
                    ResetMotionWatchdog(player, tick);
                    Replan(player, tick, stalledEdge.Valid());
                    return true;
                }

                return true;
            }

            if (moved >= VerticalPortalRecoverySuccessDistance)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.10: VERTICAL PORTAL RECOVERY PROGRESS moved=" +
                    Float(moved) +
                    "; continuing toward the Detour corridor."
                );

                verticalPortalRecoveryActive_ = false;

                const float finalDistance = Distance2D(
                    player.x,
                    player.y,
                    destination_.x,
                    destination_.y
                );

                Debug::Logger::Info(
                    "NAVMESH 12B.11: RECOVERY PROGRESS -> ROUTE REFRESH "
                    "from live position; finalDistance=" +
                    Float(finalDistance)
                );

                ResetEscalatingRecovery(
                    player,
                    tick,
                    finalDistance
                );

                if (!PlanFrom(
                        player,
                        tick,
                        true,
                        "vertical_recovery_refresh"))
                {
                    SetState(GenericNavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                }

                return true;
            }

            if (
                tick >= verticalPortalRecoveryIssuedTick_ &&
                tick - verticalPortalRecoveryIssuedTick_ >=
                    VerticalPortalRecoverySettleTicks)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.10: VERTICAL PORTAL RECOVERY STALLED; "
                    "falling back to the existing replan/escape recovery."
                );

                verticalPortalRecoveryActive_ = false;
                verticalPortalRecoveryBypassActive_ = false;
                const DirectedPolyTransition stalledEdge =
                    badVerticalTransitions_.ObserveStalled();
                if (stalledEdge.Valid())
                    Debug::Logger::Info(
                        "NAV 15B.1 VERTICAL STALL fromPoly=" +
                        HexPoly(stalledEdge.from) + " toPoly=" +
                        HexPoly(stalledEdge.to) +
                        " stalledObservations=" + std::to_string(
                            badVerticalTransitions_.StalledObservations()));
                ResetMotionWatchdog(player, tick);
                Replan(player, tick, stalledEdge.Valid());
                return true;
            }

            return true;
        }


        bool IssueFinalApproach(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const char* reason)
        {
            const float dx = destination_.x - player.x;
            const float dy = destination_.y - player.y;
            const float distance = std::sqrt(dx * dx + dy * dy);

            if (!std::isfinite(distance) || distance <= finalArrivalDistance_)
            {
                return false;
            }

            const float upwardDelta = destination_.z - player.z;
            if (upwardDelta > StairFinalDirectMaximumRise)
            {
                Debug::Logger::Info(
                    "NAVMESH 13D.6: ASCENDING FINAL DIRECT SUPPRESSED rise=" +
                    Float(upwardDelta) +
                    " finalDistance=" + Float(distance) +
                    "; preserving dense corridor steering for stairs/ramp.");
                finalApproachSuppressed_ = true;
                return false;
            }

            const float standOff = std::max(
                FinalApproachMinimumStandOff,
                finalArrivalDistance_ * 0.65f
            );

            float targetX = destination_.x;
            float targetY = destination_.y;

            if (distance > 0.10f && distance > standOff)
            {
                const float ux = dx / distance;
                const float uy = dy / distance;
                targetX = destination_.x - ux * standOff;
                targetY = destination_.y - uy * standOff;
            }

            const NavPoint finalTarget{ targetX, targetY, destination_.z };
            float reachableFraction = 0.0f;
            NavPoint reachablePoint{};
            const bool raycastOk = provider_.IsSurfaceSegmentReachable(
                PlayerPoint(player),
                finalTarget,
                reachableFraction,
                reachablePoint);

            if (raycastOk && reachableFraction < 0.985f)
            {
                Debug::Logger::Info(
                    "NAVMESH 13C.1: FINAL DIRECT APPROACH REJECTED reachableFraction=" +
                    Float(reachableFraction) +
                    "; using connected-surface recovery instead.");
                finalApproachSuppressed_ = true;
                return IssueSurfaceRecovery(
                    player,
                    tick,
                    finalTarget,
                    "blocked final direct segment");
            }

            Debug::Logger::Info(
                std::string("NAVMESH 12B.4: FINAL DIRECT APPROACH reason=") +
                reason +
                " finalDistance=" + Float(distance) +
                " target=(" + Float(targetX) + "," +
                Float(targetY) + "," + Float(destination_.z) + ")"
            );

            if (!IssueNavMovement(player,
                    {targetX,targetY,destination_.z},CtmPrecision,
                    NavCommandSource::FinalDirect))
            {
                finalApproachSuppressed_ = true;
                return false;
            }

            ++commands_;
            ++finalApproachCommands_;
            finalApproachActive_ = true;
            finalApproachStartX_ = player.x;
            finalApproachStartY_ = player.y;
            finalApproachStartDistance_ = distance;
            finalApproachIssuedTick_ = tick;
            lastCommandTick_ = tick;
            return true;
        }

        bool UpdateFinalApproach(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            float finalDistance)
        {
            if (!finalApproachActive_)
            {
                return false;
            }

            const float moved = Distance2D(
                player.x,
                player.y,
                finalApproachStartX_,
                finalApproachStartY_
            );

            const float gained = finalApproachStartDistance_ - finalDistance;

            if (
                moved >= FinalApproachMinimumProgress ||
                gained >= FinalApproachMinimumProgress)
            {
                if (
                    tick >= finalApproachIssuedTick_ &&
                    tick - finalApproachIssuedTick_ >= FinalApproachSettleTicks)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.4: FINAL APPROACH PROGRESS moved=" +
                        Float(moved) +
                        " gained=" + Float(gained) +
                        " finalDistance=" + Float(finalDistance)
                    );

                    finalApproachStartX_ = player.x;
                    finalApproachStartY_ = player.y;
                    finalApproachStartDistance_ = finalDistance;
                    finalApproachIssuedTick_ = tick;
                }

                return true;
            }

            if (
                tick >= finalApproachIssuedTick_ &&
                tick - finalApproachIssuedTick_ >= FinalApproachSettleTicks)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.4: FINAL APPROACH STALLED; returning to NavMesh recovery. "
                    "finalDistance=" + Float(finalDistance)
                );

                finalApproachActive_ = false;
                finalApproachSuppressed_ = true;
                ResetMotionWatchdog(player, tick);
                return false;
            }

            return true;
        }

        bool IssueEscapeProbe(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            int side)
        {
            NavPoint toward = destination_;
            if (pointIndex_ < points_.size())
            {
                toward = points_[pointIndex_];
            }

            float dx = toward.x - player.x;
            float dy = toward.y - player.y;
            float length = std::sqrt(dx * dx + dy * dy);

            if (!std::isfinite(length) || length < 0.10f)
            {
                dx = destination_.x - player.x;
                dy = destination_.y - player.y;
                length = std::sqrt(dx * dx + dy * dy);
            }

            if (!std::isfinite(length) || length < 0.10f)
            {
                return false;
            }

            const float ux = dx / length;
            const float uy = dy / length;
            const float px = -uy;
            const float py = ux;
            const float signedSide = side < 0 ? -1.0f : 1.0f;

            const float targetX =
                player.x + ux * EscapeProbeForwardBias +
                px * EscapeProbeLateralDistance * signedSide;
            const float targetY =
                player.y + uy * EscapeProbeForwardBias +
                py * EscapeProbeLateralDistance * signedSide;

            Debug::Logger::Info(
                std::string("NAVMESH 12B.2: LOCAL ESCAPE PROBE side=") +
                (side < 0 ? "left" : "right") +
                " target=(" + Float(targetX) + "," +
                Float(targetY) + "," + Float(player.z) + ")"
            );

            if (!IssueNavMovement(player,
                    {targetX,targetY,player.z},0.50f,
                    NavCommandSource::EscapeProbe))
            {
                return false;
            }

            ++commands_;
            escapeProbeActive_ = true;
            escapeProbeSide_ = side;
            escapeProbeStartX_ = player.x;
            escapeProbeStartY_ = player.y;
            escapeProbeIssuedTick_ = tick;
            lastCommandTick_ = tick;
            ResetMotionWatchdog(player, tick);
            return true;
        }

        bool UpdateEscapeProbe(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            float finalDistance)
        {
            if (!escapeProbeActive_)
            {
                return false;
            }

            const float moved = Distance2D(
                player.x,
                player.y,
                escapeProbeStartX_,
                escapeProbeStartY_
            );

            if (moved >= EscapeProbeSuccessDistance)
            {
                Debug::Logger::Info(
                    "NAVMESH 12B.2: LOCAL ESCAPE PROBE SUCCESS moved=" +
                    Float(moved) +
                    "; replanning from escaped live position."
                );

                escapeProbeActive_ = false;
                escapeProbeSide_ = 0;
                ResetEscalatingRecovery(player, tick, finalDistance);
                Replan(player, tick);
                return true;
            }

            if (
                tick >= escapeProbeIssuedTick_ &&
                tick - escapeProbeIssuedTick_ >= EscapeProbeSettleTicks)
            {
                if (escapeProbeSide_ < 0)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.2: left escape probe made no movement; trying right side."
                    );

                    escapeProbeActive_ = false;
                    if (!IssueEscapeProbe(player, tick, +1))
                    {
                        SetState(GenericNavMeshFollowState::Failed);
                        StopAtCurrentPosition(player);
                    }
                    return true;
                }

                ++escapeExhaustionRecoveries_;
                Debug::Logger::Info(
                    "NAVMESH 14G.3.1: ESCAPE EXHAUSTED - both local sides are blocked; "
                    "recovery=" + std::to_string(escapeExhaustionRecoveries_) +
                    "/" + std::to_string(MaximumEscapeExhaustionRecoveries));

                escapeProbeActive_ = false;
                escapeProbeSide_ = 0;
                QuarantineCurrentTransition(
                    tick,
                    "both local escape probes blocked");

                if (escapeExhaustionRecoveries_ <=
                        MaximumEscapeExhaustionRecoveries &&
                    BeginLastSafeBacktrack(
                        player,
                        tick,
                        "local escape probes exhausted"))
                {
                    ResetMotionWatchdog(player, tick);
                    return true;
                }

                if (escapeExhaustionRecoveries_ <=
                    MaximumEscapeExhaustionRecoveries)
                {
                    Debug::Logger::Info(
                        "NAVMESH 14G.3.1: no usable last-safe backtrack; "
                        "replanning with quarantined transition.");
                    ResetMotionWatchdog(player, tick);
                    Replan(player, tick);
                    return true;
                }

                Debug::Logger::Info(
                    "NAVMESH 14G.3.1: bounded escape/backtrack recovery exhausted; "
                    "failing route instead of looping."
                );
                SetState(GenericNavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return true;
            }

            return true;
        }

        void LogSteeringSelection(
            std::uint64_t tick,
            std::size_t fromIndex,
            const SteeringCandidateEvidence& evidence,
            const char* decision,
            const NavPoint& target)
        {
            if (lastSteeringLogFingerprint_ == lastPathFingerprint_ &&
                lastSteeringLogCandidate_ == evidence.index &&
                lastSteeringLogDecision_ == decision &&
                tick >= lastSteeringLogTick_ &&
                tick - lastSteeringLogTick_ < ReissueTicks)
            {
                return;
            }

            lastSteeringLogTick_ = tick;
            lastSteeringLogFingerprint_ = lastPathFingerprint_;
            lastSteeringLogCandidate_ = evidence.index;
            lastSteeringLogDecision_ = decision;
            const bool finalRaycastKnown = evidence.adjusted
                ? evidence.adjustedRaycastValid : evidence.raycastValid;
            const float finalRaycastFraction = evidence.adjusted
                ? evidence.adjustedRaycastFraction : evidence.raycastFraction;
            Debug::Logger::Info(
                "NAV 14N.1 STEERING: corridorFingerprint=" +
                std::to_string(lastPathFingerprint_) +
                " fromIndex=" + std::to_string(fromIndex) +
                " candidateIndex=" + std::to_string(evidence.index) +
                " candidateClearance=" +
                (evidence.clearanceKnown
                    ? Float(evidence.candidateClearance) : std::string("unknown")) +
                " adjustedClearance=" +
                (evidence.adjustedClearanceKnown
                    ? Float(evidence.adjustedClearance) : std::string("unknown")) +
                " raycastFraction=" +
                (finalRaycastKnown
                    ? Float(finalRaycastFraction) : std::string("unknown")) +
                " decision=" + decision +
                " target=(" + Float(target.x) + "," +
                Float(target.y) + "," + Float(target.z) + ")");
        }

        DirectedPolyTransition ProvenIssuedSteeringEdge(
            const Objects::PlayerState& player, std::size_t candidateIndex,
            const NavPoint& commandPoint, bool adjusted) const
        {
            if (candidateIndex >= pointPolyRefs_.size() ||
                !pointPolyRefs_[candidateIndex]) return {};
            NavPoint projectedPlayer{};
            std::uint64_t playerPoly=0;
            if (!provider_.ProjectToNavMesh(PlayerPoint(player),
                    projectedPlayer,playerPoly)) return {};
            const auto edge=LocalPortalSteeringPolicy::CandidateEdge(
                corridorPolys_,playerPoly,pointPolyRefs_[candidateIndex]);
            NavPoint a{}, b{};
            const bool portalKnown=edge.Valid() &&
                provider_.GetDirectedPortal(edge.from,edge.to,a,b);
            std::uint64_t targetPoly=0;
            if (adjusted)
            {
                NavPoint projectedTarget{};
                if (!provider_.ProjectToNavMesh(commandPoint,
                        projectedTarget,targetPoly)) return {};
            }
            // The unadjusted point's Detour straight-path ref is authoritative
            // even at overlapping portal XY where nearest-poly picks another
            // surface. This is command intent, not proof of a blocked wall.
            return IssuedSteeringCommandPolicy::ProvenRouteEdge(
                corridorPolys_,playerPoly,pointPolyRefs_[candidateIndex],
                portalKnown,adjusted,targetPoly);
        }

        DirectedPolyTransition AttributeFailedSteering(
            const Objects::PlayerState& player, std::size_t fromIndex,
            std::size_t candidateIndex) const
        {
            DirectedPolyTransition proven{};
            DirectedPolyTransition candidateEdge{};
            NavSurfaceRayTrace trace{};
            float fraction = 0.0f;
            NavPoint reached{};
            bool adjacent = false;
            const char* attribution = "unknown";
            const std::uint64_t candidatePoly =
                candidateIndex < pointPolyRefs_.size()
                    ? pointPolyRefs_[candidateIndex] : 0;
            if (candidateIndex < points_.size() && candidatePoly &&
                provider_.IsSurfaceSegmentReachable(PlayerPoint(player),
                    points_[candidateIndex], fraction, reached, &trace) &&
                trace.complete && fraction <
                    SteeringSelectionPolicy::MinimumRaycastFraction)
            {
                candidateEdge = LocalPortalSteeringPolicy::CandidateEdge(
                    corridorPolys_, trace.lastVisitedPoly, candidatePoly);
                NavPoint portalA{}, portalB{};
                adjacent = candidateEdge.Valid() &&
                    provider_.GetDirectedPortal(candidateEdge.from,
                        candidateEdge.to, portalA, portalB);
                if (adjacent)
                {
                    proven = LocalPortalSteeringPolicy::AttributeRayFailure(
                        corridorPolys_, trace.startPoly, candidatePoly,
                        trace.lastVisitedPoly,
                        {reached.x, reached.y, reached.z},
                        {portalA.x, portalA.y, portalA.z},
                        {portalB.x, portalB.y, portalB.z},
                        SteeringSelectionPolicy::MinimumWallClearance);
                    attribution = proven.Valid() ? "ray_hit_at_directed_portal"
                        : "ray_hit_not_at_proven_portal";
                }
                else attribution = "directed_portal_unavailable";
            }
            else attribution = "no_complete_local_blocked_ray";
            Debug::Logger::Info(
                "STEERING FAILURE ATTRIBUTION map="+std::to_string(mapId_)+
                " position=("+Float(player.x)+","+Float(player.y)+","+
                    Float(player.z)+")"+
                " corridorFingerprint="+
                std::to_string(lastPathFingerprint_)+
                " fromIndex="+std::to_string(fromIndex)+
                " candidateIndex="+std::to_string(candidateIndex)+
                " playerPoly="+HexPoly(trace.startPoly)+
                " candidatePoly="+HexPoly(candidatePoly)+
                " hitPoly="+HexPoly(trace.lastVisitedPoly)+
                " fromPoly="+HexPoly(proven.from)+
                " toPoly="+HexPoly(proven.to)+
                " adjacent="+(adjacent?"yes":"no")+
                " transitionKnown="+(proven.Valid()?"yes":"no")+
                " reason="+attribution);
            return proven;
        }

        bool IssueVerifiedPortalStage(const Objects::PlayerState& player,
            std::uint64_t tick, DirectedPolyTransition edge)
        {
            if (!edge.Valid() || startOptions_.planningOnly ||
                surfaceRecoveryAttempts_ >= MaximumSurfaceRecoveryAttempts)
                return false;
            NavPoint a{}, b{}, stage{};
            if (!provider_.GetDirectedPortal(edge.from,edge.to,a,b) ||
                !provider_.ProjectGroundNear(
                    {(a.x+b.x)*0.5f,(a.y+b.y)*0.5f,(a.z+b.z)*0.5f},
                    1.5f,3.0f,stage))
                return false;
            NavPoint projected{};
            std::uint64_t stagePoly=0;
            if (!provider_.ProjectToNavMesh(stage,projected,stagePoly) ||
                (stagePoly!=edge.from && stagePoly!=edge.to))
                return false;
            float horizontal=0.0f, vertical=0.0f;
            if (IsUnsafeVerticalPortal(player,stage,horizontal,vertical) ||
                vertical>SurfaceRecoveryMaximumVerticalDelta ||
                Distance2D(player.x,player.y,stage.x,stage.y)>
                    SurfaceRecoveryMaximumStep)
                return false;
            float fraction=0.0f;
            NavPoint reached{};
            const bool rayKnown=provider_.IsSurfaceSegmentReachable(
                PlayerPoint(player),stage,fraction,reached);
            float clearance=0.0f;
            NavPoint wall{};
            if (!provider_.FindWallDistance(stage,WallSteeringProbeRadius,
                    clearance,wall) ||
                !LocalPortalSteeringPolicy::AssessStage(
                    {player.x,player.y,player.z},
                    {a.x,a.y,a.z},{b.x,b.y,b.z},
                    {stage.x,stage.y,stage.z},clearance,rayKnown,fraction,
                    SteeringSelectionPolicy::MinimumWallClearance,
                    SurfaceRecoveryMinimumStep).allowed)
                return false;
            const float finalDistance=Distance2D(player.x,player.y,
                destination_.x,destination_.y);
            if (surfaceEpisodeActive_ &&
                SurfaceRecoveryEpisodePolicy::Assess(
                    surfaceEpisodeInitialDistance_,finalDistance,
                    RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_)!=
                    SurfaceRecoveryQuality::Progress &&
                std::any_of(surfaceEpisodeTargets_.begin(),
                    surfaceEpisodeTargets_.end(),[&](const NavPoint& previous)
                    {
                        return SurfaceRecoveryEpisodePolicy::SameLocalTarget(
                            previous.x,previous.y,stage.x,stage.y,
                            SurfaceRecoveryArrivalDistance);
                    }))
                return false;
            if (!IssueNavMovement(player,stage,CtmPrecision,
                    NavCommandSource::PortalStage))
                return false;
            RegisterFailedCorridor(player,
                "no clearance-safe corridor steering target",false,edge);
            ++commands_;
            ++surfaceRecoveryAttempts_;
            surfaceRecoveryActive_=true;
            surfaceRecoveryStartX_=player.x;
            surfaceRecoveryStartY_=player.y;
            surfaceRecoveryTarget_=stage;
            BeginSurfaceRecoveryAttempt(player,stage,finalDistance,edge);
            surfaceRecoveryBestTargetDistance_=Distance2D(
                player.x,player.y,stage.x,stage.y);
            surfaceRecoveryProgressTick_=tick;
            lastCommandTick_=tick;
            Debug::Logger::Info("NAV ENTRANCE STAGE intent="+
                std::to_string(intentId_)+" fromPoly="+HexPoly(edge.from)+
                " toPoly="+HexPoly(edge.to)+" target=("+
                Float(stage.x)+","+Float(stage.y)+","+Float(stage.z)+")"+
                " attempt="+std::to_string(surfaceRecoveryAttempts_)+"/"+
                std::to_string(MaximumSurfaceRecoveryAttempts)+
                " reason=verified_clipped_portal_midpoint");
            return true;
        }

        bool IssueCurrentCorner(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const char* reason,
            std::size_t minimumIndex = 0,
            std::size_t proposedStartIndex = static_cast<std::size_t>(-1))
        {
            if (!RouteCostProbePolicy::MayIssueMovement(
                    startOptions_.planningOnly))
                return false;
            if (pointIndex_ >= points_.size())
                return true;

            issuedOrdinarySteeringTargetValid_ = false;
            steeringAtCorridorEnd_ = false;
            const std::size_t fromIndex = pointIndex_;
            std::size_t firstCandidate = proposedStartIndex !=
                    static_cast<std::size_t>(-1)
                ? proposedStartIndex
                : std::max(pointIndex_, minimumIndex);
            while (firstCandidate + 1 < points_.size() &&
                   Distance2D(player.x, player.y,
                       points_[firstCandidate].x,
                       points_[firstCandidate].y) <= CornerArrivalDistance)
            {
                ++firstCandidate;
            }
            if (firstCandidate >= points_.size() ||
                (firstCandidate + 1 == points_.size() &&
                 Distance2D(player.x, player.y,
                     points_[firstCandidate].x,
                     points_[firstCandidate].y) <= CornerArrivalDistance))
            {
                // The next Update handles final-portal arrival/replanning.
                steeringAtCorridorEnd_ = true;
                return true;
            }

            const std::size_t lastCandidate = std::min(
                points_.size() - 1,
                firstCandidate + SurfaceLookaheadPoints);
            bool rejectedFarther = false;
            bool haveVerticalRecovery = false;
            std::size_t verticalRecoveryCandidateIndex = 0;
            float verticalRecoveryHorizontal = 0.0f;
            float verticalRecoveryDelta = 0.0f;
            NavPoint verticalRecoveryPoint{};
            SteeringCandidateEvidence lastEvidence{};
            lastEvidence.index = firstCandidate;

            for (std::size_t candidate = lastCandidate;; --candidate)
            {
                const NavPoint& point = points_[candidate];
                const float distance = Distance2D(
                    player.x, player.y, point.x, point.y);
                const float rise = point.z - player.z;
                const float vertical = std::fabs(rise);
                bool geometryValid = std::isfinite(distance) &&
                    std::isfinite(vertical);
                if (candidate > firstCandidate)
                {
                    geometryValid = geometryValid &&
                        distance <= SurfaceLookaheadMaximumDistance &&
                        vertical <= SurfaceLookaheadMaximumVerticalDelta &&
                        rise <= StairLookaheadMaximumRise;
                    float previousZ = player.z;
                    for (std::size_t check = firstCandidate;
                         geometryValid && check <= candidate; ++check)
                    {
                        const float stepRise =
                            std::fabs(points_[check].z - previousZ);
                        geometryValid = std::isfinite(stepRise) &&
                            stepRise <= SurfaceLookaheadMaximumVerticalDelta;
                        previousZ = points_[check].z;
                    }
                }

                float unsafeHorizontal = 0.0f;
                float unsafeVertical = 0.0f;
                if (geometryValid && IsUnsafeVerticalPortal(
                        player, point, unsafeHorizontal, unsafeVertical))
                {
                    geometryValid = false;
                    if (!haveVerticalRecovery)
                    {
                        haveVerticalRecovery = true;
                        verticalRecoveryHorizontal = unsafeHorizontal;
                        verticalRecoveryDelta = unsafeVertical;
                        verticalRecoveryPoint = point;
                        verticalRecoveryCandidateIndex = candidate;
                    }
                }

                SteeringCandidateEvidence evidence{};
                evidence.index = candidate;
                evidence.distanceAndRiseValid = geometryValid;
                NavPoint commandPoint = point;
                if (geometryValid)
                {
                    NavPoint reached{};
                    evidence.raycastValid = provider_.IsSurfaceSegmentReachable(
                        PlayerPoint(player), point,
                        evidence.raycastFraction, reached);
                    if (evidence.raycastValid &&
                        std::isfinite(evidence.raycastFraction) &&
                        evidence.raycastFraction >=
                            SteeringSelectionPolicy::MinimumRaycastFraction)
                    {
                        evidence.adjusted = BuildWallSafeSteeringPoint(
                            player, point, commandPoint,
                            evidence.candidateClearance,
                            evidence.adjustedClearance,
                            evidence.clearanceKnown,
                            evidence.adjustedClearanceKnown);
                        if (evidence.adjusted)
                        {
                            NavPoint adjustedReached{};
                            evidence.adjustedRaycastValid =
                                provider_.IsSurfaceSegmentReachable(
                                    PlayerPoint(player), commandPoint,
                                    evidence.adjustedRaycastFraction,
                                    adjustedReached);
                        }
                    }
                }

                lastEvidence = evidence;
                const auto decision = SteeringSelectionPolicy::Assess(evidence);
                if (decision == SteeringCandidateDecision::RejectedClearance)
                {
                    LogSteeringSelection(
                        tick, fromIndex, evidence,
                        "rejected_clearance", point);
                }
                if (decision == SteeringCandidateDecision::Accepted)
                {
                    const float commandDistance = Distance2D(
                        player.x, player.y,
                        commandPoint.x, commandPoint.y);
                    Debug::Logger::Info(
                        "NAVMESH 11B: CTM portal " +
                        std::to_string(candidate + 1) + "/" +
                        std::to_string(points_.size()) +
                        " reason=" + reason +
                        " distance=" + Float(commandDistance));
                    const auto issuedEdge=ProvenIssuedSteeringEdge(player,
                        candidate,commandPoint,evidence.adjusted);
                    if (!IssueNavMovement(player,commandPoint,CtmPrecision,
                            rejectedFarther ? NavCommandSource::FallbackNearer :
                                NavCommandSource::OrdinarySteering,issuedEdge))
                    {
                        return false;
                    }

                    pointIndex_ = SteeringSelectionPolicy::CommitIndex(
                        fromIndex, candidate, true, true);
                    issuedOrdinarySteeringTarget_ = commandPoint;
                    issuedOrdinarySteeringTargetValid_ = true;
                    ++commands_;
                    lastCommandTick_ = tick;
                    lastProgressTick_ = tick;
                    bestCornerDistance_ = distance;
                    if (candidate > fromIndex)
                    {
                        Debug::Logger::Info(
                            "NAVMESH 13C.1: SURFACE LOOKAHEAD portal=" +
                            std::to_string(fromIndex + 1) + "->" +
                            std::to_string(candidate + 1) + "/" +
                            std::to_string(points_.size()));
                    }
                    if (rise > 0.35f && lastStairLogPoint_ != candidate)
                    {
                        lastStairLogPoint_ = candidate;
                        Debug::Logger::Info(
                            "NAVMESH 13D.7: STAIR / ASCENT MODE portal=" +
                            std::to_string(candidate + 1) + "/" +
                            std::to_string(points_.size()) +
                            " rise=" + Float(rise) +
                            "; dense polygon-by-polygon steering retained");
                    }
                    if (evidence.adjusted)
                    {
                        Debug::Logger::Info(
                            "NAVMESH 14I.1: WALL-CLEARANCE STEERING portal=" +
                            std::to_string(candidate + 1) + "/" +
                            std::to_string(points_.size()) +
                            " clearance=" + Float(evidence.candidateClearance) +
                            " -> " + Float(evidence.adjustedClearance) +
                            " insetTarget=(" + Float(commandPoint.x) + "," +
                            Float(commandPoint.y) + "," +
                            Float(commandPoint.z) + ")");
                    }
                    LogSteeringSelection(
                        tick, fromIndex, evidence,
                        rejectedFarther ? "fallback_nearer" : "accepted",
                        commandPoint);
                    return true;
                }

                rejectedFarther = true;
                if (candidate == firstCandidate)
                    break;
            }

            LogSteeringSelection(
                tick, fromIndex, lastEvidence,
                "recovery", points_[firstCandidate]);
            if (haveVerticalRecovery && IssueVerticalPortalRecovery(
                    player, tick, verticalRecoveryPoint,
                    verticalRecoveryHorizontal, verticalRecoveryDelta,
                    verticalRecoveryCandidateIndex))
            {
                return true;
            }
            const DirectedPolyTransition localFailure=
                AttributeFailedSteering(player,fromIndex,firstCandidate);
            if (IssueVerifiedPortalStage(player,tick,localFailure))
                return true;
            if (IssueSurfaceRecovery(
                    player, tick, points_[firstCandidate],
                    "no clearance-safe corridor steering target",
                    localFailure))
            {
                return true;
            }
            if (IssueWallTrapRecovery(player, tick, points_[firstCandidate]))
                return true;
            const auto exhaustion = LocalRecoveryExhaustionPolicy::Assess(
                surfaceRecoveryAttempts_, MaximumSurfaceRecoveryAttempts,
                lastSafeNav_.valid && !lastSafeBacktrackActive_,
                lastSafeBacktrackAttempts_, MaximumLastSafeBacktracks);
            if (exhaustion != LocalRecoveryExhaustionDecision::NotExhausted)
            {
                const char* routeKind = completeLongStageActive_
                    ? "complete_long_stage" : "ordinary";
                Debug::Logger::Info(
                    "NAV 14N.4.1c LOCAL RECOVERY EXHAUSTED"
                    " routeKind=" + std::string(routeKind) +
                    " stageIndex=" + std::to_string(completeLongStageCount_) +
                    " destination=(" + Float(destination_.x) + "," +
                    Float(destination_.y) + "," + Float(destination_.z) + ")" +
                    " activeStageDestination=" +
                    (completeLongStageActive_
                        ? "(" + Float(activeStageDestination_.x) + "," +
                            Float(activeStageDestination_.y) + "," +
                            Float(activeStageDestination_.z) + ")"
                        : std::string("none")) +
                    " playerPosition=(" + Float(player.x) + "," +
                    Float(player.y) + "," + Float(player.z) + ")" +
                    " surfaceRecoveryAttempts=" +
                    std::to_string(surfaceRecoveryAttempts_) +
                    " failedFromPoly=" + HexPoly(failedCorridor_.failedFromPoly) +
                    " failedToPoly=" + HexPoly(failedCorridor_.failedToPoly) +
                    " transitionKnown=" +
                    (failedCorridor_.transitionKnown ? "yes" : "no") +
                    " lastSafePoly=" + HexPoly(lastSafeNav_.valid
                        ? lastSafeNav_.polyRef : 0) +
                    " lastSafeDistance=" + Float(lastSafeNav_.valid
                        ? Distance2D(player.x, player.y,
                            lastSafeNav_.position.x, lastSafeNav_.position.y)
                        : 0.0f) +
                    " lastSafeBacktrackAttempts=" +
                    std::to_string(lastSafeBacktrackAttempts_) +
                    " persistentHazardCount=" + std::to_string(
                        NavigationHazardMemory::Instance().HardCellCount(mapId_)) +
                    " corridorFingerprint=" + std::to_string(lastPathFingerprint_) +
                    " physicalProgress=" + Float(Distance2D(
                        player.x, player.y,
                        surfaceRecoveryStartX_, surfaceRecoveryStartY_)) +
                    " distanceToStage=" + Float(completeLongStageActive_
                        ? Distance2D(player.x, player.y,
                            activeStageDestination_.x, activeStageDestination_.y)
                        : 0.0f) +
                    " classification=surface_recovery_exhausted");
                if (exhaustion ==
                        LocalRecoveryExhaustionDecision::TryLastSafeBacktrack &&
                    BeginLastSafeBacktrack(
                        player, tick, "14N.4.1c surface recovery exhausted"))
                {
                    Debug::Logger::Info("NAV RECOVERY ESCALATE intent="+
                        std::to_string(intentId_)+" episode="+
                        std::to_string(surfaceEpisodeId_)+
                        " decision=backtrack reason=surface_attempt_budget_exhausted");
                    Debug::Logger::Info(
                        "NAV 14N.4.1c LOCAL RECOVERY ESCALATION"
                        " decision=last_safe_backtrack"
                        " routeKind=" + std::string(routeKind) +
                        " reason=surface_recovery_exhausted"
                        " stageIndex=" + std::to_string(completeLongStageCount_));
                    return true;
                }
                lastPlanFailure_ = NavigationPlanFailure::SurfaceRecoveryExhausted;
                Debug::Logger::Info("NAV RECOVERY ESCALATE intent="+
                    std::to_string(intentId_)+" episode="+
                    std::to_string(surfaceEpisodeId_)+
                    " decision=fail reason=surface_and_backtrack_budget_exhausted");
                Debug::Logger::Info(
                    "NAV 14N.4.1c LOCAL RECOVERY ESCALATION"
                    " decision=fail reason=surface_recovery_exhausted"
                    " routeKind=" + std::string(routeKind) +
                    " stageIndex=" + std::to_string(completeLongStageCount_));
            }
            return false;
        }

        bool HasSafePartialVerticalProfile(
            const NavPathResult& path,
            float& maximumRise,
            float& maximumAscentSlope,
            std::size_t& worstSegmentIndex) const
        {
            maximumRise = 0.0f;
            maximumAscentSlope = 0.0f;
            worstSegmentIndex = 0;

            for (std::size_t i = 1; i < path.points.size(); ++i)
            {
                const NavPoint& previous = path.points[i - 1];
                const NavPoint& current = path.points[i];

                const float rise = current.z - previous.z;
                const float run = Distance2D(
                    previous.x, previous.y, current.x, current.y);

                if (!std::isfinite(rise) || !std::isfinite(run))
                    return false;

                if (rise <= 0.0f)
                    continue;

                const float slope =
                    run >= PartialStageMinimumSlopeRun
                        ? rise / run
                        : (rise > PartialStageMaximumAscentPerSegment
                            ? 1.0e30f
                            : 0.0f);

                if (rise > maximumRise)
                    maximumRise = rise;

                if (slope > maximumAscentSlope)
                {
                    maximumAscentSlope = slope;
                    worstSegmentIndex = i;
                }

                if (rise > PartialStageMaximumAscentPerSegment &&
                    (run < PartialStageMinimumSlopeRun ||
                     slope > PartialStageMaximumAscentSlope))
                {
                    return false;
                }
            }

            return true;
        }

        void UpdateLastSafeNavState(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (hardStallEpisodes_ > 0 || surfaceRecoveryActive_ ||
                verticalPortalRecoveryActive_ || lastSafeBacktrackActive_)
            {
                return;
            }

            NavPoint projected{};
            std::uint64_t polyRef = 0;
            if (!provider_.ProjectToNavMesh(
                    PlayerPoint(player), projected, polyRef))
            {
                return;
            }

            LastSafeNavState observed{polyRef, projected, tick, true};
            if (!lastSafeCandidate_.valid)
            {
                lastSafeCandidate_ = observed;
                lastSafeNav_ = observed;
                Debug::Logger::Info(
                    "NAVMESH 13D.7: LAST SAFE POLY ref=" + HexPoly(polyRef) +
                    " position=(" + Float(projected.x) + "," +
                    Float(projected.y) + "," + Float(projected.z) + ")");
                return;
            }

            const float moved = Distance2D(
                lastSafeCandidate_.position.x,
                lastSafeCandidate_.position.y,
                projected.x,
                projected.y);
            if (moved < LastSafePromotionDistance)
                return;

            // Keep one proven movement interval behind the live position so
            // recovery has somewhere useful to return to.
            lastSafeNav_ = lastSafeCandidate_;
            lastSafeCandidate_ = observed;
            Debug::Logger::Info(
                "NAVMESH 13D.7: LAST SAFE POLY ref=" +
                HexPoly(lastSafeNav_.polyRef) + " position=(" +
                Float(lastSafeNav_.position.x) + "," +
                Float(lastSafeNav_.position.y) + "," +
                Float(lastSafeNav_.position.z) + ") timestamp=" +
                std::to_string(lastSafeNav_.timestamp));
        }

        void QuarantinePartialTail(
            const NavPathResult& path,
            std::uint64_t tick)
        {
            LearnPersistentHazard(
                tick, 2, "repeated partial corridor without physical progress");
            if (path.corridorPolys.size() < 2)
                return;

            const std::uint64_t from = path.corridorPolys[path.corridorPolys.size() - 2];
            const std::uint64_t to = path.corridorPolys.back();
            for (BlockedTransition& item : blockedTransitions_)
            {
                if (item.fromPoly == from && item.toPoly == to)
                {
                    item.failures = std::max(
                        item.failures, BlockedTransitionFailureThreshold);
                    item.expiresAtTick = tick + BlockedTransitionTtlTicks;
                    return;
                }
            }

            if (blockedTransitions_.size() >= MaximumBlockedTransitions)
                blockedTransitions_.erase(blockedTransitions_.begin());
            blockedTransitions_.push_back(BlockedTransition{
                from, to, BlockedTransitionFailureThreshold,
                tick + BlockedTransitionTtlTicks});
        }

        bool BeginLastSafeBacktrack(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const char* reason)
        {
            if (!RouteCostProbePolicy::MayIssueMovement(
                    startOptions_.planningOnly))
                return false;
            if (!lastSafeNav_.valid ||
                lastSafeBacktrackAttempts_ >= MaximumLastSafeBacktracks)
            {
                return false;
            }

            NavPoint target = lastSafeNav_.position;
            const float initialDistance = Distance2D(
                player.x, player.y, target.x, target.y);
            if (initialDistance <= LastSafeBacktrackArrivalDistance)
                return false;

            float reachableFraction = 0.0f;
            NavPoint reached{};
            if (!provider_.IsSurfaceSegmentReachable(
                    PlayerPoint(player), target, reachableFraction, reached) ||
                reachableFraction < 0.985f)
            {
                if (!provider_.MoveAlongSurface(
                        PlayerPoint(player), target, reached))
                {
                    return false;
                }
                const float recoveredStep = Distance2D(
                    player.x, player.y, reached.x, reached.y);
                if (recoveredStep < SurfaceRecoveryMinimumStep)
                    return false;
                target = reached;
                Debug::Logger::Info(
                    "NAVMESH 13D.7: MOVE ALONG SURFACE RECOVERY reason=last-safe backtrack target=(" +
                    Float(target.x) + "," + Float(target.y) + "," +
                    Float(target.z) + ")");
            }

            if (!IssueNavMovement(player,target,0.50f,
                    NavCommandSource::Backtrack))
            {
                return false;
            }

            ++commands_;
            ++lastSafeBacktrackAttempts_;
            lastSafeBacktrackActive_ = true;
            lastSafeBacktrackTarget_ = target;
            lastSafeBacktrackBestDistance_ = Distance2D(
                player.x, player.y, target.x, target.y);
            lastSafeBacktrackProgressTick_ = tick;
            lastCommandTick_ = tick;
            SetState(GenericNavMeshFollowState::Moving);
            Debug::Logger::Info(
                std::string("NAVMESH 13D.7: BACKTRACK TO LAST SAFE reason=") +
                reason + " ref=" + HexPoly(lastSafeNav_.polyRef) +
                " attempt=" + std::to_string(lastSafeBacktrackAttempts_) +
                "/" + std::to_string(MaximumLastSafeBacktracks) +
                " distance=" + Float(initialDistance));
            return true;
        }

        bool UpdateLastSafeBacktrack(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (!lastSafeBacktrackActive_)
                return false;

            const float remaining = Distance2D(
                player.x, player.y,
                lastSafeBacktrackTarget_.x, lastSafeBacktrackTarget_.y);
            if (remaining <= LastSafeBacktrackArrivalDistance)
            {
                lastSafeBacktrackActive_ = false;
                Debug::Logger::Info(
                    "NAVMESH 13D.7: REPLAN FROM LAST SAFE ref=" +
                    HexPoly(lastSafeNav_.polyRef) +
                    " remaining=" + Float(remaining));
                ResetEscalatingRecovery(
                    player,
                    tick,
                    Distance2D(
                        player.x,
                        player.y,
                        destination_.x,
                        destination_.y));
                PlanFrom(player, tick, true, "last_safe_backtrack");
                return true;
            }

            if (remaining + LastSafeBacktrackProgressThreshold <
                lastSafeBacktrackBestDistance_)
            {
                lastSafeBacktrackBestDistance_ = remaining;
                lastSafeBacktrackProgressTick_ = tick;
                return true;
            }

            if (tick >= lastSafeBacktrackProgressTick_ &&
                tick - lastSafeBacktrackProgressTick_ >=
                    LastSafeBacktrackStallTicks)
            {
                lastSafeBacktrackActive_ = false;
                Debug::Logger::Info(
                    "NAVMESH 13D.7: BACKTRACK TO LAST SAFE stalled remaining=" +
                    Float(remaining));
                SetState(GenericNavMeshFollowState::Failed);
                StopAtCurrentPosition(player);
                return true;
            }

            return true;
        }

        bool PlanFrom(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            bool replan,
            const char* planReason = "ordinary_replan",
            bool countOrdinaryReplan = true)
        {
            lastPlanFailure_ = NavigationPlanFailure::None;
            lastValidationDetail_ = {};
            lastDestinationProjected_ = false;
            const bool reusingActiveStage =
                CompleteLongStagePolicy::PlanTarget(
                    completeLongStageActive_, completeLongStageNeedsFinalPlan_) ==
                CompleteLongPlanTarget::ActiveStage;
            const NavPoint& planningDestination = reusingActiveStage
                ? activeStageDestination_ : destination_;
            if (reusingActiveStage)
            {
                Debug::Logger::Info(
                    "NAV 14N.4.1 COMPLETE LONG STAGE REPLAN"
                    " stageIndex=" + std::to_string(completeLongStageCount_) +
                    " activeStageDestination=(" + Float(activeStageDestination_.x) +
                    "," + Float(activeStageDestination_.y) + "," +
                    Float(activeStageDestination_.z) + ")" +
                    " originalDestination=(" + Float(destination_.x) + "," +
                    Float(destination_.y) + "," + Float(destination_.z) + ")" +
                    " playerPosition=(" + Float(player.x) + "," +
                    Float(player.y) + "," + Float(player.z) + ")" +
                    " distanceToStage=" + Float(Distance2D(
                        player.x, player.y, activeStageDestination_.x,
                        activeStageDestination_.y)) +
                    " distanceToFinal=" + Float(Distance2D(
                        player.x, player.y, destination_.x, destination_.y)) +
                    " reason=" + planReason + " decision=reuse_active_stage");
            }
            SetState(
                GenericNavMeshFollowState::
                    Planning
            );

            NavPathResult path{};
            PathValidationDetail validationDetail{};
            std::string underlyingHazardQueryError{};

            struct PlanProfileScope
            {
                const NavPathResult& path;
                const NavigationInitTier& tier;
                const NavigationPlanFailure& reason;
                const PathValidationDetail& detail;
                PathValidationDetail& retainedDetail;
                bool& retainedProjection;
                NavigationInitTier& retainedQueryTier;
                std::string& retainedError;
                const std::uint64_t intent;
                const std::string& underlyingHazardError;
                std::chrono::steady_clock::time_point start =
                    std::chrono::steady_clock::now();
                double queryMs = 0.0;
                double validationMs = 0.0;
                float pathLength = 0.0f;
                bool pathLengthKnown = false;
                const char* result = "failed";

                ~PlanProfileScope()
                {
                    retainedDetail = detail;
                    retainedProjection = path.endPoly != 0;
                    retainedQueryTier = tier;
                    retainedError = path.error;
                    const double totalMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();
                    std::ostringstream stream;
                    stream << std::fixed << std::setprecision(3)
                        << "NAV 14N.3 PLAN PROFILE: mode="
                        << NavigationInitTelemetryPolicy::TierName(tier)
                        << " threadId=" << GetCurrentThreadId()
                        << " queryMs=" << queryMs
                        << " validationMs=" << validationMs
                        << " totalMs=" << totalMs
                        << " result=" << result
                        << " reason=" << NavigationInitTelemetryPolicy::ReasonName(reason)
                        << " pathLength=";
                    if (pathLengthKnown)
                        stream << pathLength;
                    else
                        stream << "unknown";
                    stream << " polygonCount=" << path.polygonCount
                        << " validationDetail=" <<
                            PathValidationDiagnosticPolicy::Name(detail.reason);
                    Debug::Logger::Info(stream.str());
                    if (std::string_view(result) == "failed")
                    {
                        std::ostringstream failure;
                        failure << "NAV PLAN FAILURE intent=" << intent
                            << " tier=" << NavigationInitTelemetryPolicy::TierName(tier)
                            << " reason=" << NavigationInitTelemetryPolicy::ReasonName(reason)
                            << " startPoly=0x" << std::hex << path.startPoly
                            << " destinationPoly=0x" << path.endPoly
                            << " queryStatus=0x" << path.findPathStatus << std::dec
                            << " loadedTiles=" << path.loadedTiles
                            << " polygonCount=" << path.polygonCount
                            << " outOfNodes=" << (path.findPathOutOfNodes ? "yes" : "no")
                            << " bufferTooSmall=" << (path.findPathBufferTooSmall ? "yes" : "no")
                            << " partial=" << (path.findPathPartialResult ? "yes" : "no")
                            << " includeFlags=" << path.includeFlags
                            << " excludeFlags=" << path.excludeFlags
                            << " avoidanceRequested=" << path.avoidanceRequestedCount
                            << " queryError={" << path.error << "}"
                            << " underlyingHazardQueryError={" << underlyingHazardError << "}"
                            << " validationDetail=" << PathValidationDiagnosticPolicy::Name(detail.reason);
                        Debug::Logger::Info(failure.str());
                    }
                }
            } planProfile{path, currentInitTier_, lastPlanFailure_,
                validationDetail, lastValidationDetail_, lastDestinationProjected_,
                lastQueryTier_, lastQueryError_, intentId_, underlyingHazardQueryError};

            const auto queryStarted =
                std::chrono::steady_clock::now();

            std::vector<std::uint64_t> blockedPolygons =
                startOptions_.planningOnly
                    ? std::vector<std::uint64_t>{}
                    : ActiveBlockedPolygons(tick);
            const std::size_t transientBlockedCount = blockedPolygons.size();
            const std::vector<std::uint64_t> persistentHazardPolygons =
                ResolvePersistentHazardPolygons(player);

            Debug::Logger::Info(
                "NAV ROUTE CONTRACT mode=" + std::string(startOptions_.planningOnly ? "probe" : "execution") +
                " origin=(" + Float(player.x) + "," + Float(player.y) + "," + Float(player.z) +
                ") destination=(" + Float(planningDestination.x) + "," + Float(planningDestination.y) +
                "," + Float(planningDestination.z) + ") persistentHazardPolys=" +
                std::to_string(persistentHazardPolygons.size()) + " hazardFingerprint=" +
                std::to_string(RouteCostProbePolicy::HazardFingerprint(persistentHazardPolygons)) +
                " validation=14O.1 revalidation=every_plan maximumPathLength=" + Float(MaximumPathLength));

            for (const std::uint64_t ref : persistentHazardPolygons)
            {
                if (std::find(blockedPolygons.begin(), blockedPolygons.end(), ref) ==
                    blockedPolygons.end())
                {
                    blockedPolygons.push_back(ref);
                }
            }

            bool queryOk = false;
            bool directedNoAlternative = false;
            bool terrainRejected = false;
            std::vector<std::uint64_t> effectiveBlockedPolygons =
                blockedPolygons;
            if (!blockedPolygons.empty())
            {
                Debug::Logger::Info(
                    "NAVMESH 14I.0: SAFE REPLAN transientBlocked=" +
                    std::to_string(transientBlockedCount) +
                    " persistentHazardPolys=" +
                    std::to_string(persistentHazardPolygons.size()) +
                    " totalAvoided=" + std::to_string(blockedPolygons.size()));

                queryOk = provider_.FindPathAvoidingPolygons(
                    PlayerPoint(player),
                    planningDestination,
                    blockedPolygons,
                    path);

                // A temporary transition blacklist may over-constrain a route.
                // Persistent hard cells are stronger evidence and are never
                // silently discarded. Retry once with only persistent cells.
                if (!queryOk && !persistentHazardPolygons.empty())
                {
                    Debug::Logger::Info(
                        "NAVMESH 14I.0: combined avoidance had no corridor; "
                        "retrying with persistent hard hazards only.");
                    queryOk = provider_.FindPathAvoidingPolygons(
                        PlayerPoint(player),
                        planningDestination,
                        persistentHazardPolygons,
                        path);
                    effectiveBlockedPolygons = persistentHazardPolygons;
                }

                if (!queryOk && persistentHazardPolygons.empty())
                {
                    Debug::Logger::Info(
                        "NAVMESH 13D.4: transient avoidance produced no complete corridor; "
                        "falling back once to the unmodified mesh query while preserving "
                        "the transition failure history.");
                    queryOk = provider_.FindPath(
                        PlayerPoint(player),
                        planningDestination,
                        path);
                    effectiveBlockedPolygons.clear();
                }
                else if (!queryOk)
                {
                    const bool criticalCorpseRecovery =
                        destinationLabel_.find("death recovery corpse route") !=
                            std::string::npos;

                    if (criticalCorpseRecovery)
                    {
                        Debug::Logger::Info(
                            "NAV HAZARD 14I.0: no hazard-free corpse corridor exists; "
                            "using one critical recovery fallback while local stall guards remain active.");
                        queryOk = provider_.FindPath(
                            PlayerPoint(player), planningDestination, path);
                        effectiveBlockedPolygons.clear();
                    }
                    else
                    {
                        underlyingHazardQueryError = path.error;
                        path.error =
                            "Persistent navigation hazard memory rejected every safe corridor.";
                        Debug::Logger::Info(
                            "NAV HAZARD 14I.0: ROUTE REJECTED because every available corridor "
                            "crosses a learned hard hazard; caller should choose another target/sector.");
                    }
                }
            }
            else
            {
                queryOk = provider_.FindPath(
                    PlayerPoint(player),
                    planningDestination,
                    path);
            }

            // 13D.4 can only mask a whole target polygon. Apply that
            // approximation *after* proving that this particular route uses
            // the learned directed edge. A reverse B->A route is untouched.
            const DirectedPolyTransition matchedBadEdge = queryOk
                ? badVerticalTransitions_.FirstMatch(path.corridorPolys)
                : DirectedPolyTransition{};
            if (matchedBadEdge.Valid())
            {
                const std::uint64_t candidateFingerprint =
                    FingerprintPath(path.points);
                std::vector<std::uint64_t> avoidancePolygons =
                    effectiveBlockedPolygons;
                const auto matchedTargets =
                    badVerticalTransitions_.MatchedTargetPolygons(
                        path.corridorPolys);
                for (const std::uint64_t target : matchedTargets)
                {
                    if (std::find(avoidancePolygons.begin(),
                                  avoidancePolygons.end(), target) ==
                        avoidancePolygons.end())
                        avoidancePolygons.push_back(target);
                }
                NavPathResult alternative{};
                const bool alternateQueryOk = provider_.FindPathAvoidingPolygons(
                    PlayerPoint(player), planningDestination,
                    avoidancePolygons, alternative);
                const auto alternativeDecision =
                    badVerticalTransitions_.AssessAlternative(
                        path.corridorPolys, alternateQueryOk,
                        alternative.corridorPolys);
                const bool alternativeAvoidsBadEdge =
                    alternativeDecision == EpisodeBadTransitionPolicy::
                        AlternativeDecision::UseAlternative;
                Debug::Logger::Info(
                    "NAV 15B.1 TRANSITION AVOIDANCE fromPoly=" +
                    HexPoly(matchedBadEdge.from) + " toPoly=" +
                    HexPoly(matchedBadEdge.to) +
                    " decision=" + (alternativeAvoidsBadEdge
                        ? std::string("applied") : std::string("no_alternative")) +
                    " requestedTargets=" + std::to_string(matchedTargets.size()) +
                    " appliedPolygons=" +
                    std::to_string(alternative.avoidanceAppliedCount) +
                    " corridorFingerprint=" +
                    std::to_string(candidateFingerprint));
                if (alternativeAvoidsBadEdge)
                    path = std::move(alternative);
                else
                {
                    directedNoAlternative = true;
                    queryOk = false;
                    path.error =
                        "No alternate corridor around exhausted vertical transition.";
                }
            }

            // A Detour link is not proof that WoW can walk its portal. Check
            // every ALL_CROSSINGS movement leg before entering Moving. Reuse
            // the same directed episode memory/13D.4 polygon masking for a
            // bounded alternate, rather than issuing a CTM toward the cliff.
            for (int terrainQuery = 0; queryOk; ++terrainQuery)
            {
                const NavTerrainValidation terrain =
                    provider_.ValidateTerrainRoute(PlayerPoint(player), path);
                if (terrain.valid)
                    break;

                const DirectedPolyTransition rejected{
                    terrain.fromPoly, terrain.toPoly};
                Debug::Logger::Info(
                    "NAV 14O.1 TERRAIN REJECT fromPoly=" +
                    HexPoly(terrain.fromPoly) + " toPoly=" +
                    HexPoly(terrain.toPoly) +
                    " horizontal=" + Float(terrain.geometry.horizontal) +
                    " vertical=" + Float(terrain.geometry.vertical) +
                    " riseRun=" + Float(terrain.geometry.riseRun) +
                    " fromFlags=" + std::to_string(terrain.fromFlags) +
                    " toFlags=" + std::to_string(terrain.toFlags) +
                    " portalKnown=" +
                    std::string(terrain.portalKnown ? "yes" : "no") +
                    " portalA=(" + Float(terrain.portalA.x) + "," +
                    Float(terrain.portalA.y) + "," +
                    Float(terrain.portalA.z) + ")" +
                    " portalB=(" + Float(terrain.portalB.x) + "," +
                    Float(terrain.portalB.y) + "," +
                    Float(terrain.portalB.z) + ")" +
                    " reason=" + terrain.reason);

                if (!rejected.Valid() ||
                    terrainQuery >= MaximumTerrainAlternativeQueries)
                {
                    validationDetail={
                        PathValidationSubreason::UnsafeTerrainAttemptLimit,0,
                        terrain.fromPoly,terrain.toPoly,
                        terrain.geometry.horizontal,terrain.geometry.vertical,
                        terrain.fromFlags,terrain.toFlags,terrainQuery+1};
                    queryOk = false;
                    terrainRejected = true;
                    path.error = "Terrain route contains an unsafe vertical transition.";
                    break;
                }

                badVerticalTransitions_.Learn(rejected);
                std::vector<std::uint64_t> avoidancePolygons =
                    effectiveBlockedPolygons;
                for (const std::uint64_t target :
                     badVerticalTransitions_.MatchedTargetPolygons(
                         path.corridorPolys))
                    if (std::find(avoidancePolygons.begin(),
                                  avoidancePolygons.end(), target) ==
                        avoidancePolygons.end())
                        avoidancePolygons.push_back(target);

                NavPathResult alternative{};
                const bool alternativeOk = provider_.FindPathAvoidingPolygons(
                    PlayerPoint(player), planningDestination,
                    avoidancePolygons, alternative);
                const bool useAlternative =
                    badVerticalTransitions_.AssessAlternative(
                        path.corridorPolys, alternativeOk,
                        alternative.corridorPolys) ==
                    EpisodeBadTransitionPolicy::AlternativeDecision::
                        UseAlternative;
                Debug::Logger::Info(
                    "NAV 14O.1 FALLBACK reason=unsafe_vertical_transition "
                    "decision=" + std::string(useAlternative
                        ? "alternate" : "no_alternative") +
                    " fromPoly=" + HexPoly(rejected.from) +
                    " toPoly=" + HexPoly(rejected.to) +
                    " attempt=" + std::to_string(terrainQuery + 1) +
                    "/" + std::to_string(MaximumTerrainAlternativeQueries));
                if (!useAlternative)
                {
                    validationDetail=
                        PathValidationDiagnosticPolicy::UnsafeTerrainNoAlternative(
                            terrain.fromPoly,terrain.toPoly,
                            terrain.geometry.horizontal,
                            terrain.geometry.vertical,terrain.fromFlags,
                            terrain.toFlags,terrainQuery+1);
                    queryOk = false;
                    terrainRejected = true;
                    path.error = "No alternate corridor around unsafe terrain transition.";
                    break;
                }
                path = std::move(alternative);
            }

            const auto queryElapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - queryStarted
                ).count();
            planProfile.queryMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - queryStarted).count();

            Debug::Logger::Info(
                "NAVMESH 11B.7: Detour query time=" +
                std::to_string(queryElapsedMs) +
                " ms."
            );

            if (!queryOk)
            {
                if (terrainRejected)
                    Debug::Logger::Info("PATH VALIDATION FAILED reason="+
                        std::string(PathValidationDiagnosticPolicy::Name(
                            validationDetail.reason))+
                        " fromPoly="+HexPoly(validationDetail.fromPoly)+
                        " toPoly="+HexPoly(validationDetail.toPoly)+
                        " horizontal="+Float(validationDetail.segmentLength)+
                        " verticalDelta="+Float(validationDetail.verticalDelta)+
                        " fromFlags="+std::to_string(validationDetail.fromFlags)+
                        " toFlags="+std::to_string(validationDetail.toFlags)+
                        " alternativeAttempt="+
                            std::to_string(validationDetail.alternativeAttempt));
                lastPlanFailure_ = directedNoAlternative
                    ? NavigationPlanFailure::SurfaceRecoveryExhausted
                    : terrainRejected
                        ? NavigationPlanFailure::PathValidationFailed
                        : NavigationInitTelemetryPolicy::QueryFailure(path.error);
                Debug::Logger::Info(
                    "NAVMESH 11B: path query failed."
                );

                Debug::Logger::Info(
                    "Reason: " +
                    path.error
                );

                SetState(
                    GenericNavMeshFollowState::
                        Failed
                );

                StopAtCurrentPosition(
                    player
                );

                return false;
            }

            if (LongPathDiagnosticPolicy::UsesPartialStaging(path.partial))
            {
                const float startFinalDistance = Distance2D(
                    player.x, player.y,
                    planningDestination.x, planningDestination.y);
                const float residualDistance = Distance2D(
                    path.corridorEnd.x, path.corridorEnd.y,
                    planningDestination.x, planningDestination.y);
                const float verticalResidual = std::fabs(
                    path.corridorEnd.z - planningDestination.z);
                const float destinationProgress =
                    startFinalDistance - residualDistance;
                const float reachableAdvance = Distance2D(
                    path.projectedStart.x, path.projectedStart.y,
                    path.corridorEnd.x, path.corridorEnd.y);
                const float physicalProgress = partialStageActive_
                    ? Distance2D(
                        player.x, player.y,
                        partialCorridorStartX_, partialCorridorStartY_)
                    : 0.0f;
                const std::uint64_t corridorFingerprint =
                    FingerprintCorridor(path.corridorPolys);
                // Raw dtPolyRef values change when route-scoped tiles are
                // unloaded/reloaded. Use the quantized steering geometry for
                // repeated-partial detection so the same physical corridor
                // remains recognizable across mesh generations.
                const std::uint64_t geometryFingerprint =
                    FingerprintPath(path.points);
                const bool repeatedCorridor =
                    partialCorridorFingerprint_ != 0 &&
                    geometryFingerprint == partialCorridorFingerprint_;
                const bool repeatedWithoutProgress =
                    repeatedCorridor &&
                    physicalProgress < PartialStageMinimumPhysicalProgress;
                const bool localBoundedStage =
                    residualDistance <= PartialStageMaximumResidualDistance &&
                    verticalResidual <= PartialStageMaximumVerticalResidual;
                const bool capacityLimitedStage =
                    LongPathDiagnosticPolicy::CapacityLimitedPartialStage(
                        path.partial, path.findPathOutOfNodes,
                        reachableAdvance, destinationProgress,
                        PartialStageMinimumProgress, PartialStageRepeatProgress);
                float maximumPartialRise = 0.0f;
                float maximumPartialAscentSlope = 0.0f;
                std::size_t worstPartialSegment = 0;
                const bool safeVerticalProfile =
                    HasSafePartialVerticalProfile(
                        path,
                        maximumPartialRise,
                        maximumPartialAscentSlope,
                        worstPartialSegment);

                std::ostringstream corridorStream;
                for (std::size_t i = 0; i < path.corridorPolys.size(); ++i)
                {
                    if (i != 0)
                        corridorStream << ',';
                    corridorStream << HexPoly(path.corridorPolys[i]);
                }

                if (path.findPathOutOfNodes)
                {
                    Debug::Logger::Info(
                        "NAVMESH 13D.7.1: OUT OF NODES queryNodes=" +
                        std::to_string(path.queryNodePoolSize) +
                        " startDistance=" + Float(startFinalDistance) +
                        " remainingDistance=" + Float(residualDistance) +
                        " reachableAdvance=" + Float(reachableAdvance) +
                        " destinationProgress=" + Float(destinationProgress));
                }

                Debug::Logger::Info(
                    "NAVMESH 13D.7: PARTIAL CORRIDOR startRef=" +
                    HexPoly(path.startPoly) + " endRef=" + HexPoly(path.endPoly) +
                    " pathCount=" + std::to_string(path.polygonCount) +
                    " firstRef=" + HexPoly(path.corridorPolys.empty()
                        ? 0 : path.corridorPolys.front()) +
                    " lastRef=" + HexPoly(path.lastPoly) +
                    " straightPoints=" + std::to_string(path.points.size()) +
                    " startDistance=" + Float(startFinalDistance) +
                    " remainingDistance=" + Float(residualDistance) +
                    " verticalDelta=" + Float(verticalResidual) +
                    " reachableAdvance=" + Float(reachableAdvance) +
                    " physicalProgress=" + Float(physicalProgress) +
                    " partialAttempt=" + std::to_string(partialStageAttempts_ + 1) +
                    "/" + std::to_string(MaximumPartialStageAttempts) +
                    " queryNodes=" + std::to_string(path.queryNodePoolSize) +
                    " corridorFingerprint=" + std::to_string(corridorFingerprint) +
                    " geometryFingerprint=" + std::to_string(geometryFingerprint) +
                    " maxLocalRise=" + Float(maximumPartialRise) +
                    " maxLocalAscentSlope=" + Float(maximumPartialAscentSlope) +
                    " worstSegment=" + std::to_string(worstPartialSegment) +
                    " connected=" + (path.corridorConnected ? "yes" : "no") +
                    " outOfNodes=" + (path.findPathOutOfNodes ? "yes" : "no"));
                Debug::Logger::Info(
                    "NAVMESH 13D.7: PARTIAL CORRIDOR refs=[" +
                    corridorStream.str() + "]");

                if (repeatedWithoutProgress)
                {
                    Debug::Logger::Info(
                        "NAVMESH 13D.7.1: REPEATED PARTIAL CORRIDOR geometryFingerprint=" +
                        std::to_string(geometryFingerprint) +
                        " polyFingerprint=" + std::to_string(corridorFingerprint) +
                        " physicalProgress=" + Float(physicalProgress));
                    QuarantinePartialTail(path, tick);
                    if (BeginLastSafeBacktrack(
                            player, tick, "repeated partial corridor without physical progress"))
                    {
                        return true;
                    }
                }

                std::string partialRejectReason;
                if (partialStageAttempts_ >= MaximumPartialStageAttempts)
                    partialRejectReason = "partial staging attempt budget exhausted";
                else if (!std::isfinite(residualDistance) ||
                         !std::isfinite(verticalResidual) ||
                         !std::isfinite(destinationProgress) ||
                         !std::isfinite(reachableAdvance))
                    partialRejectReason = "partial route metrics are non-finite";
                else if (!path.corridorConnected)
                    partialRejectReason = "partial polygon corridor is not connected";
                else if (!safeVerticalProfile)
                    partialRejectReason = "unsafe local ascent profile";
                else if (reachableAdvance < PartialStageMinimumProgress)
                    partialRejectReason = "reachable prefix is too short";
                else if (destinationProgress < PartialStageRepeatProgress)
                    partialRejectReason = "reachable prefix does not reduce destination distance";
                else if (!localBoundedStage && !capacityLimitedStage)
                    partialRejectReason = "partial result is neither a local seam nor node-pool-limited prefix";
                else if (repeatedWithoutProgress)
                    partialRejectReason = "same geometric partial corridor repeated without physical progress";

                if (!partialRejectReason.empty())
                {
                    lastPlanFailure_ = NavigationPlanFailure::PartialOrUnusablePath;
                    Debug::Logger::Info(
                        "NAVMESH 13D.7.1: PARTIAL CORRIDOR REJECTED startDistance=" +
                        Float(startFinalDistance) +
                        " remainingDistance=" + Float(residualDistance) +
                        " destinationProgress=" + Float(destinationProgress) +
                        " reachableAdvance=" + Float(reachableAdvance) +
                        " verticalDelta=" + Float(verticalResidual) +
                        " physicalProgress=" + Float(physicalProgress) +
                        " partialAttempt=" + std::to_string(partialStageAttempts_ + 1) +
                        "/" + std::to_string(MaximumPartialStageAttempts) +
                        " queryNodes=" + std::to_string(path.queryNodePoolSize) +
                        " geometryFingerprint=" + std::to_string(geometryFingerprint) +
                        " maxLocalRise=" + Float(maximumPartialRise) +
                        " maxLocalAscentSlope=" + Float(maximumPartialAscentSlope) +
                        " rejectGuard=\"" + partialRejectReason + "\"");
                    Debug::Logger::Info(
                        "Reason: " + partialRejectReason + ".");
                    SetState(GenericNavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                    return false;
                }

                if (!startOptions_.planningOnly)
                {
                    ++partialStageAttempts_;
                    partialStageActive_ = true;
                    partialStageStartFinalDistance_ = startFinalDistance;
                    partialStageBestResidualDistance_ = std::min(
                        partialStageBestResidualDistance_, residualDistance);
                    partialCorridorFingerprint_ = geometryFingerprint;
                    partialCorridorStartX_ = player.x;
                    partialCorridorStartY_ = player.y;
                }

                Debug::Logger::Info(
                    "NAVMESH 13D.7.1: LONG/LOCAL PARTIAL STAGE ACCEPTED partialAttempt=" +
                    std::to_string(partialStageAttempts_) +
                    "/" + std::to_string(MaximumPartialStageAttempts) +
                    " startDistance=" + Float(startFinalDistance) +
                    " remainingDistance=" + Float(residualDistance) +
                    " destinationProgress=" + Float(destinationProgress) +
                    " reachableAdvance=" + Float(reachableAdvance) +
                    " verticalDelta=" + Float(verticalResidual) +
                    " physicalProgress=" + Float(physicalProgress) +
                    " queryNodes=" + std::to_string(path.queryNodePoolSize) +
                    " polyFingerprint=" + std::to_string(corridorFingerprint) +
                    " geometryFingerprint=" + std::to_string(geometryFingerprint) +
                    " maxLocalRise=" + Float(maximumPartialRise) +
                    " maxLocalAscentSlope=" + Float(maximumPartialAscentSlope) +
                    " mode=" + (capacityLimitedStage
                        ? "node-pool-capacity-prefix" : "bounded-local-stage"));
                Debug::Logger::Info(
                    "Policy: follow the connected Detour prefix, then replan from the live endpoint instead of idling on DT_PARTIAL_RESULT.");
            }
            else if (partialStageActive_)
            {
                Debug::Logger::Info(
                    "NAVMESH 13D.7: COMPLETE CORRIDOR RECOVERED after partial staging attempts=" +
                    std::to_string(partialStageAttempts_));
                partialStageActive_ = false;
                partialStageAttempts_ = 0;
                partialStageBestResidualDistance_ = 1.0e30f;
                partialStageStartFinalDistance_ = 1.0e30f;
                partialCorridorFingerprint_ = 0;
            }

            std::string error;

            float length =
                0.0f;

            bool selectedCompleteLongStage = false;
            NavPoint selectedStageDestination{};
            std::size_t selectedStagePointIndex = 0;
            double selectedStageTotalLength = 0.0;
            double selectedStagePrefixLength = 0.0;

            const auto validationStarted = std::chrono::steady_clock::now();
            bool pathValid = ValidatePath(
                path, error, length, &lastPlanFailure_,&validationDetail);
            if (!pathValid &&
                lastPlanFailure_ == NavigationPlanFailure::PathLengthExceeded &&
                !reusingActiveStage)
            {
                LogLongPathDiagnostic(player, path, length);
                const bool complete = !path.partial &&
                    !path.findPathPartialResult &&
                    !path.findPathOutOfNodes &&
                    path.corridorConnected && path.endPoly != 0 &&
                    path.lastPoly == path.endPoly;
                const bool capacityAvailable =
                    !path.findPathBufferTooSmall &&
                    !path.findStraightPathBufferTooSmall &&
                    path.maximumPolygons > 0 &&
                    path.polygonCount < path.maximumPolygons &&
                    path.maximumStraightPoints > 0 &&
                    path.points.size() <
                        static_cast<std::size_t>(path.maximumStraightPoints) &&
                    path.pointPolys.size() == path.points.size();
                std::vector<CompleteLongStagePoint> stagePoints;
                stagePoints.reserve(path.points.size());
                for (const NavPoint& point : path.points)
                    stagePoints.push_back({point.x, point.y, point.z});
                const CompleteLongStageSelection stage =
                    CompleteLongStagePolicy::Select(
                        stagePoints, complete, capacityAvailable,
                        MaximumPathLength, CompleteLongStageLength,
                        MaximumSegment, MaximumVerticalSegment,
                        RecoveryResetProgressDistance,
                        RecoveryResetProgressDistance);
                if (stage.result == CompleteLongStageResult::Selected)
                {
                    if (!CompleteLongStagePolicy::CanStartStage(
                            completeLongStageCount_, MaximumCompleteLongStages))
                    {
                        Debug::Logger::Info(
                            "NAV 14N.4.1 COMPLETE LONG STAGE FAILED reason=stage_budget_exhausted"
                            " stageIndex=" + std::to_string(completeLongStageCount_));
                        lastPlanFailure_ = NavigationPlanFailure::PathLengthExceeded;
                        SetState(GenericNavMeshFollowState::Failed);
                        StopAtCurrentPosition(player);
                        return false;
                    }
                    selectedStagePointIndex = stage.pointIndex;
                    selectedStageDestination = path.points[stage.pointIndex];
                    selectedStageTotalLength = stage.totalLength;
                    selectedStagePrefixLength = stage.prefixLength;
                    path.points.resize(stage.pointIndex + 1);
                    path.pointPolys.resize(stage.pointIndex + 1);
                    pathValid = ValidatePath(path, error, length,
                                             &lastPlanFailure_,&validationDetail);
                    selectedCompleteLongStage = pathValid;
                }
                if (!pathValid)
                {
                    if (stage.result == CompleteLongStageResult::UnsafeSegment)
                    {
                        lastPlanFailure_ = NavigationPlanFailure::PathValidationFailed;
                        error = "complete path contains an unsafe later segment.";
                    }
                    Debug::Logger::Info(
                        "NAV 14N.4.1 COMPLETE LONG STAGE FAILED reason=" +
                        std::string(stage.result == CompleteLongStageResult::UnsafeSegment
                            ? "unsafe_segment" : stage.result == CompleteLongStageResult::NoForwardStage
                                ? "no_forward_stage" : stage.result == CompleteLongStageResult::Ineligible
                                    ? "ineligible" : "prefix_validation_failed"));
                }
            }
            if (!pathValid)
            {
                Debug::Logger::Info("PATH VALIDATION FAILED reason="+
                    std::string(PathValidationDiagnosticPolicy::Name(
                        validationDetail.reason))+
                    " pointIndex="+std::to_string(validationDetail.pointIndex)+
                    " length="+Float(validationDetail.segmentLength)+
                    " verticalDelta="+Float(validationDetail.verticalDelta));
                planProfile.validationMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - validationStarted).count();
                Debug::Logger::Info(
                    "NAVMESH 11B: path validation failed."
                );

                Debug::Logger::Info(
                    "Reason: " +
                    error
                );

                SetState(
                    GenericNavMeshFollowState::
                        Failed
                );

                StopAtCurrentPosition(
                    player
                );

                return false;
            }

            if (selectedCompleteLongStage)
            {
                const float priorStageDistance = Distance2D(
                    activeStageDestination_.x, activeStageDestination_.y,
                    selectedStageDestination.x, selectedStageDestination.y);
                const float progressSinceCompletion = Distance2D(
                    player.x, player.y,
                    completeLongStageCompletionPosition_.x,
                    completeLongStageCompletionPosition_.y);
                if (CompleteLongStagePolicy::RejectRepeatedNewStage(
                        completeLongStageCount_ > 0 &&
                            !completeLongStageActive_,
                        priorStageDistance, progressSinceCompletion,
                        RecoveryResetProgressDistance))
                {
                    Debug::Logger::Info(
                        "NAV 14N.4.1 COMPLETE LONG STAGE FAILED reason=repeated_stage_without_progress"
                        " stageIndex=" + std::to_string(completeLongStageCount_) +
                        " progressSinceCompletion=" +
                        Float(progressSinceCompletion));
                    lastPlanFailure_ = NavigationPlanFailure::OtherUnknown;
                    SetState(GenericNavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                    return false;
                }
            }

            planProfile.pathLength = length;
            planProfile.pathLengthKnown = true;
            planProfile.validationMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - validationStarted).count();

            Debug::Logger::Info(
                "NAV 14O.1 ROUTE ACCEPT mode=" +
                std::string(path.steepFallback
                    ? "validated_steep_fallback" : "non_steep") +
                " polygons=" + std::to_string(path.polygonCount) +
                " steepPolygons=" +
                std::to_string(path.steepPolygonCount) +
                " length=" + Float(length));

            Debug::Logger::Info("NAV ROUTE VALIDATED mode=" +
                std::string(startOptions_.planningOnly ? "probe" : "execution") +
                " startPoly=" + HexPoly(path.startPoly) + " endPoly=" + HexPoly(path.endPoly) +
                " pathFingerprint=" + std::to_string(FingerprintPath(path.points)) +
                " length=" + Float(length) + " proof=validated_current_plan");

            if (startOptions_.planningOnly)
            {
                // A probe shares Detour initialization/query and the normal
                // path validation, but stops before movement, steering,
                // recovery, or mutable hazard/hysteresis episode state.
                plannedPathLength_ = length;
                plannedProjectedDestination_ = path.projectedDestination;
                planningOnlyReachedDestination_ =
                    !selectedCompleteLongStage && !path.partial &&
                    !path.findPathPartialResult && path.corridorConnected &&
                    path.endPoly != 0 && path.lastPoly == path.endPoly;
                planProfile.result = "planning_only_ready";
                SetState(GenericNavMeshFollowState::Planned);
                return true;
            }

            if (replan)
            {
                if (countOrdinaryReplan)
                    ++replans_;
                ++totalReplans_;
            }

            const std::uint64_t fingerprint = FingerprintPath(path.points);
            if (replan && failedCorridor_.active)
            {
                std::uint64_t candidateFromPoly = 0;
                std::uint64_t candidateToPoly = 0;
                const bool candidateTransitionKnown =
                    ResolveCandidateLocalTransition(
                        player, path, candidateFromPoly, candidateToPoly);
                const bool recoveryAvailable =
                    surfaceRecoveryAttempts_ < MaximumSurfaceRecoveryAttempts ||
                    (lastSafeNav_.valid &&
                     lastSafeBacktrackAttempts_ < MaximumLastSafeBacktracks);
                const CorridorReplanAssessment assessment =
                    CorridorReplanHysteresisPolicy::Assess(
                        failedCorridor_, fingerprint, player.x, player.y,
                        RecoveryResetProgressDistance, recoveryAvailable,
                        candidateTransitionKnown,
                        candidateFromPoly, candidateToPoly,
                        !surfaceEpisodeActive_ ||
                            SurfaceRecoveryEpisodePolicy::Assess(
                                surfaceEpisodeInitialDistance_,Distance2D(player.x,player.y,
                                    destination_.x,destination_.y),
                                RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_)==
                                SurfaceRecoveryQuality::Progress);
                const bool meaningfulProgress =
                    assessment.decision ==
                    CorridorReplanDecision::AllowAfterProgress;
                const bool repeatedFailure =
                    assessment.decision ==
                        CorridorReplanDecision::SuppressRepeated ||
                    assessment.decision == CorridorReplanDecision::Escalate;

                if (repeatedFailure)
                    ++repeatedCorridorPlans_;
                else
                    repeatedCorridorPlans_ = 0;

                Debug::Logger::Info(
                    "NAV 14N.2 HYSTERESIS: corridorFingerprint=" +
                    std::to_string(fingerprint) +
                    " failedFingerprint=" +
                    std::to_string(failedCorridor_.fingerprint) +
                    " failedFromPoly=" + HexPoly(failedCorridor_.failedFromPoly) +
                    " failedToPoly=" + HexPoly(failedCorridor_.failedToPoly) +
                    " candidateLocalFromPoly=" + HexPoly(candidateFromPoly) +
                    " candidateLocalToPoly=" + HexPoly(candidateToPoly) +
                    " transitionKnown=" +
                    (failedCorridor_.transitionKnown ? "yes" : "no") +
                    " candidateTransitionKnown=" +
                    (candidateTransitionKnown ? "yes" : "no") +
                    " localTransitionMatch=" +
                    (assessment.localTransitionMatch ? "yes" : "no") +
                    " failureAnchorDistance=" + Float(assessment.anchorDistance) +
                    " meaningfulProgress=" +
                    (meaningfulProgress ? "yes" : "no") +
                    " repeatCount=" + std::to_string(repeatedCorridorPlans_) +
                    " decision=" +
                    (assessment.decision == CorridorReplanDecision::Escalate
                        ? "escalate" : repeatedFailure
                            ? "suppress_repeated" : "allow") +
                    " reason=" +
                    CorridorReplanHysteresisPolicy::ReasonName(
                        assessment.reason));

                if (meaningfulProgress)
                    failedCorridor_ = CorridorFailureRecord{};

                if (repeatedFailure)
                {
                    Debug::Logger::Info(
                        "NAVMESH 13C.1: REPEATED CORRIDOR fingerprint=" +
                        std::to_string(fingerprint) +
                        " repeat=" + std::to_string(repeatedCorridorPlans_) +
                        "; ordinary corridor CTM suppressed.");

                    // Keep the failed corridor and its steering cursor intact.
                    // Recovery completes on a later Update; never replan here.
                    if (surfaceRecoveryAttempts_ < MaximumSurfaceRecoveryAttempts &&
                        IssueSurfaceRecovery(
                            player, tick, path.points[1],
                            "14N.2 repeated failed corridor"))
                    {
                        planProfile.result = "recovery";
                        SetState(GenericNavMeshFollowState::Moving);
                        ResetMotionWatchdog(player, tick);
                        return true;
                    }
                    if (BeginLastSafeBacktrack(
                            player, tick, "14N.2 repeated failed corridor"))
                    {
                        planProfile.result = "recovery";
                        ResetMotionWatchdog(player, tick);
                        return true;
                    }

                    Debug::Logger::Info(
                        "NAV 14N.2 HYSTERESIS: corridorFingerprint=" +
                        std::to_string(fingerprint) +
                        " failedFingerprint=" +
                        std::to_string(failedCorridor_.fingerprint) +
                        " failedFromPoly=" + HexPoly(failedCorridor_.failedFromPoly) +
                        " failedToPoly=" + HexPoly(failedCorridor_.failedToPoly) +
                        " candidateLocalFromPoly=" + HexPoly(candidateFromPoly) +
                        " candidateLocalToPoly=" + HexPoly(candidateToPoly) +
                        " transitionKnown=" +
                        (failedCorridor_.transitionKnown ? "yes" : "no") +
                        " candidateTransitionKnown=" +
                        (candidateTransitionKnown ? "yes" : "no") +
                        " localTransitionMatch=" +
                        (assessment.localTransitionMatch ? "yes" : "no") +
                        " failureAnchorDistance=" + Float(assessment.anchorDistance) +
                        " meaningfulProgress=no repeatCount=" +
                        std::to_string(repeatedCorridorPlans_) +
                        " decision=escalate reason=bounded_recovery_unavailable");
                    lastPlanFailure_ = NavigationPlanFailure::BoundedLocalRecoveryUnavailable;
                    SetState(GenericNavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                    return false;
                }
            }
            else
            {
                repeatedCorridorPlans_ = 0;
            }

            points_ =
                path.points;
            ++routeGeneration_;
            if(surfaceEpisodeActive_ && replan)
            {
                ++surfaceEpisodeRouteRefreshes_;
                Debug::Logger::Info("NAV RECOVERY EPISODE intent="+
                    std::to_string(intentId_)+" episode="+
                    std::to_string(surfaceEpisodeId_)+
                    " routeGeneration="+std::to_string(routeGeneration_)+
                    " routeRefreshes="+std::to_string(surfaceEpisodeRouteRefreshes_)+
                    " candidateLocalFromPoly="+HexPoly(path.startPoly)+
                    " candidateLocalNextPoly="+HexPoly(path.corridorPolys.size()>1
                        ?path.corridorPolys[1]:0)+
                    " result=validated_route_refresh_no_reset");
            }
            corridorPolys_ = path.corridorPolys;
            pointPolyRefs_ = path.pointPolys;
            pathStartPoly_ = path.startPoly;
            pathEndPoly_ = path.endPoly;

            if (path.partial)
            {
                Debug::Logger::Info(
                    "NAVMESH 13D.7: following partial staging corridor polygons=" +
                    std::to_string(path.corridorPolys.size()) +
                    " steeringPoints=" + std::to_string(path.points.size()));
            }

            lastPathFingerprint_ = fingerprint;
            issuedOrdinarySteeringTargetValid_ = false;

            pointIndex_ =
                1;
            steeringAtCorridorEnd_ = false;

            plannedPathLength_ =
                length;

            LogPath(
                path,
                length,
                replan
            );

            pausedForCombat_ =
                false;

            finalApproachActive_ = false;
            finalApproachSuppressed_ = false;
            finalApproachIssuedTick_ = 0;

            ResetMotionWatchdog(
                player,
                tick
            );

            SetState(
                GenericNavMeshFollowState::
                    Moving
            );

            if (!IssueCurrentCorner(
                    player,
                    tick,
                    replan
                        ? "replan"
                        : "initial"))
            {
                if (lastPlanFailure_ == NavigationPlanFailure::None)
                    lastPlanFailure_ = NavigationPlanFailure::OtherUnknown;
                SetState(
                    GenericNavMeshFollowState::
                        Failed
                );

                StopAtCurrentPosition(
                    player
                );

                return false;
            }

            if (selectedCompleteLongStage)
            {
                completeLongStageCount_ =
                    CompleteLongStagePolicy::StageIndexAfterAcceptedPlan(
                        completeLongStageCount_, true);
                completeLongStageActive_ = true;
                activeStageDestination_ = selectedStageDestination;
                completeLongStageStart_ = PlayerPoint(player);
                completeLongStageStartFinalDistance_ = Distance2D(
                    player.x, player.y, destination_.x, destination_.y);
                completeLongStageStarted_ = std::chrono::steady_clock::now();
                const std::string stageEvent = completeLongStageCount_ == 1
                    ? "BEGIN" : "ADVANCE";
                Debug::Logger::Info(
                    "NAV 14N.4.1 COMPLETE LONG STAGE " + stageEvent +
                    " mode=" + NavigationInitTelemetryPolicy::TierName(currentInitTier_) +
                    " originalDestination=(" + Float(destination_.x) + "," +
                    Float(destination_.y) + "," + Float(destination_.z) + ")" +
                    " stageDestination=(" + Float(selectedStageDestination.x) + "," +
                    Float(selectedStageDestination.y) + "," +
                    Float(selectedStageDestination.z) + ")" +
                    " directDistanceToFinal=" +
                    Float(completeLongStageStartFinalDistance_) +
                    " remainingStraightPathLength=" +
                    Float(static_cast<float>(selectedStageTotalLength -
                                             selectedStagePrefixLength)) +
                    " stagePrefixLength=" +
                    Float(static_cast<float>(selectedStagePrefixLength)) +
                    " stageIndex=" + std::to_string(completeLongStageCount_) +
                    " polygonCount=" + std::to_string(path.polygonCount) +
                    " straightPointIndex=" + std::to_string(selectedStagePointIndex) +
                    " reason=complete_long_path");
            }
            else if (!reusingActiveStage)
            {
                completeLongStageActive_ = false;
            }
            completeLongStageNeedsFinalPlan_ = false;

            planProfile.result = issuedOrdinarySteeringTargetValid_
                ? "accepted" : "started_non_corridor";
            return true;
        }

        bool Replan(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            bool fromVerticalStall = false)
        {
            if (
                replans_ >=
                    MaximumReplans)
            {
                const DirectedPolyTransition failedEdge =
                    badVerticalTransitions_.BoundaryEvidence(
                        fromVerticalStall, true,
                        !boundaryVerticalAvoidanceAttempted_);
                if (failedEdge.Valid() &&
                    badVerticalTransitions_.Learn(failedEdge))
                {
                    boundaryVerticalAvoidanceAttempted_ = true;
                    Debug::Logger::Info(
                        "NAV 15B.1 TRANSITION LEARNED fromPoly=" +
                        HexPoly(failedEdge.from) + " toPoly=" +
                        HexPoly(failedEdge.to) +
                        " reason=replan_boundary_after_vertical_stalls" +
                        " stalledObservations=" + std::to_string(
                            badVerticalTransitions_.StalledObservations()) +
                        " verticalAttempts=" + std::to_string(
                            verticalPortalRecoveryAttempts_) +
                        " corridorFingerprint=" +
                        std::to_string(lastPathFingerprint_) +
                        " occurrence=" + std::to_string(
                            badVerticalTransitions_.ConsecutiveIssued()) +
                        " avoidedTransitions=" + std::to_string(
                            badVerticalTransitions_.Size()));
                    // Replace the would-be terminal replan with one directed
                    // alternative query. Do not refill the ordinary budget.
                    if (PlanFrom(player, tick, true,
                                 "vertical_stall_replan_boundary", false))
                        return true;
                    Debug::Logger::Info(
                        "NAV 15B.1 TRANSITION AVOIDANCE fromPoly=" +
                        HexPoly(failedEdge.from) + " toPoly=" +
                        HexPoly(failedEdge.to) +
                        " decision=no_alternative reason=boundary_plan_failed" +
                        " planFailure=" +
                        NavigationInitTelemetryPolicy::ReasonName(
                            lastPlanFailure_));
                }
                Debug::Logger::Info(
                    "NAVMESH 11B: replan safety limit reached."
                );

                lastPlanFailure_ = NavigationPlanFailure::ReplanBudgetExhausted;
                Debug::Logger::Info("NAV STUCK RESULT result=failed reason=replan_budget_exhausted");

                SetState(
                    GenericNavMeshFollowState::
                        Failed
                );

                StopAtCurrentPosition(
                    player
                );

                return false;
            }

            Debug::Logger::Info(
                "NAVMESH 11B: STUCK - replanning from live position."
            );

            return
                PlanFrom(
                    player,
                    tick,
                    true
                );
        }

        void LogIncrementalEvent(const char* event, const char* reason,
                                 double lastStepMs = 0.0) const
        {
            const auto progress = provider_.InitializationProgress();
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3)
                << "NAV 14N.3.1 INIT " << event
                << " mode=" << NavigationInitTelemetryPolicy::TierName(currentInitTier_)
                << " tilesTotal=" << progress.total
                << " tilesProcessed=" << progress.processed
                << " tilesLoaded=" << progress.loaded
                << " tilesFailed=" << progress.failed
                << " elapsedMs=" << progress.elapsedMs
                << " workMs=" << progress.workMs
                << " lastStepMs=" << lastStepMs
                << " threadId=" << GetCurrentThreadId()
                << " destination=\"" << destinationLabel_ << "\""
                << " intent=" << intentId_
                << " reason=" << reason;
            Debug::Logger::Info(stream.str());
        }

        bool BeginInitializationTier(const Objects::PlayerState& player,
                                     NavigationInitTier tier,
                                     std::string& error)
        {
            currentInitTier_ = tier;
            bool began = false;
            if (tier == NavigationInitTier::FullMap)
            {
                fullMapFallbackAttempted_ = true;
                began = provider_.BeginIncrementalFullMap(
                    initializationDirectory_, mapId_, error);
            }
            else
            {
                began = provider_.BeginIncrementalForRoute(
                    initializationDirectory_, mapId_, PlayerPoint(player),
                    destination_, tier == NavigationInitTier::Route
                        ? InitialRouteTileMargin : ExpandedRouteTileMargin,
                    error, NavigationInitTelemetryPolicy::TierName(tier));
            }
            initializationPending_ = began;
            initializationProgressLogBucket_ = 0;
            if (!began && startOptions_.planningOnly)
                lastPlanFailure_ = NavigationPlanFailure::InitializationFailed;
            SetState(began ? GenericNavMeshFollowState::Planning
                           : GenericNavMeshFollowState::Failed);
            LogIncrementalEvent(began ? "BEGIN" : "FAILED",
                                began ? "none" : error.c_str());
            return began;
        }

        void BeginFullMapFallback(const Objects::PlayerState& player)
        {
            const NavigationInitTier nextTier =
                NavigationInitTelemetryPolicy::NextTier(
                    NavigationInitTier::Expanded,
                    startOptions_.allowFullMapFallback);
            Debug::Logger::Info(
                "NAV 14N.3 FALLBACK: from=expanded to=" +
                std::string(NavigationInitTelemetryPolicy::TierName(nextTier)) +
                " reason=" +
                NavigationInitTelemetryPolicy::ReasonName(lastPlanFailure_) +
                " destination=\"" + destinationLabel_ + "\" threadId=" +
                std::to_string(GetCurrentThreadId()));
            if (!startOptions_.allowFullMapFallback)
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.8: full-map fallback disabled for this route attempt.");
                SetState(GenericNavMeshFollowState::Failed);
                return;
            }
            Debug::Logger::Info(
                "NAVMESH 11B.8: route-scoped loading exhausted; falling back "
                "to the full-map loader for correctness.");
            provider_.Shutdown();
            std::string error;
            BeginInitializationTier(player, NavigationInitTier::FullMap, error);
        }

        void AdvanceInitialization(const Objects::PlayerState& player,
                                   std::uint64_t tick)
        {
            if (!initializationPending_)
                return;
            const auto stepStarted = std::chrono::steady_clock::now();
            std::string error;
            const auto result = provider_.StepIncremental(error);
            const double stepMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - stepStarted).count();
            const auto progress = provider_.InitializationProgress();
            if (result == DetourNavigationProvider::IncrementalStatus::Pending)
            {
                const std::size_t bucket = progress.processed / 25;
                if (bucket > initializationProgressLogBucket_)
                {
                    initializationProgressLogBucket_ = bucket;
                    LogIncrementalEvent("PROGRESS", "none", stepMs);
                }
                return;
            }
            initializationPending_ = false;
            LogIncrementalEvent(
                result == DetourNavigationProvider::IncrementalStatus::Ready
                    ? "READY" : "FAILED",
                result == DetourNavigationProvider::IncrementalStatus::Ready
                    ? "none" : error.c_str(), stepMs);
            const bool ready =
                result == DetourNavigationProvider::IncrementalStatus::Ready;
            retainIntentForInitialFallback_ = ready &&
                NavigationInitTelemetryPolicy::RetainIntentForFallback(
                    currentInitTier_, startOptions_.allowFullMapFallback);
            const bool planReady = ready && PlanFrom(player, tick, false);
            retainIntentForInitialFallback_ = false;
            if (planReady)
                return;

            if (result == DetourNavigationProvider::IncrementalStatus::Failed)
                lastPlanFailure_ = NavigationPlanFailure::InitializationFailed;
            if (currentInitTier_ == NavigationInitTier::Route)
            {
                // The original Start stopped on route initialization failure;
                // only an unusable planned route escalated to expanded tiles.
                if (result == DetourNavigationProvider::IncrementalStatus::Failed)
                {
                    SetState(GenericNavMeshFollowState::Failed);
                    return;
                }
                Debug::Logger::Info(
                    "NAVMESH 11B.8: initial route-scoped corridor did not "
                    "produce a usable path; retrying with expanded ADT margin.");
                Debug::Logger::Info(
                    "NAV 14N.3 FALLBACK: from=route to=expanded reason=" +
                    std::string(NavigationInitTelemetryPolicy::ReasonName(
                        lastPlanFailure_)) + " destination=\"" +
                    destinationLabel_ + "\" threadId=" +
                    std::to_string(GetCurrentThreadId()));
                provider_.Shutdown();
                if (!BeginInitializationTier(
                        player, NavigationInitTier::Expanded, error))
                {
                    lastPlanFailure_ = NavigationPlanFailure::InitializationFailed;
                    BeginFullMapFallback(player);
                }
                return;
            }
            if (currentInitTier_ == NavigationInitTier::Expanded)
            {
                BeginFullMapFallback(player);
                return;
            }
            SetState(GenericNavMeshFollowState::Failed);
        }

    public:
        ~GenericNavMeshPathFollower()
        {
            ReleaseIntent("owner_released");
            if (initializationPending_)
                LogIncrementalEvent("CANCELLED", "follower_destroyed");
        }

        bool Start(
            const Objects::PlayerState& player,
            std::uint64_t tick,
            const NavPoint& destination,
            std::uint32_t mapId,
            float arrivalDistance,
            const std::string& label,
            bool healthSafetyEnabled = true,
            const GenericNavMeshStartOptions& options = {},
            const std::source_location& source = std::source_location::current())
        {
            if (
                state_ !=
                    GenericNavMeshFollowState::
                        Idle)
            {
                return false;
            }

            ReleaseIntent("restarted");
            destination_ =
                destination;

            mapId_ =
                mapId;

            startOptions_ = options;
            planningOriginPlayer_ = player;
            RememberPlayerPosition(player);
            if (!startOptions_.planningOnly)
                NavigationHazardMemory::Instance().InitializeForMap(mapId_);

            healthSafetyEnabled_ =
                healthSafetyEnabled;

            finalArrivalDistance_ =
                arrivalDistance;

            destinationLabel_ =
                label;
            if(!options.planningOnly)
            {
                intentId_=++nextIntentId_;
                intentReleased_=false;
                intentOwner_=source.function_name();
                intentTick_=intentStarted_=tick;
                intentPosition_={player.x,player.y,player.z};
                intentDestination_=destination;
                std::ostringstream log;
                log<<"MOVEMENT INTENT intent="<<intentId_<<" owner={"<<intentOwner_
                   <<"} purpose={"<<label<<"} tick="<<tick<<" map="<<mapId
                   <<" origin="<<player.x<<","<<player.y<<","<<player.z
                   <<" destination="<<destination.x<<","<<destination.y<<","<<destination.z
                   <<" reason=owner_started_navigation";
                Debug::Logger::Info(log.str());
            }
            lastCorpsePortalDiagnosticFingerprint_ = 0;

            commands_ = 0;
            planningOnlyReachedDestination_ = false;
            plannedProjectedDestination_ = NavPoint{};
            replans_ = 0;
            totalReplans_ = 0;
            hardStallEpisodes_ = 0;
            hardStallBestFinalDistance_ = Distance2D(
                player.x, player.y, destination_.x, destination_.y);
            surfaceRecoveryActive_ = false;
            surfaceRecoveryAttempts_ = 0;
            surfaceEpisodeActive_ = false;
            surfaceEpisodeSequence_ = 0;
            surfaceEpisodeId_ = 0;
            surfaceEpisodeTargets_.clear();
            surfaceEpisodeForwardPortal_ = false;
            surfaceEpisodeRouteRefreshes_ = 0;
            routeGeneration_ = 0;
            surfaceRecoveryProgressTick_ = 0;
            surfaceRecoveryBestTargetDistance_ = 0.0f;
            repeatedCorridorPlans_ = 0;
            lastPathFingerprint_ = 0;
            failedCorridor_ = CorridorFailureRecord{};
            issuedOrdinarySteeringTargetValid_ = false;
            partialStageActive_ = false;
            partialStageAttempts_ = 0;
            partialStageBestResidualDistance_ = 1.0e30f;
            partialStageStartFinalDistance_ = 1.0e30f;
            partialCorridorFingerprint_ = 0;
            partialCorridorStartX_ = player.x;
            partialCorridorStartY_ = player.y;
            completeLongStageActive_ = false;
            completeLongStageNeedsFinalPlan_ = false;
            completeLongStageCount_ = 0;
            activeStageDestination_ = NavPoint{};
            completeLongStageStart_ = NavPoint{};
            completeLongStageCompletionPosition_ = NavPoint{};
            completeLongStageStartFinalDistance_ = 0.0f;
            lastSafeNav_ = LastSafeNavState{};
            lastSafeCandidate_ = LastSafeNavState{};
            lastSafeBacktrackActive_ = false;
            lastSafeBacktrackBestDistance_ = 0.0f;
            lastSafeBacktrackProgressTick_ = 0;
            lastSafeBacktrackAttempts_ = 0;
            lastStairLogPoint_ = static_cast<std::size_t>(-1);
            corridorPolys_.clear();
            pointPolyRefs_.clear();
            pathStartPoly_ = 0;
            pathEndPoly_ = 0;
            blockedTransitions_.clear();
            badVerticalTransitions_.Reset();
            verticalTransitionAvoidancePending_ = false;
            boundaryVerticalAvoidanceAttempted_ = false;
            escapeProbeActive_ = false;
            escapeProbeSide_ = 0;
            escapeExhaustionRecoveries_ = 0;
            escapeProbeIssuedTick_ = 0;
            finalApproachActive_ = false;
            finalApproachSuppressed_ = false;
            finalApproachIssuedTick_ = 0;
            finalApproachCommands_ = 0;
            verticalPortalRecoveryActive_ = false;
            verticalPortalRecoveryBypassActive_ = false;
            verticalPortalBypassRoutePoints_.clear();
            verticalPortalBypassRouteIndex_ = 0;
            verticalPortalBypassBestCornerDistance_ = 0.0f;
            verticalPortalRecoveryIssuedTick_ = 0;
            verticalPortalRecoveryProgressTick_ = 0;
            verticalPortalRecoveryInitialTargetDistance_ = 0.0f;
            verticalPortalRecoveryBestTargetDistance_ = 0.0f;
            verticalPortalRecoveryAttempts_ = 0;
            verticalPortalRecoveryTotalAttempts_ = 0;
            verticalPortalRecoveryBudgetBestFinalDistance_ = 1.0e30f;
            verticalPortalRecoveryAnchorValid_ = false;
            motionWatchdogInitialized_ = false;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "NAVMESH PHASE 11B: GENERIC OBJECTIVE ROUTE"
            );

            Debug::Logger::Info(
                "NAVMESH PHASE 13D.1-13D.4: POLY CORRIDOR + BLOCKED TRANSITION AVOIDANCE"
            );

            Debug::Logger::Info(
                "NAVMESH PHASE 13D.6: PARTIAL CORRIDOR STAGING + STAIR-AWARE STEERING"
            );

            Debug::Logger::Info(
                "NAVMESH PHASE 13D.7.1: MAX-CAPACITY LONG PARTIAL STAGING + LAST SAFE POLY"
            );
            Debug::Logger::Info(
                "NAVMESH PHASE 14I.1: WALL-CLEARANCE STEERING + CORNER-TRAP RECOVERY"
            );

            Debug::Logger::Info(
                "NAVMESH PHASE 14I.0: PERSISTENT SPATIAL HAZARD MEMORY + HARD-CELL AVOIDANCE"
            );

            Debug::Logger::Info(
                "Destination: " +
                destinationLabel_ +
                " (" +
                Float(destination_.x) +
                "," +
                Float(destination_.y) +
                "," +
                Float(destination_.z) +
                ")"
            );

            Debug::Logger::Info(
                "Arrival handoff distance: " +
                Float(
                    finalArrivalDistance_
                )
            );

            (void)tick;
            initializationDirectory_ =
                DetourNavigationProvider::ResolveMmapsDirectory();
            fullMapFallbackAttempted_ = false;
            std::string error;
            // Active followers retain their existing hold-on-start behavior.
            // Planning-only probes never issue a movement command.
            StopAtCurrentPosition(player);
            const bool began = BeginInitializationTier(
                player, NavigationInitTier::Route, error);
            if (began && options.initialAvoidedTransition.Valid())
            {
                if (SameNavMeshGeneration(options.initialTransitionMeshGeneration,
                        provider_.CacheStats().generation))
                    badVerticalTransitions_.Learn(options.initialAvoidedTransition);
                else
                    Debug::Logger::Info(
                        "NAV CACHE EDGE REJECT reason=stale_or_unknown_generation");
            }
            return began;
        }

        bool RejectInvalidatedTopology()
        {
            if (provider_.CacheGenerationCurrent()) return false;
            // A world/map generation change never reinterprets old poly refs
            // against a new mesh or dispatches CTM from a stale corridor.
            initializationPending_ = false;
            pausedForCombat_ = false;
            retainIntentForInitialFallback_ = false;
            lastPlanFailure_ = NavigationPlanFailure::MeshCacheInvalidated;
            points_.clear();
            corridorPolys_.clear();
            pointPolyRefs_.clear();
            pathStartPoly_ = pathEndPoly_ = 0;
            lastSafeNav_ = LastSafeNavState{};
            lastSafeCandidate_ = LastSafeNavState{};
            blockedTransitions_.clear();
            badVerticalTransitions_.Reset();
            IssuedSteeringCommandPolicy::Invalidate(issuedNavCommand_);
            SetState(GenericNavMeshFollowState::Failed);
            provider_.Shutdown();
            return true;
        }

        void Update(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            intentTick_=tick;
            intentPosition_={player.x,player.y,player.z};
            if ((initializationPending_ || pausedForCombat_ ||
                 state_ == GenericNavMeshFollowState::Moving ||
                 state_ == GenericNavMeshFollowState::Planned) &&
                RejectInvalidatedTopology()) return;
            const Objects::PlayerState& effectivePlayer =
                startOptions_.planningOnly ? planningOriginPlayer_ : player;
            RememberPlayerPosition(effectivePlayer);

            if (initializationPending_ && !pausedForCombat_)
            {
                AdvanceInitialization(effectivePlayer, tick);
                return;
            }

            if (startOptions_.planningOnly)
                return;

            if (
                state_ !=
                    GenericNavMeshFollowState::
                        Moving)
            {
                return;
            }

            const float hp =
                HealthPercent(
                    player
                );

            if (
                healthSafetyEnabled_ &&
                hp <=
                    MinimumSafetyHealthPercent)
            {
                Debug::Logger::Info(
                    "NAVMESH 11B: safety abort at HP " +
                    Float(hp) +
                    "%."
                );

                SetState(
                    GenericNavMeshFollowState::
                        Failed
                );

                StopAtCurrentPosition(
                    player
                );

                return;
            }

            const float finalDistance =
                Distance2D(
                    player.x,
                    player.y,
                    destination_.x,
                    destination_.y
                );

            if (verticalTransitionAvoidancePending_)
            {
                verticalTransitionAvoidancePending_ = false;
                Replan(player, tick);
                return;
            }

            if (UpdateLastSafeBacktrack(player, tick))
                return;

            UpdateLastSafeNavState(player, tick);

            if (
                CompleteLongStagePolicy::FinalArrivalAllowed(
                    completeLongStageActive_,
                    completeLongStageNeedsFinalPlan_) &&
                finalDistance <=
                    finalArrivalDistance_)
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "NAVMESH 11B: OBJECTIVE AREA REACHED"
                );

                Debug::Logger::Info(
                    "Destination: " +
                    destinationLabel_
                );

                Debug::Logger::Info(
                    "Final distance: " +
                    Float(
                        finalDistance
                    )
                );

                Debug::Logger::Info(
                    "Commands: " +
                    std::to_string(
                        commands_
                    ) +
                    " episodeReplans=" +
                    std::to_string(
                        replans_
                    ) +
                    " lifetimeReplans=" +
                    std::to_string(
                        totalReplans_
                    ) +
                    " finalApproachCommands=" +
                    std::to_string(
                        finalApproachCommands_
                    ) +
                    " motionAgeTicks=" +
                    std::to_string(
                        motionWatchdogInitialized_ && tick >= lastMotionTick_
                            ? tick - lastMotionTick_
                            : 0
                    )
                );

                Debug::Logger::Info(
                    "================================"
                );

                if (completeLongStageCount_ > 0)
                {
                    Debug::Logger::Info(
                        "NAV 14N.4.1 COMPLETE LONG ROUTE COMPLETE"
                        " stageCount=" + std::to_string(completeLongStageCount_) +
                        " finalDistance=" + Float(finalDistance));
                    completeLongStageActive_ = false;
                }

                SetState(
                    GenericNavMeshFollowState::
                        Arrived
                );

                return;
            }

            if (completeLongStageActive_ &&
                Distance2D(player.x, player.y, activeStageDestination_.x,
                           activeStageDestination_.y) <= CornerArrivalDistance)
            {
                const float physicalProgress = Distance2D(
                    player.x, player.y, completeLongStageStart_.x,
                    completeLongStageStart_.y);
                const float finalImprovement =
                    completeLongStageStartFinalDistance_ - finalDistance;
                if (CompleteLongStagePolicy::OnArrival(
                        physicalProgress, finalImprovement,
                        RecoveryResetProgressDistance) ==
                    CompleteLongStageArrivalDecision::Fail)
                {
                    Debug::Logger::Info(
                        "NAV 14N.4.1 COMPLETE LONG STAGE FAILED"
                        " reason=no_meaningful_stage_progress"
                        " stageIndex=" + std::to_string(completeLongStageCount_) +
                        " physicalProgress=" + Float(physicalProgress) +
                        " finalImprovement=" + Float(finalImprovement));
                    completeLongStageActive_ = false;
                    SetState(GenericNavMeshFollowState::Failed);
                    StopAtCurrentPosition(player);
                    return;
                }
                const auto elapsedMs = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() -
                        completeLongStageStarted_).count();
                Debug::Logger::Info(
                    "NAV 14N.4.1 COMPLETE LONG STAGE REACHED"
                    " stageIndex=" + std::to_string(completeLongStageCount_) +
                    " physicalProgress=" + Float(physicalProgress) +
                    " remainingDirectDistance=" + Float(finalDistance) +
                    " elapsedMs=" + std::to_string(elapsedMs));
                completeLongStageCompletionPosition_ = PlayerPoint(player);
                completeLongStageActive_ = false;
                completeLongStageNeedsFinalPlan_ = true;
                steeringAtCorridorEnd_ = false;
                Replan(player, tick);
                return;
            }

            if (steeringAtCorridorEnd_)
            {
                steeringAtCorridorEnd_ = false;
                Replan(player, tick);
                return;
            }

            if (UpdateSurfaceRecovery(player, tick))
            {
                return;
            }

            if (UpdateFinalApproach(player, tick, finalDistance))
            {
                return;
            }

            if (UpdateVerticalPortalRecovery(player, tick))
            {
                return;
            }

            if (
                CompleteLongStagePolicy::FinalArrivalAllowed(
                    completeLongStageActive_,
                    completeLongStageNeedsFinalPlan_) &&
                !finalApproachSuppressed_ &&
                finalDistance <= FinalApproachEnterDistance)
            {
                if (IssueFinalApproach(player, tick, "inside final band"))
                {
                    return;
                }
            }

            if (LocalRecoveryExhaustionPolicy::EarnedProgressReset(
                    finalDistance, hardStallBestFinalDistance_,
                    RecoveryResetProgressDistance) &&
                (!surfaceEpisodeActive_ || SurfaceRecoveryEpisodePolicy::Assess(
                    surfaceEpisodeInitialDistance_,finalDistance,
                    RecoveryResetProgressDistance,surfaceEpisodeForwardPortal_)==
                    SurfaceRecoveryQuality::Progress))
            {
                if (hardStallEpisodes_ > 0)
                {
                    Debug::Logger::Info(
                        "NAVMESH 12B.2: route made meaningful progress; "
                        "clearing stuck escalation. finalDistance=" +
                        Float(finalDistance)
                    );
                }
                ResetEscalatingRecovery(player, tick, finalDistance);
            }

            if (UpdateEscapeProbe(player, tick, finalDistance))
            {
                return;
            }

            if (
                pointIndex_ >=
                    points_.size())
            {
                if (partialStageActive_)
                {
                    Debug::Logger::Info(
                        "NAVMESH 13D.7: PARTIAL STAGE END REACHED -> reproject live player/destination and replan finalDistance=" +
                        Float(finalDistance));
                }
                Replan(
                    player,
                    tick
                );

                return;
            }

            const auto& point =
                points_[pointIndex_];

            const float cornerDistance =
                Distance2D(
                    player.x,
                    player.y,
                    point.x,
                    point.y
                );

            if (!motionWatchdogInitialized_)
            {
                ResetMotionWatchdog(
                    player,
                    tick
                );
            }
            else
            {
                const float motionDistance =
                    Distance2D(
                        player.x,
                        player.y,
                        lastMotionX_,
                        lastMotionY_
                    );

                if (motionDistance >= MotionProgressThreshold)
                {
                    ResetMotionWatchdog(
                        player,
                        tick
                    );
                }
                else if (
                    tick >= lastMotionTick_ &&
                    tick - lastMotionTick_ >= HardStallTicks)
                {
                    ++hardStallEpisodes_;

                    Debug::Logger::Info("NAV STUCK DETECT owner=current_navigator distance="+
                        Float(finalDistance)+" displacement="+Float(motionDistance)+
                        " elapsedTicks="+std::to_string(tick-lastMotionTick_)+
                        " reason=movement_expected_without_displacement");

                    Debug::Logger::Info(
                        "NAVMESH 12B.2: HARD STALL - no physical movement; "
                        "episode=" + std::to_string(hardStallEpisodes_) +
                        "/" + std::to_string(MaximumHardStallEpisodes) +
                        " cornerDistance=" + Float(cornerDistance) +
                        " finalDistance=" + Float(finalDistance)
                    );

                    RegisterFailedCorridor(player, "hard_stall", true);

                    // Phase 14I.1: before skipping a portal or repeating the
                    // same corridor, detect the common wall/corner trap shown
                    // by live runtime: the character has zero movement while
                    // pressed directly against collision geometry. Move away
                    // from the nearest Detour wall on a validated surface leg.
                    if (IssueWallTrapRecovery(player, tick, point))
                    {
                        ResetMotionWatchdog(player, tick);
                        return;
                    }

                    if (
                        hardStallEpisodes_ == 1 &&
                        cornerDistance <= HardStallNearPortalDistance &&
                        pointIndex_ + 1 < points_.size())
                    {
                        ResetMotionWatchdog(player, tick);

                        if (!IssueCurrentCorner(
                                player,
                                tick,
                                "12B.2 first-stall portal lookahead",
                                pointIndex_ + 1))
                        {
                            SetState(GenericNavMeshFollowState::Failed);
                            StopAtCurrentPosition(player);
                        }
                        return;
                    }

                    if (hardStallEpisodes_ <= 2)
                    {
                        if (repeatedCorridorPlans_ >= 1 &&
                            IssueSurfaceRecovery(
                                player,
                                tick,
                                point,
                                "repeated corridor after hard stall"))
                        {
                            ResetMotionWatchdog(player, tick);
                            return;
                        }

                        RegisterCurrentTransitionFailure(
                            tick,
                            "hard stall before replan");
                        ResetMotionWatchdog(player, tick);
                        Replan(player, tick);
                        return;
                    }

                    if (hardStallEpisodes_ == 3)
                    {
                        ResetMotionWatchdog(player, tick);
                        if (!IssueEscapeProbe(player, tick, -1))
                        {
                            if (!IssueEscapeProbe(player, tick, +1))
                            {
                                SetState(GenericNavMeshFollowState::Failed);
                                StopAtCurrentPosition(player);
                            }
                        }
                        return;
                    }

                    if (hardStallEpisodes_ >= MaximumHardStallEpisodes)
                    {
                        Debug::Logger::Info(
                            "NAVMESH 12B.2: REPEATED CORRIDOR STALL - "
                            "failing route instead of looping identical replans."
                        );
                        lastPlanFailure_ = NavigationPlanFailure::HardStallExhausted;
                        SetState(GenericNavMeshFollowState::Failed);
                        StopAtCurrentPosition(player);
                        return;
                    }

                    ResetMotionWatchdog(player, tick);
                    Replan(player, tick);
                    return;
                }
            }

            if (
                cornerDistance <=
                CornerArrivalDistance)
            {
                if(surfaceEpisodeActive_ && issuedOrdinarySteeringTargetValid_ &&
                    pointIndex_+1<points_.size())
                {
                    surfaceEpisodeForwardPortal_=true;
                    if(SurfaceRecoveryEpisodePolicy::Assess(
                        surfaceEpisodeInitialDistance_,finalDistance,
                        RecoveryResetProgressDistance,true)==
                        SurfaceRecoveryQuality::Progress)
                        ResetEscalatingRecovery(player,tick,finalDistance);
                }
                if (
                    pointIndex_ + 1 >=
                        points_.size())
                {
                    if (partialStageActive_)
                    {
                        Debug::Logger::Info(
                            "NAVMESH 13D.7: PARTIAL STAGE LAST PORTAL REACHED -> reproject live player/destination and replan finalDistance=" +
                            Float(finalDistance));
                    }
                    Replan(
                        player,
                        tick
                    );

                    return;
                }

                if (!IssueCurrentCorner(
                        player,
                        tick,
                        "next portal",
                        pointIndex_ + 1))
                {
                    SetState(
                        GenericNavMeshFollowState::
                            Failed
                    );

                    StopAtCurrentPosition(
                        player
                    );
                }

                return;
            }

            if (
                cornerDistance +
                    ProgressThreshold <
                bestCornerDistance_)
            {
                bestCornerDistance_ =
                    cornerDistance;

                lastProgressTick_ =
                    tick;
            }

            if (
                tick >=
                    lastProgressTick_ &&
                tick -
                    lastProgressTick_ >=
                    StuckTicks)
            {
                // Corner-distance stagnation can occur while the character
                // is still moving; it does not prove which local edge failed.
                RegisterFailedCorridor(player, "progress_stall", false);
                if (
                    cornerDistance <=
                        NearCornerRecoveryDistance &&
                    pointIndex_ + 1 <
                        points_.size())
                {
                    if (!IssueCurrentCorner(
                            player,
                            tick,
                            "near-portal lookahead",
                            pointIndex_ + 1))
                    {
                        SetState(
                            GenericNavMeshFollowState::
                                Failed
                        );

                        StopAtCurrentPosition(
                            player
                        );
                    }

                    return;
                }

                if (repeatedCorridorPlans_ >= 1 &&
                    IssueSurfaceRecovery(
                        player,
                        tick,
                        point,
                        "repeated corridor after progress stall"))
                {
                    return;
                }

                if (repeatedCorridorPlans_ >= 2)
                {
                    QuarantineCurrentTransition(
                        tick,
                        "repeated corridor after progress stall");
                    if (BeginLastSafeBacktrack(
                            player,
                            tick,
                            "repeated corridor after progress stall"))
                    {
                        ResetMotionWatchdog(player, tick);
                        return;
                    }
                }

                RegisterCurrentTransitionFailure(
                    tick,
                    "progress stall before replan");

                Replan(
                    player,
                    tick
                );

                return;
            }

            if (
                tick >=
                    lastCommandTick_ &&
                tick -
                    lastCommandTick_ >=
                    ReissueTicks)
            {
                if (!IssueCurrentCorner(
                        player,
                        tick,
                        "periodic refresh"))
                {
                    SetState(
                        GenericNavMeshFollowState::
                            Failed
                    );

                    StopAtCurrentPosition(
                        player
                    );
                }

                return;
            }

            if (
                tick >=
                    lastStatusTick_ &&
                tick -
                    lastStatusTick_ >=
                    20)
            {
                lastStatusTick_ =
                    tick;

                Debug::Logger::Info(
                    "NavMesh11B: state=" +
                    std::string(
                        StateNameInternal(
                            state_
                        )
                    ) +
                    " portal=" +
                    std::to_string(
                        pointIndex_ + 1
                    ) +
                    "/" +
                    std::to_string(
                        points_.size()
                    ) +
                    " portalDistance=" +
                    Float(
                        cornerDistance
                    ) +
                    " finalDistance=" +
                    Float(
                        finalDistance
                    ) +
                    " commands=" +
                    std::to_string(
                        commands_
                    ) +
                    " replans=" +
                    std::to_string(
                        replans_
                    )
                );
            }
        }

        bool PauseForCombat(
            const Objects::PlayerState& player,
            const std::string& reason)
        {
            if (
                state_ !=
                    GenericNavMeshFollowState::Moving &&
                state_ !=
                    GenericNavMeshFollowState::Planning)
            {
                return false;
            }

            if(intentId_!=0 && !pausedForCombat_)
                Debug::Logger::Info("MOVEMENT INTENT SUSPEND intent="+std::to_string(intentId_)+
                    " tick="+std::to_string(intentTick_)+" reason="+reason);
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "NAVMESH 11B.5: PAUSED FOR DEFENSIVE COMBAT"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "================================"
            );

            StopAtCurrentPosition(
                player
            );

            /*
             * Do NOT unload Detour or discard the verified corridor here.
             * Phase 11B.5 used Shutdown()+clear(), which forced all Kalimdor
             * tiles to be loaded again and a full long-range path query after
             * every incidental fight. Combat usually moves the player only a
             * few yards, so preserve the route and resume from a nearby
             * forward portal first.
             */
            pausedForCombat_ =
                true;

            lastCommandTick_ = 0;
            lastProgressTick_ = 0;
            lastStatusTick_ = 0;
            bestCornerDistance_ = 0.0f;
            motionWatchdogInitialized_ = false;
            lastMotionTick_ = 0;
            escapeProbeActive_ = false;
            escapeProbeSide_ = 0;
            surfaceRecoveryActive_ = false;
            surfaceRecoveryProgressTick_ = 0;
            verticalPortalRecoveryActive_ = false;
            verticalPortalRecoveryBypassActive_ = false;
            verticalPortalBypassRoutePoints_.clear();
            verticalPortalBypassRouteIndex_ = 0;
            verticalPortalBypassBestCornerDistance_ = 0.0f;
            verticalPortalRecoveryIssuedTick_ = 0;
            verticalPortalRecoveryProgressTick_ = 0;
            finalApproachActive_ = false;
            finalApproachSuppressed_ = false;
            finalApproachIssuedTick_ = 0;
            hardStallEpisodes_ = 0;
            hardStallBestFinalDistance_ = Distance2D(
                player.x, player.y, destination_.x, destination_.y);

            Debug::Logger::Info(
                "NAVMESH 11B.7: route cache preserved across combat; "
                "steeringPoints=" +
                std::to_string(points_.size()) +
                " currentPortal=" +
                std::to_string(
                    points_.empty()
                        ? 0
                        : pointIndex_ + 1
                )
            );

            SetState(
                GenericNavMeshFollowState::Idle
            );

            return true;
        }

        bool ResumeAfterCombat(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (pausedForCombat_ && RejectInvalidatedTopology()) return false;
            if (!pausedForCombat_)
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.7: cached resume requested without "
                    "a combat pause."
                );
                return false;
            }

            intentTick_=tick;
            intentPosition_={player.x,player.y,player.z};
            if(intentId_!=0)
                Debug::Logger::Info("MOVEMENT INTENT RESUME intent="+std::to_string(intentId_)+
                    " tick="+std::to_string(tick)+" reason=defensive_combat_released");

            if (initializationPending_)
            {
                // No Detour query exists yet. Resume the same tile episode
                // through Update rather than querying an incomplete mesh.
                pausedForCombat_ = false;
                SetState(GenericNavMeshFollowState::Planning);
                return true;
            }

            if (points_.size() >= 2)
            {
                const std::size_t begin =
                    pointIndex_ > CachedResumeLookBehind
                        ? pointIndex_ - CachedResumeLookBehind
                        : 1;

                const std::size_t end =
                    std::min(
                        points_.size() - 1,
                        pointIndex_ + CachedResumeLookAhead
                    );

                std::size_t bestIndex =
                    begin;

                float bestDistance =
                    1.0e30f;

                for (std::size_t index = begin; index <= end; ++index)
                {
                    const float distance =
                        Distance2D(
                            player.x,
                            player.y,
                            points_[index].x,
                            points_[index].y
                        );

                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestIndex = index;
                    }
                }

                if (bestDistance <= MaximumCachedResumeDistance)
                {
                    lastCommandTick_ =
                        tick;

                    lastProgressTick_ =
                        tick;

                    lastStatusTick_ =
                        tick;

                    bestCornerDistance_ =
                        bestDistance;

                    pausedForCombat_ =
                        false;

                    ResetMotionWatchdog(
                        player,
                        tick
                    );

                    SetState(
                        GenericNavMeshFollowState::Moving
                    );

                    Debug::Logger::Info(
                        "NAVMESH 11B.7: FAST RESUME from cached corridor "
                        "portal=" +
                        std::to_string(bestIndex + 1) +
                        "/" +
                        std::to_string(points_.size()) +
                        " distance=" +
                        Float(bestDistance)
                    );

                    if (!IssueCurrentCorner(
                            player,
                            tick,
                            "combat-resume cached path",
                            0,
                            bestIndex))
                    {
                        SetState(
                            GenericNavMeshFollowState::Failed
                        );

                        StopAtCurrentPosition(
                            player
                        );

                        return false;
                    }

                    return true;
                }

                Debug::Logger::Info(
                    "NAVMESH 11B.7: cached corridor is too far after combat "
                    "(nearest=" +
                    Float(bestDistance) +
                    "); running a live-position query without reloading tiles."
                );
            }
            else
            {
                Debug::Logger::Info(
                    "NAVMESH 11B.7: no cached steering corridor remains; "
                    "running a live-position query without reloading tiles."
                );
            }

            pausedForCombat_ =
                false;

            return
                PlanFrom(
                    player,
                    tick,
                    true
                );
        }

        bool OwnsMovement() const
        {
            return
                state_ ==
                    GenericNavMeshFollowState::
                        Planning ||
                state_ ==
                    GenericNavMeshFollowState::
                        Moving;
        }

        bool Arrived() const
        {
            return
                state_ ==
                    GenericNavMeshFollowState::
                        Arrived;
        }

        bool Failed() const
        {
            return
                state_ ==
                    GenericNavMeshFollowState::
                        Failed;
        }

        NavigationPlanFailure LastPlanFailure() const
        {
            return lastPlanFailure_;
        }

        NavigationFailureEvidence FailureEvidence() const
        {
            NavigationFailureEvidence evidence{};
            evidence.reason = lastPlanFailure_;
            evidence.tier = currentInitTier_;
            evidence.queryTier = lastQueryTier_;
            evidence.queryError = lastQueryError_;
            evidence.validation = lastValidationDetail_;
            evidence.destinationProjected = lastDestinationProjected_;
            evidence.failurePosition = lastObservedPlayerPosition_;
            evidence.failurePositionKnown = lastObservedPlayerPositionValid_;
            evidence.lastSafePosition = lastSafeNav_.position;
            evidence.lastSafePositionKnown = lastSafeNav_.valid;
            evidence.corridorFingerprint = lastPathFingerprint_;
            evidence.meshGeneration = provider_.CacheStats().generation;
            if (failedCorridor_.transitionKnown)
                evidence.failedTransition = {
                    failedCorridor_.failedFromPoly,
                    failedCorridor_.failedToPoly};
            evidence.learnedTransition = badVerticalTransitions_.LatestLearned();
            return evidence;
        }

        bool PlanningOnlyCorridorContainsTransition(
            DirectedPolyTransition edge) const
        {
            if (!startOptions_.planningOnly || !edge.Valid())
                return false;
            for (std::size_t i = 1; i < corridorPolys_.size(); ++i)
                if (corridorPolys_[i - 1] == edge.from &&
                    corridorPolys_[i] == edge.to)
                    return true;
            return false;
        }

        RouteCostProbeResult PlanningOnlyResult() const
        {
            if (!startOptions_.planningOnly)
                return RouteCostProbePolicy::Failed(
                    NavigationPlanFailure::OtherUnknown);
            if (state_ == GenericNavMeshFollowState::Planned)
                return RouteCostProbePolicy::Ready(plannedPathLength_);
            if (state_ == GenericNavMeshFollowState::Failed)
                return RouteCostProbePolicy::Failed(lastPlanFailure_);
            return {};
        }

        bool PlanningOnlyReachedDestination() const
        {
            return startOptions_.planningOnly &&
                state_ == GenericNavMeshFollowState::Planned &&
                planningOnlyReachedDestination_;
        }

        NavPoint PlanningOnlyProjectedDestination() const
        {
            return startOptions_.planningOnly &&
                state_ == GenericNavMeshFollowState::Planned
                ? plannedProjectedDestination_ : NavPoint{};
        }

        bool FullMapFallbackAttempted() const
        {
            return fullMapFallbackAttempted_;
        }

        bool InitializationPending() const
        {
            return initializationPending_;
        }

        bool InitializationProgressing() const
        {
            return initializationPending_ && !pausedForCombat_;
        }

        NavigationInitializationObservation InitializationObservation() const
        {
            const auto progress = provider_.InitializationProgress();
            return {InitializationProgressing(), intentId_, currentInitTier_,
                progress.processed, progress.total};
        }

        bool HasUsablePath() const
        {
            return state_ == GenericNavMeshFollowState::Moving ||
                state_ == GenericNavMeshFollowState::Arrived;
        }

        const char* StateName() const
        {
            return
                StateNameInternal(
                    state_
                );
        }

        int Commands() const
        {
            return
                commands_;
        }

        int Replans() const
        {
            return
                replans_;
        }

        float PlannedPathLength() const
        {
            return
                plannedPathLength_;
        }
    };
}
