#pragma once

#include "ClassAwareRewardController.h"
#include "AutoAttackController.h"
#include "MovementController.h"
#include "PlayerPostureController.h"
#include "AutonomySupervisor.h"
#include "ActiveBotAfkSafeguard.h"
#include "AfkDiagnosticTracker.h"
#include "SharedAfkController.h"
#include "WaterEvidenceObserve5875.h"
#include "LivingWaterEmergencyPolicy.h"
#include "DisconnectDiagnosticPolicy.h"
#include "ConnectionEvidence5875.h"
#include "ConnectionLifecycleObserver5875.h"
#include "RuntimeRobustnessSupervisor.h"
#include "CombatController.h"
#include "DeathRecoveryController.h"
#include "LivingDeathRecoveryController.h"
#include "BotDeathOwnershipPolicy.h"
#include "GrindModeController.h"
#include "GrindEnduranceDiagnosticPolicy.h"
#include "GrindTargetPolicy.h"
#include "EscapeDecisionPolicy.h"
#include "EscapeEvaluationEpisodePolicy.h"
#include "EscapeRouteDiagnostic.h"
#include "ExperienceTracker.h"
#include "QuestStateReader.h"
#include "QuestPlannerProbe.h"
#include "QuestPlannerRuntimeController.h"
#include "QuestFocusPolicy.h"
#include "TargetSelector.h"
#include "VileFamiliarsTurnInController.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Navigation/NavMeshPathFollower.h"
#include "../Control/RuntimeControl.h"

#include <windows.h>

#include <cerrno>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Bot
{
    class WorldMonitor
    {
    private:
        static constexpr DWORD PollIntervalMs =
            250;

        // Phase 14G.1 temporary test mode. While enabled, the quest planner
        // remains constructed but receives no runtime ownership; GrindMode
        // exclusively drives combat/loot/recovery/vendor behavior.
        inline static bool TemporaryGrindModeEnabled = true;

        static std::string Hex64(
            std::uint64_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(16)
                << std::setfill('0')
                << value;

            return stream.str();
        }

        static std::string Hex32(std::uint32_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase
                   << std::setw(8) << std::setfill('0') << value;
            return stream.str();
        }

        static std::string UtcTimestamp()
        {
            SYSTEMTIME utc{};
            GetSystemTime(&utc);
            std::ostringstream timestamp;
            timestamp << std::setfill('0') << std::setw(4) << utc.wYear
                << '-' << std::setw(2) << utc.wMonth
                << '-' << std::setw(2) << utc.wDay
                << 'T' << std::setw(2) << utc.wHour
                << ':' << std::setw(2) << utc.wMinute
                << ':' << std::setw(2) << utc.wSecond << 'Z';
            return timestamp.str();
        }

        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(4)
                << value;

            return stream.str();
        }

        static const char* EntryName(
            std::uint32_t entry)
        {
            switch (entry)
            {
                case 3098:
                    return "Mottled Boar";

                case 3124:
                    return "Scorpid Worker";

                case 3101:
                    return "Vile Familiar";

                case 3183:
                    return "Yarrog Baneshadow";

                default:
                    return "Unknown";
            }
        }

    public:
        static void Run()
        {
            const auto connectionMode = ParseConnectionMode(
                std::getenv("WOW_INTERNAL_CONNECTION_MODE"));
            if (connectionMode != ConnectionMode::Normal)
            {
                ConnectionLifecycleObserver5875::Run(connectionMode);
                return;
            }

            struct NavMeshSessionLifetime
            {
                ~NavMeshSessionLifetime()
                {
                    Navigation::DetourNavigationProvider::InvalidateSessionCache(
                        "session_teardown");
                }
            } navMeshSessionLifetime;
            static std::uint64_t generation = 0;
            const auto monitorGeneration = ++generation;
            Debug::Logger::Event("BOT SESSION START worldMonitorGeneration=" +
                std::to_string(monitorGeneration));
            // All ordinary exits unwind the local controllers. This trace
            // does not add a restart or change any ownership decision.
            struct SessionExitTrace
            {
                std::uint64_t generation;
                const char* reason = "monitor_return";
                ~SessionExitTrace()
                {
                    Debug::Logger::Event("BOT SESSION STOP worldMonitorGeneration=" +
                        std::to_string(generation) + " reason=" + reason);
                }
            } sessionExit{monitorGeneration};
            Debug::Logger::Info(
                "=== WorldMonitor started ==="
            );

            Control::RuntimeControlChannel runtimeControl;
            const bool startupControlAvailable = runtimeControl.OpenExisting();
            if (startupControlAvailable)
            {
                TemporaryGrindModeEnabled =
                    runtimeControl.RequestedMode(Control::BotMode::Grind) !=
                    Control::BotMode::Questing;
            }
            else
            {
                TemporaryGrindModeEnabled = true;
            }

            const Control::BotMode activeGuiMode =
                TemporaryGrindModeEnabled
                    ? Control::BotMode::Grind
                    : Control::BotMode::Questing;

            if (runtimeControl.IsOpen())
                runtimeControl.MarkRuntimeAttached(activeGuiMode);

            // P0.4.1 is a separate read-only diagnostic session. Branch before
            // combat/controller Start and before any workload can acquire input.
            const char* waterMode=std::getenv("WOW_INTERNAL_WATER_MODE");
            if (waterMode && *waterMode)
            {
                if (std::strcmp(waterMode,"observe")!=0 || !runtimeControl.IsOpen())
                {
                    Debug::Logger::Info("WATER OBSERVE CONFIG result=blocked reason=invalid_mode_or_control");
                    return;
                }
                WaterEvidenceObserve5875::Run(runtimeControl,activeGuiMode);
                return;
            }

            Debug::Logger::Info(
                std::string("GUI CONTROL 14H.2: startup mode=") +
                (TemporaryGrindModeEnabled ? "Grind" : "Questing") +
                (startupControlAvailable
                    ? " source=Honor-style GUI shared control."
                    : " source=default; GUI control channel not present at startup."));

            if (TemporaryGrindModeEnabled)
            {
                Debug::Logger::Info(
                    "Mode: Phase 14G.2 GENERIC WIDE-AREA GRIND + XP/VENDOR"
                );
                Debug::Logger::Info(
                    "Quest automation is paused; combat/loot/recovery remain active."
                );
            }
            else
            {
                Debug::Logger::Info(
                    "Mode: Phase 11B + Generic CollectItemFromMob Executor"
                );
            }

            Debug::Logger::Info(
                "Pipeline: target -> Charge opener/CTM chase -> "
                "face -> autoattack + Crossroads Warrior rotation -> "
                "death -> native loot -> recovery gate -> next target"
            );

            Debug::Logger::Info(
                "Polling every 250 ms."
            );
            Debug::Logger::Info(
                "WATER AVOIDANCE policy=avoid_until_qualified player=living"
                " waterAllowed=no ghostDeathRecovery=preserved");

            Debug::Logger::Info(
                "Looting is enabled in this phase."
            );

            Debug::Logger::Info(
                "Recovery gate 13C.0: no new target below "
                "70% HP or, for mana users, below 60% mana; "
                "resume at >=95% HP and >=90% mana."
            );

            Debug::Logger::Info(
                "Recovery method 13C.0: best usable bag food/drink "
                "with natural regeneration fallback."
            );

            Debug::Logger::Info(
                "Unreachable target blacklist: 120 seconds."
            );

            Debug::Logger::Info(
                "Quest Phase 7B: quest state controls "
                "which mob entry may be acquired."
            );

            Debug::Logger::Info(
                "Quest Phase 8B: when quest 789 becomes complete, "
                "return to Gornek, advance the turn-in dialog and "
                "select Battleworn Cape only when the exact reward "
                "name is present."
            );

            Debug::Logger::Info(
                "Quest Phase 9A: automatic Vile Familiars "
                "pickup remains enabled."
            );

            Debug::Logger::Info(
                "Quest Phase 9B: when quest 792 is active, "
                "route to its hunting grounds, acquire only "
                "entry 3101, patrol quest hotspots when no "
                "target is nearby, and stop new pulls when "
                "the quest complete flag becomes true."
            );

            Debug::Logger::Info(
                "Navigation 9B.1: patrol now tracks real "
                "progress, uses alternating fan detours on "
                "stuck detection, and skips a blocked "
                "hotspot instead of reissuing the same CTM "
                "line indefinitely."
            );

            Debug::Logger::Info(
                "Combat 9B.2: supported quest targets are "
                "validated directly by CombatController so "
                "the legacy TargetSelector allowlist cannot "
                "silently reject Vile Familiar entry 3101."
            );

            Debug::Logger::Info(
                "Crash fix 9B.3: FrameScript__Execute "
                "uses bool __fastcall(const char*, "
                "const char*) with a non-null script name "
                "for combat, quest and loot Lua calls."
            );

            Debug::Logger::Info(
                "Combat fix 9B.4: autoattack is no longer "
                "blocked by facing convergence after Charge; "
                "Rend waits for facing, Battle Shout can run "
                "independently, and Heroic Strike still "
                "requires active autoattack."
            );

            Debug::Logger::Info(
                "Combat fix 9B.5: TargetSelector shared "
                "allowlist now includes Vile Familiar "
                "entry 3101, so ChaseController can accept "
                "the same quest target as CombatController."
            );

            Debug::Logger::Info(
                "Navigation Phase 10B.2: the verified Detour "
                "path to Zureetha is followed one straight-"
                "path corner at a time with CTM, progress "
                "detection and bounded replanning."
            );

            Debug::Logger::Info(
                "Phase 10C ownership rule: NavMesh return "
                "owns movement until arrival; then the "
                "Vile Familiars turn-in controller owns "
                "interaction and dialog. Combat remains "
                "paused for this end-to-end test."
            );

            Debug::Logger::Info(
                "Phase 10D reward policy: Warrior reward "
                "choices are scored from live item tooltips "
                "against currently equipped gear. The best "
                "usable reward is claimed and auto-equipped "
                "only when it is a verified upgrade."
            );

            Debug::Logger::Info(
                "Phase 10D verification rule: quest-log "
                "removal alone is not PASS; reward delivery "
                "must also be verified first."
            );

            Debug::Logger::Info(
                "Phase 11A: generic read-only quest planner "
                "maps the live Vanilla quest log to a "
                "data-driven Valley of Trials profile catalogue."
            );

            Debug::Logger::Info(
                "Phase 11A planner remains enabled for observation."
            );

            Debug::Logger::Info(
                "Phase 11B executes the first generic objective "
                "handler: CollectItemFromMob for quest 794 "
                "Burning Blade Medallion."
            );

            Debug::Logger::Info(
                "Phase 11B flow: generic NavMesh route -> live "
                "Yarrog target handoff -> existing combat/loot -> "
                "quest completion flag -> ReadyForTurnIn."
            );

            Debug::Logger::Info(
                "Generic quest turn-in is intentionally deferred "
                "to Phase 11C."
            );

            Debug::Logger::Info(
                "CROSSROADS WARRIOR 14M.0: Charge/Rend remain authoritative; Overpower uses Vanilla usability probes, Bloodrage is low-rage/high-HP guarded, and Hamstring controls low-health runners."
            );

            Debug::Logger::Info(
                "CROSSROADS WARRIOR 14M.0: Thunder Clap requires >=2 direct aggressors within 8 yd; Heroic Strike preserves a 35-rage reserve threshold."
            );

            CombatController combat;

            if (!combat.Start(0))
            {
                sessionExit.reason = "combat_start_failed";
                Debug::Logger::Info(
                    "WorldMonitor: "
                    "CombatController failed to start."
                );

                return;
            }

            GrindModeController grindMode;
            grindMode.SetVendorAutomationEnabled(
                runtimeControl.VendorAutomationEnabled(false));
            ExperienceTracker experienceTracker;
            AutonomySupervisor autonomySupervisor;
            Navigation::NavigationInitializationObservation lastAutonomyInitialization{};
            EscapeDecisionPolicy escapeDiagnostic;
            EscapeEvaluationEpisodePolicy escapeEvaluationEpisode;
            EscapeRouteDiagnostic escapeRouteDiagnostic;
            ActiveBotAfkSafeguard antiAfkSafeguard;
            SharedAfkController sharedAfk;
            std::string lastAfkSafetyDecision;
            RuntimeRobustnessSupervisor runtimeRobustness;
            DeathRecoveryController deathRecovery;
            LivingDeathRecoveryController livingDeathRecovery;

            QuestPlannerProbe questPlannerProbe;

            QuestPlannerRuntimeController
                questPlannerRuntime;
            int questFocusId = 0;

            if (!TemporaryGrindModeEnabled)
            {
                // Optional explicit one-quest diagnostic scope. Invalid
                // input fails closed instead of enabling the whole catalogue.
                const char* requested =
                    std::getenv("WOW_INTERNAL_QUEST_FOCUS_ID");
                const auto focusConfiguration =
                    QuestFocusPolicy::ParseConfiguration(requested);
                const bool configured = focusConfiguration.configured;
                questFocusId = focusConfiguration.questId;
                questPlannerRuntime.ConfigureFocusQuest(questFocusId);
                Debug::Logger::Info(
                    std::string("QUEST 16A FOCUS configuration=") +
                    QuestFocusPolicy::ConfigurationName(focusConfiguration.kind));
                Debug::Logger::Info(
                    std::string("QUEST 16B FOCUS configured=") +
                    (configured ? "yes" : "no") +
                    " rawValue=" +
                    (configured ? std::string(requested).substr(0, 32)
                                : requested != nullptr ? "<empty>" : "<unset>") +
                    " parsedQuestId=" + std::to_string(questFocusId) +
                    " result=" +
                    QuestFocusPolicy::ConfigurationName(focusConfiguration.kind));
                ClassAwareRewardController::
                    RunReadOnlyProbe();

                questPlannerProbe.Initialize();
                questPlannerRuntime.Initialize();
            }
            else
            {
                Debug::Logger::Info(
                    "QUEST 16B FOCUS configured=unknown rawValue=<not_read> "
                    "parsedQuestId=0 result=quest_mode_inactive");
                Debug::Logger::Info(
                    "GRIND 14G.1: skipped quest planner/reward probe initialization for temporary grind test."
                );
            }

            std::uint64_t tick =
                0;

            /*
             * Phase 7B applies the verified quest state
             * directly to combat target acquisition.
             */
            QuestStateReader::Snapshot questState{};

            bool haveQuestState =
                false;

            int consecutiveQuestProbeFailures =
                0;

            Navigation::NavMeshPathFollower
                navMeshReturn;

            bool navMeshReturnStartAttempted =
                false;

            VileFamiliarsTurnInController
                vileFamiliarsTurnIn;

            bool vileFamiliarsTurnInStartAttempted =
                false;

            int consecutiveWorldFailures =
                0;

            DisconnectDiagnosticPolicy disconnectDiagnostic;
            AfkDiagnosticTracker afkDiagnostic;
            LivingWaterEmergencyPolicy waterEmergency;
            std::uint32_t lastAfkRawFlags = 0;
            std::uint64_t loopHeartbeat = 0;
            bool haveMovementAnchor = false;
            float movementAnchorX = 0.0f;
            float movementAnchorY = 0.0f;
            float movementAnchorZ = 0.0f;
            std::uint64_t lastMeaningfulMovementMs = 0;
            std::uint64_t enduranceCombatStarts = 0;
            std::uint64_t enduranceDeaths = 0;
            std::uint64_t enduranceManualVendorPauses = 0;
            std::uint64_t enduranceUnexpectedIdleEvents = 0;
            GrindEnduranceEdgeTracker enduranceEdges;
            bool enduranceGrindFailed = false;
            bool enduranceCombatFailed = false;
            bool enduranceDeathFailed = false;
            int observedNavigationFailures = 0;
            int observedLootFailures = 0;

            std::uint64_t previousUiTargetGuid =
                0;

            std::uint32_t previousPlayerHealth =
                0xFFFFFFFF;

            std::uint64_t deathRecoveryEpisode = 0;
            bool deathEntryPendingLogged = false;
            bool hp1ReconciliationActive = false;
            bool hp1EpisodeLogged = false;
            bool hp1ReconciliationStallLogged = false;
            bool hp1AliveEmptyWorldLogged = false;
            bool terminalDeathOwnerLogged = false;
            std::uint64_t hp1ReconciliationStartTick = 0;

            std::uint32_t previousPowerRaw =
                0xFFFFFFFF;

            std::unordered_map<
                std::uint64_t,
                std::uint32_t
            > previousMobHealth;

            while (true)
            {
                // Phase 14H.2: Stop Bot is a real runtime detach. The DLL
                // cooperatively stops owned actions, marks the shared channel
                // detached, returns from WorldMonitor and is then unloaded by
                // BootstrapThread via FreeLibraryAndExitThread. WoW stays open.
                if (!runtimeControl.IsOpen() && runtimeControl.OpenExisting())
                {
                    runtimeControl.MarkRuntimeAttached(activeGuiMode);
                    grindMode.SetVendorAutomationEnabled(
                        runtimeControl.VendorAutomationEnabled(false));
                    Debug::Logger::Info(
                        "GUI CONTROL 14H.2: external control channel attached after runtime startup.");
                }

                const bool guiUnloadRequested =
                    runtimeControl.IsOpen() &&
                    (runtimeControl.UnloadRequested(false) ||
                     !runtimeControl.RunRequested(true));

                if (guiUnloadRequested)
                {
                    sharedAfk.Reset();
                    sessionExit.reason = "gui_stop_or_unload_requested";
                    runtimeControl.MarkRuntimeState(
                        Control::BotRunState::Unloading,
                        static_cast<LONG>(tick & 0x7FFFFFFFULL));

                    Debug::Logger::Info(
                        "GUI CONTROL 14H.2: STOP BOT requested; stopping actions and unloading wow_internal.dll. WoW remains open.");

                    AutoAttackController::Stop();

                    Objects::WorldState stopWorld{};
                    if (Objects::WorldStateReader::Read(stopWorld))
                        MovementController::HoldPosition(stopWorld.player);

                    runtimeControl.MarkRuntimeDetached();
                    Debug::Logger::Info(
                        "GUI CONTROL 14H.2: RUNTIME DETACHED; bootstrap thread will unload wow_internal.dll now.");
                    return;
                }

                // Unlike the controller tick, this heartbeat advances even
                // while the world snapshot is unavailable. An external GUI
                // can therefore distinguish a live monitor from a stalled one.
                ++loopHeartbeat;
                if (runtimeControl.IsOpen())
                {
                    runtimeControl.MarkRuntimeAttached(activeGuiMode);
                    runtimeControl.MarkRuntimeState(
                        Control::BotRunState::Running,
                        static_cast<LONG>(loopHeartbeat & 0x7FFFFFFFULL));
                }

                Objects::WorldState world{};

                Objects::WorldReadStage worldReadStage =
                    Objects::WorldReadStage::Complete;
                const bool snapshotValid = Objects::WorldStateReader::Read(
                    world, &worldReadStage);
                const std::uint64_t nowMs = GetTickCount64();
                const std::uint64_t priorIncidentAgeMs =
                    disconnectDiagnostic.IncidentAgeMs(nowMs);
                const DisconnectDiagnosticEvent diagnosticEvent =
                    disconnectDiagnostic.Update(snapshotValid, nowMs);

                if (snapshotValid)
                {
                    if (!haveMovementAnchor)
                    {
                        haveMovementAnchor = true;
                        movementAnchorX = world.player.x;
                        movementAnchorY = world.player.y;
                        movementAnchorZ = world.player.z;
                        lastMeaningfulMovementMs = nowMs;
                    }
                    else
                    {
                        const float dx = world.player.x - movementAnchorX;
                        const float dy = world.player.y - movementAnchorY;
                        const float dz = world.player.z - movementAnchorZ;
                        if (std::sqrt(dx * dx + dy * dy + dz * dz) >= 2.5f)
                        {
                            movementAnchorX = world.player.x;
                            movementAnchorY = world.player.y;
                            movementAnchorZ = world.player.z;
                            lastMeaningfulMovementMs = nowMs;
                        }
                    }
                }

                const bool afkSampleKnown =
                    snapshotValid && world.player.afkCandidateKnown;
                const AfkDiagnosticTransition afkTransition =
                    afkDiagnostic.Observe(
                        afkSampleKnown,
                        world.player.afkCandidate,
                        nowMs);
                if (afkSampleKnown)
                    lastAfkRawFlags = world.player.playerFlagsRaw;
                const std::string afkTransitionUtc =
                    afkTransition.event == AfkDiagnosticEvent::None
                        ? std::string{} : UtcTimestamp();

                if (diagnosticEvent != DisconnectDiagnosticEvent::None)
                {
                    // Source-verified read-only predicates; no disconnect/UI
                    // action is inferred from ObjectManager or historical screen.
                    const auto connection = ConnectionEvidence5875::Observe(
                        Wow5875::Client::Base(), [](std::uintptr_t address, auto& value)
                        {
                            SIZE_T copied = 0;
                            return ReadProcessMemory(GetCurrentProcess(),
                                reinterpret_cast<const void*>(address), &value,
                                sizeof(value), &copied) && copied == sizeof(value);
                        });
                    Debug::Logger::Info(std::string("CONNECTION EVIDENCE world=") +
                        (snapshotValid ? "valid" : "unavailable") +
                        " sourceVerified=" + (connection.signaturesKnown ? "yes" : "no") +
                        " serverConnection=" + (connection.serverConnectionKnown
                            ? (connection.serverConnected ? "yes" : "no") : "unknown") +
                        " lastGlueScreen=" + connection.lastGlueScreen +
                        " glueVisibility=unknown dialogState=unknown processAlive=yes" +
                        " decision=observe_only reason=" + connection.reason);
                    const char* classification = snapshotValid
                        ? (diagnosticEvent == DisconnectDiagnosticEvent::SnapshotRecovered
                            ? "snapshot_recovered_cause_unknown" : "healthy")
                        : "world_snapshot_unavailable_cause_unknown";
                    Debug::Logger::Info(
                        "DISCONNECT DIAGNOSTIC 14D: utc=" + UtcTimestamp() +
                        " processAlive=yes runtimeLoopHeartbeat=" +
                        std::to_string(loopHeartbeat) +
                        " snapshot=" + (snapshotValid ? std::string("valid") : std::string("unavailable")) +
                        " snapshotStage=" + Objects::WorldReadStageName(worldReadStage) +
                        " snapshotAgeMs=" +
                        (disconnectDiagnostic.HaveSuccessfulSnapshot()
                            ? std::to_string(disconnectDiagnostic.SnapshotAgeMs(nowMs))
                            : std::string("unknown")) +
                        " incidentAgeMs=" + std::to_string(
                            snapshotValid ? priorIncidentAgeMs : disconnectDiagnostic.IncidentAgeMs(nowMs)) +
                        " playerValid=" + (world.player.valid ? std::string("yes") : std::string("no")) +
                        " playerGuid=" + Hex64(world.activePlayerGuid) +
                        " objectManager=" + std::to_string(world.manager) +
                        " objectListPresent=" + (world.firstObject != 0 ? std::string("yes") : std::string("no")) +
                        " localPlayerPresent=" + (world.localPlayer != 0 ? std::string("yes") : std::string("no")) +
                        " worldLoaded=" + (snapshotValid ? std::string("yes") : std::string("unknown")) +
                        " movementAgeMs=" +
                        (haveMovementAnchor ? std::to_string(nowMs - lastMeaningfulMovementMs) : std::string("unknown")) +
                        " meaningfulActionAgeMs=unknown" +
                        " antiAfkSafeIdleAgeTicks=" + std::to_string(antiAfkSafeguard.SafeIdleAgeTicks(tick)) +
                        " antiAfkRequests=" + std::to_string(antiAfkSafeguard.Requests()) +
                        " antiAfkActions=" + std::to_string(grindMode.AntiAfkActions()) +
                        // SignalKnown means a prior successful read. Stale
                        // explicitly distinguishes that from a current read.
                        " afkSignalKnown=" + (afkDiagnostic.EverKnown() ? std::string("yes") : std::string("no")) +
                        " afkCandidate=" + (afkDiagnostic.EverKnown()
                            ? (afkDiagnostic.CandidateAfk() ? std::string("yes") : std::string("no"))
                            : std::string("unknown")) +
                        " afkSignalVerified=no" +
                        " afkObservationStale=" + (afkDiagnostic.Stale() ? std::string("yes") : std::string("no")) +
                        " afkRawFlags=" + (afkDiagnostic.EverKnown()
                            ? Hex32(lastAfkRawFlags) : std::string("unknown")) +
                        " afkObservedDurationMs=" + (afkDiagnostic.EverKnown() && afkDiagnostic.CandidateAfk()
                            ? std::to_string(afkDiagnostic.ObservedDurationMs()) : std::string("0")) +
                        " afkLastObservedAgeMs=" + (afkDiagnostic.EverKnown()
                            ? std::to_string(afkDiagnostic.LastObservedAgeMs(nowMs)) : std::string("unknown")) +
                        " gameThreadResponsive=unknown clientState=unknown classification=" + classification);
                }

                if (!snapshotValid)
                {
                    waterEmergency.InvalidateWorld();
                    grindMode.InvalidateMaintenanceEvidenceOnWorldGap();
                    if (consecutiveWorldFailures == 0)
                        Navigation::DetourNavigationProvider::InvalidateSessionCache(
                            "world_unload_or_snapshot_gap");
                    // Two terminal manual-alive probes must belong to a
                    // continuous run of valid snapshots for the same player.
                    deathRecovery.InvalidateTerminalAliveEvidenceOnWorldGap();
                    livingDeathRecovery.InvalidateWorld();
                    escapeDiagnostic.Reset();
                    const EscapeEvaluationEpisodeInput invalidEscapeWorld{};
                    const auto escapeEvent =
                        escapeEvaluationEpisode.Observe(invalidEscapeWorld);
                    if (escapeEvent.kind ==
                        EscapeEvaluationEpisodeEventKind::Cancel)
                    {
                        escapeRouteDiagnostic.Reset();
                        Debug::Logger::Info(
                            "ESCAPE 15C.1 EPISODE CANCEL reason=" +
                            std::string(EscapeEvaluationEpisodePolicy::ReasonName(
                                escapeEvent.reason)));
                    }
                    ++consecutiveWorldFailures;
                    sharedAfk.Reset();

                    if (
                        consecutiveWorldFailures == 1 ||
                        consecutiveWorldFailures % 20 == 0)
                    {
                        Debug::Logger::Info(
                            "WorldMonitor: "
                            "world state unavailable."
                        );
                    }

                    if (consecutiveWorldFailures == 40)
                    {
                        Debug::Logger::Info(
                            "ROBUSTNESS 14K.0: WORLD SNAPSHOT DEGRADED for ~10 seconds; controller updates remain suspended until a valid snapshot returns.");
                    }

                    Sleep(
                        PollIntervalMs
                    );

                    continue;
                }

                if (
                    consecutiveWorldFailures > 0)
                {
                    Debug::Logger::Info(
                        "WorldMonitor: "
                        "world state available again."
                    );

                    Debug::Logger::Info(
                        "ROBUSTNESS 14K.0: world snapshot recovered; high-level liveness timer rebaselined to avoid false recovery after a loading/read gap.");
                    runtimeRobustness.Reset();

                    consecutiveWorldFailures =
                        0;
                }

                // Phase 14G.2: XP telemetry is sampled from the same live
                // Player update fields as the world snapshot. The tracker uses
                // a rolling five-minute window and survives one-level rollover.
                experienceTracker.Update(world.player);

                // =====================================
                // Player HP
                // =====================================

                if (
                    previousPlayerHealth !=
                    world.player.health)
                {
                    if (
                        previousPlayerHealth !=
                        0xFFFFFFFF)
                    {
                        Debug::Logger::Info(
                            "PlayerHealth changed: " +
                            std::to_string(
                                previousPlayerHealth
                            ) +
                            " -> " +
                            std::to_string(
                                world.player.health
                            ) +
                            "/" +
                            std::to_string(
                                world.player.maxHealth
                            )
                        );
                    }

                    previousPlayerHealth =
                        world.player.health;
                }

                // =====================================
                // Rage / power
                // =====================================

                if (
                    previousPowerRaw !=
                    world.player.powerRaw)
                {
                    if (
                        previousPowerRaw !=
                        0xFFFFFFFF)
                    {
                        Debug::Logger::Info(
                            "PlayerPower changed: " +
                            std::to_string(
                                previousPowerRaw
                            ) +
                            " -> " +
                            std::to_string(
                                world.player.powerRaw
                            ) +
                            " displayed=" +
                            Float(
                                world.player.power
                            )
                        );
                    }

                    previousPowerRaw =
                        world.player.powerRaw;
                }

                // =====================================
                // UI target
                // =====================================

                if (
                    previousUiTargetGuid !=
                    world.player.targetGuid)
                {
                    Debug::Logger::Info(
                        "Target changed: " +
                        Hex64(
                            previousUiTargetGuid
                        ) +
                        " -> " +
                        Hex64(
                            world.player.targetGuid
                        )
                    );

                    previousUiTargetGuid =
                        world.player.targetGuid;

                    if (
                        combat.LockedGuid() != 0 &&
                        world.player.targetGuid !=
                            combat.LockedGuid())
                    {
                        Debug::Logger::Info(
                            "Combat lock remains: " +
                            Hex64(
                                combat.LockedGuid()
                            )
                        );
                    }
                }

                // =====================================
                // Quest mob HP changes
                // =====================================

                for (
                    const auto& unit :
                    world.units)
                {
                    if (!TargetSelector::
                            IsAllowedEntry(
                                unit.entryId))
                    {
                        continue;
                    }

                    const auto existing =
                        previousMobHealth.find(
                            unit.guid
                        );

                    if (
                        existing ==
                        previousMobHealth.end())
                    {
                        previousMobHealth[
                            unit.guid
                        ] =
                            unit.health;

                        continue;
                    }

                    if (
                        existing->second !=
                        unit.health)
                    {
                        const std::uint32_t oldHealth =
                            existing->second;

                        existing->second =
                            unit.health;

                        Debug::Logger::Info(
                            std::string(
                                "Quest mob HP changed: "
                            ) +
                            EntryName(
                                unit.entryId
                            ) +
                            " guid=" +
                            Hex64(
                                unit.guid
                            ) +
                            " " +
                            std::to_string(
                                oldHealth
                            ) +
                            " -> " +
                            std::to_string(
                                unit.health
                            ) +
                            "/" +
                            std::to_string(
                                unit.maxHealth
                            )
                        );
                    }
                }

                // A dead/ghost player must not give a quest executor one
                // update in which to turn death into an objective failure.
                // The existing recovery controller performs the authoritative
                // Lua confirmation; this preflight only gates mode ownership.
                deathRecovery.ObserveClearlyAlivePosition(world);
                const bool deathBootstrap =
                    world.player.health == 1 &&
                    deathRecovery.ConfirmDeathWhileIdle(world, tick);
                const bool deathShouldOwn = BotDeathOwnershipPolicy::ShouldOwn(
                    deathRecovery.IsActive(), deathRecovery.IsFailed(),
                    world.player.valid, world.player.health,
                    world.player.maxHealth, deathBootstrap);
                if (deathShouldOwn && livingDeathRecovery.Owns())
                {
                    // Rearm already retained the repeat-death record; do not reset it here.
                    livingDeathRecovery.YieldWater(world.player);
                    livingDeathRecovery.Reset();
                    Debug::Logger::Info("DEATH LIVING state=redeath_handoff normalModeBlocked=yes");
                }
                livingDeathRecovery.ObserveDeadline(nowMs);
                const auto livingEvidence = livingDeathRecovery.Owns()
                    ? CombatClientEvidence5875::Living(world)
                    : CombatClientEvidence5875::LivingEvidence{};
                const bool freshIdleAliveProbe =
                    deathRecovery.FreshIdleAliveConfirmed(tick);
                const bool normalModeHeldForReconciliation =
                    BotDeathOwnershipPolicy::HoldNormalMode(
                        deathShouldOwn, world.player.valid,
                        world.player.health, world.player.maxHealth,
                        freshIdleAliveProbe, !world.units.empty());
                const bool ambiguousHp1Hold =
                    normalModeHeldForReconciliation && !deathShouldOwn;
                if (ambiguousHp1Hold && !deathEntryPendingLogged)
                {
                    deathEntryPendingLogged = true;
                    Debug::Logger::Info(
                        "DEATH RECOVERY ENTRY GATE reason=hp1_probe_pending"
                        " state=" + std::string(deathRecovery.StateName()) +
                        " valid=" + (world.player.valid ? "yes" : "no") +
                        " hp=" + std::to_string(world.player.health) +
                        "/" + std::to_string(world.player.maxHealth));
                }
                else if (!ambiguousHp1Hold)
                    deathEntryPendingLogged = false;

                if (ambiguousHp1Hold)
                {
                    if (BotDeathOwnershipPolicy::BeginReconciliationHold(
                            hp1ReconciliationActive, ambiguousHp1Hold))
                    {
                        hp1ReconciliationActive = true;
                        MovementController::HoldPosition(world.player);
                    }
                    if (!hp1EpisodeLogged)
                    {
                        hp1EpisodeLogged = true;
                        hp1ReconciliationStartTick = tick;
                        Debug::Logger::Info(
                            "WORLD RECONCILIATION reason=hp1_ambiguous"
                            " hp=" + std::to_string(world.player.health) +
                            "/" + std::to_string(world.player.maxHealth) +
                            " units=" + std::to_string(world.units.size()) +
                            " objectsSeen=" + std::to_string(world.objectsSeen) +
                            " unitObjectsSeen=" + std::to_string(world.unitObjectsSeen) +
                            " unitReadFailures=" + std::to_string(world.unitReadFailures) +
                            " enumerationInterrupted=" +
                            (world.objectEnumerationInterrupted ? "yes" : "no") +
                            " probeState=" + deathRecovery.IdleProbeStateName(tick) +
                            " deathOwner=no mode=" +
                            (TemporaryGrindModeEnabled ? "Grind" : "Questing"));
                    }
                    if (!hp1ReconciliationStallLogged &&
                        tick >= hp1ReconciliationStartTick +
                            DeathRecoveryPolicy::StatusLogTicks * 4)
                    {
                        hp1ReconciliationStallLogged = true;
                        Debug::Logger::Info(
                            "WORLD RECONCILIATION STALL reason=hp1_ambiguous"
                            " ageTicks=" +
                            std::to_string(tick - hp1ReconciliationStartTick) +
                            " probeState=" + deathRecovery.IdleProbeStateName(tick) +
                            " units=" + std::to_string(world.units.size()) +
                            " objectsSeen=" + std::to_string(world.objectsSeen) +
                            " unitObjectsSeen=" + std::to_string(world.unitObjectsSeen) +
                            " unitReadFailures=" + std::to_string(world.unitReadFailures) +
                            " enumerationInterrupted=" +
                            (world.objectEnumerationInterrupted ? "yes" : "no"));
                    }
                    if (freshIdleAliveProbe && world.units.empty() &&
                        !hp1AliveEmptyWorldLogged)
                    {
                        hp1AliveEmptyWorldLogged = true;
                        Debug::Logger::Info(
                            "WORLD RECONCILIATION reason=alive_probe_world_empty"
                            " probeState=alive deathOwner=no units=0"
                            " objectsSeen=" + std::to_string(world.objectsSeen) +
                            " unitObjectsSeen=" + std::to_string(world.unitObjectsSeen) +
                            " unitReadFailures=" + std::to_string(world.unitReadFailures));
                    }
                }
                else
                {
                    hp1ReconciliationActive = false;
                    if (world.player.health != 1)
                    {
                        hp1EpisodeLogged = false;
                        hp1ReconciliationStallLogged = false;
                        hp1AliveEmptyWorldLogged = false;
                    }
                }
                if (deathRecovery.IsFailed() && !terminalDeathOwnerLogged)
                {
                    terminalDeathOwnerLogged = true;
                    Debug::Logger::Info(
                        std::string("WORLD RECONCILIATION reason=terminal_death_recovery_failure"
                        " deathOwner=yes mode=") +
                        (TemporaryGrindModeEnabled ? "Grind" : "Questing") +
                        " grindState=" + grindMode.StateName() +
                        " hp=" + std::to_string(world.player.health) +
                        "/" + std::to_string(world.player.maxHealth) +
                        " units=" + std::to_string(world.units.size()));
                }
                else if (!deathRecovery.IsFailed())
                    terminalDeathOwnerLogged = false;

                // P0.4-TEMP: the verified SWIMMING bit is sufficient to stop
                // living autonomous navigation, but not to infer a breathable
                // surface or dry ground. DeathRecovery retains Ghost ownership.
                const auto waterLife = AfkClient5875::ReadLife(world.player);
                const bool deathWaterOwner =
                    deathShouldOwn || AfkDeadGhostPolicy::DeadOrGhost(waterLife) ||
                    (waterLife == AfkLifeState::Unknown &&
                     (deathRecovery.IsActive() || deathRecovery.IsFailed()));
                const auto waterEvidence = WaterEvidence5875::Read(world);
                const bool swimming = waterEvidence.movementKnown &&
                    (waterEvidence.movementFlags &
                     WaterEvidenceTracker5875::SwimmingMask) != 0;
                // The existing water block has priority over ordinary ground
                // movement. Egress may take its stopped owner only with a
                // complete threat observation and no live combat/safety owner.
                const auto waterOwnerSafe = [&](const Objects::WorldState& observed)
                {
                    bool threat = observed.objectEnumerationInterrupted ||
                        observed.unitReadFailures != 0 || observed.units.empty();
                    bool lockDead = combat.LockedGuid() == 0;
                    for (const auto& unit : observed.units)
                    {
                        if (unit.valid && unit.guid == combat.LockedGuid())
                            lockDead = unit.health == 0;
                        if (unit.valid && unit.health > 0 && unit.targetGuid == observed.activePlayerGuid)
                            threat = true;
                    }
                    return TemporaryGrindModeEnabled && !deathShouldOwn &&
                        !normalModeHeldForReconciliation && !threat && lockDead &&
                        livingDeathRecovery.AllowsWaterHandoff(observed) &&
                        !combat.Recovery().IsActive() && !grindMode.FirstAidActive() &&
                        !grindMode.Failed() && !vileFamiliarsTurnIn.IsActive() &&
                        !questPlannerRuntime.OwnsControl() &&
                        grindMode.State() != GrindModeState::Vendoring &&
                        (combat.State() == CombatState::Idle ||
                         combat.State() == CombatState::AcquiringTarget ||
                         combat.State() == CombatState::PostKillDelay ||
                         combat.State() == CombatState::Looting);
                };
                const bool waterHandoffSafe = waterLife == AfkLifeState::Alive && waterOwnerSafe(world);
                const WaterEgressSample waterSample{
                    world.valid && world.player.valid, waterLife == AfkLifeState::Alive,
                    deathWaterOwner, waterEvidence.movementKnown, swimming, waterHandoffSafe,
                    world.activePlayerGuid, world.manager, world.localPlayer, GetTickCount64(),
                    world.player.x, world.player.y, world.player.z};
                const auto waterDecision = waterEmergency.Update(waterSample);
                if (waterDecision.entered)
                {
                    livingDeathRecovery.YieldWater(world.player);
                    Debug::Logger::Info("WATER EMERGENCY state=entered trigger=swimming previousOwner=" +
                        std::string(combat.StateName()) + " previousGrindOwner=" + grindMode.StateName() +
                        " previousWriter=" + ClickToMoveController::LastCommand().writer +
                        " lastSafeKnown=" + (waterEmergency.AnchorKnown() ? "yes" : "no") +
                        " owner=LivingWaterEmergency");
                    Debug::Logger::Info(
                        "WATER BLOCK state=entered swimmingKnown=yes swimming=yes"
                        " owner=living_workload decision=neutralize_navigation");
                    if (!MovementController::HoldPosition(world.player))
                    {
                        Debug::Logger::Info(
                            "WATER BLOCK state=fault reason=ctm_neutralization_failed"
                            " decision=stop_session");
                        return;
                    }
                    grindMode.ReleaseNavigationForLivingWater();
                    navMeshReturn.CancelForLivingWater();
                    questPlannerRuntime.ReleaseNavigationForLivingWater();
                    combat.PauseMovementForLivingWater();
                    if (waterHandoffSafe) combat.InvalidateIntentForLivingWater(tick);
                    AutonomySample inactiveWaterSample{};
                    autonomySupervisor.Update(inactiveWaterSample, tick);
                    runtimeRobustness.PauseForLivingWater();
                    Debug::Logger::Info("WATER EMERGENCY navigationIntent=invalidated"
                        " exit=three_non_swimming_observations");
                }
                if (waterDecision.action == WaterEgressAction::Stop && !waterDecision.entered)
                {
                    if (!MovementController::HoldPosition(world.player))
                    {
                        Debug::Logger::Info("WATER EMERGENCY state=fault reason=ctm_neutralization_failed decision=stop_session");
                        return;
                    }
                }
                if (waterDecision.recovered)
                    Debug::Logger::Info("WATER EMERGENCY state=non_swimming_candidate proof=3/3"
                        " swimmingKnown=yes swimming=no");
                if (waterDecision.action == WaterEgressAction::Backtrack)
                {
                    const auto& destination = waterEmergency.Destination();
                    Debug::Logger::Info("WATER EMERGENCY state=attempt attempt=" +
                        std::to_string(waterEmergency.Attempts()) + " maxAttempts=" +
                        std::to_string(LivingWaterEmergencyPolicy::MaximumAttempts) +
                        " destinationSource=recent_same_world_non_swimming_observation destination=(" +
                        std::to_string(destination.x) + "," + std::to_string(destination.y) + "," +
                        std::to_string(destination.z) + ") reason=" + waterDecision.reason);
                    // Fresh identity, life, threat and movement observations
                    // before dispatch; the earlier tick snapshot is not a
                    // command authorization across an intervening world gap.
                    Objects::WorldState commandWorld;
                    const bool commandValid = Objects::WorldStateReader::Read(commandWorld);
                    const auto commandWater = WaterEvidence5875::Read(commandWorld);
                    const WaterEgressSample commandSample{
                        commandValid, AfkClient5875::ReadLife(commandWorld.player) == AfkLifeState::Alive,
                        deathWaterOwner, commandWater.movementKnown,
                        (commandWater.movementFlags & WaterEvidenceTracker5875::SwimmingMask) != 0,
                        waterOwnerSafe(commandWorld), commandWorld.activePlayerGuid, commandWorld.manager,
                        commandWorld.localPlayer, GetTickCount64(),
                        commandWorld.player.x, commandWorld.player.y, commandWorld.player.z};
                    if (!waterEmergency.CanDispatch(commandSample) ||
                        !ClickToMoveController::MoveTo(commandWorld.player,
                            destination.x, destination.y, destination.z, 0.1f))
                        waterEmergency.DispatchFailed();
                }
                else if (waterDecision.event)
                    Debug::Logger::Info("WATER EMERGENCY state=" + std::string(waterDecision.event) +
                        " reason=" + waterDecision.reason +
                        " proof=" + std::to_string(waterDecision.proof) + "/3" +
                        (std::string(waterDecision.event) == "failed" ? " decision=manual_recovery" : ""));
                if (waterDecision.recovered)
                {
                    if (waterHandoffSafe) combat.InvalidateIntentForLivingWater(tick);
                    runtimeRobustness.PauseForLivingWater();
                    Debug::Logger::Info(
                        "WATER BLOCK state=exited reason=non_swimming_confirmed"
                        " groundContact=unknown");
                    // No workload uses the confirmation snapshot. Re-read the
                    // world before fresh acquisition/navigation on the next tick.
                    ++tick;
                    Sleep(PollIntervalMs);
                    continue;
                }
                else if (waterDecision.deathHandoff)
                    Debug::Logger::Info(
                        "WATER BLOCK state=exited reason=death_recovery_ownership");
                if (waterEmergency.Blocked())
                {
                    livingDeathRecovery.YieldWater(world.player);
                    // No ordinary owner or watchdog runs during bounded egress
                    // or terminal manual recovery. AFK observes, but a
                    // water-unsafe candidate is not dispatched speculatively.
                    AfkSafety waterAfkSafety{};
                    waterAfkSafety.recovery = true;
                    sharedAfk.Update(world.player, world.activePlayerGuid,
                        waterAfkSafety, nowMs, "LivingWaterBlocked");
                    ++tick;
                    Sleep(PollIntervalMs);
                    continue;
                }

                if (livingDeathRecovery.Owns())
                {
                    // Separate living owner: ordinary quest/grind/watchdog paths below
                    // cannot acquire, roam, vendor, loot or renew its finite budget.
                    const bool released=livingDeathRecovery.Update(world,combat,tick,livingEvidence);
                    if (TemporaryGrindModeEnabled)
                    {
                        const auto edges=enduranceEdges.Observe(combat.LivingDefenseActive(),false,false);
                        if (edges.combatStarted) ++enduranceCombatStarts;
                    }
                    if (released)
                    {
                        combat.ResumeAfterLivingRecovery(tick);
                        if (TemporaryGrindModeEnabled)
                            grindMode.ResumeAfterDeathRecovery(world,combat,tick);
                        else questPlannerRuntime.ResumeAfterDeathRecovery(tick);
                        livingDeathRecovery.Reset();
                        runtimeRobustness.Reset();
                    }
                    AutonomySample inactiveLivingSample{};
                    autonomySupervisor.Update(inactiveLivingSample,tick);
                    AfkSafety livingAfkSafety{};
                    livingAfkSafety.recovery=true;
                    sharedAfk.Update(world.player,world.activePlayerGuid,livingAfkSafety,
                        nowMs,"PostResurrectionRecovery");
                    ++tick;
                    Sleep(PollIntervalMs);
                    continue; // even completion requires a fresh next-world acquisition tick
                }

                // Explicit AFK qualification starts only on stationary healthy
                // land with no owner. Any threat/death/reconciliation aborts
                // the test and immediately returns to ordinary survival work.
                if (sharedAfk.ControlledIdleRequested())
                {
                    bool directThreat=false;
                    for (const auto& unit : world.units)
                        if (unit.valid && unit.health>0 && world.activePlayerGuid &&
                            unit.targetGuid==world.activePlayerGuid)
                            directThreat=true;
                    const char* blocker=nullptr;
                    if (deathRecovery.IsActive() || deathRecovery.IsFailed() || world.player.health<=1)
                        blocker="death_recovery_or_dead";
                    else if (directThreat) blocker="direct_aggressor_targets_player";
                    else if (combat.LockedGuid()!=0 ||
                        (combat.State()!=CombatState::Idle && combat.State()!=CombatState::AcquiringTarget))
                        blocker="combat_owner";
                    else if (combat.Recovery().IsActive()) blocker="health_recovery_owner";
                    else if (combat.HasDeferredCorpseLootPending()) blocker="loot_transaction";
                    else if (navMeshReturn.OwnsMovement()) blocker="navigation_owner";
                    else if (vileFamiliarsTurnIn.IsActive()) blocker="quest_dialog_owner";
                    else if (questPlannerRuntime.OwnsControl()) blocker="quest_workload_owner";
                    else if (grindMode.FirstAidActive()) blocker="first_aid_transaction";
                    else if (grindMode.State()!=GrindModeState::Idle && grindMode.State()!=GrindModeState::Grinding)
                        blocker="grind_movement_or_transaction_owner";
                    else if (!world.player.maxHealth ||
                        RecoveryController::HealthPercent(world.player)<RecoveryController::ExitThresholdPercent())
                        blocker="health_not_safe";
                    else blocker=AfkClient5875::StationarySafetyReason(world.player);
                    if (sharedAfk.AdvanceQualificationHold(world.player,blocker,nowMs))
                    {
                        AfkSafety gate; gate.healthyIdle=true;
                        sharedAfk.Update(world.player,world.activePlayerGuid,gate,nowMs,"ControlledIdle");
                        ++tick;
                        Sleep(PollIntervalMs);
                        continue;
                    }
                }

                // =====================================
                // Phase 11A data-driven quest planner probe
                // =====================================

                if (!TemporaryGrindModeEnabled &&
                    !normalModeHeldForReconciliation)
                {
                    questPlannerProbe.Update(
                        tick
                    );
                }

                // =====================================
                // Read-only quest state probe
                // =====================================

                if (!TemporaryGrindModeEnabled &&
                    !normalModeHeldForReconciliation && questFocusId == 0 &&
                    (tick % 20) == 0)
                {
                    QuestStateReader::Snapshot nextQuestState{};

                    if (
                        QuestStateReader::Read(
                            nextQuestState
                        ))
                    {
                        consecutiveQuestProbeFailures =
                            0;

                        if (
                            !haveQuestState ||
                            !QuestStateReader::Same(
                                questState,
                                nextQuestState
                            ))
                        {
                            questState =
                                nextQuestState;

                            haveQuestState =
                                true;

                            QuestStateReader::Log(
                                questState
                            );
                        }

                        combat.ApplyQuestState(
                            nextQuestState
                        );

                        if (
                            vileFamiliarsTurnInStartAttempted &&
                            !nextQuestState.vileFamiliarsActive &&
                            !vileFamiliarsTurnIn.IsDone())
                        {
                            vileFamiliarsTurnIn.
                                MarkQuestRemoved();
                        }

                        /*
                         * Phase 10B starts once when quest 792
                         * is complete. NPC interaction remains
                         * disabled in this phase.
                         */
                        if (
                            !navMeshReturnStartAttempted &&
                            nextQuestState.vileFamiliarsActive &&
                            nextQuestState.vileFamiliarsComplete)
                        {
                            navMeshReturnStartAttempted =
                                true;

                            navMeshReturn.
                                StartReturnToZureetha(
                                    world.player,
                                    tick
                                );
                        }
                    }
                    else
                    {
                        ++consecutiveQuestProbeFailures;

                        if (
                            consecutiveQuestProbeFailures == 1 ||
                            consecutiveQuestProbeFailures % 12 == 0)
                        {
                            Debug::Logger::Info(
                                "QUEST PROBE: read failed; "
                                "combat continues unchanged."
                            );
                        }
                    }
                }

                // =====================================
                // Phase 11B generic objective execution
                // =====================================

                if (!TemporaryGrindModeEnabled &&
                    !normalModeHeldForReconciliation)
                {
                    questPlannerRuntime.SetVendorAutomationEnabled(
                        runtimeControl.VendorAutomationEnabled(false));
                    questPlannerRuntime.Update(
                        world,
                        combat,
                        tick
                    );
                }

                // =====================================
                // Phase 10C / legacy ownership fallback
                // =====================================

                if (
                    !normalModeHeldForReconciliation &&
                    navMeshReturn.Arrived() &&
                    !vileFamiliarsTurnInStartAttempted &&
                    haveQuestState &&
                    questState.vileFamiliarsActive &&
                    questState.vileFamiliarsComplete)
                {
                    vileFamiliarsTurnInStartAttempted =
                        true;

                    if (!vileFamiliarsTurnIn.Start(
                            tick))
                    {
                        Debug::Logger::Info(
                            "QUEST 792 TURN-IN: failed to "
                            "start after NavMesh arrival."
                        );
                    }
                }

                bool deathRecoveryOwnedTick = normalModeHeldForReconciliation;

                // =====================================
                // Phase 14G.4.2 death / corpse recovery
                // =====================================

                if (deathShouldOwn)
                {
                    deathRecoveryOwnedTick = true;

                    if (deathRecovery.State() == DeathRecoveryState::Idle)
                    {
                        Navigation::NavPoint deathRecordPosition{
                            world.player.x, world.player.y, world.player.z};
                        if (TemporaryGrindModeEnabled &&
                            (!deathBootstrap ||
                             deathRecovery.LastClearlyAlivePosition(deathRecordPosition)))
                        {
                            grindMode.RecordDeath(
                                world, tick, deathRecordPosition);
                        }

                        if (!TemporaryGrindModeEnabled)
                            questPlannerRuntime.ObserveDeathAtOwnershipBoundary(
                                world, combat, tick);

                        combat.SuspendForDeathRecovery(
                            world.player,
                            tick);

                        if (TemporaryGrindModeEnabled)
                            grindMode.SuspendForDeathRecovery(
                                world, combat, tick);
                        else
                            questPlannerRuntime.SuspendForDeathRecovery(
                                world, combat, tick);

                        if (deathRecovery.Start(
                                world,
                                tick,
                                GrindModeController::MapIdValue(),
                                deathBootstrap))
                        {
                            ++deathRecoveryEpisode;
                            Debug::Logger::Info(
                                "DEATH RECOVERY ENTER episode=" +
                                std::to_string(deathRecoveryEpisode) +
                                " mode=" +
                                std::string(TemporaryGrindModeEnabled
                                    ? "Grind" : "Questing") +
                                " reason=player_dead");
                        }
                        else
                        {
                            Debug::Logger::Info(
                                "DEATH RECOVERY 14G.4.2: start deferred; retrying from next live world snapshot.");
                        }
                    }

                    if (deathRecovery.IsActive() || deathRecovery.IsFailed())
                    {
                        deathRecovery.Update(
                            world,
                            tick);
                    }

                    if (deathRecovery.IsDone())
                    {
                        const bool automatic=deathRecovery.AutomaticCompletion();
                        const bool rearmed =
                            deathRecovery.RearmAfterConfirmedAlive();
                        combat.ResumeAfterDeathRecovery(tick,automatic);
                        if (automatic)
                        {
                            if (TemporaryGrindModeEnabled)
                                grindMode.ObserveAutomaticAliveForLivingRecovery();
                            livingDeathRecovery.Begin(world,GrindModeController::MapIdValue(),GetTickCount64());
                        }
                        else if (TemporaryGrindModeEnabled)
                            grindMode.ResumeAfterDeathRecovery(
                                world, combat, tick);
                        else
                            questPlannerRuntime.ResumeAfterDeathRecovery(tick);

                        Debug::Logger::Info(
                            "DEATH RECOVERY EXIT episode=" +
                            std::to_string(deathRecoveryEpisode) +
                            " resumeMode=" +
                            std::string(automatic ? "LivingRecovery" : TemporaryGrindModeEnabled
                                ? "Grind" : "Questing") +
                            " aliveConfirmed=yes");

                        AutonomySample resetSample{};
                        autonomySupervisor.Update(resetSample, tick);
                        Debug::Logger::Info(
                            "DEATH RECOVERY REARM episode=" +
                            std::to_string(deathRecoveryEpisode) +
                            " nextEpisode=" +
                            std::to_string(deathRecoveryEpisode + 1) +
                            " state=" + deathRecovery.StateName() +
                            " result=" + (rearmed ? "armed" : "failed"));
                    }
                }

                if (!deathRecoveryOwnedTick && TemporaryGrindModeEnabled)
                {
                    grindMode.Update(
                        world,
                        combat,
                        tick
                    );
                }
                else if (!deathRecoveryOwnedTick &&
                    questPlannerRuntime.
                        OwnsControl())
                {
                    /*
                     * Phase 11B runtime already updated the
                     * active objective executor above.
                     *
                     * Do not let the legacy 7B-10D state
                     * machines issue competing movement or
                     * combat commands during planner-owned
                     * objective execution.
                     */
                }
                else if (!deathRecoveryOwnedTick &&
                    navMeshReturn.
                        OwnsMovement())
                {
                    navMeshReturn.Update(
                        world.player,
                        tick
                    );
                }
                else if (!deathRecoveryOwnedTick &&
                    vileFamiliarsTurnIn.
                        IsActive())
                {
                    vileFamiliarsTurnIn.Update(
                        world,
                        tick
                    );
                }
                else if (!deathRecoveryOwnedTick && (
                    navMeshReturn.Arrived() ||
                    vileFamiliarsTurnIn.
                        IsDone() ||
                    vileFamiliarsTurnIn.
                        Failed()))
                {
                    /*
                     * Phase 10C intentionally holds the
                     * combat FSM after arrival/turn-in so
                     * the legacy pickup flow cannot start
                     * again after quest 792 disappears.
                     */
                }
                else if (!deathRecoveryOwnedTick)
                {
                    combat.Update(
                        world,
                        tick
                    );
                }

                if (!TemporaryGrindModeEnabled)
                    questPlannerRuntime.ObserveIdle(world,combat,tick,
                        deathRecoveryOwnedTick || normalModeHeldForReconciliation ||
                        navMeshReturn.OwnsMovement() || navMeshReturn.Arrived() ||
                        vileFamiliarsTurnIn.IsActive() || vileFamiliarsTurnIn.IsDone() || vileFamiliarsTurnIn.Failed());

                // Phase 15A observes ownership outcomes without altering them.
                bool manualWaitEntered = false;
                bool manualWaitCleared = false;
                if (TemporaryGrindModeEnabled)
                {
                    const bool combatActive =
                        combat.State() == CombatState::WarriorChargeFacing ||
                        combat.State() == CombatState::WarriorOpening ||
                        combat.State() == CombatState::Chasing ||
                        combat.State() == CombatState::Fighting;
                    const GrindEnduranceEdges edges = enduranceEdges.Observe(
                        combatActive,
                        deathRecoveryOwnedTick,
                        grindMode.State() == GrindModeState::WaitingForManualVendor);
                    if (edges.combatStarted)
                        ++enduranceCombatStarts;
                    if (edges.deathStarted)
                        ++enduranceDeaths;
                    manualWaitEntered = edges.manualVendorEntered;
                    manualWaitCleared = edges.manualVendorCleared;
                    if (manualWaitEntered)
                        ++enduranceManualVendorPauses;
                }

                const auto logEnduranceInterruption =
                    [&](GrindInterruptionReason reason,
                        const char* event,
                        RuntimeRobustnessReason watchdogReason)
                {
                    const std::uint64_t targetGuid = combat.LockedGuid() != 0
                        ? combat.LockedGuid() : grindMode.ApproachGuid();
                    std::string targetDistance = "unknown";
                    for (const auto& unit : world.units)
                    {
                        if (unit.valid && unit.guid == targetGuid && targetGuid != 0)
                        {
                            targetDistance = Float(unit.distance);
                            break;
                        }
                    }
                    const auto& maintenance = grindMode.Maintenance();
                    const std::uint64_t diagnosticNowMs = GetTickCount64();
                    const std::string maintenanceNeed = maintenance.valid
                        ? ManualVendorModePolicy::NeedName(
                            false, AutonomousMaintenancePolicy::Evaluate(maintenance))
                        : "unknown";
                    Debug::Logger::Info(
                        std::string("GRIND 15A INTERRUPTION event=") + event +
                        " reason=" + GrindEnduranceDiagnosticPolicy::Name(reason) +
                        " utc=" + UtcTimestamp() +
                        " tick=" + std::to_string(tick) +
                        " grindState=" + grindMode.StateName() +
                        " combatState=" + combat.StateName() +
                        " navigationState=" + grindMode.NavigationStateName() +
                        " targetGuid=" + Hex64(targetGuid) +
                        " targetDistance=" + targetDistance +
                        " movementAgeMs=" + (haveMovementAnchor
                            ? std::to_string(diagnosticNowMs >= lastMeaningfulMovementMs
                                ? diagnosticNowMs - lastMeaningfulMovementMs : 0)
                            : "unknown") +
                        " lastMeaningfulProgressAgeTicks=" +
                        std::to_string(runtimeRobustness.ProgressAgeTicks(tick)) +
                        " maintenanceNeed=" + maintenanceNeed +
                        " bagsFree=" + (grindMode.Bags().valid
                            ? std::to_string(grindMode.Bags().freeSlots) : "unknown") +
                        " deathOwnership=" + (deathRecoveryOwnedTick ? "yes" : "no") +
                        " vendorOwnership=" +
                        (grindMode.State() == GrindModeState::Vendoring ? "yes" : "no") +
                        " watchdogOwner=" + RuntimeActivityOwnerName(runtimeRobustness.Owner()) +
                        " watchdogReason=" + GrindEnduranceDiagnosticPolicy::WatchdogName(watchdogReason) +
                        " lastNavigationFailure=" + Navigation::NavigationInitTelemetryPolicy::ReasonName(
                            grindMode.LastNavigationFailure()));
                };

                if (TemporaryGrindModeEnabled)
                {
                    if (manualWaitEntered)
                        logEnduranceInterruption(
                            GrindInterruptionReason::ManualVendorRequired,
                            "manual_vendor_wait_entered", RuntimeRobustnessReason::None);
                    if (manualWaitCleared)
                        Debug::Logger::Info(
                            "GRIND 15A INTENTIONAL WAIT CLEARED utc=" + UtcTimestamp() +
                            " tick=" + std::to_string(tick) +
                            " state=" + grindMode.StateName());
                    if (grindMode.NavigationFailures() != observedNavigationFailures)
                    {
                        observedNavigationFailures = grindMode.NavigationFailures();
                        GrindInterruptionFacts facts{};
                        facts.navigationFailure = true;
                        facts.approachFailure = grindMode.LastNavigationFailureWasApproach();
                        facts.navigationReason = grindMode.LastNavigationFailure();
                        logEnduranceInterruption(
                            GrindEnduranceDiagnosticPolicy::Classify(facts),
                            "navigation_failed", RuntimeRobustnessReason::None);
                    }
                    if (combat.LootsFailed() != observedLootFailures)
                    {
                        observedLootFailures = combat.LootsFailed();
                        GrindInterruptionFacts facts{};
                        facts.lootFailed = true;
                        logEnduranceInterruption(
                            GrindEnduranceDiagnosticPolicy::Classify(facts),
                            "loot_failed", RuntimeRobustnessReason::None);
                    }
                    if (deathRecovery.IsFailed() && !enduranceDeathFailed)
                    {
                        GrindInterruptionFacts facts{};
                        facts.deathRecoveryFailed = true;
                        logEnduranceInterruption(
                            GrindEnduranceDiagnosticPolicy::Classify(facts),
                            "death_recovery_failed", RuntimeRobustnessReason::None);
                    }
                    enduranceDeathFailed = deathRecovery.IsFailed();
                    if (grindMode.Failed() && !enduranceGrindFailed)
                        logEnduranceInterruption(
                            GrindInterruptionReason::OtherUnknown,
                            "grind_failed", RuntimeRobustnessReason::None);
                    enduranceGrindFailed = grindMode.Failed();
                    if (combat.Failed() && !enduranceCombatFailed)
                        logEnduranceInterruption(
                            GrindInterruptionReason::OtherUnknown,
                            "combat_failed", RuntimeRobustnessReason::None);
                    enduranceCombatFailed = combat.Failed();
                }

                // =====================================
                // Phase 14K.1.7 active-bot AFK safeguard
                // =====================================

                if (TemporaryGrindModeEnabled)
                {
                    ActiveBotAfkSample antiAfkSample{};
                    antiAfkSample.active =
                        !deathRecoveryOwnedTick &&
                        world.player.valid &&
                        world.player.maxHealth > 0 &&
                        world.player.health > 0 &&
                        grindMode.State() != GrindModeState::Failed &&
                        grindMode.State() != GrindModeState::WaitingForManualVendor &&
                        combat.State() != CombatState::Failed;

                    antiAfkSample.safeIdle =
                        antiAfkSample.active &&
                        grindMode.State() == GrindModeState::Grinding &&
                        (combat.State() == CombatState::Idle ||
                         combat.State() == CombatState::AcquiringTarget ||
                         combat.State() == CombatState::WaitingForTargetSelection) &&
                        combat.LockedGuid() == 0 &&
                        !combat.HasDeferredCorpseLootPending() &&
                        !combat.Recovery().IsActive() &&
                        !grindMode.FirstAidActive() &&
                        RecoveryController::HealthPercent(world.player) >=
                            RecoveryController::ExitThresholdPercent();

                    antiAfkSample.x = world.player.x;
                    antiAfkSample.y = world.player.y;
                    antiAfkSample.z = world.player.z;

                    const ActiveBotAfkEvent antiAfkEvent =
                        antiAfkSafeguard.Update(antiAfkSample, tick);

                    if (antiAfkEvent.due)
                    {
                        grindMode.ForceAntiAfkSafeguard(
                            world,
                            combat,
                            tick,
                            "safe stationary grind acquisition for " +
                                std::to_string(antiAfkEvent.safeIdleTicks) +
                                " polling ticks; request=" +
                                std::to_string(antiAfkEvent.request));
                    }
                }

                if (afkTransition.event != AfkDiagnosticEvent::None)
                {
                    Debug::Logger::Info(
                        std::string("AFK FLAG CANDIDATE 14K.1.8: ") +
                        (afkTransition.event == AfkDiagnosticEvent::Entered ? "SET" : "UNSET") +
                        " utc=" + afkTransitionUtc +
                        " tick=" + std::to_string(tick) +
                        " source=player_flags_candidate signalVerified=no" +
                        " rawFlags=" + Hex32(lastAfkRawFlags) +
                        " candidateAfk=" + (world.player.afkCandidate ? std::string("yes") : std::string("no")) +
                        " initialObservation=" + (afkTransition.initialObservation ? std::string("yes") : std::string("no")) +
                        " observedDurationMs=" + std::to_string(afkTransition.observedDurationMs) +
                        " grindState=" + grindMode.StateName() +
                        " combatState=" + combat.StateName() +
                        " deathRecoveryState=" + deathRecovery.StateName() +
                        " deathRecoveryOwned=" + (deathRecoveryOwnedTick ? std::string("yes") : std::string("no")) +
                        " vendorState=" + grindMode.Vendor().StateName() +
                        " playerValid=" + (world.player.valid ? std::string("yes") : std::string("no")) +
                        " health=" + std::to_string(world.player.health) +
                        " movementAgeMs=" + (haveMovementAnchor
                            ? std::to_string(nowMs >= lastMeaningfulMovementMs
                                ? nowMs - lastMeaningfulMovementMs : 0)
                            : std::string("unknown")) +
                        " lockedGuid=" + Hex64(combat.LockedGuid()) +
                        " antiAfkSafeIdleActive=" + (antiAfkSafeguard.SafeIdleActive() ? std::string("yes") : std::string("no")) +
                        " antiAfkSafeIdleAgeTicks=" + std::to_string(antiAfkSafeguard.SafeIdleAgeTicks(tick)) +
                        " antiAfkRequests=" + std::to_string(antiAfkSafeguard.Requests()) +
                        " antiAfkActions=" + std::to_string(grindMode.AntiAfkActions()));
                }

                // =====================================
                // Phase 14G.4.1 autonomy supervisor
                // =====================================

                const auto observeEscapeDiagnostic =
                    [&](bool combatOwned, bool combatHardStall)
                {
                    EscapeDecisionInput input{};
                    input.combatOwned = combatOwned;
                    input.combatHardStall = combatHardStall;
                    input.playerGuid = world.activePlayerGuid;
                    input.lifeEpisode =
                        static_cast<std::uint64_t>(enduranceDeaths);
                    input.observedAtMs = nowMs;
                    input.targetGuid = combatOwned ? combat.LockedGuid() : 0;
                    input.playerHealthKnown = world.player.valid &&
                        world.player.maxHealth > 0 && world.player.health > 0;
                    if (input.playerHealthKnown)
                        input.playerHealthPct =
                            RecoveryController::HealthPercent(world.player);
                    input.aggressorsKnown = world.activePlayerGuid != 0;
                    bool targetPresent = false;
                    bool targetAlive = false;
                    bool targetVitalsKnown = false;

                    if (combatOwned && input.targetGuid != 0)
                    {
                        for (const auto& unit : world.units)
                        {
                            if (unit.valid && unit.guid == input.targetGuid)
                            {
                                targetPresent = true;
                                targetAlive = unit.health > 0;
                                targetVitalsKnown = unit.maxHealth > 0;
                                if (targetAlive && targetVitalsKnown)
                                {
                                    input.targetHealthKnown = true;
                                    input.targetHealthPct = 100.0f *
                                        static_cast<float>(unit.health) /
                                        static_cast<float>(unit.maxHealth);
                                }
                            }
                            // Positive, current-snapshot evidence only. A
                            // transiently cleared UNIT_FIELD_TARGET may
                            // undercount; it must not invent an aggressor.
                            if (input.aggressorsKnown &&
                                GrindTargetPolicy::LooksLikeCombatCreature(unit) &&
                                unit.targetGuid == world.activePlayerGuid &&
                                unit.distance >= 0.0f &&
                                unit.distance <=
                                    EscapeDecisionPolicy::LocalAggressorRadius)
                            {
                                ++input.observedDirectAggressors;
                            }
                        }
                    }

                    const EscapeDecisionAssessment assessment =
                        escapeDiagnostic.Observe(input);
                    const CombatHealthTrendAssessment healthTrend =
                        escapeDiagnostic.HealthTrend();

                    // A candidate starts one target-bound evaluation. A
                    // transient hard-stall pulse clearing may downgrade the
                    // decision to Warning without cancelling that evaluation.
                    EscapeEvaluationEpisodeInput episodeInput{};
                    episodeInput.worldValid = true;
                    episodeInput.playerAlive = world.player.valid &&
                        world.player.health > 0 && world.player.maxHealth > 0;
                    episodeInput.combatOwned = combatOwned;
                    episodeInput.targetGuid = input.targetGuid;
                    episodeInput.targetPresent = targetPresent;
                    episodeInput.targetAlive = targetAlive;
                    episodeInput.targetVitalsKnown = targetVitalsKnown;
                    episodeInput.playerHealthKnown = input.playerHealthKnown;
                    episodeInput.playerHealthPct = input.playerHealthPct;
                    episodeInput.decision = assessment.decision;
                    const auto episodeEvent =
                        escapeEvaluationEpisode.Observe(episodeInput);
                    std::optional<EscapeRouteSelection> routeResult;
                    if (episodeEvent.kind ==
                        EscapeEvaluationEpisodeEventKind::Start)
                    {
                        routeResult = escapeRouteDiagnostic.Begin(
                            world, grindMode, input.targetGuid, tick);
                        Debug::Logger::Info(
                            "ESCAPE 15C.1 EPISODE START reason=" +
                            std::string(EscapeEvaluationEpisodePolicy::ReasonName(
                                episodeEvent.reason)) +
                            " targetGuid=" + Hex64(input.targetGuid) +
                            " healthPct=" + Float(input.playerHealthPct));
                        Debug::Logger::Info(
                            "ESCAPE 15C.1 ROUTE decision=probing" +
                            std::string(" candidateCount=") +
                            std::to_string(escapeRouteDiagnostic.CandidateCount()) +
                            " origin=(" + Float(world.player.x) + "," +
                            Float(world.player.y) + "," +
                            Float(world.player.z) + ")" +
                            " targetGuid=" + Hex64(input.targetGuid) +
                            " mode=planning_only_route_or_expanded");
                    }
                    else if (episodeEvent.kind ==
                             EscapeEvaluationEpisodeEventKind::Keep)
                    {
                        Debug::Logger::Info(
                            "ESCAPE 15C.1 EPISODE KEEP reason=" +
                            std::string(EscapeEvaluationEpisodePolicy::ReasonName(
                                episodeEvent.reason)) +
                            " targetGuid=" + Hex64(input.targetGuid) +
                            " healthPct=" + Float(input.playerHealthPct));
                    }
                    else if (episodeEvent.kind ==
                             EscapeEvaluationEpisodeEventKind::Cancel)
                    {
                        escapeRouteDiagnostic.Reset();
                        Debug::Logger::Info(
                            "ESCAPE 15C.1 EPISODE CANCEL reason=" +
                            std::string(EscapeEvaluationEpisodePolicy::ReasonName(
                                episodeEvent.reason)) +
                            " targetGuid=" + Hex64(input.targetGuid));
                    }

                    if (escapeEvaluationEpisode.Active() && !routeResult &&
                        escapeRouteDiagnostic.Active())
                        routeResult = escapeRouteDiagnostic.Step(tick);
                    if (routeResult)
                    {
                            const auto& selected = *routeResult;
                            const bool available = selected.decision ==
                                EscapeRouteDecisionKind::CandidateAvailable;
                            const auto& generation =
                                escapeRouteDiagnostic.Generation();
                            const auto& evidence =
                                escapeRouteDiagnostic.Evidence();
                            Debug::Logger::Info(
                                "ESCAPE 15C.1 ROUTE decision=" +
                                std::string(EscapeRouteSelectionPolicy::DecisionName(
                                    selected.decision)) +
                                " candidateCount=" +
                                std::to_string(generation.candidates.size()) +
                                " evaluatedCount=" +
                                std::to_string(escapeRouteDiagnostic.EvaluatedCount()) +
                                " candidateIndex=" +
                                (escapeRouteDiagnostic.EvaluatedCount() > 0
                                    ? std::to_string(
                                        escapeRouteDiagnostic.LastCandidateIndex())
                                    : std::string("unknown")) +
                                " probeTicks=" +
                                std::to_string(escapeRouteDiagnostic.LastProbeTicks()) +
                                " episodeTicks=" +
                                std::to_string(escapeRouteDiagnostic.LastEpisodeTicks()) +
                                " reachableCount=" +
                                std::to_string(selected.reachableCount) +
                                " currentMinAggressorDistance=" +
                                Float(selected.currentMinimumThreatDistance) +
                                " selectedMinAggressorDistance=" +
                                (available ? Float(selected.selectedMinimumThreatDistance)
                                           : std::string("unknown")) +
                                " selectedDestination=" + (available
                                    ? "(" + Float(generation.candidates[
                                        selected.selectedIndex].destination.x) + "," +
                                        Float(generation.candidates[
                                        selected.selectedIndex].destination.y) + "," +
                                        Float(generation.candidates[
                                        selected.selectedIndex].destination.z) + ")"
                                    : std::string("unknown")) +
                                " routeCost=" + (available
                                    ? Float(selected.routeCost) : std::string("unknown")) +
                                " dangerCurrent=" + (available
                                    ? Float(evidence[selected.selectedIndex].currentDeathRisk)
                                    : std::string("unknown")) +
                                " dangerSelected=" + (available
                                    ? Float(evidence[selected.selectedIndex].destinationDeathRisk)
                                    : std::string("unknown")) +
                                " navHazardCurrent=" + (available
                                    ? Float(evidence[selected.selectedIndex].currentNavHazardRisk)
                                    : std::string("unknown")) +
                                " navHazardSelected=" + (available
                                    ? Float(evidence[selected.selectedIndex].destinationNavHazardRisk)
                                    : std::string("unknown")) +
                                " movementCommands=" +
                                std::to_string(escapeRouteDiagnostic.MovementCommands()) +
                                " reason=" + selected.reason);
                        const auto completeEvent =
                            escapeEvaluationEpisode.Complete();
                        Debug::Logger::Info(
                            "ESCAPE 15C.1 EPISODE COMPLETE reason=" +
                            std::string(EscapeEvaluationEpisodePolicy::ReasonName(
                                completeEvent.reason)) +
                            " decision=" +
                            EscapeRouteSelectionPolicy::DecisionName(
                                selected.decision) +
                            " targetGuid=" + Hex64(input.targetGuid));
                    }
                    if (!assessment.changed)
                        return;
                    Debug::Logger::Info(
                        "ESCAPE 15C.0 DECISION decision=" +
                        std::string(EscapeDecisionPolicy::DecisionName(
                            assessment.decision)) +
                        " reason=" + EscapeDecisionPolicy::ReasonName(
                            assessment.reason) +
                        " healthPct=" + (input.playerHealthKnown
                            ? Float(input.playerHealthPct) : std::string("unknown")) +
                        " targetHealthPct=" + (input.targetHealthKnown
                            ? Float(input.targetHealthPct) : std::string("unknown")) +
                        " aggressors=" + (input.aggressorsKnown
                            ? std::to_string(input.observedDirectAggressors)
                            : std::string("unknown")) +
                        " multiAggro=" +
                            (input.observedDirectAggressors >= 2 ? "yes" : "no") +
                        " hardStall=" + (combatHardStall ? "yes" : "no") +
                        " corroboratedSamples=" +
                            std::to_string(assessment.corroboratedSamples) +
                        " recentHealthLossPct=" +
                            Float(healthTrend.recentHealthLossPct) +
                        " trendSamples=" +
                            std::to_string(healthTrend.trendSamples) +
                        " declineObservations=" +
                            std::to_string(healthTrend.declineObservations) +
                        " deteriorating=" +
                            (healthTrend.deteriorating ? "yes" : "no") +
                        " targetGuid=" + Hex64(input.targetGuid) +
                        " combatState=" + combat.StateName() +
                        " grindState=" + grindMode.StateName());
                };

                if (TemporaryGrindModeEnabled && !deathRecoveryOwnedTick)
                {
                    AutonomySample autonomySample{};

                    if (
                        combat.State() == CombatState::WarriorChargeFacing ||
                        combat.State() == CombatState::WarriorOpening ||
                        combat.State() == CombatState::Chasing ||
                        combat.State() == CombatState::Fighting)
                    {
                        autonomySample.activity = AutonomyActivity::Combat;
                        autonomySample.identity = combat.LockedGuid();

                        for (const auto& unit : world.units)
                        {
                            if (unit.valid && unit.guid == combat.LockedGuid())
                            {
                                autonomySample.targetHealth = unit.health;
                                break;
                            }
                        }
                    }
                    else if (
                        combat.State() == CombatState::DefensiveContainment)
                    {
                        // The follower owns movement liveness only while it
                        // is planning or moving. Attack-stop/disengagement
                        // observation is bounded by containment itself.
                        if (combat.DefensiveContainmentOwnsMovement())
                        {
                            autonomySample.activity = AutonomyActivity::Movement;
                            autonomySample.identity = combat.LockedGuid();
                        }
                    }
                    else if (
                        grindMode.State() == GrindModeState::ApproachingTarget ||
                        grindMode.State() == GrindModeState::Roaming)
                    {
                        autonomySample.activity = AutonomyActivity::Movement;
                        autonomySample.identity =
                            static_cast<std::uint64_t>(grindMode.State()) + 1ULL;
                    }

                    autonomySample.x = world.player.x;
                    autonomySample.y = world.player.y;
                    autonomySample.z = world.player.z;
                    autonomySample.monotonicMs = GetTickCount64();
                    autonomySample.navigationInitialization =
                        combat.State() == CombatState::DefensiveContainment
                            ? combat.DefensiveContainmentInitialization()
                            : grindMode.NavigationInitializationObservation();
                    const auto& work = autonomySample.navigationInitialization;
                    if (work.Valid() &&
                        (!lastAutonomyInitialization.pending ||
                         work.intent != lastAutonomyInitialization.intent ||
                         work.tier != lastAutonomyInitialization.tier))
                    {
                        Debug::Logger::Info(
                            "NAV STALL CLASSIFICATION intent=" + std::to_string(work.intent) +
                            " owner=Grinding class=initialization_pending decision=preserve_intent"
                            " physicalProgress=no tier=" +
                            Navigation::NavigationInitTelemetryPolicy::TierName(work.tier) +
                            " tilesProcessed=" + std::to_string(work.tilesProcessed) +
                            " tilesTotal=" + std::to_string(work.tilesTotal));
                    }
                    lastAutonomyInitialization = work;

                    const AutonomyEvent autonomyEvent =
                        autonomySupervisor.Update(autonomySample, tick);

                    observeEscapeDiagnostic(
                        autonomySample.activity == AutonomyActivity::Combat,
                        autonomyEvent.kind == AutonomyEventKind::HardStall &&
                            autonomyEvent.activity == AutonomyActivity::Combat);

                    if (autonomyEvent.kind == AutonomyEventKind::SoftStall)
                    {
                        Debug::Logger::Info(
                            "AUTONOMY 14G.4.1: SUSPECTED GLOBAL STALL activity=" +
                            std::string(
                                autonomyEvent.activity == AutonomyActivity::Combat
                                    ? "Combat"
                                    : "Movement") +
                            " noProgressTicks=" +
                            std::to_string(autonomyEvent.stalledTicks));
                    }
                    else if (autonomyEvent.kind == AutonomyEventKind::HardStall)
                    {
                        if (autonomyEvent.activity == AutonomyActivity::Movement)
                        {
                            const auto& init = autonomySample.navigationInitialization;
                            Debug::Logger::Info(
                                "NAV STALL CLASSIFICATION intent=" + std::to_string(init.intent) +
                                " owner=Grinding class=" + autonomyEvent.classification +
                                " state=" + grindMode.StateName() +
                                " initializationPending=" + (init.pending ? "yes" : "no") +
                                " tier=" + Navigation::NavigationInitTelemetryPolicy::TierName(init.tier) +
                                " tilesProcessed=" + std::to_string(init.tilesProcessed) +
                                " tilesTotal=" + std::to_string(init.tilesTotal) +
                                " position=" + Float(world.player.x) + "," +
                                    Float(world.player.y) + "," + Float(world.player.z));
                        }
                        logEnduranceInterruption(
                            autonomyEvent.activity == AutonomyActivity::Combat
                                ? GrindInterruptionReason::CombatHardStall
                                : GrindInterruptionReason::NavigationFailure,
                            "autonomy_hard_stall", RuntimeRobustnessReason::None);
                        const std::string reason =
                            "no meaningful physical/HP progress for " +
                            std::to_string(autonomyEvent.stalledTicks) +
                            " polling ticks";

                        if (autonomyEvent.activity == AutonomyActivity::Combat)
                        {
                            combat.ForceAutonomyCombatRecovery(
                                world,
                                tick,
                                reason);
                        }
                        else if (autonomyEvent.activity == AutonomyActivity::Movement)
                        {
                            grindMode.ForceAutonomyMovementRecovery(
                                world,
                                combat,
                                tick,
                                reason);
                        }
                    }
                }
                else if (TemporaryGrindModeEnabled && deathRecoveryOwnedTick)
                {
                    AutonomySample resetSample{};
                    autonomySupervisor.Update(resetSample, tick);
                    observeEscapeDiagnostic(false, false);
                }

                // =====================================
                // Phase 14K.0 high-level runtime robustness
                // =====================================

                if (TemporaryGrindModeEnabled && !deathRecoveryOwnedTick)
                {
                    RuntimeRobustnessSample robustnessSample{};
                    robustnessSample.active =
                        world.player.valid &&
                        world.player.maxHealth > 0 &&
                        world.player.health > 0 &&
                        grindMode.State() != GrindModeState::Failed &&
                        grindMode.State() != GrindModeState::WaitingForManualVendor &&
                        combat.State() != CombatState::Failed;
                    robustnessSample.level = world.player.level;
                    robustnessSample.currentXp = world.player.currentXp;
                    robustnessSample.xpValid = world.player.xpValid;
                    robustnessSample.kills = combat.Kills();
                    robustnessSample.vendorTrips = grindMode.VendorTrips();
                    robustnessSample.x = world.player.x;
                    robustnessSample.y = world.player.y;
                    robustnessSample.z = world.player.z;
                    robustnessSample.playerHealth = world.player.health;
                    robustnessSample.playerMaxHealth = world.player.maxHealth;
                    robustnessSample.playerPowerType = world.player.powerType;
                    robustnessSample.playerPowerRaw = world.player.powerRaw;
                    robustnessSample.playerMaxPowerRaw = world.player.maxPowerRaw;
                    robustnessSample.targetGuid = combat.LockedGuid();
                    robustnessSample.grindState =
                        static_cast<int>(grindMode.State());
                    robustnessSample.combatState =
                        static_cast<int>(combat.State());
                    robustnessSample.recoveryActive =
                        combat.Recovery().IsActive();
                    robustnessSample.recoveryNoSupplyStalled =
                        combat.Recovery().IsNoSupplyHealthStalled();
                    robustnessSample.firstAidActive =
                        grindMode.FirstAidActive();
                    robustnessSample.firstAidCraftsIssued =
                        grindMode.FirstAidCraftsIssued();
                    robustnessSample.vendorActive =
                        grindMode.State() == GrindModeState::Vendoring;
                    robustnessSample.navigationInitializationPending =
                        grindMode.NavigationInitializationPending() ||
                        combat.DefensiveContainmentInitialization().pending;
                    robustnessSample.navigationOwned =
                        grindMode.NavigationOwnsMovement() || navMeshReturn.OwnsMovement() ||
                        combat.DefensiveContainmentOwnsMovement();
                    robustnessSample.deathRecoveryActive = deathRecoveryOwnedTick;
                    robustnessSample.dialogActive = vileFamiliarsTurnIn.IsActive();
                    robustnessSample.vendorState =
                        static_cast<int>(grindMode.Vendor().State());
                    robustnessSample.vendorProgressSerial =
                        grindMode.Vendor().SalesObserved() +
                        grindMode.Vendor().RepairActions() +
                        grindMode.Vendor().FoodPurchases() +
                        grindMode.Vendor().DrinkPurchases();

                    std::uint8_t liveStandState = 0;
                    robustnessSample.postureValid =
                        PlayerPostureController::ReadStandState(
                            world.player,
                            liveStandState);
                    robustnessSample.playerStanding =
                        !robustnessSample.postureValid || liveStandState == 0;

                    if (robustnessSample.targetGuid != 0)
                    {
                        for (const auto& unit : world.units)
                        {
                            if (unit.valid &&
                                unit.guid == robustnessSample.targetGuid)
                            {
                                robustnessSample.targetHealth = unit.health;
                                break;
                            }
                        }
                    }

                    const RuntimeRobustnessEvent robustnessEvent =
                        runtimeRobustness.Update(robustnessSample, tick);

                    if (const char* decision = runtimeRobustness.AcquisitionDecision())
                    {
                        Debug::Logger::Info(
                            std::string("ACQUISITION LIVENESS state=") + combat.StateName() +
                            " grindState=" + grindMode.StateName() +
                            " owner=" + RuntimeActivityOwnerName(runtimeRobustness.Owner()) +
                            " epochActive=" + (runtimeRobustness.AcquisitionEpochActive() ? "yes" : "no") +
                            " ageTicks=" + std::to_string(runtimeRobustness.AcquisitionDecisionAgeTicks()) +
                            " decision=" + decision +
                            " reason=" + (robustnessEvent.reason == RuntimeRobustnessReason::IdleDeadlock
                                ? "bounded_acquisition_window_exhausted"
                                : robustnessSample.navigationInitializationPending ? "navigation_initialization_owner"
                                : robustnessSample.navigationOwned ? "navigation_movement_owner"
                                : "actual_owner_handoff"));
                    }

                    if (robustnessEvent.kind != RuntimeRobustnessEventKind::None)
                    {
                        GrindInterruptionFacts facts{};
                        facts.watchdogReason = robustnessEvent.reason;
                        facts.recoveryBlocked =
                            robustnessSample.recoveryNoSupplyStalled;
                        facts.noTarget =
                            robustnessEvent.owner == RuntimeActivityOwner::Acquisition &&
                            combat.LockedGuid() == 0 &&
                            grindMode.ApproachGuid() == 0 &&
                            grindMode.LocalSuitableCount() == 0;
                        facts.intentionalWait =
                            robustnessSample.navigationInitializationPending;
                        const GrindInterruptionReason classification =
                            GrindEnduranceDiagnosticPolicy::Classify(facts);
                        const char* eventName =
                            robustnessEvent.kind == RuntimeRobustnessEventKind::Watching
                                ? "watch"
                                : (robustnessEvent.kind == RuntimeRobustnessEventKind::Recovery
                                    ? "bounded_recovery" : "escalated_recovery");
                        logEnduranceInterruption(
                            classification, eventName, robustnessEvent.reason);
                        if (robustnessEvent.kind != RuntimeRobustnessEventKind::Watching &&
                            GrindEnduranceDiagnosticPolicy::CountsAsUnexpectedIdle(classification))
                            ++enduranceUnexpectedIdleEvents;
                    }

                    if (robustnessEvent.kind ==
                        RuntimeRobustnessEventKind::Watching)
                    {
                        Debug::Logger::Info(
                            "ROBUSTNESS 14K.0: NO-PROGRESS WATCH armed ageTicks=" +
                            std::to_string(robustnessEvent.noProgressTicks) +
                            " grind=" + grindMode.StateName() +
                            " combat=" + combat.StateName() +
                            " target=" + Hex64(combat.LockedGuid()) +
                            " owner=" + RuntimeActivityOwnerName(runtimeRobustness.Owner()) +
                            " ownerAgeTicks=" +
                            std::to_string(runtimeRobustness.OwnerAgeTicks(tick)) +
                            " outcomeAgeTicks=" +
                            std::to_string(runtimeRobustness.OutcomeAgeTicks(tick)));
                    }
                    else if (
                        robustnessEvent.kind == RuntimeRobustnessEventKind::Recovery ||
                        robustnessEvent.kind == RuntimeRobustnessEventKind::EscalatedRecovery)
                    {
                        const bool escalated =
                            robustnessEvent.kind ==
                            RuntimeRobustnessEventKind::EscalatedRecovery;

                        std::string reasonPrefix;
                        switch (robustnessEvent.reason)
                        {
                            case RuntimeRobustnessReason::IdleDeadlock:
                                reasonPrefix =
                                    "acquisition deadlock: Grind acquisition epoch (Idle/AcquiringTarget/WaitingForTargetSelection) failed to hand off to combat for ";
                                break;
                            case RuntimeRobustnessReason::UnexpectedSeatedIdle:
                                reasonPrefix =
                                    "unexpected seated idle with no legitimate recovery/crafting/vendor owner for ";
                                break;
                            case RuntimeRobustnessReason::RepeatedActivityLoop:
                                reasonPrefix =
                                    "repeated controller-state/target churn without physical or resource progress for ";
                                break;
                            case RuntimeRobustnessReason::RecoveryOwnerTimeout:
                                reasonPrefix =
                                    "Recovery owner exceeded its bounded no-resource-progress window for ";
                                break;
                            case RuntimeRobustnessReason::FirstAidOwnerTimeout:
                                reasonPrefix =
                                    "First Aid owner exceeded its bounded stationary craft window for ";
                                break;
                            case RuntimeRobustnessReason::VendorOwnerTimeout:
                                reasonPrefix =
                                    "Vendor owner exceeded its bounded no-progress window for ";
                                break;
                            case RuntimeRobustnessReason::MovementOwnerTimeout:
                                reasonPrefix =
                                    "navigation/approach owner made no physical progress for ";
                                break;
                            case RuntimeRobustnessReason::CombatOwnerTimeout:
                                reasonPrefix =
                                    "combat owner made no movement/damage progress for ";
                                break;
                            case RuntimeRobustnessReason::StrategicNoOutcome:
                                reasonPrefix =
                                    "strategic no-outcome loop (no XP/level/kill/vendor completion) for ";
                                break;
                            case RuntimeRobustnessReason::TacticalNoProgress:
                            default:
                                reasonPrefix =
                                    "no meaningful movement/XP/kill/damage/healing/mana-recovery progress for ";
                                break;
                        }

                        const std::string reason =
                            reasonPrefix +
                            std::to_string(robustnessEvent.noProgressTicks) +
                            " polling ticks; owner=" +
                            RuntimeActivityOwnerName(robustnessEvent.owner) +
                            " ownerState=" +
                            std::to_string(robustnessEvent.ownerState) +
                            " recoveryAttempt=" +
                            std::to_string(robustnessEvent.recoveryAttempt);

                        if (robustnessEvent.reason ==
                            RuntimeRobustnessReason::IdleDeadlock)
                        {
                            Debug::Logger::Info(
                                "ROBUSTNESS 14K.1.4: ACQUISITION EPOCH DEADLOCK; transient target/SetTarget churn did not reset liveness, forcing bounded ownership reset and verified stand-up.");
                        }
                        else if (robustnessEvent.reason ==
                            RuntimeRobustnessReason::UnexpectedSeatedIdle)
                        {
                            Debug::Logger::Info(
                                "ROBUSTNESS 14K.1.1: UNEXPECTED SEATED IDLE detected; stale posture/ownership will be reset.");
                        }
                        else if (robustnessEvent.reason ==
                            RuntimeRobustnessReason::RepeatedActivityLoop)
                        {
                            Debug::Logger::Info(
                                "ROBUSTNESS 14K.1.1: REPEATED ACTIVITY LOOP detected; state churn is not accepted as progress.");
                        }
                        else if (robustnessEvent.reason == RuntimeRobustnessReason::RecoveryOwnerTimeout ||
                                 robustnessEvent.reason == RuntimeRobustnessReason::FirstAidOwnerTimeout ||
                                 robustnessEvent.reason == RuntimeRobustnessReason::VendorOwnerTimeout ||
                                 robustnessEvent.reason == RuntimeRobustnessReason::MovementOwnerTimeout ||
                                 robustnessEvent.reason == RuntimeRobustnessReason::CombatOwnerTimeout)
                        {
                            Debug::Logger::Info(
                                std::string("ROBUSTNESS 14K.1.1: OWNER TIMEOUT owner=") +
                                RuntimeActivityOwnerName(robustnessEvent.owner) +
                                " state=" + std::to_string(robustnessEvent.ownerState) +
                                "; forcing bounded ownership recovery.");
                        }

                        grindMode.ForceRuntimeRobustnessRecovery(
                            world,
                            combat,
                            tick,
                            reason,
                            escalated);
                    }
                }
                else
                {
                    runtimeRobustness.Reset();
                }

                if (TemporaryGrindModeEnabled && tick != 0 && (tick % 240) == 0)
                {
                    Debug::Logger::Info(
                        "GRIND 15A SESSION utc=" + UtcTimestamp() +
                        " tick=" + std::to_string(tick) +
                        " kills=" + std::to_string(combat.Kills()) +
                        " lootAttempts=" + std::to_string(combat.Loot().TotalLootAttempts()) +
                        " lootSuccesses=" + std::to_string(combat.LootsSucceeded()) +
                        " combatStarts=" + std::to_string(enduranceCombatStarts) +
                        " navigationFailures=" + std::to_string(grindMode.NavigationFailures()) +
                        " deaths=" + std::to_string(enduranceDeaths) +
                        " successfulDeathRecoveries=" +
                        std::to_string(deathRecovery.Recoveries()) +
                        " manualVendorPauses=" +
                        std::to_string(enduranceManualVendorPauses) +
                        " unexpectedIdleEvents=" +
                        std::to_string(enduranceUnexpectedIdleEvents));
                }

                // =====================================
                // Periodic status
                // =====================================

                if ((tick % 20) == 0)
                {
                    Debug::Logger::Info(
                        "WorldState: "
                        "hp=" +
                        std::to_string(
                            world.player.health
                        ) +
                        "/" +
                        std::to_string(
                            world.player.maxHealth
                        ) +
                        " rage=" +
                        Float(
                            world.player.power
                        ) +
                        "/" +
                        Float(
                            world.player.maxPower
                        ) +
                        " rotation=" +
                        Float(
                            world.player.rotation
                        ) +
                        " target=" +
                        Hex64(
                            world.player.targetGuid
                        ) +
                        " units=" +
                        std::to_string(
                            world.units.size()
                        ) +
                        " pos=(" +
                        Float(
                            world.player.x
                        ) +
                        "," +
                        Float(
                            world.player.y
                        ) +
                        "," +
                        Float(
                            world.player.z
                        ) +
                        ")"
                    );

                    if (
                        navMeshReturn.OwnsMovement() ||
                        navMeshReturn.Arrived() ||
                        navMeshReturn.Failed())
                    {
                        Debug::Logger::Info(
                            "NavMeshReturn: state=" +
                            std::string(navMeshReturn.StateName()) +
                            " pointIndex=" +
                            std::to_string(navMeshReturn.PointIndex()) +
                            "/" +
                            std::to_string(navMeshReturn.PointCount()) +
                            " commands=" +
                            std::to_string(navMeshReturn.Commands()) +
                            " replans=" +
                            std::to_string(navMeshReturn.Replans()) +
                            " plannedLength=" +
                            Float(navMeshReturn.PlannedPathLength())
                        );
                    }

                    if (TemporaryGrindModeEnabled)
                    {
                        const auto& grindBags = grindMode.Bags();
                        const auto& maintenance = grindMode.Maintenance();
                        const auto& xp = experienceTracker.Current();

                        Debug::Logger::Info(
                            std::string("Grind14G2: state=") +
                            grindMode.StateName() +
                            " normalModeUpdate=" +
                            (deathRecoveryOwnedTick ? "no" : "yes") +
                            " deathOwner=" +
                            (deathShouldOwn ? "yes" : "no") +
                            " grayMigration=" +
                            (grindMode.GrayMigrationActive() ? "yes" : "no") +
                            " grayBoundary=" +
                            std::to_string(grindMode.GrayLevel(world.player.level)) +
                            " localSuitable=" +
                            std::to_string(grindMode.LocalSuitableCount()) +
                            " localGray=" +
                            std::to_string(grindMode.LocalGrayCount()) +
                            " localTooHigh=" +
                            std::to_string(grindMode.LocalTooHighCount()) +
                            " sector=" +
                            std::to_string(grindMode.ActiveSectorIndex()) +
                            " roam=" +
                            std::to_string(grindMode.RoamArrivals()) + "/" +
                            std::to_string(grindMode.RoamAttempts()) +
                            " migrationAdvances=" +
                            std::to_string(grindMode.MigrationRegionAdvances()) +
                            " bags=" +
                            (grindBags.valid
                                ? (std::to_string(grindBags.usedSlots) +
                                   "/" + std::to_string(grindBags.totalSlots) +
                                   " free=" + std::to_string(grindBags.freeSlots))
                                : std::string("unknown")) +
                            " vendorState=" +
                            grindMode.Vendor().StateName() +
                            " vendorTrips=" +
                            std::to_string(grindMode.VendorTrips()) +
                            " startupVendorGrace=" +
                            std::string(grindMode.StartupVendorGraceActive(tick) ? "yes" : "no") +
                            " startupVendorGraceTicks=" +
                            std::to_string(grindMode.StartupVendorGraceRemainingTicks(tick)) +
                            " startupVendorSuppressions=" +
                            std::to_string(grindMode.StartupVendorGraceSuppressions()) +
                            " dangerDeaths=" +
                            std::to_string(grindMode.DangerDeaths()) +
                            " dangerHotspots=" +
                            std::to_string(grindMode.DangerHotspots()) +
                            " quarantines=" +
                            std::to_string(grindMode.ActiveDangerQuarantines(world.player.level)) +
                            " postDeathEscape=" +
                            (grindMode.PostDeathEscapePending() ? "yes" : "no") +
                            " maintenance=" +
                            (maintenance.valid
                                ? (std::string("dur=") +
                                   std::to_string(maintenance.minimumDurabilityPercent) +
                                   "% food=" + std::to_string(maintenance.foodCount) +
                                   " drink=" + std::to_string(maintenance.drinkCount) +
                                   " money=" + std::to_string(maintenance.money))
                                : std::string("unknown"))
                        );

                        if (xp.valid)
                        {
                            const std::string rate = xp.rateReady
                                ? ExperienceTracker::FormatRate(xp.xpPerMinute)
                                : std::string("calculating");
                            const std::string eta =
                                xp.level >= 60 || xp.nextLevelXp == 0
                                    ? std::string("level-cap")
                                    : (xp.rateReady
                                        ? ExperienceTracker::FormatEta(xp.etaMinutes)
                                        : std::string("calculating"));

                            Debug::Logger::Info(
                                "XP14G2: level=" + std::to_string(xp.level) +
                                " xp=" + std::to_string(xp.currentXp) + "/" +
                                std::to_string(xp.nextLevelXp) +
                                " remaining=" + std::to_string(xp.remainingXp) +
                                " xp/min=" + rate +
                                " timeLeft=" + eta +
                                " sessionGain=" +
                                std::to_string(xp.sessionXpGained));
                        }
                    }

                    if (
                        !TemporaryGrindModeEnabled &&
                        questPlannerRuntime.
                            OwnsControl())
                    {
                        Debug::Logger::Info(
                            "QuestPlanner11B: state=" +
                            std::string(
                                questPlannerRuntime.
                                    StateName()
                            )
                        );
                    }

                    if (
                        vileFamiliarsTurnInStartAttempted)
                    {
                        Debug::Logger::Info(
                            "VileFamiliarsTurnIn: state=" +
                            std::string(
                                vileFamiliarsTurnIn.StateName()
                            ) +
                            " moves=" +
                            std::to_string(
                                vileFamiliarsTurnIn.MoveCommands()
                            ) +
                            " interactions=" +
                            std::to_string(
                                vileFamiliarsTurnIn.
                                    InteractionAttempts()
                            ) +
                            " dialogActions=" +
                            std::to_string(
                                vileFamiliarsTurnIn.
                                    DialogActions()
                            ) +
                            " rewardChoices=" +
                            std::to_string(
                                vileFamiliarsTurnIn.
                                    RewardChoices()
                            ) +
                            " rewardState=" +
                            vileFamiliarsTurnIn.
                                RewardStateName() +
                            " selectedItem=" +
                            std::to_string(
                                vileFamiliarsTurnIn.
                                    SelectedRewardItemId()
                            ) +
                            " autoEquip=" +
                            std::string(
                                vileFamiliarsTurnIn.
                                    RewardShouldEquip()
                                    ? "yes"
                                    : "no"
                            )
                        );
                    }

                    Debug::Logger::Info(
                        std::string("Death14G4.2: state=") +
                        deathRecovery.StateName() +
                        " recoveries=" +
                        std::to_string(deathRecovery.Recoveries()) +
                        " releaseAttempts=" +
                        std::to_string(deathRecovery.ReleaseAttempts()) +
                        " retrieveAttempts=" +
                        std::to_string(deathRecovery.RetrieveAttempts()) +
                        " routeStarts=" +
                        std::to_string(deathRecovery.RouteStarts()) +
                        " precisionRoutes=" +
                        std::to_string(deathRecovery.PrecisionRouteStarts()) +
                        " deathStalls=" +
                        std::to_string(deathRecovery.DeadStateStalls()) +
                        " reclaimPrecisionRecoveries=" +
                        std::to_string(deathRecovery.ReclaimPrecisionRecoveries()) +
                        " deathProgressAgeTicks=" +
                        std::to_string(deathRecovery.DeathProgressAgeTicks(tick))
                    );

                    Debug::Logger::Info(
                        std::string("Autonomy14G4: activity=") +
                        autonomySupervisor.ActivityName() +
                        " progressAgeTicks=" +
                        std::to_string(autonomySupervisor.ProgressAgeTicks(tick)) +
                        " softEvents=" +
                        std::to_string(autonomySupervisor.SoftEvents()) +
                        " hardEvents=" +
                        std::to_string(autonomySupervisor.HardEvents()) +
                        " combatRecoveries=" +
                        std::to_string(combat.AutonomyCombatRecoveries()) +
                        " targetsAbandoned=" +
                        std::to_string(combat.AutonomyTargetsAbandoned()) +
                        " movementRecoveries=" +
                        std::to_string(grindMode.AutonomyMovementRecoveries()) +
                        " runtimeAgeTicks=" +
                        std::to_string(runtimeRobustness.ProgressAgeTicks(tick)) +
                        " runtimeRecoveries=" +
                        std::to_string(runtimeRobustness.RecoveryEvents()) +
                        " runtimeStrategic=" +
                        std::to_string(runtimeRobustness.StrategicEvents()) +
                        " runtimeIdleDeadlocks=" +
                        std::to_string(runtimeRobustness.IdleDeadlockEvents()) +
                        " acquisitionEpoch=" +
                        std::string(runtimeRobustness.AcquisitionEpochActive() ? "yes" : "no") +
                        " acquisitionAgeTicks=" +
                        std::to_string(runtimeRobustness.AcquisitionEpochAgeTicks(tick)) +
                        " recoveryWakeEvents=" +
                        std::to_string(combat.Recovery().LivenessWakeEvents()) +
                        " recoveryNoSupplyStall=" +
                        std::string(combat.Recovery().IsNoSupplyHealthStalled() ? "yes" : "no") +
                        " recoveryStallEvents=" +
                        std::to_string(combat.Recovery().NoSupplyStallEvents()) +
                        " recoveryStallPulses=" +
                        std::to_string(combat.Recovery().NoSupplyPulseEvents()) +
                        " antiAfkIdle=" +
                        std::string(antiAfkSafeguard.SafeIdleActive() ? "yes" : "no") +
                        " antiAfkAgeTicks=" +
                        std::to_string(antiAfkSafeguard.SafeIdleAgeTicks(tick)) +
                        " antiAfkRequests=" +
                        std::to_string(antiAfkSafeguard.Requests()) +
                        " antiAfkActions=" +
                        std::to_string(grindMode.AntiAfkActions()) +
                        " antiAfkNav=" +
                        std::to_string(grindMode.AntiAfkNavigationActions()) +
                        " antiAfkAcquire=" +
                        std::to_string(grindMode.AntiAfkAcquisitionActions()) +
                        " antiAfkFallback=" +
                        std::to_string(grindMode.AntiAfkFallbackPulses()) +
                        " outcomeAgeTicks=" +
                        std::to_string(runtimeRobustness.OutcomeAgeTicks(tick)) +
                        " runtimeEscalations=" +
                        std::to_string(runtimeRobustness.EscalatedEvents()) +
                        " combatRuntimeResets=" +
                        std::to_string(combat.RuntimeSupervisorResets()) +
                        " grindRuntimeResets=" +
                        std::to_string(grindMode.RuntimeSupervisorResets())
                    );

                    Debug::Logger::Info(
                        std::string(
                            "CombatLoop: state="
                        ) +
                        combat.StateName() +
                        " locked=" +
                        Hex64(
                            combat.LockedGuid()
                        ) +
                        " kills=" +
                        std::to_string(
                            combat.Kills()
                        ) +
                        " targets=" +
                        std::to_string(
                            combat.TargetsStarted()
                        ) +
                        " facingCommands=" +
                        std::to_string(
                            combat.FacingCommands()
                        ) +
                        " attackCommands=" +
                        std::to_string(
                            combat.AttackCommands()
                        ) +
                        " chargeCommands=" +
                        std::to_string(
                            combat.ChargeCommands()
                        ) +
                        " chargeMoveObserved=" +
                        std::to_string(
                            combat.ChargeMovementObservations()
                        ) +
                        " rendCommands=" +
                        std::to_string(
                            combat.RendCommands()
                        ) +
                        " thunderClapCommands=" +
                        std::to_string(
                            combat.ThunderClapCommands()
                        ) +
                        " overpowerProbes=" +
                        std::to_string(
                            combat.OverpowerProbes()
                        ) +
                        " overpowerRageSpend=" +
                        std::to_string(
                            combat.OverpowerRageSpendObservations()
                        ) +
                        " bloodrageProbes=" +
                        std::to_string(
                            combat.BloodrageProbes()
                        ) +
                        " hamstringCommands=" +
                        std::to_string(
                            combat.HamstringCommands()
                        ) +
                        " battleShoutCommands=" +
                        std::to_string(
                            combat.BattleShoutCommands()
                        ) +
                        " battleShoutRageSpend=" +
                        std::to_string(
                            combat.BattleShoutRageSpendObservations()
                        ) +
                        " heroicStrikeCommands=" +
                        std::to_string(
                            combat.HeroicStrikeCommands()
                        ) +
                        " hsPostDamage=" +
                        std::to_string(
                            combat.HeroicStrikePostQueueDamageEvents()
                        ) +
                        " hsRageSpend=" +
                        std::to_string(
                            combat.HeroicStrikeRageSpendObservations()
                        ) +
                        " lootOk=" +
                        std::to_string(
                            combat.LootsSucceeded()
                        ) +
                        " lootSkipped=" +
                        std::to_string(
                            combat.LootsSkipped()
                        ) +
                        " lootFailed=" +
                        std::to_string(
                            combat.LootsFailed()
                        ) +
                        " recoveryEntries=" +
                        std::to_string(
                            combat.RecoveryEntries()
                        ) +
                        " recoveryDone=" +
                        std::to_string(
                            combat.RecoveryCompletions()
                        ) +
                        " recoveryAggroPreempts=" +
                        std::to_string(
                            combat.RecoveryAggressorPreemptions()
                        ) +
                        " lowHpLatchRepairs=" +
                        std::to_string(
                            combat.LowHealthFinisherLatchRepairs()
                        ) +
                        " lowHpHardStallRecoveries=" +
                        std::to_string(
                            combat.LowHealthFinisherHardStallRecoveries()
                        ) +
                        " emergencyEvents=" +
                        std::to_string(
                            combat.EmergencyHealthEvents()
                        ) +
                        " blacklisted=" +
                        std::to_string(
                            combat.BlacklistedTargetCount()
                        )
                    );

                    if (
                        combat.State() ==
                            CombatState::Looting)
                    {
                        Debug::Logger::Info(
                            std::string(
                                "LootLoop: state="
                            ) +
                            combat.Loot().StateName() +
                            " corpse=" +
                            Hex64(
                                combat.Loot().CorpseGuid()
                            ) +
                            " moveCommands=" +
                            std::to_string(
                                combat.Loot().MoveCommands()
                            ) +
                            " openAttempts=" +
                            std::to_string(
                                combat.Loot().OpenAttempts()
                            ) +
                            " lootAttempts=" +
                            std::to_string(
                                combat.Loot().LootAttempts()
                            )
                        );
                    }

                    if (
                        combat.State() ==
                            CombatState::Recovering)
                    {
                        Debug::Logger::Info(
                            std::string(
                                "RecoveryLoop: state="
                            ) +
                            combat.Recovery().StateName() +
                            " hpPercent=" +
                            Float(
                                RecoveryController::
                                    HealthPercent(
                                        world.player
                                    )
                            ) +
                            " enterBelow=" +
                            Float(
                                RecoveryController::
                                    EnterThresholdPercent()
                            ) +
                            " resumeAt=" +
                            Float(
                                RecoveryController::
                                    ExitThresholdPercent()
                            )
                        );
                    }

                    Debug::Logger::Info(
                        std::string(
                            "QuestPolicy: ready="
                        ) +
                        (
                            combat.QuestPolicyReady()
                                ? "yes"
                                : "no"
                        ) +
                        " desiredEntry=" +
                        std::to_string(
                            combat.DesiredQuestEntry()
                        ) +
                        " target=" +
                        combat.DesiredQuestTargetName()
                    );

                    if (
                        std::string(
                            combat.StateName()
                        ) == "QuestTravel")
                    {
                        Debug::Logger::Info(
                            std::string(
                                "QuestTravel: state="
                            ) +
                            combat.QuestTravel().
                                StateName() +
                            " waypointIndex=" +
                            std::to_string(
                                combat.QuestTravel().
                                    WaypointIndex()
                            ) +
                            " commands=" +
                            std::to_string(
                                combat.QuestTravel().
                                    Commands()
                            ) +
                            " reissues=" +
                            std::to_string(
                                combat.QuestTravel().
                                    Reissues()
                            ) +
                            " detours=" +
                            std::to_string(
                                combat.QuestTravel().
                                    Detours()
                            ) +
                            " localAttempt=" +
                            std::to_string(
                                combat.QuestTravel().
                                    LocalDetourAttempt()
                            ) +
                            " totalDetours=" +
                            std::to_string(
                                combat.QuestTravel().
                                    TotalDetours()
                            ) +
                            " detourActive=" +
                            std::string(
                                combat.QuestTravel().
                                    DetourActive()
                                        ? "yes"
                                        : "no"
                            )
                        );
                    }

                    if (
                        std::string(
                            combat.StateName()
                        ) == "QuestReturn")
                    {
                        Debug::Logger::Info(
                            std::string(
                                "QuestReturn: state="
                            ) +
                            combat.QuestReturn().
                                StateName() +
                            " waypointIndex=" +
                            std::to_string(
                                combat.QuestReturn().
                                    WaypointIndex()
                            ) +
                            " commands=" +
                            std::to_string(
                                combat.QuestReturn().
                                    Commands()
                            ) +
                            " reissues=" +
                            std::to_string(
                                combat.QuestReturn().
                                    Reissues()
                            ) +
                            " detours=" +
                            std::to_string(
                                combat.QuestReturn().
                                    Detours()
                            ) +
                            " localAttempt=" +
                            std::to_string(
                                combat.QuestReturn().
                                    LocalDetourAttempt()
                            ) +
                            " totalDetours=" +
                            std::to_string(
                                combat.QuestReturn().
                                    TotalDetours()
                            ) +
                            " detourActive=" +
                            std::string(
                                combat.QuestReturn().
                                    DetourActive()
                                        ? "yes"
                                        : "no"
                            )
                        );
                    }

                    if (
                        std::string(
                            combat.StateName()
                        ) == "QuestTurnIn")
                    {
                        Debug::Logger::Info(
                            std::string(
                                "QuestTurnIn: state="
                            ) +
                            combat.QuestTurnIn().
                                StateName() +
                            " interactions=" +
                            std::to_string(
                                combat.QuestTurnIn().
                                    InteractionAttempts()
                            ) +
                            " dialogActions=" +
                            std::to_string(
                                combat.QuestTurnIn().
                                    DialogActions()
                            ) +
                            " rewardChoices=" +
                            std::to_string(
                                combat.QuestTurnIn().
                                    RewardChoices()
                            )
                        );
                    }

                    if (
                        std::string(
                            combat.StateName()
                        ) == "QuestObjectiveTravel")
                    {
                        Debug::Logger::Info(
                            std::string(
                                "QuestObjectiveTravel: state="
                            ) +
                            combat.QuestObjectiveTravel().
                                StateName() +
                            " waypointIndex=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    WaypointIndex()
                            ) +
                            " commands=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    Commands()
                            ) +
                            " detours=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    Detours()
                            ) +
                            " patrolIndex=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    PatrolIndex()
                            ) +
                            " patrolCommands=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    PatrolCommands()
                            ) +
                            " patrolDetours=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    PatrolDetours()
                            ) +
                            " patrolSkips=" +
                            std::to_string(
                                combat.QuestObjectiveTravel().
                                    PatrolHotspotSkips()
                            ) +
                            " patrolDetourActive=" +
                            std::string(
                                combat.QuestObjectiveTravel().
                                    PatrolDetourActive()
                                        ? "yes"
                                        : "no"
                            )
                        );
                    }

                    if (
                        std::string(
                            combat.StateName()
                        ) == "QuestPickup")
                    {
                        Debug::Logger::Info(
                            std::string(
                                "QuestPickup: state="
                            ) +
                            combat.QuestPickup().
                                StateName() +
                            " waypointIndex=" +
                            std::to_string(
                                combat.QuestPickup().
                                    WaypointIndex()
                            ) +
                            " moveCommands=" +
                            std::to_string(
                                combat.QuestPickup().
                                    MoveCommands()
                            ) +
                            " interactions=" +
                            std::to_string(
                                combat.QuestPickup().
                                    InteractionAttempts()
                            ) +
                            " dialogActions=" +
                            std::to_string(
                                combat.QuestPickup().
                                    DialogActions()
                            ) +
                            " notFound=" +
                            std::to_string(
                                combat.QuestPickup().
                                    NotFoundResults()
                            )
                        );
                    }

                    if (
                        combat.LockedGuid() != 0)
                    {
                        const auto* locked =
                            TargetSelector::
                                FindByGuid(
                                    world,
                                    combat.LockedGuid()
                                );

                        if (
                            locked != nullptr)
                        {
                            Debug::Logger::Info(
                                "CombatLoop target: "
                                "entry=" +
                                std::to_string(
                                    locked->entryId
                                ) +
                                " hp=" +
                                std::to_string(
                                    locked->health
                                ) +
                                "/" +
                                std::to_string(
                                    locked->maxHealth
                                ) +
                                " dist=" +
                                Float(
                                    locked->distance
                                )
                            );
                        }
                    }
                }

                if (combat.Failed())
                {
                    sessionExit.reason = "combat_controller_failed";
                    Debug::Logger::Info(
                        "WorldMonitor: "
                        "CombatController entered "
                        "Failed state."
                    );

                    return;
                }

                // Lowest priority, after every gameplay owner and watchdog.
                // Both workloads use the same observation/action controller.
                AfkSafety afkSafety;
                afkSafety.death=deathRecoveryOwnedTick || world.player.health<=1;
                const auto afkFault=AfkRuntimeFaultPolicy::Assess({
                    combat.Failed(),
                    TemporaryGrindModeEnabled && grindMode.Failed(),
                    runtimeRobustness.RecoveriesWithoutProgress()});
                afkSafety.fault=afkFault.terminal;
                afkSafety.faultReason=afkFault.reason;
                afkSafety.combat=combat.LockedGuid()!=0 ||
                    (combat.State()!=CombatState::Idle && combat.State()!=CombatState::AcquiringTarget);
                for (const auto& unit : world.units)
                    if (unit.valid && unit.health>0 && world.activePlayerGuid &&
                        unit.targetGuid==world.activePlayerGuid)
                        afkSafety.combat=true;
                afkSafety.recovery=combat.Recovery().IsActive();
                if (TemporaryGrindModeEnabled)
                {
                    afkSafety.recovery=afkSafety.recovery || grindMode.FirstAidActive();
                    // A transaction owns input; a stopped maintenance wait does
                    // not. Native stationary/UI guards still qualify every pulse.
                    afkSafety.vendor=grindMode.State()==GrindModeState::Vendoring;
                }
                afkSafety.loot=combat.HasDeferredCorpseLootPending();
                afkSafety.navigation=navMeshReturn.OwnsMovement() ||
                    (TemporaryGrindModeEnabled && grindMode.NavigationOwnsMovement());
                afkSafety.dialog=vileFamiliarsTurnIn.IsActive();
                afkSafety.healthyIdle=TemporaryGrindModeEnabled
                    ? (grindMode.State()==GrindModeState::Grinding ||
                       grindMode.State()==GrindModeState::WaitingForManualVendor) &&
                        !grindMode.FirstAidActive()
                    : questPlannerRuntime.SafeIdleForAfk();
                // HP=1 is normal for a ghost, not a living health-recovery
                // request. All independent recovery/input owners still block.
                const auto afkLife=AfkClient5875::ReadLife(world.player);
                if (!AfkDeadGhostPolicy::DeadOrGhost(afkLife) &&
                    RecoveryController::HealthPercent(world.player)<RecoveryController::ExitThresholdPercent())
                    afkSafety.recovery=true;
                const auto afkDeathState=deathRecovery.State();
                AfkDeathGap afkDeathGap=AfkDeathGap::Unknown;
                // This runs AFTER synchronous DeathRecovery::Update. The three
                // command/confirmation states remain closed across ticks.
                switch (afkDeathState)
                {
                case DeathRecoveryState::Idle: afkDeathGap=AfkDeathGap::Idle; break;
                case DeathRecoveryState::RoutingToCorpse: afkDeathGap=AfkDeathGap::RoutingToCorpse; break;
                case DeathRecoveryState::WaitingForReclaim: afkDeathGap=AfkDeathGap::WaitingForReclaim; break;
                case DeathRecoveryState::Failed: afkDeathGap=AfkDeathGap::Failed; break;
                case DeathRecoveryState::ReleasingSpirit:
                case DeathRecoveryState::WaitingForGhost:
                case DeathRecoveryState::WaitingForAlive: afkDeathGap=AfkDeathGap::CommandInFlight; break;
                default: break;
                }
                sharedAfk.Update(world.player,world.activePlayerGuid,afkSafety,nowMs,
                    TemporaryGrindModeEnabled ? "Grinding" : "Questing",
                    TemporaryGrindModeEnabled &&
                        (grindMode.State()==GrindModeState::Grinding ||
                         grindMode.State()==GrindModeState::ApproachingTarget ||
                         grindMode.State()==GrindModeState::Roaming),
                    afkDeathGap,deathRecovery.StateName(),
                    [&deathRecovery,afkDeathState] { return deathRecovery.State()==afkDeathState; });
                const auto afkStatus=sharedAfk.Snapshot();
                if (afkStatus.known && afkStatus.sourceThresholdMs &&
                    afkStatus.inputAgeMs>=afkStatus.sourceThresholdMs-
                        afkStatus.sourceThresholdMs/5 &&
                    (afkStatus.status==AfkStatus::Blocked ||
                     afkStatus.status==AfkStatus::ApproachingThreshold))
                {
                    const std::string decisionKey=std::to_string(static_cast<int>(afkStatus.status))+":"+afkStatus.reason;
                    if (lastAfkSafetyDecision!=decisionKey)
                        Debug::Logger::Info(std::string("AFK SAFETY ")+
                            (afkStatus.status==AfkStatus::Blocked ? "BLOCK" : "ELIGIBLE")+
                            " reason="+afkStatus.reason+
                            " combatFailed="+(combat.Failed() ? "yes" : "no")+
                            " grindFailed="+(TemporaryGrindModeEnabled && grindMode.Failed() ? "yes" : "no")+
                            " recoveriesWithoutProgress="+std::to_string(afkFault.recoveryDebt)+
                            " navigationPlanning="+(TemporaryGrindModeEnabled && grindMode.NavigationInitializationPending() ? "yes" : "no")+
                            " combatState="+combat.StateName()+
                            " grindState="+grindMode.StateName()+
                            " worldValid="+(world.valid && world.player.valid ? "yes" : "no"));
                    lastAfkSafetyDecision=decisionKey;
                }
                else
                    lastAfkSafetyDecision.clear();

                ++tick;

                Sleep(
                    PollIntervalMs
                );
            }
        }
    };
}
