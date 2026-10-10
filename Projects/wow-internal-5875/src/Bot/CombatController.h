#pragma once

#include "CombatSelectionTimeoutPolicy.h"
#include "MaintenanceCombatHandoffPolicy.h"

#include "AutoAttackController.h"
#include "CombatLivenessPolicy.h"
#include "CombatBootstrapVerificationPolicy.h"
#include "CombatOwnershipDeadlinePolicy.h"
#include "CombatTerminalPolicy.h"
#include "CombatDefensiveContainmentPolicy.h"
#include "ChaseController.h"
#include "CombatFacingPolicy.h"
#include "CombatInitiationPolicy.h"
#include "CombatTargetConsistencyPolicy.h"
#include "CombatPositioningPolicy.h"
#include "FacingController.h"
#include "GrindTargetPolicy.h"
#include "GrindTargetProbe.h"
#include "LootController.h"
#include "QuestStateReader.h"
#include "QuestPickupController.h"
#include "QuestObjectiveTravelController.h"
#include "QuestTravelController.h"
#include "QuestReturnController.h"
#include "QuestTurnInController.h"
#include "RecoveryController.h"
#include "PullSafetyCandidateTelemetry.h"
#include "SafeTargetSelector.h"
#include "TargetController.h"
#include "TargetSelector.h"
#include "WarriorRotationController.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Navigation/DetourNavigationProvider.h"
#include "../Navigation/GenericNavMeshPathFollower.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Bot
{
    enum class CombatState
    {
        Idle,
        AcquiringTarget,
        WaitingForTargetSelection,
        QuestTravel,
        QuestReturn,
        QuestTurnIn,
        QuestPickup,
        QuestObjectiveTravel,
        WarriorChargeFacing,
        WarriorOpening,
        Chasing,
        Fighting,
        Looting,
        PostKillDelay,
        Recovering,
        Failed,
        DefensiveContainment
    };

    class CombatController
    {
    private:
        static constexpr float SelectorMaxDistance =
            60.0f;

        /*
         * Phase 14G.1.1:
         * temporary grind acquisition is deliberately narrower
         * than generic quest combat so a newly locked Warrior
         * target normally starts inside the 8-25 yd Charge band.
         * Distant mobs remain owned by GrindModeController's
         * NavMesh staging leg until they enter this radius.
         */
        static constexpr float TemporaryGrindAcquisitionDistance =
            24.0f;

        static constexpr std::uint64_t TargetRetryTicks =
            8;

        /*
         * Active combat target restoration must be much faster than initial
         * acquisition retries. UI selection is read from the native client
         * selection cache, NOT UNIT_FIELD_TARGET (the server attack victim).
         */
        static constexpr std::uint64_t ActiveTargetRestoreCooldownTicks =
            1;

        // Phase 14G.5.2: reconcile the controller latch with the real Attack
        // action periodically while stationary in melee. 4 ticks ~= 1 sec.
        static constexpr std::uint64_t AutoAttackProbeCooldownTicks =
            4;

        static constexpr std::uint32_t TargetMismatchGraceSnapshots =
            2;

        static constexpr float FacingTolerance =
            CombatFacingPolicy::AbilityToleranceRadians;

        static constexpr std::uint64_t FacingCooldownTicks =
            CombatFacingPolicy::CorrectionCooldownTicks;

        static constexpr float CombatSeparationStepDistance =
            CombatPositioningPolicy::SeparationStepDistance;

        static constexpr std::uint64_t CombatSeparationMaximumTicks =
            CombatPositioningPolicy::SeparationMaximumTicks;

        static constexpr std::uint64_t CombatSeparationCooldownTicks =
            CombatPositioningPolicy::SeparationCooldownTicks;

        /*
         * Give a Charge opener one second to execute before
         * the verified CTM chase is allowed to take over.
         *
         * If Charge is not trained or the cast is rejected
         * by the client, this simply becomes a short opener
         * delay and then normal chase resumes.
         */
        static constexpr std::uint64_t WarriorOpenerWaitTicks =
            4;

        /*
         * Phase 14G.5.2.2:
         * A target that enters the Charge band while our current
         * snapshot is not yet aligned must not immediately fall
         * through to CTM chase. Hold the stale approach destination,
         * acquire facing on fresh snapshots, and then cast Charge.
         *
         * 6 ticks * 250 ms ~= 1.5 seconds. The correction command
         * budget is separately bounded so a pathological target
         * cannot cause an infinite facing loop.
         */
        static constexpr std::uint64_t WarriorChargeFacingMaximumTicks =
            6;

        static constexpr std::uint32_t WarriorChargeFacingMaximumCommands =
            4;

        /*
         * Phase 14G.1.4:
         * once Charge has actually delivered the Warrior into
         * melee range, do not sit in the opener wait state.
         * Autoattack/rotation are handed off immediately.
         */
        static constexpr float PostChargeImmediateMeleeDistance =
            5.5f;

        /*
         * Remember a mob for a short time after it was directly
         * observed targeting the player. UNIT_FIELD_TARGET can
         * transiently clear during combat transitions; without
         * this memory a multi-pull can be mistaken for combat
         * having ended between two snapshots.
         *
         * 16 ticks * 250 ms ~= 4 seconds.
         */
        static constexpr std::uint64_t AggressorMemoryTicks =
            16;

        // Phase 14M.0: only local pack pressure should unlock Thunder Clap.
        static constexpr float ThunderClapAggressorRadius =
            8.0f;

        /*
         * 4 ticks * 250 ms = about 1 second after
         * looting finishes before we acquire the next
         * target.
         */
        static constexpr std::uint64_t PostKillDelayTicks =
            4;

        static constexpr int MaximumConsecutiveTargetFailures =
            5;

        // Phase 14G.4.1: global liveness recovery gets a bounded per-target
        // budget. Non-aggressors are abandoned after repeated hard stalls.
        static constexpr std::uint32_t MaximumAutonomyCombatRecoveriesPerTarget =
            3;

        /*
         * Phase 14M.0.2 low-HP finisher ownership.
         *
         * The runtime log captured a low-health target at 5% HP and
         * 2.26 yd while Fighting stayed alive but the autoattack latch had
         * already been paused by the facing guard. At this point repeated
         * chase/separation recovery is more dangerous than retaining melee
         * ownership. Keep the threshold deliberately narrow.
         */
        static constexpr float LowHealthFinisherTargetPercent =
            15.0f;

        static constexpr float LowHealthFinisherMeleeDistance =
            5.0f;

        static constexpr std::uint64_t LowHealthFinisherRepairCooldownTicks =
            4;

        /*
         * Survival policy.
         *
         * No new mob is acquired below 70% HP.
         * Recovery finishes at 95% HP.
         *
         * RecoveryController owns the threshold values;
         * the combat controller owns when recovery is
         * allowed to interrupt the target loop.
         */
        static constexpr float EmergencyHealthPercent =
            20.0f;

        /*
         * 480 ticks * 250 ms = 120 seconds.
         *
         * A mob that causes ChaseController to fail is
         * temporarily ignored so the bot cannot loop on
         * the same unreachable target forever.
         */
        static constexpr std::uint64_t BlacklistDurationTicks =
            480;

        CombatState state_ =
            CombatState::Idle;

        ChaseController chase_{};

        LootController loot_{};

        RecoveryController recovery_{};

        QuestTravelController questTravel_{};

        QuestReturnController questReturn_{};

        QuestTurnInController questTurnIn_{};

        QuestPickupController questPickup_{};

        QuestObjectiveTravelController questObjectiveTravel_{};

        WarriorRotationController warrior_{};

        std::uint64_t lockedGuid_ =
            0;

        std::uint64_t pendingTargetGuid_ =
            0;
        int selectionAttemptsForGuid_ = 0;

        std::uint64_t lastTargetCommandTick_ =
            0;

        std::uint64_t lastFacingCommandTick_ =
            0;

        std::uint32_t facingStableSnapshots_ =
            0;

        bool facingGuardHolding_ =
            false;

        std::uint32_t selectedTargetMismatchSnapshots_ =
            0;

        std::uint32_t facingHoldSnapshots_ =
            0;

        bool combatSeparationActive_ =
            false;

        std::uint64_t combatSeparationUntilTick_ =
            0;

        std::uint64_t lastCombatSeparationTick_ =
            0;

        std::uint32_t combatSeparationAttempts_ =
            0;

        std::uint64_t nextAcquireTick_ =
            0;

        bool pullHealthGateLogged_ = false;
        bool pullDefensiveRecoveryLogged_ = false;
        bool pullSelectionLogged_ = false;
        PullSafetyDecision lastPullDecision_ =
            PullSafetyDecision::NoEligibleCandidate;
        std::uint64_t lastPullEvaluationTick_ = 0;
        std::uint32_t lastPullEvaluationEntry_ = 0;
        std::uint64_t lastPullSelectedGuid_ = 0;
        std::uint64_t nextPullSafetyLogTick_ = 0;

        std::uint64_t warriorOpenerUntilTick_ =
            0;

        std::uint64_t warriorChargeFacingUntilTick_ =
            0;

        std::uint32_t warriorChargeFacingStableSnapshots_ =
            0;

        std::uint32_t warriorChargeFacingCommandsForTarget_ =
            0;

        bool attackStarted_ =
            false;

        CombatLivenessPolicy meleeLiveness_{};
        CombatTerminalPolicy meleeTerminal_{};
        CombatDefensiveContainmentPolicy defensiveContainment_{};
        bool livingDefenseOnly_ = false;
        std::uint64_t livingDefenseGuid_ = 0;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> defensiveRoute_{};
        bool defensiveContainmentUsed_=false;
        bool postContainmentRelease_=false;
        std::uint64_t episodeAttackGuid_=0;
        bool episodeAttackOwnershipEstablished_=false;
        bool defensiveRouteStartFailed_=false;
        const char* defensiveRouteFailure_="none";
        float defensiveLastDistance_=0.0f;
        float defensiveLastX_=0.0f, defensiveLastY_=0.0f;
        std::uint64_t defensiveStopIssuedAtMs_=0;
        bool meleeTerminalPending_=false;
        const char* lastMeleeTerminalReason_=nullptr;
        const char* terminalCause_="offensive_no_progress";
        bool chaseTerminalPending_=false;
        std::uint64_t ownerTargetFailureGuid_=0;
        std::uint32_t ownerTargetFailureEntry_=0;
        const char* ownerTargetFailureReason_=nullptr;
        CombatLivenessDecision meleeDecision_{};
        AutoAttackController::CombatActionEvidence meleeActionEvidence_{};
        CombatStallClass lastMeleeClassification_=CombatStallClass::UnknownOrStale;
        bool lastMeleeTargetFresh_=false;
        bool lastMeleeSelectionKnown_=false;
        std::uint64_t lastMeleeSelectedGuid_=0;
        std::uint64_t lastInitGateGuid_=0;
        CombatState lastInitGateState_=CombatState::Idle;
        CombatInitiationReason lastInitGateReason_=CombatInitiationReason::Ready;
        bool lastInitGateOffenseAllowed_=false;
        std::string lastActionProbeReason_;
        std::string lastBootstrapProbeReason_;
        std::string lastAttackReadback_;
        CombatBootstrapVerificationPolicy bootstrapVerification_;
        CombatOwnershipDeadlinePolicy ownershipDeadline_;
        std::uint32_t lastMeleeAttackPeriodMs_=0;

        static std::uint64_t ClientSelectedGuid(const Objects::WorldState& world)
        {
            std::uint64_t guid=0;
            return CombatClientEvidence5875::Selection(world,guid) ? guid : 0;
        }

        void LogCombatExecution(const Objects::UnitState& target, const CombatLivenessSample& s,
            const CombatClientEvidence5875::ExecutionEvidence& e) const
        {
            Debug::Logger::Info("COMBAT EXECUTION EVIDENCE targetGuid="+Hex64(lockedGuid_)+
                " targetObject="+Hex64(target.address)+" known="+(e.known ? "yes" : "no")+
                " targetHp="+std::to_string(e.targetHp)+" targetMaxHp="+std::to_string(e.targetMaxHp)+
                " playerHp="+std::to_string(e.playerHp)+" selectedGuid="+Hex64(e.selected)+
                " serverVictimGuid="+Hex64(e.playerVictim)+" targetVictimGuid="+Hex64(e.targetVictim)+
                " playerCombat="+(e.known ? (e.PlayerCombat() ? "yes" : "no") : "unknown")+
                " targetCombat="+(e.known ? (e.TargetCombat() ? "yes" : "no") : "unknown")+
                " autoattack="+(s.actionKnown ? (s.attackActive ? "yes" : "no") : "unknown")+
                " attackTimer=unknown attackPeriodMs="+std::to_string(e.attackPeriodMs)+
                " unitFlags="+Hex64(e.targetFlags)+" playerUnitFlags="+Hex64(e.playerFlags)+
                " dynamicFlags="+Hex64(e.dynamicFlags)+" faction="+std::to_string(e.faction)+
                " movementFlags="+Hex64(e.targetMovementFlags)+" playerMovementFlags="+Hex64(e.playerMovementFlags)+
                " playerPacified="+(e.known ? ((e.playerFlags&0x20000u) ? "yes" : "no") : "unknown")+
                " targetNonAttackable="+(e.known ? ((e.targetFlags&0x82010182u) ? "yes" : "no") : "unknown")+
                " evade=unknown range="+Float(target.distance)+" facing="+(s.facing ? "yes" : "no")+
                " castOrGcdWait="+(!meleeActionEvidence_.known ? "unknown" : s.actionWait ? "yes" : "no")+
                " actionProbe="+meleeActionEvidence_.reason+
                " attackReadback="+meleeActionEvidence_.attackReason+
                " classification="+CombatStallName(meleeDecision_.classification));
        }

        void BeginChaseTerminal(const Objects::WorldState& world,
            const Objects::UnitState& target, const char* reason)
        {
            if (!MovementController::HoldPosition(world.player))
            {
                Fail("combat_terminal_system_failure:chase_stop_rejected", false);
                return;
            }
            if (chase_.IsActive()) chase_.Stop();
            terminalCause_=reason;
            chaseTerminalPending_=true;
            meleeTerminal_.Reset();
            meleeTerminalPending_=true;
            lastMeleeTerminalReason_=nullptr;
            Debug::Logger::Info("COMBAT CHASE RECOVERY guid="+Hex64(target.guid)+
                " entry="+std::to_string(target.entryId)+
                " reason="+reason+" decision=verify_terminal_owner");
        }

        bool ResolveMeleeTerminal(const Objects::WorldState& world,
            const Objects::UnitState& target, const CombatLivenessSample& s, std::uint64_t tick)
        {
            const auto e=CombatClientEvidence5875::Execution(world,target);
            CombatTerminalSample terminal{};
            terminal.nowMs=s.nowMs; terminal.targetGuid=lockedGuid_;
            terminal.selectedGuid=e.selected; terminal.serverVictimGuid=e.playerVictim;
            terminal.playerHp=e.playerHp;
            // Grind owns this episode even when an unrelated quest status flag
            // remains active. The planner target bit is the actual exception.
            terminal.optionalGrind=grindModeActive_ && !plannerQuestTargetActive_;
            terminal.mandatoryObjective=plannerQuestTargetActive_ && !vileFamiliarsActive_ &&
                !grindModeActive_;
            terminal.known=e.known && e.aggressorsKnown && s.fresh && s.selectionKnown;
            terminal.inputSafe=s.inputSafe && !s.actionWait;
            terminal.hostileEngaged=e.PlayerCombat() || e.aggressor ||
                e.targetVictim==world.activePlayerGuid ||
                (!e.aggressorsKnown && FindBestDirectAggressor(world)!=nullptr);
            terminal.attackKnown=s.actionKnown; terminal.attackActive=s.attackActive;
            std::string selectionInputReason="not_in_release";
            if (postContainmentRelease_)
            {
                const auto selectionInput=AutoAttackController::ProbeSelectionReleaseInput();
                terminal.selectionInputSafe=selectionInput.known && selectionInput.inputSafe;
                selectionInputReason=selectionInput.reason;
                terminal.hostileEngaged=terminal.hostileEngaged || e.TargetCombat();
            }
            terminal.episodeAttackOwnershipEstablished=episodeAttackGuid_==lockedGuid_ &&
                episodeAttackOwnershipEstablished_;
            if ((terminal.hostileEngaged || !terminal.known) && !defensiveContainmentUsed_ &&
                !meleeTerminal_.Issued())
            {
                BeginDefensiveContainment(world,target,s,terminal,tick);
                return true;
            }
            const auto d=meleeTerminal_.Observe(terminal);
            if (d.reason!=lastMeleeTerminalReason_)
            {
                lastMeleeTerminalReason_=d.reason;
                if (postContainmentRelease_)
                {
                    const char* attackOwnership=
                        terminal.episodeAttackOwnershipEstablished &&
                        terminal.attackKnown && terminal.attackActive
                            ? "owned" : terminal.attackKnown && !terminal.attackActive
                                ? "not_owned" : "unknown";
                    Debug::Logger::Info("COMBAT CONTAINMENT RELEASE state=eligibility targetGuid="+
                        Hex64(lockedGuid_)+" directAggressors="+
                        (e.aggressorsKnown ? std::to_string(e.aggressorCount) : "unknown")+
                        " attackOwnership="+attackOwnership+
                        " episodeCommandIssued="+
                        (terminal.episodeAttackOwnershipEstablished ? "yes" : "no")+
                        " selectedGuid="+Hex64(terminal.selectedGuid)+
                        " selectionProbe="+selectionInputReason+
                        " decision="+(d.action==CombatTerminalAction::ClearOwnSelection ? "clear_selection" :
                            d.action==CombatTerminalAction::StopAndClearOwnTarget ? "stop_attack" :
                            d.action==CombatTerminalAction::Abandoned ||
                            d.action==CombatTerminalAction::OwnerFailure ? "release" : "wait")+
                        " reason="+d.reason);
                    Debug::Logger::Info("COMBAT CONTAINMENT RELEASE VERIFY previousTarget="+
                        Hex64(lockedGuid_)+" currentTarget="+Hex64(terminal.selectedGuid)+
                        " attackOwnership="+attackOwnership+
                        " containmentOwner=Combat result="+
                        (d.action==CombatTerminalAction::Abandoned ||
                         d.action==CombatTerminalAction::OwnerFailure ? "confirmed" :
                         d.action==CombatTerminalAction::SystemFail ? "failed" : "pending")+
                        " reason="+d.reason);
                    Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=eligibility guid="+
                        Hex64(lockedGuid_)+" selectedGuid="+Hex64(terminal.selectedGuid)+
                        " serverVictimGuid="+Hex64(terminal.serverVictimGuid)+
                        " attackOwnershipEstablished="+
                        (terminal.episodeAttackOwnershipEstablished ? "yes" : "no")+
                        " attackKnown="+(terminal.attackKnown ? "yes" : "no")+
                        " attackActive="+(terminal.attackKnown ? (terminal.attackActive ? "yes" : "no") : "unknown")+
                        " decision="+(d.action==CombatTerminalAction::ClearOwnSelection ? "clear_selection" :
                            (d.action==CombatTerminalAction::StopAndClearOwnTarget ? "stop_attack" :
                             (d.action==CombatTerminalAction::SystemFail ? "fail" : "observe")))+
                        " reason="+d.reason);
                    Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=verify guid="+
                        Hex64(lockedGuid_)+" selectedGuid="+Hex64(terminal.selectedGuid)+
                        " serverVictimGuid="+Hex64(terminal.serverVictimGuid)+
                        " playerCombat="+(e.known ? (e.PlayerCombat() ? "yes" : "no") : "unknown")+
                        " targetCombat="+(e.known ? (e.TargetCombat() ? "yes" : "no") : "unknown")+
                        " targetVictimGuid="+Hex64(e.targetVictim)+
                        " attackKnown="+(terminal.attackKnown ? "yes" : "no")+
                        " attackActive="+(terminal.attackKnown ? (terminal.attackActive ? "yes" : "no") : "unknown")+
                        " result="+(d.action==CombatTerminalAction::SystemFail ? "conflict" :
                            (d.action==CombatTerminalAction::Abandoned ||
                             d.action==CombatTerminalAction::OwnerFailure ? "confirmed" : "pending"))+
                        " reason="+d.reason);
                }
                LogCombatExecution(target,s,e);
                Debug::Logger::Info("COMBAT TERMINAL TARGET targetGuid="+Hex64(lockedGuid_)+
                    " class="+terminalCause_+" hostileStillEngaged="+
                    (terminal.known ? (terminal.hostileEngaged ? "yes" : "no") : "unknown")+
                    " repairDispatches="+std::to_string(meleeLiveness_.Repairs())+
                    " repairLimit="+std::to_string(CombatLivenessPolicy::MaximumRepairs)+
                    " hardRefreshLimit=1"+
                    " decision="+(d.action==CombatTerminalAction::SystemFail ? "system_fail" :
                        (terminal.mandatoryObjective ? "verify_owner_failure" : "verify_safe_abandon"))+
                    " reason="+d.reason);
            }
            if (d.action==CombatTerminalAction::SystemFail)
            {
                if (postContainmentRelease_)
                    Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=failed guid="+
                        Hex64(lockedGuid_)+" reason="+d.reason);
                Fail(std::string("combat_terminal_system_failure:")+d.reason,
                    terminal.known && terminal.inputSafe &&
                    CombatAttackOwnershipPolicy::MayStopOwnAttack(
                        terminal.episodeAttackOwnershipEstablished,
                        terminal.attackKnown,terminal.attackActive));
                return true;
            }
            if (d.action==CombatTerminalAction::HostileReturned)
            {
                Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=hostile_returned guid="+
                    Hex64(lockedGuid_)+" reason="+d.reason);
                postContainmentRelease_=false;
                meleeTerminal_.Reset();
                lastMeleeTerminalReason_=nullptr;
                return true; // Same bounded containment episode, never passive abandon.
            }
            if (d.action==CombatTerminalAction::StopAndClearOwnTarget)
            {
                if (postContainmentRelease_)
                    Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=command action=stop_attack_clear_selection attempt=1 guid="+
                        Hex64(lockedGuid_));
                meleeTerminal_.Dispatched(s.nowMs); // one bounded dispatch, never a kill
                const bool issued=AutoAttackController::AbandonOwnCombatTarget(world,lockedGuid_,
                    PostChargeImmediateMeleeDistance);
                if (postContainmentRelease_)
                    Debug::Logger::Info("COMBAT CONTAINMENT RELEASE state=command targetGuid="+
                        Hex64(lockedGuid_)+" decision=stop_attack result="+
                        (issued ? "dispatched" : "rejected"));
                if (!issued) { Fail("combat_terminal_system_failure:target_abandon_dispatch_rejected",false); return true; }
                if (chase_.TargetGuid()==lockedGuid_) chase_.Stop();
            }
            if (d.action==CombatTerminalAction::ClearOwnSelection)
            {
                Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=command action=clear_own_selection attempt=1 guid="+
                    Hex64(lockedGuid_));
                meleeTerminal_.Dispatched(s.nowMs,d.action);
                const bool issued=AutoAttackController::ClearOwnSelection(world,target,lockedGuid_);
                Debug::Logger::Info("COMBAT CONTAINMENT RELEASE state=command targetGuid="+
                    Hex64(lockedGuid_)+" decision=clear_selection result="+
                    (issued ? "dispatched" : "rejected"));
                if (!issued)
                {
                    Fail("combat_terminal_system_failure:selection_clear_dispatch_rejected",false);
                    return true;
                }
                if (chase_.TargetGuid()==lockedGuid_) chase_.Stop();
            }
            if (d.action==CombatTerminalAction::Abandoned)
            {
                if (postContainmentRelease_)
                    Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=complete guid="+
                        Hex64(lockedGuid_)+" result=optional_blacklist");
                const auto abandoned=lockedGuid_;
                if (chaseTerminalPending_)
                    ++autonomyTargetsAbandoned_;
                BlacklistTarget(abandoned,tick,std::string(terminalCause_)+
                    ": verified safe optional abandonment");
                ++consecutiveTargetFailures_;
                warrior_.EndTarget();
                Debug::Logger::Info("COMBAT TARGET ABANDON guid="+Hex64(abandoned)+
                    " reason="+terminalCause_+" outcome=abandoned_not_killed blacklistMs=120000"+
                    " combatStateAfter="+(consecutiveTargetFailures_>=MaximumConsecutiveTargetFailures ? "Failed" : "AcquiringTarget"));
                if (consecutiveTargetFailures_>=MaximumConsecutiveTargetFailures)
                    Fail("too many consecutive safely abandoned combat targets");
                else BeginAcquire(tick+1);
            }
            if (d.action==CombatTerminalAction::OwnerFailure)
            {
                if (postContainmentRelease_)
                    Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=complete guid="+
                        Hex64(lockedGuid_)+" result=mandatory_owner_failure");
                ownerTargetFailureGuid_=lockedGuid_;
                ownerTargetFailureEntry_=target.entryId;
                ownerTargetFailureReason_=d.reason;
                Debug::Logger::Info("COMBAT OWNER HANDOFF from=Combat to=QuestObjective guid="+
                    Hex64(lockedGuid_)+" reason="+d.reason+
                    " outcome=failed_not_killed");
                BeginAcquire(tick+1);
            }
            return true;
        }

        void BeginDefensiveContainment(const Objects::WorldState& world,
            const Objects::UnitState& target, const CombatLivenessSample& s,
            const CombatTerminalSample& terminal, std::uint64_t tick)
        {
            defensiveContainmentUsed_=true;
            postContainmentRelease_=false;
            defensiveContainment_.Begin(lockedGuid_,s.targetHp,s.nowMs);
            defensiveRouteStartFailed_=false;
            defensiveRouteFailure_="none";
            defensiveLastDistance_=target.distance;
            defensiveLastX_=world.player.x;
            defensiveLastY_=world.player.y;
            defensiveStopIssuedAtMs_=0;
            meleeTerminal_.Reset();
            lastMeleeTerminalReason_=nullptr;
            meleeLiveness_.Pause(s.nowMs); // elapsed escape time is not offense
            Debug::Logger::Info("COMBAT TERMINAL DECISION guid="+Hex64(lockedGuid_)+
                " optional="+(terminal.optionalGrind ? "yes" : "no")+
                " mandatory="+(terminal.mandatoryObjective ? "yes" : "no")+
                " aggressor="+(terminal.hostileEngaged ? "yes" : "unknown")+
                " decision=continue_defense reason=bounded_offensive_repair_exhausted");
            Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=enter guid="+
                Hex64(lockedGuid_)+" reason="+terminalCause_+
                " playerHp="+std::to_string(world.player.health)+
                " targetHp="+std::to_string(target.health)+
                " range="+Float(target.distance)+
                " repairDispatches="+std::to_string(meleeLiveness_.Repairs())+
                " repairLimit="+std::to_string(CombatLivenessPolicy::MaximumRepairs));
            if (!terminal.inputSafe || !s.inputSafe)
            {
                defensiveRouteStartFailed_=true;
                defensiveRouteFailure_="containment_input_conflict";
            }
            else
            {
                if (chase_.IsActive()) chase_.Stop();
                if (!MovementController::HoldPosition(world.player))
                {
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="input_neutralization_failed";
                }
                else if (s.actionKnown && s.attackActive)
                {
                    if (!CombatAttackOwnershipPolicy::MayStopOwnAttack(
                            episodeAttackOwnershipEstablished_,s.actionKnown,
                            s.attackActive))
                    {
                        defensiveRouteStartFailed_=true;
                        defensiveRouteFailure_="unowned_attack_active";
                    }
                    else if (!AutoAttackController::Stop())
                    {
                        defensiveRouteStartFailed_=true;
                        defensiveRouteFailure_="input_neutralization_failed";
                    }
                    else defensiveStopIssuedAtMs_=s.nowMs;
                }
                else if (!s.actionKnown && episodeAttackOwnershipEstablished_)
                {
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="owned_attack_readback_unknown";
                }
                else defensiveStopIssuedAtMs_=s.nowMs;
            }
            attackStarted_=false;
            Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=attack_neutralization guid="+
                Hex64(lockedGuid_)+" episodeCommandIssued="+
                (episodeAttackOwnershipEstablished_ ? "yes" : "no")+
                " attackKnown="+(s.actionKnown ? "yes" : "no")+
                " attackActive="+(s.actionKnown ? (s.attackActive ? "yes" : "no") : "unknown")+
                " result="+(defensiveRouteStartFailed_ ? defensiveRouteFailure_ :
                    s.actionKnown && s.attackActive ? "own_attack_stop_dispatched" :
                    "no_attack_stop_needed"));
            SetState(CombatState::DefensiveContainment);
            (void)tick;
        }

        void UpdateDefensiveContainment(const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const auto nowMs=GetTickCount64();
            if (postContainmentRelease_ && defensiveContainment_.Expired(nowMs))
            {
                Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=failed guid="+
                    Hex64(lockedGuid_)+" reason=containment_timeout");
                Fail("defensive_containment_exhausted:containment_timeout",false);
                return;
            }
            const auto* target=TargetSelector::FindByGuid(world,lockedGuid_);
            if (target && target->valid && target->health==0)
            {
                if (defensiveRoute_ && !MovementController::HoldPosition(world.player))
                { Fail("defensive_containment_stop_rejected",false); return; }
                defensiveRoute_.reset();
                FinishTarget(world,target,tick,true);
                return;
            }
            const auto evidence=target ? CombatClientEvidence5875::Execution(world,*target)
                : CombatClientEvidence5875::ExecutionEvidence{};
            const bool known=target && evidence.known && evidence.aggressorsKnown &&
                evidence.targetHp==target->health &&
                evidence.playerHp==world.player.health;
            const bool hostile= !known || evidence.PlayerCombat() || evidence.aggressor ||
                evidence.targetVictim==world.activePlayerGuid ||
                (!evidence.aggressorsKnown && FindBestDirectAggressor(world)!=nullptr);
            const auto attack=AutoAttackController::ProbeCombatAction();
            if (postContainmentRelease_)
            {
                if (!target || !known)
                {
                    // Missing fresh world evidence is not a release proof.
                    // The terminal policy's bounded observation expires if it persists.
                    CombatTerminalSample pending{};
                    pending.nowMs=nowMs; pending.targetGuid=lockedGuid_;
                    pending.playerHp=world.player.health;
                    pending.optionalGrind=grindModeActive_ && !plannerQuestTargetActive_;
                    pending.mandatoryObjective=plannerQuestTargetActive_ &&
                        !vileFamiliarsActive_ && !grindModeActive_;
                    pending.known=false;
                    const auto d=meleeTerminal_.Observe(pending);
                    if (d.action==CombatTerminalAction::SystemFail)
                    {
                        Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=failed guid="+
                            Hex64(lockedGuid_)+" reason="+d.reason);
                        Fail(std::string("combat_terminal_system_failure:")+d.reason,false);
                    }
                    return;
                }
                CombatLivenessSample terminalSample{};
                terminalSample.nowMs=nowMs; terminalSample.fresh=known;
                terminalSample.selectionKnown=known;
                terminalSample.inputSafe=attack.known && attack.inputSafe;
                terminalSample.actionWait=attack.waiting;
                terminalSample.actionKnown=attack.attack.valid && attack.attack.actionSlotFound;
                terminalSample.attackActive=attack.attack.active;
                ResolveMeleeTerminal(world,*target,terminalSample,tick);
                return;
            }
            DefensiveContainmentSample sample{};
            sample.nowMs=nowMs; sample.guid=lockedGuid_;
            sample.playerAlive=world.player.health>0;
            sample.targetValid=target && target->valid && target->health>0;
            sample.targetHp=known ? evidence.targetHp : 0;
            sample.evidenceKnown=known; sample.hostileEngaged=hostile;
            sample.reengageReady=known && evidence.selected==lockedGuid_ &&
                attack.known && attack.inputSafe && !attack.waiting &&
                attack.attack.valid && attack.attack.active &&
                target->distance<=PostChargeImmediateMeleeDistance &&
                CombatFacingPolicy::IsAbilityFacingReady(
                    FacingController::AngularDifference(world.player.rotation,
                        FacingController::CalculateFacing(world.player,*target)));
            sample.routeFailed=defensiveRouteStartFailed_ ||
                (defensiveRoute_ && defensiveRoute_->Failed());
            sample.routeArrived=defensiveRoute_ && defensiveRoute_->Arrived();
            const auto decision=defensiveContainment_.Observe(sample);
            if (decision.action==DefensiveContainmentAction::DeathHandoff)
                return; // WorldMonitor hands verified death to DeathRecovery.
            if (decision.action==DefensiveContainmentAction::VerifiedDisengagement)
            {
                if (defensiveRoute_ && !MovementController::HoldPosition(world.player))
                { Fail("defensive_containment_stop_rejected",false); return; }
                defensiveRoute_.reset();
                Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=disengaged guid="+
                    Hex64(lockedGuid_)+" reason="+decision.reason);
                postContainmentRelease_=true;
                meleeTerminal_.BeginPostContainment(sample.nowMs,lockedGuid_,evidence.playerHp);
                lastMeleeTerminalReason_=nullptr;
                Debug::Logger::Info("COMBAT POST-CONTAINMENT RELEASE state=enter guid="+
                    Hex64(lockedGuid_)+" selectedGuid="+Hex64(evidence.selected)+
                    " serverVictimGuid="+Hex64(evidence.playerVictim)+
                    " attackKnown="+(attack.attack.valid && attack.attack.actionSlotFound ? "yes" : "no")+
                    " attackActive="+(attack.attack.valid ? (attack.attack.active ? "yes" : "no") : "unknown"));
                return; // Next tick supplies fresh post-containment evidence.
            }
            if (decision.action==DefensiveContainmentAction::Reengage)
            {
                if (defensiveRoute_ && !MovementController::HoldPosition(world.player))
                { Fail("defensive_containment_stop_rejected",false); return; }
                defensiveRoute_.reset();
                meleeLiveness_.Pause(sample.nowMs);
                ownershipDeadline_.Pause(sample.nowMs); // verified containment damage
                meleeTerminalPending_=false;
                Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=reengage guid="+
                    Hex64(lockedGuid_)+" reason=verified_target_damage");
                SetState(CombatState::Fighting);
                return;
            }
            if (decision.action==DefensiveContainmentAction::Fail)
            {
                Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=failed guid="+
                    Hex64(lockedGuid_)+" reason="+decision.reason+
                    " routeReason="+defensiveRouteFailure_);
                if (defensiveRoute_) MovementController::HoldPosition(world.player);
                defensiveRoute_.reset();
                Fail(std::string("defensive_containment_exhausted:")+decision.reason,
                    attack.known && attack.inputSafe && !attack.waiting &&
                    CombatAttackOwnershipPolicy::MayStopOwnAttack(
                        episodeAttackOwnershipEstablished_,
                        attack.attack.valid && attack.attack.actionSlotFound,
                        attack.attack.active));
                return;
            }
            if (!defensiveRoute_ && !defensiveRouteStartFailed_ && hostile)
            {
                // A short deterministic away-point is only a destination for
                // the existing Detour follower. Its ordinary living filter
                // excludes water and retains all terrain/hazard validation.
                if (!grindModeActive_ || !known || !target->valid ||
                    !defensiveStopIssuedAtMs_)
                {
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="escape_position_map_or_stop_unknown";
                    return;
                }
                if (!attack.known || !attack.inputSafe || attack.waiting || !attack.attack.valid ||
                    !attack.attack.actionSlotFound || attack.attack.active)
                {
                    if (sample.nowMs-defensiveStopIssuedAtMs_ <
                        CombatLivenessPolicy::StructuralVerificationMs)
                        return;
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="attack_stop_not_confirmed";
                    return;
                }
                std::array<DefensiveThreatPosition,
                    CombatDefensiveEscapePolicy::MaximumThreats> threats{};
                unsigned threatCount=0;
                for (const auto& unit:world.units)
                {
                    if (!unit.valid || !unit.health ||
                        unit.targetGuid!=world.activePlayerGuid) continue;
                    if (threatCount==threats.size())
                    {
                        defensiveRouteStartFailed_=true;
                        defensiveRouteFailure_="escape_threat_geometry_unknown";
                        return;
                    }
                    threats[threatCount++]={unit.x,unit.y};
                }
                if (threatCount!=evidence.aggressorCount)
                {
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="escape_threat_positions_incomplete";
                    return;
                }
                DefensiveEscapePoint escape{};
                if (!CombatDefensiveEscapePolicy::AwayPoint(
                    {world.player.x,world.player.y,world.player.z},
                    threats,threatCount,escape))
                {
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="escape_threat_geometry_unknown";
                    return;
                }
                const Navigation::NavPoint destination{
                    escape.x,escape.y,escape.z};
                Navigation::GenericNavMeshStartOptions options{};
                options.allowFullMapFallback=false; // bounded local escape
                options.waterTraversal=Navigation::WaterTraversalMode::AvoidUntilQualified;
                auto route=std::make_unique<Navigation::GenericNavMeshPathFollower>();
                if (!route->Start(world.player,tick,destination,1,2.5f,
                    "combat defensive containment",false,options))
                {
                    defensiveRouteStartFailed_=true;
                    defensiveRouteFailure_="defensive_route_start_rejected";
                    return;
                }
                defensiveRoute_=std::move(route);
                Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=escape attempt=1"
                    " owner=Combat route=detour_ground_only reason=hostile_still_engaged"
                    " destination=("+Float(destination.x)+","+Float(destination.y)+","+
                    Float(destination.z)+")");
            }
            if (defensiveRoute_ && defensiveRoute_->OwnsMovement())
            {
                defensiveRoute_->Update(world.player,tick);
                if (target && std::hypot(world.player.x-defensiveLastX_,
                        world.player.y-defensiveLastY_)>=2.0f &&
                    std::isfinite(target->distance) &&
                    target->distance>=defensiveLastDistance_+2.0f)
                {
                    defensiveLastDistance_=target->distance;
                    defensiveLastX_=world.player.x;
                    defensiveLastY_=world.player.y;
                    Debug::Logger::Info("COMBAT DEFENSIVE CONTAINMENT state=escape"
                        " progress=physical_separation range="+Float(target->distance));
                }
                if (defensiveRoute_->Failed())
                    defensiveRouteFailure_=Navigation::NavigationInitTelemetryPolicy::ReasonName(
                        defensiveRoute_->LastPlanFailure());
            }
        }

        bool ObserveMeleeLiveness(const Objects::WorldState& world,
            const Objects::UnitState& target, std::uint64_t tick)
        {
            CombatLivenessSample s{};
            s.nowMs=GetTickCount64(); s.sampleTick=tick;
            s.targetGuid=target.guid; s.targetObject=target.address;
            s.targetValid=target.valid; s.alive=world.player.health>0;
            s.selectionKnown=CombatClientEvidence5875::Selection(world,s.selectedGuid);
            lastMeleeSelectionKnown_=s.selectionKnown;
            lastMeleeSelectedGuid_=s.selectedGuid;
            s.fresh=CombatClientEvidence5875::FreshHealth(world,target,
                s.targetHp,s.playerHp,s.attackPeriodMs);
            // A stale object snapshot is not a stationary live target sample.
            s.fresh=s.fresh && s.targetHp==target.health && s.playerHp==world.player.health;
            lastMeleeTargetFresh_=s.fresh;
            lastMeleeAttackPeriodMs_=s.attackPeriodMs;
            meleeActionEvidence_=AutoAttackController::ProbeCombatAction();
            // Health sampling remains authoritative even when the Attack
            // action probe is unavailable during a bounded bootstrap.
            s.inputSafe=meleeActionEvidence_.known && meleeActionEvidence_.inputSafe &&
                !combatSeparationActive_ &&
                (!chase_.TargetGuid() || chase_.TargetGuid()==lockedGuid_);
            s.actionWait=meleeActionEvidence_.waiting;
            s.melee=chase_.IsInRange() && chase_.TargetGuid()==lockedGuid_ &&
                std::isfinite(target.distance) && target.distance<=PostChargeImmediateMeleeDistance;
            s.facing=CombatFacingPolicy::IsAbilityFacingReady(
                FacingController::AngularDifference(world.player.rotation,
                    FacingController::CalculateFacing(world.player,target)));
            s.actionKnown=meleeActionEvidence_.attack.valid && meleeActionEvidence_.attack.actionSlotFound;
            s.attackActive=meleeActionEvidence_.attack.active;
            const bool ownershipExpired=ownershipDeadline_.Observe(s);
            meleeDecision_=meleeLiveness_.Observe(s);
            const auto bootstrapResult=bootstrapVerification_.Observe(s,meleeDecision_.damageObserved);
            if (bootstrapResult==CombatBootstrapResult::Confirmed)
            {
                attackStarted_=true;
                Debug::Logger::Info("COMBAT INIT state=attack_verify guid="+
                    Hex64(target.guid)+" attackKnown="+(s.actionKnown ? "yes" : "no")+
                    " attackActive="+(s.actionKnown ? (s.attackActive ? "yes" : "no") : "unknown")+
                    " targetHp="+std::to_string(s.targetHp)+
                    " result=confirmed reason="+(meleeDecision_.damageObserved ?
                        "target_hp_decreased" : "attack_latch_active"));
            }
            else if (bootstrapResult==CombatBootstrapResult::Failed)
            {
                // The normal watchdog cannot repair with unknown full action
                // evidence. Transfer to its existing bounded terminal owner;
                // never leave a spent bootstrap silently parked in Fighting.
                meleeDecision_.action=CombatRecoveryAction::Fail;
                Debug::Logger::Info("COMBAT INIT state=attack_verify guid="+
                    Hex64(target.guid)+" attackKnown="+(s.actionKnown ? "yes" : "no")+
                    " attackActive="+(s.actionKnown ? (s.attackActive ? "yes" : "no") : "unknown")+
                    " targetHp="+std::to_string(s.targetHp)+
                    " result=failed reason=bounded_offensive_observation_no_proof"
                    " decision=bounded_terminal_handoff inputGuards=unchanged");
                LogCombatExecution(target,s,CombatClientEvidence5875::Execution(world,target));
            }
            if (ownershipExpired)
            {
                meleeDecision_.action=CombatRecoveryAction::Fail;
                Debug::Logger::Info("COMBAT OWNERSHIP deadline=expired guid="+Hex64(target.guid)+
                    " maximumNoProgressMs="+std::to_string(CombatOwnershipDeadlinePolicy::MaximumNoProgressMs)+
                    " targetFresh="+(s.fresh ? "yes" : "no")+
                    " actionProbe="+meleeActionEvidence_.reason+
                    " attackReadback="+meleeActionEvidence_.attackReason+
                    " decision=bounded_terminal_handoff inputGuards=unchanged");
                LogCombatExecution(target,s,CombatClientEvidence5875::Execution(world,target));
            }
            if (meleeDecision_.damageObserved)
            {
                // Only real damage, never escape dispatch or a structural
                // latch repair, earns a new containment episode.
                defensiveContainmentUsed_=false;
                autonomyRecoveryAttemptsByGuid_.erase(target.guid);
                Debug::Logger::Info("COMBAT LIVENESS EPISODE guid="+Hex64(target.guid)+
                    " decision=reset reason=verified_target_hp_decrease hp="+
                    std::to_string(s.targetHp));
            }
            if (meleeDecision_.classification!=lastMeleeClassification_ ||
                (meleeDecision_.action!=CombatRecoveryAction::None && !meleeTerminalPending_))
            {
                lastMeleeClassification_=meleeDecision_.classification;
                if (meleeDecision_.classification==CombatStallClass::OffensiveNoProgress)
                    LogCombatExecution(target,s,CombatClientEvidence5875::Execution(world,target));
                Debug::Logger::Info("COMBAT LIVENESS targetGuid="+Hex64(lockedGuid_)+
                    " uiTargetGuid="+Hex64(s.selectedGuid)+
                    " serverVictimGuid="+Hex64(world.player.targetGuid)+
                    " resolvedTargetGuid="+Hex64(target.guid)+
                    " state="+StateName()+" range="+Float(target.distance)+
                    " facing="+(s.facing ? "yes" : "no")+
                    " autoattack="+(s.actionKnown ? (s.attackActive ? "yes" : "no") : "unknown")+
                    " targetHp="+std::to_string(s.targetHp)+" playerHp="+std::to_string(s.playerHp)+
                    " noDamageMs="+std::to_string(meleeLiveness_.NoDamageMs(s.nowMs))+
                    " actionEvidence="+meleeActionEvidence_.reason+
                    " classification="+CombatStallName(meleeDecision_.classification));
            }
            if (meleeDecision_.verified!=CombatRecoveryAction::None)
                Debug::Logger::Info("COMBAT RECOVERY VERIFY step="+
                    std::string(CombatRecoveryName(meleeDecision_.verified))+" targetGuid="+Hex64(lockedGuid_)+
                    " result=confirmed evidence="+
                    (meleeDecision_.verified==CombatRecoveryAction::RefreshAttack ? "target_health_decreased" : "post_command_state"));
            if (meleeTerminalPending_ && !meleeTerminal_.Issued() &&
                meleeDecision_.damageObserved && !ownershipExpired)
            {
                meleeTerminalPending_=false; meleeTerminal_.Reset(); lastMeleeTerminalReason_=nullptr;
                Debug::Logger::Info("COMBAT TERMINAL TARGET result=cancelled reason=verified_target_damage");
            }
            if (meleeTerminalPending_ || meleeDecision_.action==CombatRecoveryAction::Fail)
            {
                if (!meleeTerminalPending_)
                {
                    terminalCause_=bootstrapResult==CombatBootstrapResult::Failed
                        ? "initial_attack_unverified" : ownershipExpired
                            ? "combat_ownership_no_progress" : "optional_offensive_recovery_exhausted";
                    chaseTerminalPending_=false;
                    Debug::Logger::Info("COMBAT HARD STALL targetGuid="+Hex64(lockedGuid_)+
                        " durationMs="+std::to_string(meleeLiveness_.NoDamageMs(s.nowMs))+
                        " reason=bounded_same_target_recovery_exhausted");
                }
                meleeTerminalPending_=true;
                return ResolveMeleeTerminal(world,target,s,tick);
            }
            // Self-owned separation / chase reconciliation must still advance
            // their existing FSMs. They gate the new recovery input, not their
            // own bounded cleanup. External UI/cast/world guards hold all work.
            return !s.fresh || !s.selectionKnown || !meleeActionEvidence_.known ||
                !meleeActionEvidence_.inputSafe || s.actionWait;
        }

        bool IssueLivenessAttack(const Objects::WorldState& world,
            const Objects::UnitState& target, std::uint64_t tick, bool refresh,
            bool initialBootstrap=false)
        {
            if (meleeLiveness_.Pending() || meleeLiveness_.Repairs()>=CombatLivenessPolicy::MaximumRepairs)
                return false;
            const auto action=refresh ? CombatRecoveryAction::RefreshAttack : CombatRecoveryAction::ReengageAttack;
            // A rejected dispatch still spends an attempt; it is not success.
            meleeLiveness_.Dispatched(action,GetTickCount64());
            Debug::Logger::Info("COMBAT RECOVERY step="+std::string(CombatRecoveryName(action))+
                " attempt="+std::to_string(meleeLiveness_.Repairs())+" targetGuid="+Hex64(target.guid)+
                " reason="+CombatStallName(meleeDecision_.classification));
            std::string bootstrapOutcome;
            const bool issued=initialBootstrap
                ? AutoAttackController::BootstrapCombatAction(world,target.guid,
                    PostChargeImmediateMeleeDistance,&bootstrapOutcome)
                : AutoAttackController::RecoverCombatAction(world,target.guid,refresh,
                    PostChargeImmediateMeleeDistance);
            attackStarted_=issued && !initialBootstrap;
            if (initialBootstrap)
            {
                bootstrapVerification_.Begin(target.guid,GetTickCount64(),
                    lastMeleeAttackPeriodMs_,issued);
                Debug::Logger::Info("COMBAT INIT state=attack_bootstrap guid="+
                    Hex64(target.guid)+" action=attack result="+
                    (issued ? "dispatched" : "rejected")+" reason="+bootstrapOutcome);
                if (issued)
                    Debug::Logger::Info("COMBAT INIT state=attack_verify guid="+
                        Hex64(target.guid)+" result=pending reason=awaiting_latch_or_damage");
            }
            if (issued && episodeAttackGuid_==target.guid)
                episodeAttackOwnershipEstablished_=true;
            autoAttackReengagePending_=false;
            lastAutoAttackProbeTick_=tick;
            if (issued) { ++attackCommands_; ++autoAttackLivenessRecoveries_; }
            Debug::Logger::Info("COMBAT RECOVERY VERIFY step="+std::string(CombatRecoveryName(action))+
                " targetGuid="+Hex64(target.guid)+" result="+(issued ? "pending" : "dispatch_failed"));
            return issued;
        }

        bool autoAttackReengagePending_ =
            false;

        std::uint64_t lastAutoAttackProbeTick_ =
            0;

        int autoAttackLivenessRecoveries_ =
            0;

        std::uint64_t lastLowHealthFinisherRepairTick_ =
            0;

        int lowHealthFinisherLatchRepairs_ =
            0;

        int lowHealthFinisherHardStallRecoveries_ =
            0;

        bool autoAttackNoSlotLogged_ =
            false;

        int kills_ =
            0;

        int targetsStarted_ =
            0;

        int facingCommands_ =
            0;

        int attackCommands_ =
            0;

        int lootsSucceeded_ =
            0;

        int lootsSkipped_ =
            0;

        int lootsFailed_ =
            0;

        int consecutiveTargetFailures_ =
            0;

        int emergencyHealthEvents_ =
            0;

        int recoveryAggressorPreemptions_ =
            0;

        bool emergencyHealthLatched_ =
            false;

        std::unordered_map<
            std::uint64_t,
            std::uint64_t
        > targetBlacklistUntil_{};

        std::unordered_map<
            std::uint64_t,
            std::uint32_t
        > autonomyRecoveryAttemptsByGuid_{};

        int autonomyCombatRecoveries_ =
            0;

        int autonomyTargetsAbandoned_ =
            0;

        int runtimeSupervisorResets_ =
            0;

        /*
         * Quest-aware target policy.
         *
         * Phase 7B priority:
         *
         * 1. 788 Cutting Teeth -> Mottled Boar (3098)
         * 2. 789 Sting of the Scorpid -> Scorpid Worker (3124)
         *
         * Phase 9A also observes quest 792 Vile Familiars,
         * but objective targeting for 792 is intentionally
         * deferred to Phase 9B. Phase 9A verifies pickup only.
         *
         * If neither combat-enabled quest is active+incomplete, no new
         * combat target is acquired.
         */
        bool questPolicyReady_ =
            false;

        bool cuttingTeethActive_ =
            false;

        bool cuttingTeethComplete_ =
            false;

        bool stingOfTheScorpidActive_ =
            false;

        bool stingOfTheScorpidComplete_ =
            false;

        bool vileFamiliarsActive_ =
            false;

        bool vileFamiliarsComplete_ =
            false;

        bool questPickupAttempted_ =
            false;

        bool vileFamiliarsTravelComplete_ =
            false;

        std::uint32_t desiredQuestEntry_ =
            0;

        /*
         * Phase 11B planner override.
         *
         * This is intentionally a single active objective
         * target. Future BotMode/ObjectiveExecutor layers can
         * switch it without teaching CombatController about
         * individual quest IDs.
         */
        bool plannerQuestTargetActive_ =
            false;

        std::uint32_t plannerQuestTargetEntry_ =
            0;

        std::string plannerQuestTargetName_{};

        // Phase 14G.1 temporary local grinding mode. Quest automation is
        // paused by WorldMonitor while this policy is active; the existing
        // combat/loot/recovery state machine is reused unchanged.
        bool grindModeActive_ = false;
        float grindCenterX_ = 0.0f;
        float grindCenterY_ = 0.0f;
        float grindCenterZ_ = 0.0f;
        float grindRadius_ = 120.0f;

        struct DeferredCorpse
        {
            Objects::UnitState unit{};
            std::uint64_t queuedTick = 0;
        };

        std::deque<DeferredCorpse> deferredCorpses_{};
        static constexpr std::size_t MaximumDeferredCorpses = 8;
        static constexpr std::uint64_t DeferredCorpseMaxAgeTicks = 480;

        std::unordered_map<std::uint64_t, std::uint64_t> recentAggressorUntil_{};
        std::uint64_t currentCombatTick_ = 0;
        std::uint64_t stateObservedTick_ = 0;
        std::uint64_t facingDiagnosticGuid_ = 0;
        int facingDiagnosticDecision_ = -1;

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

        static const char* StateNameInternal(
            CombatState state)
        {
            switch (state)
            {
                case CombatState::Idle:
                    return "Idle";

                case CombatState::AcquiringTarget:
                    return "AcquiringTarget";

                case CombatState::WaitingForTargetSelection:
                    return "WaitingForTargetSelection";

                case CombatState::QuestTravel:
                    return "QuestTravel";

                case CombatState::QuestReturn:
                    return "QuestReturn";

                case CombatState::QuestTurnIn:
                    return "QuestTurnIn";

                case CombatState::QuestPickup:
                    return "QuestPickup";

                case CombatState::QuestObjectiveTravel:
                    return "QuestObjectiveTravel";

                case CombatState::WarriorChargeFacing:
                    return "WarriorChargeFacing";

                case CombatState::WarriorOpening:
                    return "WarriorOpening";

                case CombatState::Chasing:
                    return "Chasing";

                case CombatState::Fighting:
                    return "Fighting";

                case CombatState::Looting:
                    return "Looting";

                case CombatState::PostKillDelay:
                    return "PostKillDelay";

                case CombatState::Recovering:
                    return "Recovering";

                case CombatState::Failed:
                    return "Failed";

                case CombatState::DefensiveContainment:
                    return "DefensiveContainment";

                default:
                    return "Unknown";
            }
        }

        static bool IsUsableTarget(
            const Objects::UnitState* target)
        {
            if (
                target == nullptr ||
                !target->valid ||
                target->guid == 0 ||
                target->health == 0 ||
                target->maxHealth == 0)
            {
                return false;
            }

            /*
             * Phase 9B.1 exposed a compatibility bug:
             *
             * TargetSelector::IsSelectable() belongs to the
             * older 788/789 selector and may internally
             * apply its own entry allowlist. Adding entry
             * 3101 only in CombatController therefore is
             * not sufficient: Vile Familiar can still be
             * rejected by the old selector.
             *
             * CombatController already owns the active
             * quest-entry policy, so validate the supported
             * entry here and apply only the generic distance
             * bound. This keeps existing 3098/3124 behavior
             * and makes 3101 selectable without weakening
             * the quest-policy check.
             */
            const bool supportedEntry =
                TargetSelector::
                    IsAllowedEntry(
                        target->entryId
                    ) ||
                target->entryId == 3101;

            if (!supportedEntry)
            {
                return false;
            }

            return
                target->distance >= 0.0f &&
                target->distance <=
                    SelectorMaxDistance;
        }

        static const char* QuestEntryName(
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

                default:
                    return "None";
            }
        }

        static float Distance2D(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            return std::sqrt(dx * dx + dy * dy);
        }

        void ObserveAggressors(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            currentCombatTick_ = tick;

            for (auto it = recentAggressorUntil_.begin();
                 it != recentAggressorUntil_.end();)
            {
                if (tick > it->second)
                    it = recentAggressorUntil_.erase(it);
                else
                    ++it;
            }

            for (const auto& unit : world.units)
            {
                if (
                    unit.valid &&
                    unit.guid != 0 &&
                    unit.health > 0 &&
                    unit.maxHealth > 0 &&
                    unit.targetGuid == world.activePlayerGuid &&
                    unit.distance >= 0.0f &&
                    unit.distance <= SelectorMaxDistance)
                {
                    const bool newlyRemembered =
                        recentAggressorUntil_.find(unit.guid) == recentAggressorUntil_.end();

                    recentAggressorUntil_[unit.guid] =
                        tick + AggressorMemoryTicks;

                    if (newlyRemembered)
                    {
                        Debug::Logger::Info(
                            "GRIND 14G.1.4: AGGRESSOR TRACKED guid=" +
                            Hex64(unit.guid) +
                            " entry=" + std::to_string(unit.entryId) +
                            " distance=" + Float(unit.distance));
                    }
                }
            }
        }

        bool IsDirectAggressor(
            const Objects::WorldState& world,
            const Objects::UnitState* target) const
        {
            if (
                target == nullptr ||
                !target->valid ||
                target->guid == 0 ||
                target->health == 0 ||
                target->maxHealth == 0 ||
                target->distance < 0.0f ||
                target->distance > SelectorMaxDistance)
            {
                return false;
            }

            if (target->targetGuid == world.activePlayerGuid)
                return true;

            const auto remembered = recentAggressorUntil_.find(target->guid);
            return
                remembered != recentAggressorUntil_.end() &&
                currentCombatTick_ <= remembered->second;
        }

        const Objects::UnitState* FindBestDirectAggressor(
            const Objects::WorldState& world) const
        {
            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!IsDirectAggressor(world, &unit))
                    continue;

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }
            return best;
        }

        std::size_t CountDirectAggressorsWithin(
            const Objects::WorldState& world,
            float maximumDistance) const
        {
            std::size_t count = 0;

            for (const auto& unit : world.units)
            {
                if (
                    IsDirectAggressor(world, &unit) &&
                    unit.distance >= 0.0f &&
                    unit.distance <= maximumDistance)
                {
                    ++count;
                }
            }

            return count;
        }

        bool IsCombatCandidateUsable(
            const Objects::WorldState& world,
            const Objects::UnitState* target) const
        {
            if (grindModeActive_)
            {
                if (IsDirectAggressor(world, target))
                    return true;

                return
                    target != nullptr &&
                    GrindTargetPolicy::LooksLikeCombatCreature(*target) &&
                    target->distance >= 0.0f &&
                    target->distance <= SelectorMaxDistance;
            }

            return IsUsableTarget(target);
        }

        void QueueDeferredCorpse(
            const Objects::UnitState& corpse,
            std::uint64_t tick)
        {
            for (const auto& queued : deferredCorpses_)
            {
                if (queued.unit.guid == corpse.guid)
                    return;
            }

            if (deferredCorpses_.size() >= MaximumDeferredCorpses)
                deferredCorpses_.pop_front();

            deferredCorpses_.push_back(DeferredCorpse{corpse, tick});
            Debug::Logger::Info(
                "GRIND 14G.1.4: corpse queued for post-pack loot guid=" +
                Hex64(corpse.guid) +
                " queued=" + std::to_string(deferredCorpses_.size()));
        }

        bool TryStartDeferredLoot(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const std::size_t jobsToInspect = deferredCorpses_.size();

            for (std::size_t i = 0; i < jobsToInspect; ++i)
            {
                auto job = deferredCorpses_.front();
                deferredCorpses_.pop_front();

                if (tick > job.queuedTick + DeferredCorpseMaxAgeTicks)
                {
                    Debug::Logger::Info(
                        "GRIND 14G.1.4: dropping stale deferred corpse guid=" +
                        Hex64(job.unit.guid));
                    continue;
                }

                const auto* liveCorpse =
                    TargetSelector::FindByGuid(world, job.unit.guid);

                if (
                    liveCorpse == nullptr ||
                    !liveCorpse->valid ||
                    liveCorpse->health != 0)
                {
                    /*
                     * Keep the job boundedly alive. Reacquiring the
                     * corpse from the current ObjectManager snapshot
                     * avoids right-clicking a stale object address.
                     */
                    deferredCorpses_.push_back(job);
                    continue;
                }

                if (loot_.Start(world.player, *liveCorpse, tick))
                {
                    Debug::Logger::Info(
                        "GRIND 14G.1.4: combat clear -> starting deferred corpse loot guid=" +
                        Hex64(job.unit.guid) +
                        " remaining=" + std::to_string(deferredCorpses_.size()));
                    SetState(CombatState::Looting);
                    return true;
                }

                ++lootsFailed_;
                loot_.Reset();
                Debug::Logger::Info(
                    "GRIND 14G.1.4: deferred corpse live-reacquire succeeded but loot could not start; retrying later.");
                deferredCorpses_.push_back(job);
            }

            return false;
        }

        bool MatchesGrindPolicy(
            const Objects::WorldState& world,
            const Objects::UnitState* target) const
        {
            if (
                !grindModeActive_ ||
                target == nullptr ||
                !GrindTargetPolicy::IsPotentialTarget(world, *target))
            {
                return false;
            }

            if (
                target->distance < 0.0f ||
                target->distance >
                    TemporaryGrindAcquisitionDistance)
            {
                return false;
            }

            const float centerDistance =
                Distance2D(
                    grindCenterX_,
                    grindCenterY_,
                    target->x,
                    target->y);

            if (centerDistance > grindRadius_)
                return false;

            return true;
        }

        bool ValidateSelectedGrindTarget(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            if (!grindModeActive_ || IsDirectAggressor(world, &target))
                return true;

            GrindTargetProbe::Result probe{};
            if (!GrindTargetProbe::Read(probe))
            {
                BlacklistTarget(
                    target.guid,
                    tick,
                    "generic grind UnitCanAttack/classification probe failed closed.");
                Debug::Logger::Info(
                    "GRIND 14G.2: target probe unavailable; target skipped rather than attacking an unverified unit.");
                return false;
            }

            if (!probe.canAttack || probe.elite || probe.critter)
            {
                std::string reason =
                    "generic grind target rejected: canAttack=" +
                    std::string(probe.canAttack ? "yes" : "no") +
                    " classification=" + probe.classification +
                    " creatureType=" + probe.creatureType;
                BlacklistTarget(target.guid, tick, reason);
                Debug::Logger::Info(
                    "GRIND 14G.2: TARGET VALIDATION REJECT entry=" +
                    std::to_string(target.entryId) +
                    " level=" + std::to_string(target.level) +
                    " " + reason);
                return false;
            }

            Debug::Logger::Info(
                "GRIND 14G.2: TARGET VALIDATION PASS entry=" +
                std::to_string(target.entryId) +
                " level=" + std::to_string(target.level) +
                " classification=" + probe.classification +
                " creatureType=" + probe.creatureType);
            return true;
        }

        bool MatchesCurrentCombatPolicy(
            const Objects::WorldState& world,
            const Objects::UnitState* target) const
        {
            if (grindModeActive_)
            {
                if (IsDirectAggressor(world, target))
                    return true;
                return MatchesGrindPolicy(world, target);
            }

            return MatchesQuestPolicy(target);
        }

        bool MatchesQuestPolicy(
            const Objects::UnitState* target) const
        {
            if (
                !questPolicyReady_ ||
                desiredQuestEntry_ == 0 ||
                target == nullptr)
            {
                return false;
            }

            return
                target->entryId ==
                    desiredQuestEntry_;
        }

        void RecomputeQuestPolicy()
        {
            if (grindModeActive_)
            {
                desiredQuestEntry_ = 0;
                return;
            }

            std::uint32_t nextEntry =
                0;

            if (
                plannerQuestTargetActive_ &&
                plannerQuestTargetEntry_ != 0)
            {
                nextEntry =
                    plannerQuestTargetEntry_;
            }

            /*
             * Finish Cutting Teeth first when both
             * quests are active. This keeps the routing
             * deterministic for the Valley of Trials.
             */
            if (
                nextEntry == 0 &&
                cuttingTeethActive_ &&
                !cuttingTeethComplete_)
            {
                nextEntry =
                    3098;
            }
            else if (
                nextEntry == 0 &&
                stingOfTheScorpidActive_ &&
                !stingOfTheScorpidComplete_)
            {
                nextEntry =
                    3124;
            }
            else if (
                nextEntry == 0 &&
                vileFamiliarsActive_ &&
                !vileFamiliarsComplete_)
            {
                nextEntry =
                    3101;
            }

            if (
                desiredQuestEntry_ ==
                    nextEntry)
            {
                return;
            }

            desiredQuestEntry_ =
                nextEntry;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "QUEST TARGET POLICY: changed."
            );

            Debug::Logger::Info(
                "788 Cutting Teeth: active=" +
                std::string(
                    cuttingTeethActive_
                        ? "yes"
                        : "no"
                ) +
                " complete=" +
                std::string(
                    cuttingTeethComplete_
                        ? "yes"
                        : "no"
                )
            );

            Debug::Logger::Info(
                "789 Sting of the Scorpid: active=" +
                std::string(
                    stingOfTheScorpidActive_
                        ? "yes"
                        : "no"
                ) +
                " complete=" +
                std::string(
                    stingOfTheScorpidComplete_
                        ? "yes"
                        : "no"
                )
            );

            Debug::Logger::Info(
                "792 Vile Familiars: active=" +
                std::string(
                    vileFamiliarsActive_
                        ? "yes"
                        : "no"
                ) +
                " complete=" +
                std::string(
                    vileFamiliarsComplete_
                        ? "yes"
                        : "no"
                )
            );

            if (
                vileFamiliarsActive_ &&
                !vileFamiliarsComplete_)
            {
                Debug::Logger::Info(
                    "Phase 9B: quest 792 is active; "
                    "target entry 3101 is enabled after "
                    "objective travel."
                );
            }

            if (desiredQuestEntry_ == 0)
            {
                Debug::Logger::Info(
                    "Quest target: NONE."
                );

                Debug::Logger::Info(
                    "Combat acquisition paused until "
                    "an incomplete supported quest is active."
                );
            }
            else
            {
                const char* targetName =
                    plannerQuestTargetActive_ &&
                    desiredQuestEntry_ ==
                        plannerQuestTargetEntry_ &&
                    !plannerQuestTargetName_.empty()
                        ? plannerQuestTargetName_.c_str()
                        : QuestEntryName(
                            desiredQuestEntry_
                        );

                Debug::Logger::Info(
                    std::string(
                        "Quest target entry: "
                    ) +
                    std::to_string(
                        desiredQuestEntry_
                    ) +
                    " (" +
                    targetName +
                    ")"
                );
            }

            Debug::Logger::Info(
                "================================"
            );
        }

        bool IsBlacklisted(
            std::uint64_t guid,
            std::uint64_t tick)
        {
            const auto existing =
                targetBlacklistUntil_.find(
                    guid
                );

            if (
                existing ==
                    targetBlacklistUntil_.end())
            {
                return false;
            }

            if (
                tick >=
                    existing->second)
            {
                Debug::Logger::Info(
                    "TARGET BLACKLIST: expired guid=" +
                    Hex64(
                        guid
                    )
                );

                targetBlacklistUntil_.erase(
                    existing
                );

                return false;
            }

            return true;
        }

        void BlacklistTarget(
            std::uint64_t guid,
            std::uint64_t tick,
            const std::string& reason)
        {
            if (guid == 0)
            {
                return;
            }

            const std::uint64_t until =
                tick +
                BlacklistDurationTicks;

            targetBlacklistUntil_[
                guid
            ] =
                until;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "TARGET BLACKLIST: added."
            );

            Debug::Logger::Info(
                "GUID: " +
                Hex64(
                    guid
                )
            );

            Debug::Logger::Info(
                "Duration: 120 seconds."
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "================================"
            );
        }

        TargetCandidate SelectCandidate(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const auto snapshot = SafeTargetSelector::Capture(
                world,
                [this, &world, tick](const Objects::UnitState& unit)
                {
                    return IsCombatCandidateUsable(world, &unit) &&
                        MatchesCurrentCombatPolicy(world, &unit) &&
                        !IsBlacklisted(unit.guid, tick) &&
                        (unit.targetGuid == 0 ||
                         unit.targetGuid == world.activePlayerGuid);
                },
                [this, &world](const Objects::UnitState& unit)
                {
                    return IsDirectAggressor(world, &unit);
                });
            const float healthPercent = world.player.valid &&
                    world.player.maxHealth != 0
                ? RecoveryController::HealthPercent(world.player)
                : std::numeric_limits<float>::quiet_NaN();
            const auto selected = PullSafetyPolicy::Select(
                snapshot.units, healthPercent);
            lastPullEvaluationTick_ = tick;
            lastPullEvaluationEntry_ = plannerQuestTargetActive_
                ? plannerQuestTargetEntry_ : 0;
            const auto* unit = snapshot.At(selected.selectedIndex);
            const std::uint64_t selectedGuid = unit == nullptr ? 0 : unit->guid;

            if (!pullSelectionLogged_ || selected.decision != lastPullDecision_ ||
                selectedGuid != lastPullSelectedGuid_ ||
                tick >= nextPullSafetyLogTick_)
            {
                pullSelectionLogged_ = true;
                lastPullDecision_ = selected.decision;
                lastPullSelectedGuid_ = selectedGuid;
                nextPullSafetyLogTick_ = tick + 20;
                if (selected.decision == PullSafetyDecision::NoSafeCandidate)
                    Debug::Logger::Info(
                        "PULL SAFETY NO SAFE TARGET candidateCount=" +
                        std::to_string(selected.candidateCount) +
                        " rejectedCount=" +
                        std::to_string(selected.rejectedCount));
                else if (selected.decision == PullSafetyDecision::Voluntary &&
                         unit != nullptr)
                    Debug::Logger::Info(
                        "PULL SAFETY SELECT guid=" + Hex64(unit->guid) +
                        " entry=" + std::to_string(unit->entryId) +
                        " hpPct=" + Float(healthPercent) +
                        " predictedAdds=" +
                        std::to_string(selected.selected.predictedAdds) +
                        " nearestHostileDistance=" +
                        (std::isfinite(selected.selected.nearestHostileDistance)
                            ? Float(selected.selected.nearestHostileDistance)
                            : std::string("none")) +
                        " candidateCount=" +
                        std::to_string(selected.candidateCount));
                else if (selected.decision == PullSafetyDecision::Defensive)
                    Debug::Logger::Info(
                        "PULL SAFETY DEFENSIVE aggressors=" +
                        std::to_string(selected.aggressorCount) +
                        " selectedGuid=" + Hex64(selectedGuid));
                PullSafetyCandidateTelemetry::Emit(
                    snapshot.units, healthPercent, selected, "combat");
            }

            TargetCandidate result{};
            if (unit != nullptr &&
                (selected.decision == PullSafetyDecision::Voluntary ||
                 (selected.decision == PullSafetyDecision::Defensive &&
                  IsCombatCandidateUsable(world, unit) &&
                  MatchesCurrentCombatPolicy(world, unit))))
            {
                result.found = true;
                result.unit = *unit;
            }
            return result;
        }

        bool TryEnterRecovery(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (FindBestDirectAggressor(world) != nullptr)
            {
                if (!pullDefensiveRecoveryLogged_)
                {
                    pullDefensiveRecoveryLogged_ = true;
                    Debug::Logger::Info(
                        "PULL SAFETY DEFENSIVE: recovery suppressed while a live mob is targeting the player.");
                }
                return false;
            }
            pullDefensiveRecoveryLogged_ = false;

            const float hp = RecoveryController::HealthPercent(world.player);
            if (world.player.valid && world.player.maxHealth != 0 &&
                world.player.health != 0 &&
                hp < PullSafetyPolicy::MinimumVoluntaryHealthPercent)
            {
                if (!pullHealthGateLogged_)
                {
                    pullHealthGateLogged_ = true;
                    Debug::Logger::Info(
                        "PULL SAFETY GATE hpPct=" + Float(hp) +
                        " decision=wait reason=low_health");
                }
            }
            else if (pullHealthGateLogged_ &&
                     hp >= PullSafetyPolicy::MinimumVoluntaryHealthPercent)
            {
                pullHealthGateLogged_ = false;
                Debug::Logger::Info(
                    "PULL SAFETY READY hpPct=" + Float(hp));
            }

            if (!recovery_.ShouldStart(
                    world.player,
                    tick))
            {
                return false;
            }

            ResetTargetState();

            if (!recovery_.Start(
                    world.player,
                    tick))
            {
                return false;
            }

            Debug::Logger::Info(
                "COMBAT LOOP: pausing target "
                "acquisition for recovery."
            );

            SetState(
                CombatState::Recovering
            );

            return true;
        }

        void SetState(
            CombatState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "CombatController state: "
                ) +
                StateNameInternal(
                    state_
                ) +
                " -> " +
                StateNameInternal(
                    newState
                )
            );

            Debug::Logger::Info("ACTION LATENCY owner=Combat from=" +
                std::string(StateNameInternal(state_)) + " to=" + StateNameInternal(newState) +
                " elapsedTicks=" + std::to_string(currentCombatTick_ >= stateObservedTick_
                    ? currentCombatTick_ - stateObservedTick_ : 0) +
                " classification=state_residence_not_command_latency");
            stateObservedTick_ = currentCombatTick_;
            state_ =
                newState;
        }

        void Fail(
            const std::string& reason,
            bool allowInputCleanup=true)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "COMBAT LOOP: FAILED"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "Locked GUID: " +
                Hex64(
                    lockedGuid_
                )
            );

            Debug::Logger::Info(
                "Kills: " +
                std::to_string(
                    kills_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            if (allowInputCleanup) AutoAttackController::Stop();

            if (allowInputCleanup && chase_.IsActive())
            {
                chase_.Stop();
            }

            defensiveRoute_.reset();

            warrior_.EndTarget();

            SetState(
                CombatState::Failed
            );
        }

        void ResetTargetState()
        {
            defensiveRoute_.reset();
            defensiveContainment_.Reset();
            defensiveContainmentUsed_=false;
            postContainmentRelease_=false;
            defensiveRouteStartFailed_=false;
            defensiveRouteFailure_="none";
            defensiveLastDistance_=0.0f;
            defensiveLastX_=defensiveLastY_=0.0f;
            defensiveStopIssuedAtMs_=0;
            meleeTerminal_.Reset();
            meleeTerminalPending_=false;
            lastMeleeTerminalReason_=nullptr;
            terminalCause_="offensive_no_progress";
            chaseTerminalPending_=false;
            meleeLiveness_.Reset();
            meleeDecision_={};
            meleeActionEvidence_={};
            lastMeleeClassification_=CombatStallClass::UnknownOrStale;
            bootstrapVerification_.Reset();
            ownershipDeadline_.Reset();
            lastMeleeAttackPeriodMs_=0;
            lockedGuid_ =
                0;

            episodeAttackGuid_ = 0;
            episodeAttackOwnershipEstablished_ = false;

            pendingTargetGuid_ =
                0;
            selectionAttemptsForGuid_ = 0;

            attackStarted_ =
                false;

            autoAttackReengagePending_ =
                false;

            lastAutoAttackProbeTick_ =
                0;

            autoAttackNoSlotLogged_ =
                false;

            lastFacingCommandTick_ =
                0;

            facingStableSnapshots_ =
                0;

            facingGuardHolding_ =
                false;

            selectedTargetMismatchSnapshots_ =
                0;

            facingHoldSnapshots_ =
                0;

            combatSeparationActive_ =
                false;

            combatSeparationUntilTick_ =
                0;

            lastCombatSeparationTick_ =
                0;

            combatSeparationAttempts_ =
                0;

            warriorOpenerUntilTick_ =
                0;

            warriorChargeFacingUntilTick_ =
                0;

            warriorChargeFacingStableSnapshots_ =
                0;

            warriorChargeFacingCommandsForTarget_ =
                0;

            warrior_.EndTarget();
        }

        void BeginAcquire(
            std::uint64_t tick)
        {
            ResetTargetState();

            nextAcquireTick_ =
                tick;

            SetState(
                CombatState::AcquiringTarget
            );
        }

        void EnterPostTargetDelay(
            std::uint64_t tick)
        {
            ResetTargetState();

            nextAcquireTick_ =
                tick +
                PostKillDelayTicks;

            consecutiveTargetFailures_ =
                0;

            SetState(
                CombatState::PostKillDelay
            );
        }

        void FinishTarget(
            const Objects::WorldState& world,
            const Objects::UnitState* target,
            std::uint64_t tick,
            bool killed)
        {
            const std::uint64_t finishedGuid =
                lockedGuid_;

            if (finishedGuid != 0)
            {
                autonomyRecoveryAttemptsByGuid_.erase(finishedGuid);
            }

            Debug::Logger::Info(
                "================================"
            );

            if (killed)
            {
                ++kills_;

                Debug::Logger::Info(
                    "COMBAT LOOP: TARGET DEAD"
                );
            }
            else
            {
                Debug::Logger::Info(
                    "COMBAT LOOP: TARGET LOST"
                );
            }

            Debug::Logger::Info(
                "Finished GUID: " +
                Hex64(
                    finishedGuid
                )
            );

            Debug::Logger::Info(
                "Total kills: " +
                std::to_string(
                    kills_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            AutoAttackController::Stop();

            /*
             * TargetDead / TargetLost are terminal
             * ChaseController states. Stop() resets the
             * controller for reuse.
             */
            chase_.Stop();

            attackStarted_ =
                false;

            pendingTargetGuid_ =
                0;

            lastFacingCommandTick_ =
                0;

            facingStableSnapshots_ =
                0;

            facingGuardHolding_ =
                false;

            selectedTargetMismatchSnapshots_ =
                0;

            facingHoldSnapshots_ =
                0;

            combatSeparationActive_ =
                false;

            combatSeparationUntilTick_ =
                0;

            lastCombatSeparationTick_ =
                0;

            combatSeparationAttempts_ =
                0;

            warrior_.EndTarget();

            if (livingDefenseOnly_)
            {
                if (killed && target && target->valid && target->health==0)
                    QueueDeferredCorpse(*target,tick);
                EnterPostTargetDelay(tick);
                return; // no loot transaction or ordinary acquisition during living recovery
            }

            /*
             * Only a confirmed dead target can become a
             * corpse-loot job. A lost target skips loot.
             */
            if (
                killed &&
                target != nullptr &&
                target->valid &&
                target->guid == finishedGuid &&
                target->health == 0)
            {
                if (grindModeActive_ && FindBestDirectAggressor(world) != nullptr)
                {
                    QueueDeferredCorpse(*target, tick);
                    Debug::Logger::Info(
                        "GRIND 14G.1.2: MULTI-AGGRO CONTINUE COMBAT BEFORE LOOT");
                    BeginAcquire(tick + 1);
                    return;
                }

                Debug::Logger::Info(
                    "COMBAT LOOP: handing corpse "
                    "to LootController."
                );

                if (
                    loot_.Start(
                        world.player,
                        *target,
                        tick
                    ))
                {
                    SetState(
                        CombatState::Looting
                    );

                    return;
                }

                ++lootsFailed_;

                Debug::Logger::Info(
                    "COMBAT LOOP: LootController "
                    "failed to start; continuing."
                );

                loot_.Reset();
            }

            EnterPostTargetDelay(
                tick
            );
        }

        bool IssueTargetSelection(
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "COMBAT LOOP: selecting target."
            );

            Debug::Logger::Info(
                "Requested GUID: " +
                Hex64(
                    target.guid
                )
            );

            Debug::Logger::Info(
                "Entry: " +
                std::to_string(
                    target.entryId
                )
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    target.distance
                )
            );

            const bool issued =
                TargetController::
                    SetTarget(
                        target.guid
                    );

            if (!issued)
            {
                Debug::Logger::Info(
                    "SetTarget command rejected."
                );

                Debug::Logger::Info(
                    "================================"
                );

                ++consecutiveTargetFailures_;

                if (
                    consecutiveTargetFailures_ >=
                        MaximumConsecutiveTargetFailures)
                {
                    Fail(
                        "too many SetTarget failures."
                    );
                }

                return false;
            }

            if (pendingTargetGuid_ != target.guid)
                selectionAttemptsForGuid_ = 0;
            pendingTargetGuid_ =
                target.guid;
            ++selectionAttemptsForGuid_;

            lastTargetCommandTick_ =
                tick;

            SetState(
                CombatState::
                    WaitingForTargetSelection
            );

            Debug::Logger::Info(
                "SetTarget command issued."
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        bool StartChaseForLockedTarget(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            if (!chase_.Start(
                    world.player,
                    target,
                    tick))
            {
                Debug::Logger::Info(
                    "ChaseController failed to start."
                );

                Debug::Logger::Info(
                    "Rejected target entry: " +
                    std::to_string(
                        target.entryId
                    ) +
                    " GUID=" +
                    Hex64(
                        target.guid
                    ) +
                    " distance=" +
                    Float(
                        target.distance
                    )
                );

                Debug::Logger::Info(
                    "TargetSelector::IsAllowedEntry=" +
                    std::string(
                        TargetSelector::
                            IsAllowedEntry(
                                target.entryId
                            )
                                ? "yes"
                                : "no"
                    )
                );

                Debug::Logger::Info(
                    "TargetSelector::IsSelectable=" +
                    std::string(
                        TargetSelector::
                            IsSelectable(
                                target,
                                80.0f
                            )
                                ? "yes"
                                : "no"
                    )
                );

                Debug::Logger::Info(
                    "================================"
                );

                const auto reason=ChaseFailureName(chase_.FailureReason());
                SetState(CombatState::Chasing);
                BeginChaseTerminal(world,target,reason);

                return false;
            }

            SetState(
                chase_.IsInRange()
                    ? CombatState::Fighting
                    : CombatState::Chasing
            );

            return true;
        }

        bool StartLockedTarget(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            if (!IsCombatCandidateUsable(world, &target))
            {
                return false;
            }

            /*
             * Autoattack and Warrior abilities must operate
             * on the exact same unit the movement layer is
             * locking.
             */
            if (
                ClientSelectedGuid(world) !=
                    target.guid)
            {
                return false;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "COMBAT LOOP: locked target."
            );

            Debug::Logger::Info(
                "GUID: " +
                Hex64(
                    target.guid
                )
            );

            Debug::Logger::Info(
                "Entry: " +
                std::to_string(
                    target.entryId
                )
            );

            Debug::Logger::Info(
                "Level: " +
                std::to_string(
                    target.level
                )
            );

            Debug::Logger::Info(
                "Health: " +
                std::to_string(
                    target.health
                ) +
                "/" +
                std::to_string(
                    target.maxHealth
                )
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    target.distance
                )
            );

            if (episodeAttackGuid_ != target.guid)
            {
                episodeAttackGuid_=target.guid;
                episodeAttackOwnershipEstablished_=false;
            }
            if (lockedGuid_ != target.guid)
                autonomyRecoveryAttemptsByGuid_.erase(target.guid);
            lockedGuid_ =
                target.guid;

            pendingTargetGuid_ =
                0;

            attackStarted_ =
                false;

            autoAttackReengagePending_ =
                false;

            lastAutoAttackProbeTick_ =
                0;

            autoAttackNoSlotLogged_ =
                false;

            lastFacingCommandTick_ =
                0;

            facingStableSnapshots_ =
                0;

            facingGuardHolding_ =
                false;

            selectedTargetMismatchSnapshots_ =
                0;

            facingHoldSnapshots_ =
                0;

            combatSeparationActive_ =
                false;

            combatSeparationUntilTick_ =
                0;

            lastCombatSeparationTick_ =
                0;

            combatSeparationAttempts_ =
                0;

            warriorOpenerUntilTick_ =
                0;

            warriorChargeFacingUntilTick_ =
                0;

            warriorChargeFacingStableSnapshots_ =
                0;

            warriorChargeFacingCommandsForTarget_ =
                0;

            emergencyHealthLatched_ =
                false;

            warrior_.BeginTarget(
                target
            );

            ++targetsStarted_;

            consecutiveTargetFailures_ =
                0;

            const float chargeFacingDelta =
                FacingController::
                    AngularDifference(
                        world.player.rotation,
                        FacingController::
                            CalculateFacing(
                                world.player,
                                target
                            )
                    );

            const bool chargeFacingReady =
                CombatFacingPolicy::
                    IsAbilityFacingReady(
                        chargeFacingDelta
                    );

            const bool chargePreparationEligible =
                warrior_.CanPrepareCharge(
                    world.player,
                    target,
                    ClientSelectedGuid(world) ==
                        target.guid
                );

            if (chargePreparationEligible)
            {
                /*
                 * Phase 14G.5.2.2:
                 * Do not throw away a valid Charge opener merely because the
                 * first post-target-selection snapshot is misaligned. Neutralize
                 * any stale grind/approach CTM, acquire facing on fresh
                 * snapshots, and only then cast.
                 */
                MovementController::HoldPosition(
                    world.player
                );

                warriorChargeFacingUntilTick_ =
                    tick +
                    WarriorChargeFacingMaximumTicks;

                warriorChargeFacingStableSnapshots_ =
                    CombatInitiationPolicy::ConfirmChargeFacing(0,
                        ClientSelectedGuid(world)==target.guid,chargeFacingReady);

                warriorChargeFacingCommandsForTarget_ =
                    0;

                facingGuardHolding_ =
                    true;

                if (!chargeFacingReady)
                {
                    float issuedDesired =
                        0.0f;

                    if (
                        FacingController::Face(
                            world.player,
                            target,
                            &issuedDesired
                        ))
                    {
                        lastFacingCommandTick_ =
                            tick;

                        ++facingCommands_;

                        ++warriorChargeFacingCommandsForTarget_;

                        Debug::Logger::Info(
                            "CHARGE FACING 14G.5.2.2: initial facing correction issued; holding CTM before opener."
                        );
                    }
                    else
                    {
                        Debug::Logger::Info(
                            "CHARGE FACING 14G.5.2.2: initial facing correction failed; bounded acquisition remains active."
                        );
                    }
                }
                else
                {
                    Debug::Logger::Info(
                        "CHARGE FACING 14G.5.2.2: first aligned snapshot observed; waiting for one fresh confirmation before Charge."
                    );
                }

                SetState(
                    CombatState::
                        WarriorChargeFacing
                );

                Debug::Logger::Info(
                    "Target combat started with bounded Charge facing acquisition."
                );
            }
            else
            {
                /*
                 * Preserve WarriorRotationController's exact diagnostics for
                 * out-of-range/disabled/invalid Charge cases, then fall back to
                 * the existing verified chase path.
                 */
                warrior_.TryCharge(
                    world.player,
                    target,
                    tick,
                    ClientSelectedGuid(world) ==
                        target.guid,
                    chargeFacingReady
                );

                if (!StartChaseForLockedTarget(
                        world,
                        target,
                        tick))
                {
                    return false;
                }

                Debug::Logger::Info(
                    "Target combat started with "
                    "normal CTM chase."
                );
            }

            Debug::Logger::Info(
                "Targets started: " +
                std::to_string(
                    targetsStarted_
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }

        void MaintainTargetSelection(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            if (
                ClientSelectedGuid(world) ==
                    lockedGuid_)
            {
                return;
            }

            if (meleeDecision_.action!=CombatRecoveryAction::RestoreTarget)
            {
                return;
            }

            meleeLiveness_.Dispatched(CombatRecoveryAction::RestoreTarget,GetTickCount64());
            Debug::Logger::Info("COMBAT RECOVERY step=restore_target attempt="+
                std::to_string(meleeLiveness_.Repairs())+" targetGuid="+Hex64(target.guid)+
                " reason=target_selection_desync");

            Debug::Logger::Info(
                "COMBAT LOOP: restoring "
                "locked UI target."
            );

            Debug::Logger::Info(
                "Locked GUID: " +
                Hex64(
                    lockedGuid_
                )
            );

            if (AutoAttackController::RestoreCombatTarget(world,target))
            {
                lastTargetCommandTick_ =
                    tick;
            }
        }

        bool StartCombatSeparationRecovery(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick,
            const std::string& reason)
        {
            if (
                combatSeparationAttempts_ >=
                    CombatPositioningPolicy::MaximumSeparationAttempts)
            {
                return false;
            }

            const bool cooldownReady =
                combatSeparationAttempts_ == 0 ||
                tick >=
                    lastCombatSeparationTick_ +
                    CombatSeparationCooldownTicks;

            if (!cooldownReady)
            {
                return false;
            }

            if (attackStarted_)
            {
                AutoAttackController::Stop();

                attackStarted_ =
                    false;

                Debug::Logger::Info(
                    "COMBAT POSITIONING 14G.3.2: autoattack paused before separation recovery."
                );
            }

            if (!MovementController::MoveAwayFromTarget(
                    world.player,
                    target,
                    CombatSeparationStepDistance))
            {
                Debug::Logger::Info(
                    "COMBAT POSITIONING 14G.3.2: separation command failed; staying in facing hold."
                );

                return false;
            }

            ++combatSeparationAttempts_;

            lastCombatSeparationTick_ =
                tick;

            combatSeparationUntilTick_ =
                tick +
                CombatSeparationMaximumTicks;

            combatSeparationActive_ =
                true;

            facingStableSnapshots_ =
                0;

            facingHoldSnapshots_ =
                0;

            facingGuardHolding_ =
                true;

            Debug::Logger::Info(
                "COMBAT POSITIONING 14G.3.2: SEPARATION RECOVERY START reason=" +
                reason +
                " targetDistance=" +
                Float(target.distance) +
                " attempt=" +
                std::to_string(combatSeparationAttempts_) +
                "/" +
                std::to_string(CombatPositioningPolicy::MaximumSeparationAttempts)
            );

            return true;
        }

        bool ConsumeCombatSeparationRecovery(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            if (!combatSeparationActive_)
            {
                return false;
            }

            const bool enoughSeparation =
                target.distance >=
                    CombatPositioningPolicy::SeparationCompleteDistance;

            const bool boundedWindowExpired =
                tick >=
                    combatSeparationUntilTick_;

            if (
                !enoughSeparation &&
                !boundedWindowExpired)
            {
                return true;
            }

            MovementController::HoldPosition(
                world.player
            );

            combatSeparationActive_ =
                false;

            facingStableSnapshots_ =
                0;

            facingHoldSnapshots_ =
                0;

            lastFacingCommandTick_ =
                0;

            facingGuardHolding_ =
                true;

            Debug::Logger::Info(
                "COMBAT POSITIONING 14G.3.2: SEPARATION RECOVERY COMPLETE targetDistance=" +
                Float(target.distance) +
                (enoughSeparation
                    ? " reason=distance"
                    : " reason=bounded-window") +
                "; facing verification resumes next snapshot."
            );

            /*
             * Consume this polling tick. The next fresh snapshot verifies
             * facing after movement has actually stopped.
             */
            return true;
        }

        static float TargetHealthPercent(
            const Objects::UnitState& target)
        {
            if (target.maxHealth == 0)
                return 100.0f;

            return
                (static_cast<float>(target.health) * 100.0f) /
                static_cast<float>(target.maxHealth);
        }

        static bool IsLowHealthMeleeFinisherTarget(
            const Objects::UnitState& target)
        {
            return
                target.valid &&
                target.health > 0 &&
                target.maxHealth > 0 &&
                target.distance <= LowHealthFinisherMeleeDistance &&
                TargetHealthPercent(target) <= LowHealthFinisherTargetPercent;
        }

        bool MaintainFacingAndAttack(
            const Objects::WorldState& world,
            const Objects::UnitState& target,
            std::uint64_t tick)
        {
            /*
             * Phase 14G.2.1 facing safety:
             *
             * - never attack a UI target other than our combat lock;
             * - offensive abilities require a tightly aligned fresh snapshot;
             * - a large facing drift stops autoattack before the target can
             *   move behind the player;
             * - after a stop/new target, require two consecutive aligned
             *   snapshots before releasing autoattack/offensive abilities.
             */
            if (
                ClientSelectedGuid(world) !=
                    lockedGuid_)
            {
                facingStableSnapshots_ =
                    0;

                facingHoldSnapshots_ =
                    0;

                ++selectedTargetMismatchSnapshots_;
                if (selectedTargetMismatchSnapshots_ == 1)
                    Debug::Logger::Info("TARGET CONSISTENCY combat=" + Hex64(lockedGuid_) +
                        " chase=" + Hex64(chase_.TargetGuid()) + " client=" + Hex64(ClientSelectedGuid(world)) +
                        " result=await_selected_target_evidence");

                /*
                 * Phase 14G.3.2: the active-target restore command runs before
                 * this guard in the same Update() call, but WorldState is still
                 * the pre-command snapshot. Give that synchronous SetTarget one
                 * fresh poll to become observable before tearing down a healthy
                 * autoattack. Offensive abilities remain held during the grace
                 * snapshot.
                 */
                if (
                    selectedTargetMismatchSnapshots_ <
                        TargetMismatchGraceSnapshots)
                {
                    if (!facingGuardHolding_)
                    {
                        Debug::Logger::Info(
                            "TARGET/FACING 14G.3.2: transient selected-target mismatch; restore issued/awaiting one fresh snapshot."
                        );
                    }

                    facingGuardHolding_ =
                        true;

                    return false;
                }

                if (attackStarted_)
                {
                    AutoAttackController::Stop();

                    attackStarted_ =
                        false;

                    Debug::Logger::Info(
                        "TARGET/FACING 14G.3.2: autoattack stopped after confirmed selected-target mismatch."
                    );
                }

                if (!facingGuardHolding_)
                {
                    Debug::Logger::Info(
                        "TARGET/FACING 14G.3.2: HOLD - selected target mismatch persisted across fresh snapshots."
                    );
                }

                facingGuardHolding_ =
                    true;

                return false;
            }

            selectedTargetMismatchSnapshots_ =
                0;

            if (ConsumeCombatSeparationRecovery(
                    world,
                    target,
                    tick))
            {
                return false;
            }

            const bool lowHealthMeleeFinisher =
                IsLowHealthMeleeFinisherTarget(target);

            /*
             * Phase 14M.0.2:
             *
             * A low-HP mob can move across the player's facing arc quickly
             * enough for the facing guard to pause melee. If the target is
             * already within verified melee distance, keep/repair the Attack
             * latch while facing correction continues. WoW will not land a
             * swing while facing is invalid, but retaining the latch means the
             * first safe aligned snapshot can finish the mob instead of
             * waiting for another chase/hard-stall cycle.
             */
            if (
                lowHealthMeleeFinisher &&
                !attackStarted_ &&
                (lastLowHealthFinisherRepairTick_ == 0 ||
                 tick >=
                    lastLowHealthFinisherRepairTick_ +
                    LowHealthFinisherRepairCooldownTicks))
            {
                lastLowHealthFinisherRepairTick_ =
                    tick;

                if (IssueLivenessAttack(world,target,tick,false))
                {
                    attackStarted_ =
                        true;

                    autoAttackReengagePending_ =
                        false;

                    lastAutoAttackProbeTick_ =
                        tick;

                    ++lowHealthFinisherLatchRepairs_;

                    Debug::Logger::Info(
                        "COMBAT 14M.0.2: LOW-HP FINISHER LATCH REPAIRED hp=" +
                        Float(TargetHealthPercent(target)) +
                        "% distance=" +
                        Float(target.distance) +
                        "; facing correction retains ownership until the target dies."
                    );
                }
            }

            /*
             * Charge can place the Warrior almost exactly on top of the mob.
             * Facing derived from a ~zero-length XY vector is unstable and a
             * tiny mob movement can flip it behind us between snapshots.
             * Create a small, bounded separation before the first attack.
             */
            if (
                !lowHealthMeleeFinisher &&
                CombatPositioningPolicy::NeedsImmediateSeparation(
                    target.distance) &&
                StartCombatSeparationRecovery(
                    world,
                    target,
                    tick,
                    "overlap/near-zero post-charge separation"))
            {
                return false;
            }

            const float desired =
                FacingController::
                    CalculateFacing(
                        world.player,
                        target
                    );

            const float delta =
                FacingController::
                    AngularDifference(
                        world.player.rotation,
                        desired
                    );

            const bool facingReady =
                CombatFacingPolicy::
                    IsAbilityFacingReady(
                        delta
                    );

            const int diagnosticDecision = facingReady ? 0 : 1;
            if (facingDiagnosticGuid_ != target.guid || facingDiagnosticDecision_ != diagnosticDecision)
            {
                facingDiagnosticGuid_ = target.guid;
                facingDiagnosticDecision_ = diagnosticDecision;
                const auto& movement = ClickToMoveController::LastCommand();
                Debug::Logger::Info("COMBAT FACING targetGuid=" + Hex64(target.guid) +
                    " distance=" + Float(target.distance) + " yaw=" + Float(world.player.rotation) +
                    " bearing=" + Float(desired) + " signedError=" +
                    Float(CombatGeometry::SignedError(world.player.rotation, desired)) +
                    " decision=" + (facingReady ? "aligned" : "correct") +
                    " tick=" + std::to_string(tick) + " ctmSerial=" + std::to_string(movement.serial) +
                    " ctmWriter=" + movement.writer);
                Debug::Logger::Info("TARGET CONSISTENCY combat=" + Hex64(lockedGuid_) +
                    " chase=" + Hex64(chase_.TargetGuid()) + " client=" + Hex64(ClientSelectedGuid(world)) +
                    " autoattackLatched=" + (attackStarted_ ? "yes" : "no") +
                    " plannerEntry=" + std::to_string(DesiredQuestEntry()));
            }

            if (!facingReady)
            {
                facingStableSnapshots_ =
                    0;

                ++facingHoldSnapshots_;

                if (
                    !lowHealthMeleeFinisher &&
                    CombatPositioningPolicy::NeedsPersistentFacingRecovery(
                        target.distance,
                        facingHoldSnapshots_) &&
                    StartCombatSeparationRecovery(
                        world,
                        target,
                        tick,
                        "persistent facing hold"))
                {
                    return false;
                }

                if (
                    attackStarted_ &&
                    CombatFacingPolicy::
                        ShouldPauseAutoAttack(
                            delta
                        ))
                {
                    if (lowHealthMeleeFinisher)
                    {
                        if (facingHoldSnapshots_ == 1)
                        {
                            Debug::Logger::Info(
                                "COMBAT 14M.0.2: LOW-HP FINISHER keeps autoattack latched during facing correction; hp=" +
                                Float(TargetHealthPercent(target)) +
                                "% distance=" +
                                Float(target.distance) +
                                " delta=" +
                                Float(delta)
                            );
                        }
                    }
                    else
                    {
                        AutoAttackController::Stop();

                        attackStarted_ =
                            false;

                        Debug::Logger::Info(
                            "FACING GUARD 14G.2.1: autoattack PAUSED before unsafe facing; delta=" +
                            Float(delta)
                        );
                    }
                }

                if (!facingGuardHolding_)
                {
                    Debug::Logger::Info(
                        "FACING GUARD 14G.2.1: HOLD - correcting target facing; delta=" +
                        Float(delta)
                    );
                }

                facingGuardHolding_ =
                    true;

                const bool facingCooldownReady =
                    facingCommands_ == 0 ||
                    tick >=
                        lastFacingCommandTick_ +
                        FacingCooldownTicks;

                if (!facingCooldownReady)
                {
                    return false;
                }

                Debug::Logger::Info(
                    "CONTINUOUS FACING: correction needed."
                );

                Debug::Logger::Info(
                    "Facing delta: " +
                    Float(delta)
                );

                if (facingHoldSnapshots_ == 1)
                {
                    MovementController::HoldPosition(
                        world.player
                    );
                }

                float issuedDesired =
                    0.0f;

                if (
                    FacingController::
                        Face(
                            world.player,
                            target,
                            &issuedDesired
                        ))
                {
                    lastFacingCommandTick_ =
                        tick;

                    ++facingCommands_;

                    Debug::Logger::Info(
                        "CONTINUOUS FACING: command issued."
                    );
                }
                else
                {
                    Debug::Logger::Info(
                        "CONTINUOUS FACING: command failed."
                    );
                }

                return false;
            }

            facingHoldSnapshots_ =
                0;

            if (
                facingStableSnapshots_ <
                    CombatFacingPolicy::
                        StableSnapshotsRequired)
            {
                ++facingStableSnapshots_;
            }

            if (
                facingStableSnapshots_ <
                    CombatFacingPolicy::
                        StableSnapshotsRequired)
            {
                if (!facingGuardHolding_)
                {
                    Debug::Logger::Info(
                        "FACING GUARD 14G.2.1: HOLD - first aligned snapshot; waiting for confirmation."
                    );
                }

                facingGuardHolding_ =
                    true;

                return false;
            }

            if (facingGuardHolding_)
            {
                Debug::Logger::Info(
                    "FACING GUARD 14G.2.1: RELEASE - facing confirmed on consecutive snapshots."
                );

                facingGuardHolding_ =
                    false;
            }

            // Attack intent is not evidence. An observed current Attack action
            // verifies the structural latch; only damage re-arms the budget.
            if (meleeActionEvidence_.attack.actionSlotFound && meleeActionEvidence_.attack.active)
            {
                if (!attackStarted_)
                    Debug::Logger::Info("COMBAT INIT GATE state=attack_start guid="+
                        Hex64(lockedGuid_)+" selectedGuid="+Hex64(ClientSelectedGuid(world))+
                        " decision=already_active reason=observed_attack_latch");
                attackStarted_=true;
                autoAttackReengagePending_=false;
            }
            if (meleeDecision_.action==CombatRecoveryAction::RefreshAttack ||
                meleeDecision_.action==CombatRecoveryAction::ReengageAttack)
            {
                IssueLivenessAttack(world,target,tick,
                    meleeDecision_.action==CombatRecoveryAction::RefreshAttack);
            }
            else if (!attackStarted_ && !meleeLiveness_.Pending() && meleeLiveness_.Repairs()==0)
            {
                // Initial no-slot bootstrap: unknown latch is not active/false
                // evidence. One start is permitted; verification stays pending.
                Debug::Logger::Info("COMBAT INIT GATE state=attack_start guid="+
                    Hex64(lockedGuid_)+" selectedGuid="+Hex64(ClientSelectedGuid(world))+
                    " attackKnown="+(meleeActionEvidence_.attack.actionSlotFound ? "yes" : "no")+
                    " attackActive=no decision=start reason=initial_melee_bootstrap");
                IssueLivenessAttack(world,target,tick,false,
                    !meleeActionEvidence_.known);
            }

            return true;
        }

    public:
        bool LivingDefenseActive() const
        {
            return state_==CombatState::WaitingForTargetSelection ||
                state_==CombatState::WarriorChargeFacing || state_==CombatState::WarriorOpening ||
                state_==CombatState::Chasing || state_==CombatState::Fighting ||
                state_==CombatState::DefensiveContainment;
        }
        // Only the exact freshly observed attacker may be adopted. No fallback
        // selected target, pull selector, loot, recovery or quest owner is run.
        void UpdateLivingDefense(const Objects::WorldState& world, std::uint64_t tick,
            std::uint64_t attacker, bool disengaged)
        {
            if (disengaged && LivingDefenseActive())
            {
                AutoAttackController::Stop();
                chase_.Stop(); defensiveRoute_.reset(); recovery_.Reset();
                ResetTargetState();
                SetState(CombatState::AcquiringTarget);
                Debug::Logger::Info("DEATH LIVING defense=released reason=fresh_disengagement");
                return;
            }
            livingDefenseOnly_=true;
            livingDefenseGuid_=attacker;
            if (LivingDefenseActive()) Update(world,tick);
            else if (attacker && (state_==CombatState::Idle ||
                state_==CombatState::AcquiringTarget || state_==CombatState::PostKillDelay))
                AdoptExactTargetForDefense(world,attacker,tick);
            livingDefenseGuid_=0;
            livingDefenseOnly_=false;
        }

        // The maintenance owner does not run Update (which could acquire a
        // target). Retire only the elapsed delay; all real combat states and
        // pending corpse ownership remain authoritative.
        void RetireExpiredPostKillDelayForMaintenance(std::uint64_t tick)
        {
            if (!MaintenanceCombatHandoffPolicy::RetireDelay(
                    state_ == CombatState::PostKillDelay, tick, nextAcquireTick_,
                    lockedGuid_ != 0, HasDeferredCorpseLootPending())) return;
            SetState(CombatState::AcquiringTarget);
            Debug::Logger::Info("COMBAT MAINTENANCE HANDOFF reason=expired_post_kill_delay"
                " decision=retire_delay_without_acquisition tick=" + std::to_string(tick));
        }
        bool Start(
            std::uint64_t tick = 0)
        {
            if (
                state_ !=
                    CombatState::Idle)
            {
                return false;
            }

            if (!TargetController::Validate())
            {
                Fail(
                    "TargetController unavailable."
                );

                return false;
            }

            if (!FacingController::Validate())
            {
                Fail(
                    "FacingController unavailable."
                );

                return false;
            }

            if (!AutoAttackController::Validate())
            {
                Fail(
                    "AutoAttackController unavailable."
                );

                return false;
            }

            if (!LootController::Validate())
            {
                Fail(
                    "LootController unavailable."
                );

                return false;
            }

            if (!WarriorRotationController::Validate())
            {
                Fail(
                    "WarriorRotationController unavailable."
                );

                return false;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "COMBAT LOOP v5: START"
            );

            Debug::Logger::Info(
                "quest egress when needed -> target -> CTM chase -> "
                "face -> autoattack + Crossroads Warrior rotation -> "
                "death -> loot -> recovery gate -> on quest complete "
                "return to Gornek -> turn-in dialog -> "
                "supported next-quest pickup"
            );

            Debug::Logger::Info(
                "================================"
            );

            nextAcquireTick_ =
                tick;

            SetState(
                CombatState::
                    AcquiringTarget
            );

            return true;
        }

        void Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (
                state_ ==
                    CombatState::Idle ||
                state_ ==
                    CombatState::Failed)
            {
                return;
            }

            if (!world.valid || !world.player.valid)
                return; // Unknown world is never permission to dispatch recovery.
            if (world.player.playerFlagsKnown && (world.player.playerFlagsRaw&0x10u))
                return; // DeathRecovery owns ghost actions; no combat input.

            if (grindModeActive_)
                ObserveAggressors(world, tick);
            else
                currentCombatTick_ = tick;

            /*
             * Death handling is intentionally explicit.
             * Ghost/corpse recovery is not implemented in
             * this phase.
             */
            if (
                world.player.maxHealth > 0 &&
                world.player.health == 0)
            {
                if (state_==CombatState::DefensiveContainment)
                    return; // DeathRecovery preempts at WorldMonitor.
                Fail(
                    "player health reached zero; "
                    "death recovery is not implemented."
                );

                return;
            }

            if (state_==CombatState::DefensiveContainment)
            {
                UpdateDefensiveContainment(world,tick);
                return;
            }

            /*
             * Emergency health is advisory during an
             * already-active fight. Stopping attacks while
             * a hostile unit is hitting the player would
             * make survival worse, so Phase 5 lets the
             * current fight finish and guarantees that no
             * new target is acquired until recovery.
             */
            if (
                (
                    state_ ==
                        CombatState::WarriorChargeFacing ||
                    state_ ==
                        CombatState::WarriorOpening ||
                    state_ ==
                        CombatState::Chasing ||
                    state_ ==
                        CombatState::Fighting
                ) &&
                !emergencyHealthLatched_ &&
                RecoveryController::
                    HealthPercent(
                        world.player
                    ) <=
                    EmergencyHealthPercent)
            {
                emergencyHealthLatched_ =
                    true;

                ++emergencyHealthEvents_;

                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "SURVIVAL: EMERGENCY HEALTH"
                );

                Debug::Logger::Info(
                    "Player health: " +
                    Float(
                        RecoveryController::
                            HealthPercent(
                                world.player
                            )
                    ) +
                    "%"
                );

                Debug::Logger::Info(
                    "Policy: finish current target; "
                    "block the next pull until "
                    "recovery completes."
                );

                Debug::Logger::Info(
                    "================================"
                );
            }

            // =========================================
            // Recovery
            // =========================================

            if (
                state_ ==
                    CombatState::Recovering)
            {
                /*
                 * Phase 14K.1.6:
                 *
                 * Recovery owns the bot only while the player is actually safe.
                 * A mob can acquire us after recovery has already started. The
                 * old branch returned unconditionally from Recovering, so the
                 * bot could sit at critically low HP while a live aggressor
                 * continued to damage it.
                 *
                 * Reuse the existing direct/recent aggressor policy rather than
                 * reacting to an arbitrary selected target. Release recovery,
                 * retain the emergency-health latch, and hand ownership back to
                 * AcquiringTarget. Its existing direct-aggressor priority then
                 * performs SetTarget/chase/autoattack on the next fresh world
                 * snapshot.
                 */
                if (grindModeActive_)
                {
                    const auto* aggressor =
                        FindBestDirectAggressor(world);

                    if (aggressor != nullptr)
                    {
                        const bool liveTargetingPlayer =
                            aggressor->targetGuid ==
                                world.activePlayerGuid;

                        ++recoveryAggressorPreemptions_;

                        Debug::Logger::Info(
                            "ROBUSTNESS 14K.1.6: RECOVERY AGGRESSOR PREEMPT"
                            " guid=" + Hex64(aggressor->guid) +
                            " entry=" + std::to_string(aggressor->entryId) +
                            " distance=" + Float(aggressor->distance) +
                            " hp=" + std::to_string(world.player.health) +
                            "/" + std::to_string(world.player.maxHealth) +
                            (liveTargetingPlayer
                                ? " evidence=live-targeting-player"
                                : " evidence=recent-aggressor-memory") +
                            "; releasing passive recovery so defensive combat can resume."
                        );

                        recovery_.Reset();
                        ResetTargetState();

                        /*
                         * Keep the emergency condition latched across the
                         * recovery -> defensive-combat handoff. Once the
                         * aggressor is gone, the ordinary post-combat recovery
                         * gate is allowed to reclaim ownership before a new pull.
                         */
                        emergencyHealthLatched_ =
                            true;

                        nextAcquireTick_ =
                            tick;

                        SetState(
                            CombatState::AcquiringTarget
                        );

                        return;
                    }
                }

                recovery_.Update(
                    world.player,
                    tick
                );

                if (recovery_.IsDead())
                {
                    Fail(
                        "player died during recovery."
                    );

                    return;
                }

                if (recovery_.IsReady())
                {
                    recovery_.Reset();

                    nextAcquireTick_ =
                        tick;

                    emergencyHealthLatched_ =
                        false;

                    SetState(
                        CombatState::
                            AcquiringTarget
                    );
                }

                return;
            }

            // =========================================
            // Quest travel / questgiver-area egress
            // =========================================

            if (
                state_ ==
                    CombatState::QuestTravel)
            {
                questTravel_.Update(
                    world.player,
                    tick
                );

                if (
                    questTravel_.IsActive())
                {
                    return;
                }

                if (
                    questTravel_.IsDone())
                {
                    questTravel_.Reset();

                    nextAcquireTick_ =
                        tick +
                        1;

                    SetState(
                        CombatState::
                            AcquiringTarget
                    );

                    return;
                }

                if (
                    questTravel_.Failed())
                {
                    Fail(
                        "Valley of Trials quest egress "
                        "route failed."
                    );

                    return;
                }

                return;
            }

            // =========================================
            // Quest 789 return route
            // =========================================

            if (
                state_ ==
                    CombatState::QuestReturn)
            {
                questReturn_.Update(
                    world.player,
                    tick
                );

                if (
                    questReturn_.IsActive())
                {
                    return;
                }

                if (
                    questReturn_.IsDone())
                {
                    questReturn_.Reset();

                    if (!questTurnIn_.Start(
                            tick))
                    {
                        Fail(
                            "QuestTurnInController failed "
                            "to start after return route."
                        );

                        return;
                    }

                    SetState(
                        CombatState::
                            QuestTurnIn
                    );

                    return;
                }

                if (
                    questReturn_.Failed())
                {
                    Fail(
                        "Valley of Trials quest return "
                        "route failed."
                    );

                    return;
                }

                return;
            }

            // =========================================
            // Quest 789 turn-in interaction
            // =========================================

            if (
                state_ ==
                    CombatState::QuestTurnIn)
            {
                /*
                 * QuestStateReader is authoritative for
                 * verifying that the quest actually left
                 * the quest log.
                 *
                 * In Phase 8B the turn-in controller may
                 * claim the expected Battleworn Cape
                 * automatically. QuestStateReader remains
                 * authoritative: only disappearance of 789
                 * from the quest log records a verified
                 * turn-in.
                 */
                if (!stingOfTheScorpidActive_)
                {
                    questTurnIn_.MarkQuestRemoved();

                    questTurnIn_.Reset();

                    nextAcquireTick_ =
                        tick +
                        1;

                    SetState(
                        CombatState::
                            AcquiringTarget
                    );

                    return;
                }

                questTurnIn_.Update(
                    world,
                    tick
                );

                if (
                    questTurnIn_.Failed())
                {
                    Fail(
                        "QuestTurnInController failed."
                    );

                    return;
                }

                /*
                 * WaitingForQuestRemoval deliberately
                 * holds the combat loop until the quest log
                 * confirms the turn-in. RewardChoiceRequired
                 * remains a safe fallback if the expected
                 * reward cannot be identified.
                 */
                return;
            }

            // =========================================
            // Quest 792 pickup at Zureetha
            // =========================================

            if (
                state_ ==
                    CombatState::QuestPickup)
            {
                /*
                 * QuestStateReader is authoritative for
                 * confirming that AcceptQuest() really added
                 * Vile Familiars to the quest log.
                 */
                if (vileFamiliarsActive_)
                {
                    questPickup_.MarkQuestActive();

                    questPickup_.Reset();

                    questPickupAttempted_ =
                        true;

                    nextAcquireTick_ =
                        tick +
                        1;

                    SetState(
                        CombatState::
                            AcquiringTarget
                    );

                    return;
                }

                questPickup_.Update(
                    world,
                    tick
                );

                if (
                    questPickup_.IsUnavailable())
                {
                    Debug::Logger::Info(
                        "QUEST PICKUP: Phase 9A finished "
                        "without accepting a quest."
                    );

                    questPickup_.Reset();

                    questPickupAttempted_ =
                        true;

                    nextAcquireTick_ =
                        tick +
                        1;

                    SetState(
                        CombatState::
                            AcquiringTarget
                    );

                    return;
                }

                if (
                    questPickup_.Failed())
                {
                    Fail(
                        "QuestPickupController failed."
                    );

                    return;
                }

                return;
            }

            // =========================================
            // Quest 792 objective travel
            // =========================================

            if (
                state_ ==
                    CombatState::QuestObjectiveTravel)
            {
                questObjectiveTravel_.Update(
                    world.player,
                    tick
                );

                if (
                    questObjectiveTravel_.IsActive())
                {
                    return;
                }

                if (
                    questObjectiveTravel_.IsDone())
                {
                    vileFamiliarsTravelComplete_ =
                        true;

                    nextAcquireTick_ =
                        tick +
                        1;

                    SetState(
                        CombatState::
                            AcquiringTarget
                    );

                    return;
                }

                if (
                    questObjectiveTravel_.Failed())
                {
                    Fail(
                        "QuestObjectiveTravelController failed."
                    );

                    return;
                }

                return;
            }

            // =========================================
            // Loot dead target before acquiring another
            // =========================================

            if (
                state_ ==
                    CombatState::Looting)
            {
                loot_.Update(
                    world.player,
                    tick
                );

                if (loot_.IsActive())
                {
                    return;
                }

                if (loot_.IsFinished())
                {
                    if (loot_.Succeeded())
                    {
                        ++lootsSucceeded_;

                        Debug::Logger::Info(
                            "COMBAT LOOP: loot "
                            "completed successfully."
                        );
                    }
                    else if (loot_.NoLoot())
                    {
                        ++lootsSkipped_;

                        Debug::Logger::Info(
                            "COMBAT LOOP: corpse had "
                            "no observable loot window; "
                            "continuing."
                        );
                    }
                    else
                    {
                        ++lootsFailed_;

                        Debug::Logger::Info(
                            "COMBAT LOOP: loot failed; "
                            "continuing to next target."
                        );
                    }

                    Debug::Logger::Info(
                        "Loot totals: success=" +
                        std::to_string(
                            lootsSucceeded_
                        ) +
                        " skipped=" +
                        std::to_string(
                            lootsSkipped_
                        ) +
                        " failed=" +
                        std::to_string(
                            lootsFailed_
                        )
                    );

                    loot_.Reset();

                    EnterPostTargetDelay(
                        tick
                    );
                }

                return;
            }

            // =========================================
            // Post-kill / post-loot delay
            // =========================================

            if (
                state_ ==
                    CombatState::PostKillDelay)
            {
                if (
                    tick >=
                        nextAcquireTick_)
                {
                    if (grindModeActive_ && FindBestDirectAggressor(world) != nullptr)
                    {
                        Debug::Logger::Info(
                            "GRIND 14G.1.2: post-kill delay bypassed because another mob is attacking.");
                        SetState(CombatState::AcquiringTarget);
                    }
                    else
                    {
                        if (
                            grindModeActive_ &&
                            TryStartDeferredLoot(world, tick))
                        {
                            Debug::Logger::Info(
                                "GRIND 14G.1.4: deferred corpse loot outranks recovery/new pulls after combat pack cleared.");
                            return;
                        }

                        if (TryEnterRecovery(world, tick))
                            return;

                        SetState(CombatState::AcquiringTarget);
                    }
                }
                else
                {
                    return;
                }
            }

            // =========================================
            // Acquire target
            // =========================================

            if (
                state_ ==
                    CombatState::AcquiringTarget)
            {
                if (livingDefenseOnly_)
                {
                    if (livingDefenseGuid_)
                        AdoptExactTargetForDefense(world,livingDefenseGuid_,tick);
                    return;
                }
                if (tick < nextAcquireTick_)
                    return;

                if (grindModeActive_)
                {
                    const auto* aggressor = FindBestDirectAggressor(world);
                    if (aggressor != nullptr)
                    {
                        Debug::Logger::Info(
                            "GRIND 14G.1.2: DIRECT AGGRESSOR PRIORITY guid=" +
                            Hex64(aggressor->guid) +
                            " entry=" + std::to_string(aggressor->entryId) +
                            " distance=" + Float(aggressor->distance));

                        /*
                         * ChaseController still consults TargetSelector's
                         * shared allowlist. Temporarily expose the attacker
                         * entry through the existing planner override so an
                         * unexpected add can be defended against without
                         * adding its creature ID to the grind profile.
                         */
                        SetPlannerQuestTarget(
                            aggressor->entryId,
                            "Defensive multi-aggro");

                        if (ClientSelectedGuid(world) == aggressor->guid)
                            StartLockedTarget(world, *aggressor, tick);
                        else
                            IssueTargetSelection(*aggressor, tick);
                        return;
                    }

                    if (TryStartDeferredLoot(world, tick))
                        return;
                }

                if (TryEnterRecovery(world, tick))
                    return;

                /*
                 * Phase 8B:
                 *
                 * Once Sting of the Scorpid is complete,
                 * combat acquisition stops. Return to
                 * Gornek through the reverse of the
                 * verified 7B.4 route, then begin the
                 * turn-in interaction state machine.
                 */
                if (
                    !grindModeActive_ &&
                    !plannerQuestTargetActive_ &&
                    questPolicyReady_ &&
                    stingOfTheScorpidActive_ &&
                    stingOfTheScorpidComplete_)
                {
                    ResetTargetState();

                    if (
                        QuestReturnController::
                            ShouldStart(
                                world.player
                            ))
                    {
                        if (
                            questReturn_.Start(
                                world.player,
                                tick
                            ))
                        {
                            SetState(
                                CombatState::
                                    QuestReturn
                            );

                            return;
                        }

                        Fail(
                            "QuestReturnController failed "
                            "to start."
                        );

                        return;
                    }

                    if (!questTurnIn_.Start(
                            tick))
                    {
                        Fail(
                            "QuestTurnInController failed "
                            "to start at Gornek."
                        );

                        return;
                    }

                    SetState(
                        CombatState::
                            QuestTurnIn
                    );

                    return;
                }

                /*
                 * Phase 9A: after the currently supported
                 * 788/789 flow is no longer active, probe
                 * Zureetha for the exact quest
                 * "Vile Familiars".
                 *
                 * This phase accepts only quest 792 and does
                 * not yet begin its combat objective.
                 */
                if (
                    !grindModeActive_ &&
                    !plannerQuestTargetActive_ &&
                    questPolicyReady_ &&
                    !questPickupAttempted_ &&
                    !cuttingTeethActive_ &&
                    !stingOfTheScorpidActive_ &&
                    !vileFamiliarsActive_ &&
                    QuestPickupController::
                        ShouldStart(
                            world.player
                        ))
                {
                    ResetTargetState();

                    if (
                        questPickup_.Start(
                            world.player,
                            tick
                        ))
                    {
                        SetState(
                            CombatState::
                                QuestPickup
                        );

                        return;
                    }

                    Fail(
                        "QuestPickupController failed "
                        "to start."
                    );

                    return;
                }

                /*
                 * Phase 9B:
                 *
                 * Once Vile Familiars is active, route from
                 * Zureetha/The Den to the objective area
                 * before allowing entry 3101 acquisition.
                 */
                if (
                    !grindModeActive_ &&
                    !plannerQuestTargetActive_ &&
                    questPolicyReady_ &&
                    vileFamiliarsActive_ &&
                    !vileFamiliarsComplete_ &&
                    !vileFamiliarsTravelComplete_)
                {
                    ResetTargetState();

                    if (
                        QuestObjectiveTravelController::
                            ShouldStart(
                                world.player,
                                desiredQuestEntry_
                            ))
                    {
                        if (
                            questObjectiveTravel_.Start(
                                world.player,
                                tick
                            ))
                        {
                            SetState(
                                CombatState::
                                    QuestObjectiveTravel
                            );

                            return;
                        }

                        Fail(
                            "QuestObjectiveTravelController "
                            "failed to start."
                        );

                        return;
                    }

                    /*
                     * A restarted bot may already be inside
                     * the objective area.
                     */
                    vileFamiliarsTravelComplete_ =
                        true;

                    Debug::Logger::Info(
                        "QUEST 792 TRAVEL: player already "
                        "inside objective-area radius; "
                        "combat acquisition may begin."
                    );
                }

                /*
                 * A direct CTM chase is not pathfinding.
                 *
                 * When Sting of the Scorpid is active and
                 * the player is still beside the Valley of
                 * Trials questgivers, leave that area first
                 * through the verified egress corridor.
                 *
                 * Only after the corridor is complete may
                 * normal target selection begin.
                 */
                if (
                    !grindModeActive_ &&
                    !plannerQuestTargetActive_ &&
                    QuestTravelController::
                        ShouldStart(
                            world.player,
                            desiredQuestEntry_
                        ))
                {
                    ResetTargetState();

                    if (
                        questTravel_.Start(
                            world.player,
                            tick
                        ))
                    {
                        SetState(
                            CombatState::
                                QuestTravel
                        );

                        return;
                    }

                    Fail(
                        "QuestTravelController failed "
                        "to start."
                    );

                    return;
                }

                // A client-selected non-aggressor has no priority over a
                // safer eligible GUID. Evaluate the whole local candidate set
                // before accepting an existing UI target or issuing SetTarget.
                const TargetCandidate candidate =
                    SelectCandidate(
                        world,
                        tick
                    );

                if (!candidate.found)
                {
                    // Packed groups can disperse; defer briefly, then
                    // re-evaluate rather than permanently blacklisting a GUID
                    // or scanning the same unsafe set every monitor tick.
                    if (lastPullDecision_ == PullSafetyDecision::NoSafeCandidate ||
                        lastPullDecision_ == PullSafetyDecision::WaitForHealth)
                        nextAcquireTick_ = tick + 4;
                    if (
                        lastPullDecision_ == PullSafetyDecision::NoEligibleCandidate &&
                        vileFamiliarsActive_ &&
                        !vileFamiliarsComplete_ &&
                        vileFamiliarsTravelComplete_)
                    {
                        questObjectiveTravel_.Patrol(
                            world.player,
                            tick
                        );
                    }

                    return;
                }

                /*
                 * If WorldState already reports the
                 * candidate as selected, no SetTarget
                 * round-trip is needed.
                 */
                if (
                    ClientSelectedGuid(world) ==
                        candidate.unit.guid)
                {
                    if (!ValidateSelectedGrindTarget(
                            world,
                            candidate.unit,
                            tick))
                    {
                        BeginAcquire(tick + 1);
                        return;
                    }

                    StartLockedTarget(
                        world,
                        candidate.unit,
                        tick
                    );

                    return;
                }

                IssueTargetSelection(
                    candidate.unit,
                    tick
                );

                return;
            }

            // =========================================
            // Wait for SetTarget to become observable
            // =========================================

            if (
                state_ ==
                    CombatState::
                        WaitingForTargetSelection)
            {
                const TargetCandidate safest = SelectCandidate(world, tick);
                if (!safest.found ||
                    safest.unit.guid != pendingTargetGuid_)
                {
                    BeginAcquire(tick + 1);
                    return;
                }
                const auto* pending =
                    TargetSelector::
                        FindByGuid(
                            world,
                            pendingTargetGuid_
                        );

                if (
                    !IsCombatCandidateUsable(
                        world,
                        pending) ||
                    !MatchesCurrentCombatPolicy(
                        world,
                        pending))
                {
                    Debug::Logger::Info(
                        "Pending target disappeared, "
                        "became invalid, or no longer "
                        "matches the active quest objective."
                    );

                    BeginAcquire(
                        tick +
                        1
                    );

                    return;
                }

                if (
                    ClientSelectedGuid(world) ==
                        pendingTargetGuid_)
                {
                    if (!ValidateSelectedGrindTarget(world, *pending, tick))
                    {
                        BeginAcquire(tick + 1);
                        return;
                    }

                    StartLockedTarget(
                        world,
                        *pending,
                        tick
                    );

                    return;
                }

                const bool retryReady =
                    tick >=
                        lastTargetCommandTick_ +
                        TargetRetryTicks;

                if (retryReady)
                {
                    if (selectionAttemptsForGuid_ >= MaximumConsecutiveTargetFailures)
                    {
                        const auto e=CombatClientEvidence5875::Execution(world,*pending);
                        const auto action=AutoAttackController::ProbeCombatAction();
                        CombatSelectionTimeoutEvidence timeout{};
                        timeout.optionalGrind=grindModeActive_ && !plannerQuestTargetActive_ &&
                            !vileFamiliarsActive_;
                        timeout.mandatoryObjective=plannerQuestTargetActive_ &&
                            !grindModeActive_ && !vileFamiliarsActive_;
                        timeout.known=e.known && e.aggressorsKnown &&
                            world.player.valid && world.player.health>0;
                        timeout.hostileEngaged=e.PlayerCombat() || e.aggressor ||
                            e.targetVictim==world.activePlayerGuid ||
                            FindBestDirectAggressor(world)!=nullptr;
                        timeout.attackKnown=action.known && action.attack.valid &&
                            action.attack.actionSlotFound && action.inputSafe && !action.waiting;
                        timeout.attackActive=action.attack.active;
                        const auto result=DecideSelectionTimeout(timeout);
                        Debug::Logger::Info("COMBAT TERMINAL DECISION guid="+
                            Hex64(pendingTargetGuid_)+" entry="+std::to_string(pending->entryId)+
                            " reason=target_selection_timeout attempts="+
                            std::to_string(selectionAttemptsForGuid_)+
                            " decision="+(result==CombatSelectionTimeoutAction::AbandonOptional
                                ? "optional_abandon" : result==CombatSelectionTimeoutAction::FailMandatoryOwner
                                    ? "mandatory_owner_failure" : "system_fail"));
                        if (result==CombatSelectionTimeoutAction::AbandonOptional)
                        {
                            BlacklistTarget(pendingTargetGuid_,tick,
                                "target_selection_timeout: verified safe optional abandonment");
                            BeginAcquire(tick+1);
                        }
                        else if (result==CombatSelectionTimeoutAction::FailMandatoryOwner)
                        {
                            ownerTargetFailureGuid_=pendingTargetGuid_;
                            ownerTargetFailureEntry_=pending->entryId;
                            ownerTargetFailureReason_="mandatory_target_selection_timeout";
                            BeginAcquire(tick+1);
                        }
                        else Fail("target_selection_timeout: active_hostile_or_unknown",false);
                        return;
                    }
                    IssueTargetSelection(
                        *pending,
                        tick
                    );
                }

                return;
            }

            // =========================================
            // Active target
            // =========================================

            if (
                state_ !=
                    CombatState::WarriorChargeFacing &&
                state_ !=
                    CombatState::WarriorOpening &&
                state_ !=
                    CombatState::Chasing &&
                state_ !=
                    CombatState::Fighting)
            {
                return;
            }

            const auto* target =
                TargetSelector::
                    FindByGuid(
                        world,
                        lockedGuid_
                    );

            if (target == nullptr || !target->valid)
            {
                Debug::Logger::Info("COMBAT TARGET TERMINAL guid="+Hex64(lockedGuid_)+
                    " reason=target_unloaded outcome=not_killed");
                FinishTarget(
                    world,
                    nullptr,
                    tick,
                    false
                );

                return;
            }

            if (
                target->health == 0 ||
                target->maxHealth == 0)
            {
                const bool confirmedDead=target->health==0;
                Debug::Logger::Info("COMBAT TARGET TERMINAL guid="+Hex64(lockedGuid_)+
                    " reason="+(confirmedDead ? "target_dead" : "target_invalid")+
                    " outcome="+(confirmedDead ? "verified_kill" : "not_killed"));
                if (target->health==0 && meleeLiveness_.Pending())
                    Debug::Logger::Info("COMBAT RECOVERY VERIFY targetGuid="+Hex64(lockedGuid_)+
                        " result=confirmed evidence=target_died");
                FinishTarget(
                    world,
                    target,
                    tick,
                    confirmedDead
                );

                return;
            }

            ObserveMeleeLiveness(world,*target,tick);
            if (meleeTerminalPending_ ||
                state_==CombatState::DefensiveContainment ||
                state_==CombatState::Failed ||
                state_==CombatState::AcquiringTarget)
                return;

            // An unknown Attack-action readback is not evidence that Charge
            // facing or physical chase stopped making progress. Keep those
            // bounded FSMs advancing; authorize offensive input separately.
            AutoAttackController::CombatActionEvidence alternateInput{};
            if (!meleeActionEvidence_.known && lastMeleeTargetFresh_)
                alternateInput=AutoAttackController::ProbeCombatBootstrapInput();
            const std::string attackReadback=meleeActionEvidence_.attackReason+":"+
                (meleeActionEvidence_.attack.valid && meleeActionEvidence_.attack.actionSlotFound
                    ? (meleeActionEvidence_.attack.active ? "active" : "inactive") : "unknown");
            if (lockedGuid_!=lastInitGateGuid_ || attackReadback!=lastAttackReadback_ ||
                meleeActionEvidence_.reason!=lastActionProbeReason_ ||
                alternateInput.reason!=lastBootstrapProbeReason_)
            {
                lastAttackReadback_=attackReadback;
                Debug::Logger::Info("COMBAT ATTACK READBACK guid="+Hex64(lockedGuid_)+
                    " evidence="+attackReadback+" inputPermission=independent swing=unknown");
                lastActionProbeReason_=meleeActionEvidence_.reason;
                lastBootstrapProbeReason_=alternateInput.reason;
                Debug::Logger::Info("COMBAT INPUT PROBE mode=action result="+
                    std::string(meleeActionEvidence_.known ?
                        (meleeActionEvidence_.waiting ? "wait" :
                            (meleeActionEvidence_.inputSafe ? "ready" : "blocked")) : "unknown")+
                    " reason="+meleeActionEvidence_.reason+" guid="+Hex64(lockedGuid_));
                if (!meleeActionEvidence_.known && lastMeleeTargetFresh_)
                    Debug::Logger::Info("COMBAT INPUT PROBE mode=bootstrap result="+
                        std::string(CombatInputProbePolicy::Name(
                            CombatInputProbePolicy::Classify(alternateInput.reason)))+
                        " reason="+alternateInput.reason+" guid="+Hex64(lockedGuid_));
            }
            const auto initPhase=state_==CombatState::Fighting
                ? CombatInitiationPhase::Melee : state_==CombatState::Chasing ||
                    state_==CombatState::WarriorOpening
                    ? CombatInitiationPhase::Chase : CombatInitiationPhase::ChargeFacing;
            const CombatInitiationEvidence initEvidence{
                lastMeleeTargetFresh_,
                lastMeleeSelectionKnown_,lastMeleeSelectedGuid_==lockedGuid_,
                meleeActionEvidence_.known,meleeActionEvidence_.inputSafe,
                meleeActionEvidence_.waiting,alternateInput.known,alternateInput.inputSafe,
                alternateInput.waiting};
            const auto initGate=CombatInitiationPolicy::Decide(initPhase,initEvidence);
            if (lockedGuid_!=lastInitGateGuid_ || state_!=lastInitGateState_ ||
                initGate.reason!=lastInitGateReason_ ||
                initGate.offensiveInputAllowed!=lastInitGateOffenseAllowed_)
            {
                lastInitGateGuid_=lockedGuid_;
                lastInitGateState_=state_;
                lastInitGateReason_=initGate.reason;
                lastInitGateOffenseAllowed_=initGate.offensiveInputAllowed;
                Debug::Logger::Info("COMBAT INIT GATE state="+std::string(StateNameInternal(state_))+
                    " guid="+Hex64(lockedGuid_)+" selectedGuid="+Hex64(lastMeleeSelectedGuid_)+
                    " distance="+Float(target->distance)+
                    " selectionFresh="+(initEvidence.selectionKnown ? "yes" : "no")+
                    " targetFresh="+(initEvidence.targetFresh ? "yes" : "no")+
                    " alignedCount="+std::to_string(warriorChargeFacingStableSnapshots_)+
                    " actionEvidence="+meleeActionEvidence_.reason+
                    " bootstrapInput="+alternateInput.reason+
                    " decision="+(initGate.stateProgressAllowed ?
                        (initGate.offensiveInputAllowed ? "advance" : "advance_no_offense") : "wait")+
                    " stateDecision="+(initGate.stateProgressAllowed ?
                        (initPhase==CombatInitiationPhase::Melee &&
                         CombatInitiationPolicy::ResumeChase(target->distance) ?
                            "resume_chase" : "reconcile") : "hold")+
                    " offenseDecision="+(initGate.offensiveInputAllowed ? "eligible" : "blocked")+
                    " reason="+CombatInitiationReasonName(initGate.reason));
            }
            MaintainTargetSelection(
                world,
                *target,
                tick
            );
            if (!initGate.stateProgressAllowed)
                return;

            // =========================================
            // Phase 14G.5.2.2: bounded Charge facing acquisition
            // =========================================

            if (
                state_ ==
                    CombatState::WarriorChargeFacing)
            {
                /*
                 * MaintainTargetSelection() above may have just restored the
                 * target, but this WorldState is still the pre-command
                 * snapshot. Never cast Charge against a stale UI target.
                 */
                if (
                    ClientSelectedGuid(world) !=
                        lockedGuid_)
                {
                    warriorChargeFacingStableSnapshots_ =
                        CombatInitiationPolicy::ConfirmChargeFacing(
                            warriorChargeFacingStableSnapshots_,false,false);

                    if (
                        tick >=
                            warriorChargeFacingUntilTick_)
                    {
                        Debug::Logger::Info(
                            "CHARGE FACING 14G.5.2.2: timeout while waiting for selected-target restore; using CTM chase fallback."
                        );

                        warriorChargeFacingUntilTick_ =
                            0;

                        warriorChargeFacingCommandsForTarget_ =
                            0;

                        if (!StartChaseForLockedTarget(
                                world,
                                *target,
                                tick))
                        {
                            return;
                        }
                    }

                    return;
                }

                if (
                    !warrior_.CanPrepareCharge(
                        world.player,
                        *target,
                        true))
                {
                    Debug::Logger::Info(
                        "CHARGE FACING 14G.5.2.2: opener eligibility lost before cast (distance=" +
                        Float(target->distance) +
                        "); using CTM chase fallback."
                    );

                    warriorChargeFacingUntilTick_ =
                        0;

                    warriorChargeFacingStableSnapshots_ =
                        CombatInitiationPolicy::ConfirmChargeFacing(
                            warriorChargeFacingStableSnapshots_,true,false);

                    warriorChargeFacingCommandsForTarget_ =
                        0;

                    if (!StartChaseForLockedTarget(
                            world,
                            *target,
                            tick))
                    {
                        return;
                    }

                    return;
                }

                const float desired =
                    FacingController::
                        CalculateFacing(
                            world.player,
                            *target
                        );

                const float delta =
                    FacingController::
                        AngularDifference(
                            world.player.rotation,
                            desired
                        );

                const bool chargeFacingReady =
                    CombatFacingPolicy::
                        IsAbilityFacingReady(
                            delta
                        );

                if (!chargeFacingReady)
                {
                    warriorChargeFacingStableSnapshots_ =
                        CombatInitiationPolicy::ConfirmChargeFacing(
                            warriorChargeFacingStableSnapshots_,true,false);

                    const bool timedOut =
                        tick >=
                            warriorChargeFacingUntilTick_;

                    const bool commandBudgetExhausted =
                        warriorChargeFacingCommandsForTarget_ >=
                            WarriorChargeFacingMaximumCommands;

                    if (
                        timedOut ||
                        commandBudgetExhausted)
                    {
                        Debug::Logger::Info(
                            "CHARGE FACING 14G.5.2.2: bounded acquisition exhausted; using CTM chase fallback. delta=" +
                            Float(delta) +
                            " commands=" +
                            std::to_string(
                                warriorChargeFacingCommandsForTarget_)
                        );

                        warriorChargeFacingUntilTick_ =
                            0;

                        warriorChargeFacingCommandsForTarget_ =
                            0;

                        if (!StartChaseForLockedTarget(
                                world,
                                *target,
                                tick))
                        {
                            return;
                        }

                        return;
                    }

                    const bool facingCooldownReady =
                        facingCommands_ == 0 ||
                        tick >=
                            lastFacingCommandTick_ +
                            FacingCooldownTicks;

                    if (facingCooldownReady)
                    {
                        float issuedDesired =
                            0.0f;

                        if (
                            FacingController::Face(
                                world.player,
                                *target,
                                &issuedDesired
                            ))
                        {
                            lastFacingCommandTick_ =
                                tick;

                            ++facingCommands_;

                            ++warriorChargeFacingCommandsForTarget_;

                            Debug::Logger::Info(
                                "CHARGE FACING 14G.5.2.2: correction issued delta=" +
                                Float(delta) +
                                " command=" +
                                std::to_string(
                                    warriorChargeFacingCommandsForTarget_)
                            );
                        }
                        else
                        {
                            Debug::Logger::Info(
                                "CHARGE FACING 14G.5.2.2: correction command failed; bounded acquisition continues."
                            );
                        }
                    }

                    return;
                }

                warriorChargeFacingStableSnapshots_ =
                    CombatInitiationPolicy::ConfirmChargeFacing(
                        warriorChargeFacingStableSnapshots_,true,true);

                if (
                    warriorChargeFacingStableSnapshots_ <
                        CombatFacingPolicy::
                            StableSnapshotsRequired)
                {
                    Debug::Logger::Info(
                        "CHARGE FACING 14G.5.2.2: aligned snapshot confirmed once; waiting for one fresh confirmation."
                    );

                    return;
                }

                if (!initGate.offensiveInputAllowed)
                {
                    if (tick >= warriorChargeFacingUntilTick_)
                    {
                        Debug::Logger::Info("COMBAT INIT GATE state=charge_facing decision=fallback_melee reason=input_evidence_unavailable");
                        warriorChargeFacingUntilTick_=0;
                        warriorChargeFacingStableSnapshots_=0;
                        warriorChargeFacingCommandsForTarget_=0;
                        StartChaseForLockedTarget(world,*target,tick);
                    }
                    return;
                }

                Debug::Logger::Info(
                    "CHARGE FACING 14G.5.2.2: facing verified on consecutive snapshots; issuing Charge."
                );

                const bool chargeIssued =
                    warrior_.TryCharge(
                        world.player,
                        *target,
                        tick,
                        true,
                        true
                    );

                warriorChargeFacingUntilTick_ =
                    0;

                warriorChargeFacingStableSnapshots_ =
                    0;

                warriorChargeFacingCommandsForTarget_ =
                    0;

                if (chargeIssued)
                {
                    Debug::Logger::Info(
                        "GRIND 14G.5.2.2: Charge issued after bounded facing acquisition; autoattack latch scheduled for first melee snapshot."
                    );

                    warriorOpenerUntilTick_ =
                        tick +
                        WarriorOpenerWaitTicks;

                    SetState(
                        CombatState::
                            WarriorOpening
                    );

                    return;
                }

                Debug::Logger::Info(
                    "CHARGE FACING 14G.5.2.2: Charge command was not issued after facing verification; using CTM chase fallback."
                );

                if (!StartChaseForLockedTarget(
                        world,
                        *target,
                        tick))
                {
                    return;
                }

                return;
            }

            // =========================================
            // Warrior opener window
            // =========================================

            if (
                state_ ==
                    CombatState::WarriorOpening)
            {
                warrior_.ObserveCharge(
                    *target
                );

                /*
                 * Phase 14G.1.4:
                 * movement evidence is represented by the live
                 * target now being inside melee distance. Do not
                 * wait out the full opener timeout after a real
                 * Charge landing; establish ChaseController's
                 * InRange state and run melee immediately.
                 */
                if (target->distance <= PostChargeImmediateMeleeDistance)
                {
                    Debug::Logger::Info(
                        "GRIND 14G.1.4: POST-CHARGE MELEE HANDOFF distance=" +
                        Float(target->distance));

                    warrior_.FinishChargeWindow(*target);
                    warriorOpenerUntilTick_ = 0;

                    if (!StartChaseForLockedTarget(
                            world,
                            *target,
                            tick))
                    {
                        return;
                    }

                    if (state_ == CombatState::Fighting)
                    {
                        if (!initGate.offensiveInputAllowed)
                            return;
                        const bool facingReady =
                            MaintainFacingAndAttack(world,*target,tick);

                        if (attackStarted_)
                        {
                            Debug::Logger::Info(
                                "GRIND 14G.1.4: POST-CHARGE AUTOATTACK LATCH active; normal melee rotation resumed.");
                        }

                        warrior_.Update(
                            world.player,
                            *target,
                            tick,
                            attackStarted_,
                            true,
                            ClientSelectedGuid(world) == lockedGuid_,
                            facingReady,
                            CountDirectAggressorsWithin(
                                world,
                                ThunderClapAggressorRadius));
                    }

                    return;
                }

                if (
                    tick <
                        warriorOpenerUntilTick_)
                {
                    return;
                }

                warrior_.FinishChargeWindow(
                    *target
                );

                warriorOpenerUntilTick_ =
                    0;

                if (!StartChaseForLockedTarget(
                        world,
                        *target,
                        tick))
                {
                    return;
                }

                /*
                 * Start() can issue a CTM command. Do not
                 * immediately call ChaseController::Update
                 * again in the same polling tick; resume on
                 * the next WorldState snapshot.
                 */
                return;
            }

            const bool wasChasingBeforeUpdate =
                state_ == CombatState::Chasing;

            // A stale chase must never issue movement for a different GUID.
            // Re-enter the existing locked-target start path, with its same
            // validation and failure handling; no new retry budget.
            if (!CombatTargetConsistencyPolicy::ChaseMatches(lockedGuid_,chase_.TargetGuid()))
            {
                Debug::Logger::Info("CONTROL CONFLICT owner=CombatChase combat="+Hex64(lockedGuid_)+
                    " chase="+Hex64(chase_.TargetGuid())+" command=chase_update result=rejected");
                AutoAttackController::Stop(); attackStarted_=false;
                chase_.Stop();
                MovementController::HoldPosition(world.player);
                facingStableSnapshots_=0; facingHoldSnapshots_=0;
                facingDiagnosticGuid_=0;
                StartChaseForLockedTarget(world,*target,tick);
                return;
            }

            chase_.Update(
                world,
                tick
            );

            if (chase_.TargetDead())
            {
                FinishTarget(
                    world,
                    target,
                    tick,
                    true
                );

                return;
            }

            if (chase_.TargetLost())
            {
                FinishTarget(
                    world,
                    nullptr,
                    tick,
                    false
                );

                return;
            }

            if (chase_.Failed())
            {
                BeginChaseTerminal(world,*target,
                    ChaseFailureName(chase_.FailureReason()));
                return;
            }

            if (chase_.IsInRange())
            {
                if (wasChasingBeforeUpdate)
                {
                    Debug::Logger::Info("COMBAT INIT GATE state=chase guid="+
                        Hex64(lockedGuid_)+" distance="+Float(target->distance)+
                        " decision=enter_melee reason=verified_chase_in_range");
                    autoAttackReengagePending_ = true;
                    facingStableSnapshots_ = 0;
                    facingGuardHolding_ = true;

                    Debug::Logger::Info(
                        "AUTOATTACK 14G.5.2: CHASE RETURN -> melee re-engage armed."
                    );
                }

                SetState(
                    CombatState::Fighting
                );

                if (!initGate.offensiveInputAllowed)
                    return; // A later fresh input probe owns melee bootstrap.

                const bool facingReady =
                    MaintainFacingAndAttack(
                        world,
                        *target,
                        tick
                    );

                warrior_.Update(
                    world.player,
                    *target,
                    tick,
                    attackStarted_,
                    chase_.IsInRange(),
                    ClientSelectedGuid(world) ==
                        lockedGuid_,
                    facingReady,
                    CountDirectAggressorsWithin(
                        world,
                        ThunderClapAggressorRadius)
                );
            }
            else
            {
                SetState(
                    CombatState::Chasing
                );
            }
        }

        void PauseMovementForLivingWater()
        {
            // Preserve the same locked target and Attack state. Only ground
            // chase, loot movement and seated recovery are relinquished.
            if (chase_.IsActive()) chase_.Stop();
            if (defensiveRoute_)
            {
                defensiveRoute_->CancelForLivingWater();
                defensiveRouteFailure_="living_water_blocked";
            }
            meleeLiveness_.Pause(GetTickCount64());
            ownershipDeadline_.Pause(GetTickCount64());
            loot_.Reset();
            recovery_.Reset();
        }

        // Called only after the water block owns movement and the monitor has
        // excluded live combat/safety owners. Never resume the reset Idle loot
        // FSM from CombatState::Looting, nor replay deferred pre-water corpses.
        void InvalidateIntentForLivingWater(std::uint64_t tick)
        {
            loot_.Reset();
            recovery_.Reset();
            ResetTargetState();
            deferredCorpses_.clear();
            nextAcquireTick_ = tick + 1;
            SetState(CombatState::AcquiringTarget);
            Debug::Logger::Info("WATER EMERGENCY staleIntent=invalidated"
                " combatTarget=cleared loot=cleared deferredCorpses=cleared");
        }

        void SuspendForDeathRecovery(
            const Objects::PlayerState& player,
            std::uint64_t tick)
        {
            if (attackStarted_)
            {
                AutoAttackController::Stop();
                attackStarted_ = false;
            }

            if (chase_.IsActive())
                chase_.Stop();

            defensiveRoute_.reset();

            if (player.valid)
                MovementController::HoldPosition(player);

            loot_.Reset();
            recovery_.Reset();
            ResetTargetState();
            deferredCorpses_.clear();
            recentAggressorUntil_.clear();
            ownerTargetFailureGuid_=0;
            ownerTargetFailureEntry_=0;
            ownerTargetFailureReason_=nullptr;
            currentCombatTick_ = tick;
            emergencyHealthLatched_ = false;
            consecutiveTargetFailures_ = 0;
            SetState(CombatState::Idle);

            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.2: CombatController suspended; attack/chase/loot/recovery ownership cleared.");
        }

        void ResumeAfterDeathRecovery(
            std::uint64_t tick, bool defenseOnly = false)
        {
            loot_.Reset();
            recovery_.Reset();
            ResetTargetState();
            deferredCorpses_.clear();
            recentAggressorUntil_.clear();
            ownerTargetFailureGuid_=0;
            ownerTargetFailureEntry_=0;
            ownerTargetFailureReason_=nullptr;
            currentCombatTick_ = tick;
            emergencyHealthLatched_ = false;
            consecutiveTargetFailures_ = 0;
            nextAcquireTick_ = tick + 1;
            SetState(CombatState::AcquiringTarget);

            Debug::Logger::Info(
                defenseOnly
                    ? "DEATH LIVING combat=defense_only normalAcquisition=blocked"
                    : "DEATH RECOVERY 14G.4.2: CombatController resumed at AcquiringTarget; normal recovery gate owns low post-resurrection HP.");
        }

        void ResumeAfterLivingRecovery(std::uint64_t tick)
        {
            // Preserve corpses earned in defense; ordinary loot arbitration may
            // consume them after the living owner has actually released.
            recovery_.Reset(); ResetTargetState(); recentAggressorUntil_.clear();
            ClearPlannerQuestTarget();
            nextAcquireTick_=tick+1;
            SetState(CombatState::AcquiringTarget);
        }

        bool ForceAutonomyCombatRecovery(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const std::string& reason)
        {
            if (
                state_ != CombatState::WarriorChargeFacing &&
                state_ != CombatState::WarriorOpening &&
                state_ != CombatState::Chasing &&
                state_ != CombatState::Fighting)
            {
                return false;
            }

            const auto* target =
                TargetSelector::FindByGuid(
                    world,
                    lockedGuid_);

            if (
                target == nullptr ||
                !target->valid ||
                target->health == 0 ||
                target->maxHealth == 0)
            {
                Debug::Logger::Info(
                    "AUTONOMY 14G.4.1: combat hard-stall target vanished; returning to acquisition.");
                BeginAcquire(tick + 1);
                ++autonomyCombatRecoveries_;
                return true;
            }

            // The same-target damage watchdog owns stationary melee repair.
            // Global displacement/churn must not reset its budget or repeat
            // AttackStop/SetTarget/separation against a valid aligned target.
            if (state_==CombatState::Fighting && chase_.IsInRange() &&
                chase_.TargetGuid()==lockedGuid_)
            {
                Debug::Logger::Info("COMBAT LIVENESS owner=same_target_watchdog globalRecovery=deferred reason="+reason);
                return false;
            }

            std::uint32_t& attempts =
                autonomyRecoveryAttemptsByGuid_[target->guid];

            if (attempts < MaximumAutonomyCombatRecoveriesPerTarget)
            {
                ++attempts;
            }
            else
            {
                attempts =
                    MaximumAutonomyCombatRecoveriesPerTarget;
            }

            ++autonomyCombatRecoveries_;

            const bool lowHealthMeleeFinisher =
                IsLowHealthMeleeFinisherTarget(*target);

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "AUTONOMY 14G.4.1: COMBAT HARD-STALL RECOVERY guid=" +
                Hex64(target->guid) +
                " attempt=" + std::to_string(attempts) +
                "/" + std::to_string(MaximumAutonomyCombatRecoveriesPerTarget) +
                " distance=" + Float(target->distance) +
                " hp=" + std::to_string(target->health) +
                "/" + std::to_string(target->maxHealth));
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");

            if (attempts >= MaximumAutonomyCombatRecoveriesPerTarget)
            {
                // The third bounded recovery is exhausted. Do not infer
                // passivity from just this target's victim GUID: another
                // attacker or the player's native combat flag may remain.
                // The normal per-tick terminal policy now requires fresh
                // complete client evidence and stable HP before any release.
                BeginChaseTerminal(world,*target,
                    "optional_chase_recovery_exhausted");
                Debug::Logger::Info("COMBAT CHASE RECOVERY guid="+Hex64(lockedGuid_)+
                    " attempt="+std::to_string(attempts)+
                    " reason=chase_no_physical_progress decision=verify_terminal_owner");
                return true;
            }

            if (attackStarted_ && !lowHealthMeleeFinisher)
            {
                AutoAttackController::Stop();
                attackStarted_ = false;
            }

            if (chase_.IsActive())
                chase_.Stop();

            MovementController::HoldPosition(world.player);

            if (TargetController::SetTarget(target->guid))
                lastTargetCommandTick_ = tick;

            selectedTargetMismatchSnapshots_ = 0;
            facingStableSnapshots_ = 0;
            facingHoldSnapshots_ = 0;
            facingGuardHolding_ = true;
            lastFacingCommandTick_ = 0;
            warriorOpenerUntilTick_ = 0;

            /*
             * Phase 14M.0.2 low-HP finisher ownership:
             *
             * The runtime failure was captured at 5% target HP and 2.26 yd.
             * Do not move away from a nearly-dead melee target or wait for a
             * fresh chase cycle. Re-establish InRange ownership, arm the
             * deterministic Attack latch, and let the normal facing controller
             * continue correcting on fresh snapshots.
             */
            if (lowHealthMeleeFinisher)
            {
                ++lowHealthFinisherHardStallRecoveries_;

                autoAttackReengagePending_ =
                    true;

                if (!StartChaseForLockedTarget(
                        world,
                        *target,
                        tick))
                {
                    Debug::Logger::Info(
                        "COMBAT 14M.0.2: LOW-HP FINISHER hard-stall handoff could not restore chase ownership; normal target failure policy owns recovery."
                    );
                    return true;
                }

                if (chase_.IsInRange())
                {
                    const bool facingReady =
                        MaintainFacingAndAttack(
                            world,
                            *target,
                            tick);

                    Debug::Logger::Info(
                        "COMBAT 14M.0.2: LOW-HP FINISHER HARD-STALL OWNERSHIP hp=" +
                        Float(TargetHealthPercent(*target)) +
                        "% distance=" +
                        Float(target->distance) +
                        " attempt=" +
                        std::to_string(attempts) +
                        "/" +
                        std::to_string(MaximumAutonomyCombatRecoveriesPerTarget) +
                        " autoattack=" +
                        (attackStarted_ ? std::string("latched") : std::string("pending")) +
                        " facingReady=" +
                        (facingReady ? std::string("yes") : std::string("no"))
                    );
                }

                return true;
            }

            /*
             * First hard stall restarts chase/facing ownership. Repeated hard
             * stalls add a bounded separation step before facing resumes.
             */
            if (
                attempts >= 2 &&
                target->distance <= 6.0f &&
                StartCombatSeparationRecovery(
                    world,
                    *target,
                    tick,
                    "AutonomySupervisor repeated combat hard stall"))
            {
                SetState(CombatState::Fighting);
                return true;
            }

            if (!StartChaseForLockedTarget(world, *target, tick))
            {
                Debug::Logger::Info(
                    "AUTONOMY 14G.4.1: combat recovery re-chase could not start; normal target failure policy took ownership.");
                return true;
            }

            return true;
        }

        bool ForceRuntimeSupervisorReset(
            const Objects::WorldState& world,
            std::uint64_t tick,
            const std::string& reason,
            bool escalated)
        {
            const auto* lockedTarget =
                lockedGuid_ == 0
                    ? nullptr
                    : TargetSelector::FindByGuid(world, lockedGuid_);

            // Never abandon a live unit that is actively attacking us. Reuse
            // the combat-specific recovery path so survival retains priority.
            if (lockedTarget != nullptr &&
                lockedTarget->valid &&
                lockedTarget->health > 0 &&
                lockedTarget->targetGuid == world.activePlayerGuid &&
                (state_ == CombatState::WarriorChargeFacing ||
                 state_ == CombatState::WarriorOpening ||
                 state_ == CombatState::Chasing ||
                 state_ == CombatState::Fighting))
            {
                return ForceAutonomyCombatRecovery(
                    world,
                    tick,
                    "RuntimeRobustnessSupervisor: " + reason);
            }

            ++runtimeSupervisorResets_;

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "ROBUSTNESS 14K.0: COMBAT TRANSIENT RESET state=" +
                std::string(StateNameInternal(state_)) +
                " escalated=" + (escalated ? std::string("yes") : std::string("no")));
            Debug::Logger::Info("Reason: " + reason);

            if (attackStarted_)
            {
                AutoAttackController::Stop();
                attackStarted_ = false;
            }

            if (chase_.IsActive())
                chase_.Stop();

            if (world.player.valid)
                MovementController::HoldPosition(world.player);

            if (escalated &&
                lockedTarget != nullptr &&
                lockedTarget->valid &&
                lockedTarget->guid != 0 &&
                lockedTarget->targetGuid != world.activePlayerGuid)
            {
                BlacklistTarget(
                    lockedTarget->guid,
                    tick,
                    "RuntimeRobustnessSupervisor repeated no-progress loop.");
            }

            loot_.Reset();
            recovery_.Reset();
            ResetTargetState();
            consecutiveTargetFailures_ = 0;
            currentCombatTick_ = tick;
            nextAcquireTick_ = tick + (escalated ? 8 : 2);
            SetState(CombatState::AcquiringTarget);

            Debug::Logger::Info(
                "ROBUSTNESS 14K.0: combat ownership reset; target acquisition will restart from a clean transient state.");
            Debug::Logger::Info("================================");
            return true;
        }

        bool AdoptExactTargetForDefense(
            const Objects::WorldState& world,
            std::uint64_t exactGuid,
            std::uint64_t tick)
        {
            const auto* selected =
                TargetSelector::FindByGuid(
                    world,
                    exactGuid
                );

            if (
                exactGuid == 0 ||
                selected == nullptr ||
                !selected->valid ||
                selected->health == 0 ||
                selected->maxHealth == 0 ||
                selected->distance < 0.0f ||
                selected->distance > SelectorMaxDistance)
            {
                Debug::Logger::Info(
                    "DEFENSIVE COMBAT 11B.8: exact GUID is not a usable live WorldState unit."
                );
                return false;
            }

            SetPlannerQuestTarget(
                selected->entryId,
                "Defensive aggro"
            );

            if (
                state_ == CombatState::WarriorChargeFacing ||
                state_ == CombatState::WarriorOpening ||
                state_ == CombatState::Chasing ||
                state_ == CombatState::Fighting ||
                state_ == CombatState::Looting)
            {
                return lockedGuid_ == exactGuid;
            }

            ResetTargetState();
            SetState(CombatState::AcquiringTarget);

            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "DEFENSIVE COMBAT 11B.8: ADOPT EXACT ATTACKER"
            );
            Debug::Logger::Info(
                "GUID: " + Hex64(exactGuid)
            );
            Debug::Logger::Info(
                "Entry: " + std::to_string(selected->entryId)
            );
            Debug::Logger::Info(
                "Distance: " + Float(selected->distance)
            );
            Debug::Logger::Info(
                "Client already selected: " +
                std::string(
                    ClientSelectedGuid(world) == exactGuid
                        ? "yes"
                        : "no"
                )
            );
            Debug::Logger::Info(
                "================================"
            );

            if (ClientSelectedGuid(world) == exactGuid)
            {
                return StartLockedTarget(
                    world,
                    *selected,
                    tick
                );
            }

            return IssueTargetSelection(
                *selected,
                tick
            );
        }

        bool AdoptSelectedTargetForDefense(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            const std::uint64_t selectedGuid =
                ClientSelectedGuid(world);

            const auto* selected =
                TargetSelector::FindByGuid(
                    world,
                    selectedGuid
                );

            if (
                selectedGuid == 0 ||
                selected == nullptr ||
                !selected->valid ||
                selected->guid != selectedGuid ||
                selected->health == 0 ||
                selected->maxHealth == 0 ||
                selected->distance < 0.0f ||
                selected->distance > SelectorMaxDistance)
            {
                Debug::Logger::Info(
                    "DEFENSIVE COMBAT 11B.5: selected target "
                    "is not a usable live WorldState unit."
                );

                return false;
            }

            /*
             * The caller only invokes this after a verified HP drop and a
             * client-target change during planner-owned navigation. That
             * makes the currently selected GUID our bounded defensive signal.
             * Temporarily allow only its entry through the existing combat
             * stack, but lock the exact GUID already selected by WoW.
             */
            SetPlannerQuestTarget(
                selected->entryId,
                "Defensive aggro"
            );

            if (
                state_ == CombatState::WarriorChargeFacing ||
                state_ == CombatState::WarriorOpening ||
                state_ == CombatState::Chasing ||
                state_ == CombatState::Fighting ||
                state_ == CombatState::Looting)
            {
                Debug::Logger::Info(
                    "DEFENSIVE COMBAT 11B.5: combat controller "
                    "already owns an active target."
                );

                return
                    lockedGuid_ == selectedGuid;
            }

            ResetTargetState();

            SetState(
                CombatState::AcquiringTarget
            );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "DEFENSIVE COMBAT 11B.5: ADOPT SELECTED ATTACKER"
            );

            Debug::Logger::Info(
                "GUID: " +
                Hex64(
                    selected->guid
                )
            );

            Debug::Logger::Info(
                "Entry: " +
                std::to_string(
                    selected->entryId
                )
            );

            Debug::Logger::Info(
                "Distance: " +
                Float(
                    selected->distance
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            if (!StartLockedTarget(
                    world,
                    *selected,
                    tick))
            {
                Debug::Logger::Info(
                    "DEFENSIVE COMBAT 11B.5: exact GUID handoff failed."
                );

                return false;
            }

            return true;
        }

        void EnableTemporaryGrindMode(
            const Objects::PlayerState& player,
            float radius = 120.0f)
        {
            grindModeActive_ = true;
            TargetSelector::SetGenericGrindEntriesEnabled(true);
            grindCenterX_ = player.x;
            grindCenterY_ = player.y;
            grindCenterZ_ = player.z;
            grindRadius_ = std::max(30.0f, radius);
            deferredCorpses_.clear();
            recentAggressorUntil_.clear();
            currentCombatTick_ = 0;

            plannerQuestTargetActive_ = false;
            plannerQuestTargetEntry_ = 0;
            plannerQuestTargetName_.clear();
            TargetSelector::SetPlannerAllowedEntry(0);
            desiredQuestEntry_ = 0;
            questPolicyReady_ = true;

            Debug::Logger::Info("================================");
            Debug::Logger::Info("GRIND 14G.2: TEMPORARY GENERIC GRIND MODE ENABLED");
            Debug::Logger::Info(
                "Center=(" + Float(grindCenterX_) + "," +
                Float(grindCenterY_) + "," +
                Float(grindCenterZ_) + ") radius=" +
                Float(grindRadius_));
            Debug::Logger::Info(
                "GRIND 14G.2: generic creature entries enabled; fixed mob entry whitelist is no longer required for normal grind acquisition.");
            Debug::Logger::Info(
                "GRIND 14G.2: level policy: target <= player+1 and target must be above the Vanilla gray-level boundary.");
            Debug::Logger::Info(
                "GRIND 14G.2: selected passive targets are verified with UnitCanAttack and elite/critter classification before combat starts.");
            Debug::Logger::Info(
                "GRIND 14G.1.1: combat acquisition capped at 24 yd so Charge gets a real opener window.");
            Debug::Logger::Info(
                "GRIND 14G.1.4: post-Charge autoattack latch + remembered multi-aggro + post-pack corpse queue enabled.");
            Debug::Logger::Info("================================");
        }

        void UpdateTemporaryGrindRegion(
            const Navigation::NavPoint& center,
            float radius)
        {
            grindCenterX_ = center.x;
            grindCenterY_ = center.y;
            grindCenterZ_ = center.z;
            grindRadius_ = std::max(30.0f, radius);
        }

        bool TemporaryGrindModeActive() const
        {
            return grindModeActive_;
        }

        bool DefensiveContainmentOwnsMovement() const
        {
            return state_==CombatState::DefensiveContainment && defensiveRoute_ &&
                defensiveRoute_->OwnsMovement();
        }

        Navigation::NavigationInitializationObservation DefensiveContainmentInitialization() const
        {
            return defensiveRoute_ ? defensiveRoute_->InitializationObservation() :
                Navigation::NavigationInitializationObservation{};
        }

        bool IsTemporaryGrindTargetBlacklisted(
            std::uint64_t guid,
            std::uint64_t tick)
        {
            return IsBlacklisted(guid, tick);
        }

        // Preserve the live wide-area safeguard: travel/vendor must not
        // preempt Phase 14G.1.4 post-pack corpse ownership.
        bool HasDeferredCorpseLootPending() const
        {
            return !deferredCorpses_.empty();
        }

        float GrindCenterX() const { return grindCenterX_; }
        float GrindCenterY() const { return grindCenterY_; }
        float GrindCenterZ() const { return grindCenterZ_; }
        float GrindRadius() const { return grindRadius_; }

        void SetPlannerQuestTarget(
            std::uint32_t entry,
            const std::string& name)
        {
            if (entry == 0)
            {
                ClearPlannerQuestTarget();
                return;
            }

            // A planner combat handoff must not resume a suspended legacy
            // acquisition episode. This is cancellation, not quest acceptance;
            // the generic discovery owner still requires live-log proof.
            if (state_ == CombatState::QuestPickup)
            {
                questPickup_.Reset();
                questPickupAttempted_ = true;
                ResetTargetState();
                SetState(CombatState::AcquiringTarget);
                Debug::Logger::Info(
                    "QUEST OWNERSHIP legacyPickup=cancelled reason=planner_combat_handoff");
            }

            const bool changed =
                !plannerQuestTargetActive_ ||
                plannerQuestTargetEntry_ !=
                    entry ||
                plannerQuestTargetName_ !=
                    name;

            plannerQuestTargetActive_ =
                true;

            plannerQuestTargetEntry_ =
                entry;

            plannerQuestTargetName_ =
                name;

            TargetSelector::
                SetPlannerAllowedEntry(
                    entry
                );

            questPolicyReady_ =
                true;

            if (changed)
            {
                Debug::Logger::Info(
                    "================================"
                );

                Debug::Logger::Info(
                    "PHASE 11B PLANNER COMBAT TARGET ENABLED"
                );

                Debug::Logger::Info(
                    "Entry: " +
                    std::to_string(
                        entry
                    ) +
                    " (" +
                    name +
                    ")"
                );

                Debug::Logger::Info(
                    "================================"
                );
            }

            RecomputeQuestPolicy();
        }

        void ClearPlannerQuestTarget()
        {
            if (!plannerQuestTargetActive_)
            {
                return;
            }

            Debug::Logger::Info(
                "PHASE 11B PLANNER COMBAT TARGET CLEARED"
            );

            plannerQuestTargetActive_ =
                false;

            plannerQuestTargetEntry_ =
                0;

            plannerQuestTargetName_.clear();

            TargetSelector::
                SetPlannerAllowedEntry(
                    0
                );

            RecomputeQuestPolicy();
        }

        bool PlannerQuestTargetActive() const
        {
            return
                plannerQuestTargetActive_;
        }

        // Read-only event evidence for planner difficulty accounting. This
        // does not alter PullSafetyPolicy, ranking, or target ownership.
        PullSafetyDecision LastPullDecision() const
        {
            return lastPullDecision_;
        }

        std::uint64_t LastPullEvaluationTick() const
        {
            return lastPullEvaluationTick_;
        }

        std::uint32_t LastPullEvaluationQuestEntry() const
        {
            return lastPullEvaluationEntry_;
        }

        void ApplyQuestState(
            const QuestStateReader::Snapshot& snapshot)
        {
            if (grindModeActive_)
                return;

            if (!snapshot.valid)
            {
                return;
            }

            const bool changed =
                !questPolicyReady_ ||
                cuttingTeethActive_ !=
                    snapshot.cuttingTeethActive ||
                cuttingTeethComplete_ !=
                    snapshot.cuttingTeethComplete ||
                stingOfTheScorpidActive_ !=
                    snapshot.stingOfTheScorpidActive ||
                stingOfTheScorpidComplete_ !=
                    snapshot.stingOfTheScorpidComplete ||
                vileFamiliarsActive_ !=
                    snapshot.vileFamiliarsActive ||
                vileFamiliarsComplete_ !=
                    snapshot.vileFamiliarsComplete;

            questPolicyReady_ =
                true;

            cuttingTeethActive_ =
                snapshot.cuttingTeethActive;

            cuttingTeethComplete_ =
                snapshot.cuttingTeethComplete;

            stingOfTheScorpidActive_ =
                snapshot.stingOfTheScorpidActive;

            stingOfTheScorpidComplete_ =
                snapshot.stingOfTheScorpidComplete;

            vileFamiliarsActive_ =
                snapshot.vileFamiliarsActive;

            vileFamiliarsComplete_ =
                snapshot.vileFamiliarsComplete;

            if (
                !vileFamiliarsActive_ ||
                vileFamiliarsComplete_)
            {
                vileFamiliarsTravelComplete_ =
                    false;
            }

            if (changed)
            {
                RecomputeQuestPolicy();
            }
        }

        bool QuestPolicyReady() const
        {
            return questPolicyReady_;
        }

        std::uint32_t DesiredQuestEntry() const
        {
            return desiredQuestEntry_;
        }

        const char* DesiredQuestTargetName() const
        {
            if (
                plannerQuestTargetActive_ &&
                desiredQuestEntry_ ==
                    plannerQuestTargetEntry_ &&
                !plannerQuestTargetName_.empty())
            {
                return
                    plannerQuestTargetName_.c_str();
            }

            return
                QuestEntryName(
                    desiredQuestEntry_
                );
        }

        CombatState State() const
        {
            return state_;
        }

        const char* StateName() const
        {
            return
                StateNameInternal(
                    state_
                );
        }

        bool IsRunning() const
        {
            return
                state_ !=
                    CombatState::Idle &&
                state_ !=
                    CombatState::Failed;
        }

        bool Failed() const
        {
            return
                state_ ==
                    CombatState::Failed;
        }

        bool ConsumeOwnerTargetFailure(std::uint64_t& guid,
            std::uint32_t& entry, const char*& reason)
        {
            if (!ownerTargetFailureReason_) return false;
            guid=ownerTargetFailureGuid_;
            entry=ownerTargetFailureEntry_;
            reason=ownerTargetFailureReason_;
            ownerTargetFailureGuid_=0;
            ownerTargetFailureEntry_=0;
            ownerTargetFailureReason_=nullptr;
            return true;
        }

        std::uint64_t LockedGuid() const
        {
            return lockedGuid_;
        }

        int Kills() const
        {
            return kills_;
        }

        int TargetsStarted() const
        {
            return targetsStarted_;
        }

        int FacingCommands() const
        {
            return facingCommands_;
        }

        int AttackCommands() const
        {
            return attackCommands_;
        }

        int ChargeCommands() const
        {
            return
                warrior_.
                    ChargeCommands();
        }

        int ChargeMovementObservations() const
        {
            return
                warrior_.
                    ChargeMovementObservations();
        }

        int BattleShoutCommands() const
        {
            return
                warrior_.
                    BattleShoutCommands();
        }

        int BattleShoutRageSpendObservations() const
        {
            return
                warrior_.
                    BattleShoutRageSpendObservations();
        }

        int RendCommands() const
        {
            return
                warrior_.
                    RendCommands();
        }

        int ThunderClapCommands() const
        {
            return
                warrior_.
                    ThunderClapCommands();
        }

        int OverpowerProbes() const
        {
            return
                warrior_.
                    OverpowerProbes();
        }

        int OverpowerRageSpendObservations() const
        {
            return
                warrior_.
                    OverpowerRageSpendObservations();
        }

        int BloodrageProbes() const
        {
            return
                warrior_.
                    BloodrageProbes();
        }

        int HamstringCommands() const
        {
            return
                warrior_.
                    HamstringCommands();
        }

        int HeroicStrikeCommands() const
        {
            return
                warrior_.
                    HeroicStrikeCommands();
        }

        int HeroicStrikePostQueueDamageEvents() const
        {
            return
                warrior_.
                    PostQueueDamageEvents();
        }

        int HeroicStrikeRageSpendObservations() const
        {
            return
                warrior_.
                    RageSpendObservations();
        }

        int LootsSucceeded() const
        {
            return lootsSucceeded_;
        }

        int LootsSkipped() const
        {
            return lootsSkipped_;
        }

        int LootsFailed() const
        {
            return lootsFailed_;
        }

        int RecoveryEntries() const
        {
            return
                recovery_.Entries();
        }

        int RecoveryCompletions() const
        {
            return
                recovery_.Completions();
        }

        int RecoveryAggressorPreemptions() const
        {
            return
                recoveryAggressorPreemptions_;
        }

        int LowHealthFinisherLatchRepairs() const
        {
            return
                lowHealthFinisherLatchRepairs_;
        }

        int LowHealthFinisherHardStallRecoveries() const
        {
            return
                lowHealthFinisherHardStallRecoveries_;
        }

        int EmergencyHealthEvents() const
        {
            return
                emergencyHealthEvents_;
        }

        int AutonomyCombatRecoveries() const
        {
            return autonomyCombatRecoveries_;
        }

        int AutonomyTargetsAbandoned() const
        {
            return autonomyTargetsAbandoned_;
        }

        int RuntimeSupervisorResets() const
        {
            return runtimeSupervisorResets_;
        }

        std::size_t BlacklistedTargetCount() const
        {
            return
                targetBlacklistUntil_.size();
        }

        const QuestTravelController& QuestTravel() const
        {
            return questTravel_;
        }

        const QuestReturnController& QuestReturn() const
        {
            return questReturn_;
        }

        const QuestTurnInController& QuestTurnIn() const
        {
            return questTurnIn_;
        }

        const QuestPickupController& QuestPickup() const
        {
            return questPickup_;
        }

        const QuestObjectiveTravelController& QuestObjectiveTravel() const
        {
            return questObjectiveTravel_;
        }

        const RecoveryController& Recovery() const
        {
            return recovery_;
        }

        const LootController& Loot() const
        {
            return loot_;
        }

        const ChaseController& Chase() const
        {
            return chase_;
        }
    };
}
