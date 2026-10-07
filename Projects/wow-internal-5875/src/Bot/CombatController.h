#pragma once

#include "AutoAttackController.h"
#include "CombatLivenessPolicy.h"
#include "ChaseController.h"
#include "CombatFacingPolicy.h"
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
#include "TargetController.h"
#include "TargetSelector.h"
#include "WarriorRotationController.h"

#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Navigation/DetourNavigationProvider.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
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
        Failed
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
        CombatLivenessDecision meleeDecision_{};
        AutoAttackController::CombatActionEvidence meleeActionEvidence_{};
        CombatStallClass lastMeleeClassification_=CombatStallClass::UnknownOrStale;

        static std::uint64_t ClientSelectedGuid(const Objects::WorldState& world)
        {
            std::uint64_t guid=0;
            return CombatClientEvidence5875::Selection(world,guid) ? guid : 0;
        }

        bool ObserveMeleeLiveness(const Objects::WorldState& world,
            const Objects::UnitState& target, std::uint64_t tick)
        {
            CombatLivenessSample s{};
            s.nowMs=GetTickCount64(); s.sampleTick=tick;
            s.targetGuid=target.guid; s.targetObject=target.address;
            s.targetValid=target.valid; s.alive=world.player.health>0;
            s.selectionKnown=CombatClientEvidence5875::Selection(world,s.selectedGuid);
            s.fresh=CombatClientEvidence5875::FreshHealth(world,target,
                s.targetHp,s.playerHp,s.attackPeriodMs);
            // A stale object snapshot is not a stationary live target sample.
            s.fresh=s.fresh && s.targetHp==target.health && s.playerHp==world.player.health;
            meleeActionEvidence_=AutoAttackController::ProbeCombatAction();
            s.fresh=s.fresh && meleeActionEvidence_.known;
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
            meleeDecision_=meleeLiveness_.Observe(s);
            if (meleeDecision_.classification!=lastMeleeClassification_ ||
                meleeDecision_.action!=CombatRecoveryAction::None)
            {
                lastMeleeClassification_=meleeDecision_.classification;
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
            if (meleeDecision_.action==CombatRecoveryAction::Fail)
            {
                Debug::Logger::Info("COMBAT HARD STALL targetGuid="+Hex64(lockedGuid_)+
                    " durationMs="+std::to_string(meleeLiveness_.NoDamageMs(s.nowMs))+
                    " reason=bounded_same_target_recovery_exhausted");
                Fail("bounded same-target melee recovery exhausted without offensive progress");
                return true;
            }
            // Self-owned separation / chase reconciliation must still advance
            // their existing FSMs. They gate the new recovery input, not their
            // own bounded cleanup. External UI/cast/world guards hold all work.
            return !s.fresh || !s.selectionKnown || !meleeActionEvidence_.known ||
                !meleeActionEvidence_.inputSafe || s.actionWait;
        }

        bool IssueLivenessAttack(const Objects::WorldState& world,
            const Objects::UnitState& target, std::uint64_t tick, bool refresh)
        {
            if (meleeLiveness_.Pending() || meleeLiveness_.Repairs()>=CombatLivenessPolicy::MaximumRepairs)
                return false;
            const auto action=refresh ? CombatRecoveryAction::RefreshAttack : CombatRecoveryAction::ReengageAttack;
            // A rejected dispatch still spends an attempt; it is not success.
            meleeLiveness_.Dispatched(action,GetTickCount64());
            Debug::Logger::Info("COMBAT RECOVERY step="+std::string(CombatRecoveryName(action))+
                " attempt="+std::to_string(meleeLiveness_.Repairs())+" targetGuid="+Hex64(target.guid)+
                " reason="+CombatStallName(meleeDecision_.classification));
            const bool issued=AutoAttackController::RecoverCombatAction(world,target.guid,refresh,
                PostChargeImmediateMeleeDistance);
            attackStarted_=issued;
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
            TargetCandidate best{};
            bool bestIsAggressor = false;
            float bestDistance = SelectorMaxDistance + 1.0f;

            for (const auto& unit : world.units)
            {
                if (!IsCombatCandidateUsable(world, &unit))
                    continue;

                if (!MatchesCurrentCombatPolicy(world, &unit))
                    continue;

                const bool isAggressor = IsDirectAggressor(world, &unit);
                if (!isAggressor && IsBlacklisted(unit.guid, tick))
                    continue;

                if (!best.found ||
                    (isAggressor && !bestIsAggressor) ||
                    (isAggressor == bestIsAggressor && unit.distance < bestDistance))
                {
                    best.found = true;
                    best.unit = unit;
                    bestIsAggressor = isAggressor;
                    bestDistance = unit.distance;
                }
            }

            return best;
        }

        bool TryEnterRecovery(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (grindModeActive_ && FindBestDirectAggressor(world) != nullptr)
            {
                Debug::Logger::Info(
                    "GRIND 14G.1.2: recovery suppressed while a live mob is targeting the player.");
                return false;
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

            state_ =
                newState;
        }

        void Fail(
            const std::string& reason)
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

            AutoAttackController::Stop();

            if (chase_.IsActive())
            {
                chase_.Stop();
            }

            warrior_.EndTarget();

            SetState(
                CombatState::Failed
            );
        }

        void ResetTargetState()
        {
            meleeLiveness_.Reset();
            meleeDecision_={};
            meleeActionEvidence_={};
            lastMeleeClassification_=CombatStallClass::UnknownOrStale;
            lockedGuid_ =
                0;

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

            pendingTargetGuid_ =
                target.guid;

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

                ++consecutiveTargetFailures_;

                BlacklistTarget(
                    target.guid,
                    tick,
                    "ChaseController failed to start."
                );

                ResetTargetState();

                SetState(
                    CombatState::
                        AcquiringTarget
                );

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
                    chargeFacingReady
                        ? 1u
                        : 0u;

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
                IssueLivenessAttack(world,target,tick,false);
            }

            return true;
        }

    public:
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
                Fail(
                    "player health reached zero; "
                    "death recovery is not implemented."
                );

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

                /*
                 * Prefer a valid already-selected target.
                 */
                if (
                    ClientSelectedGuid(world) != 0)
                {
                    const auto* uiTarget =
                        TargetSelector::
                            FindByGuid(
                                world,
                                ClientSelectedGuid(world)
                            );

                    if (
                        IsCombatCandidateUsable(
                            world,
                            uiTarget) &&
                        MatchesCurrentCombatPolicy(
                            world,
                            uiTarget) &&
                        !IsBlacklisted(
                            uiTarget->guid,
                            tick
                        ))
                    {
                        if (!ValidateSelectedGrindTarget(world, *uiTarget, tick))
                        {
                            BeginAcquire(tick + 1);
                            return;
                        }

                        Debug::Logger::Info(
                            grindModeActive_
                                ? "GRIND 14G.2: adopting already-selected validated grind target."
                                : "QUEST COMBAT: adopting already-selected quest target."
                        );

                        Debug::Logger::Info(
                            "Entry: " +
                            std::to_string(
                                uiTarget->entryId
                            ) +
                            " distance=" +
                            Float(
                                uiTarget->distance
                            )
                        );

                        StartLockedTarget(
                            world,
                            *uiTarget,
                            tick
                        );

                        return;
                    }
                }

                const TargetCandidate candidate =
                    SelectCandidate(
                        world,
                        tick
                    );

                if (!candidate.found)
                {
                    if (
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

            if (target == nullptr)
            {
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
                if (target->health==0 && meleeLiveness_.Pending())
                    Debug::Logger::Info("COMBAT RECOVERY VERIFY targetGuid="+Hex64(lockedGuid_)+
                        " result=confirmed evidence=target_died");
                FinishTarget(
                    world,
                    target,
                    tick,
                    true
                );

                return;
            }

            if (ObserveMeleeLiveness(world,*target,tick))
                return;

            MaintainTargetSelection(
                world,
                *target,
                tick
            );

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
                        0;

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
                        0;

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

                if (
                    warriorChargeFacingStableSnapshots_ <
                        CombatFacingPolicy::
                            StableSnapshotsRequired)
                {
                    ++warriorChargeFacingStableSnapshots_;
                }

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
                        const bool facingReady =
                            MaintainFacingAndAttack(
                                world,
                                *target,
                                tick);

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
                ++consecutiveTargetFailures_;

                Debug::Logger::Info(
                    "ChaseController failed for "
                    "current target."
                );

                BlacklistTarget(
                    lockedGuid_,
                    tick,
                    "ChaseController entered Failed."
                );

                if (
                    consecutiveTargetFailures_ >=
                        MaximumConsecutiveTargetFailures)
                {
                    Fail(
                        "too many consecutive "
                        "target/chase failures."
                    );

                    return;
                }

                FinishTarget(
                    world,
                    nullptr,
                    tick,
                    false
                );

                return;
            }

            if (chase_.IsInRange())
            {
                if (wasChasingBeforeUpdate)
                {
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

            if (player.valid)
                MovementController::HoldPosition(player);

            loot_.Reset();
            recovery_.Reset();
            ResetTargetState();
            deferredCorpses_.clear();
            recentAggressorUntil_.clear();
            currentCombatTick_ = tick;
            emergencyHealthLatched_ = false;
            consecutiveTargetFailures_ = 0;
            SetState(CombatState::Idle);

            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.2: CombatController suspended; attack/chase/loot/recovery ownership cleared.");
        }

        void ResumeAfterDeathRecovery(
            std::uint64_t tick)
        {
            loot_.Reset();
            recovery_.Reset();
            ResetTargetState();
            deferredCorpses_.clear();
            recentAggressorUntil_.clear();
            currentCombatTick_ = tick;
            emergencyHealthLatched_ = false;
            consecutiveTargetFailures_ = 0;
            nextAcquireTick_ = tick + 1;
            SetState(CombatState::AcquiringTarget);

            Debug::Logger::Info(
                "DEATH RECOVERY 14G.4.2: CombatController resumed at AcquiringTarget; normal recovery gate owns low post-resurrection HP.");
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

            const bool directAggressor =
                target->targetGuid == world.activePlayerGuid;

            if (
                attempts >= MaximumAutonomyCombatRecoveriesPerTarget &&
                !directAggressor)
            {
                ++autonomyTargetsAbandoned_;
                BlacklistTarget(
                    target->guid,
                    tick,
                    "AutonomySupervisor hard-stall recovery budget exhausted.");
                FinishTarget(world, target, tick, false);
                Debug::Logger::Info(
                    "AUTONOMY 14G.4.1: stalled non-aggressor abandoned; selecting different work.");
                return true;
            }

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
