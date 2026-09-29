#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    enum class RuntimeRobustnessEventKind
    {
        None,
        Watching,
        Recovery,
        EscalatedRecovery
    };

    enum class RuntimeActivityOwner
    {
        None,
        Acquisition,
        Recovery,
        FirstAid,
        Vendor,
        GrindMovement,
        Combat
    };

    inline const char* RuntimeActivityOwnerName(RuntimeActivityOwner owner)
    {
        switch (owner)
        {
            case RuntimeActivityOwner::Acquisition: return "Acquisition";
            case RuntimeActivityOwner::Recovery: return "Recovery";
            case RuntimeActivityOwner::FirstAid: return "FirstAid";
            case RuntimeActivityOwner::Vendor: return "Vendor";
            case RuntimeActivityOwner::GrindMovement: return "Navigation";
            case RuntimeActivityOwner::Combat: return "Combat";
            case RuntimeActivityOwner::None:
            default:
                return "None";
        }
    }

    enum class RuntimeRobustnessReason
    {
        None,
        TacticalNoProgress,
        StrategicNoOutcome,
        IdleDeadlock,
        UnexpectedSeatedIdle,
        RepeatedActivityLoop,
        RecoveryOwnerTimeout,
        FirstAidOwnerTimeout,
        VendorOwnerTimeout,
        MovementOwnerTimeout,
        CombatOwnerTimeout
    };

    struct RuntimeRobustnessSample
    {
        bool active = false;
        std::uint32_t level = 0;
        std::uint32_t currentXp = 0;
        bool xpValid = false;
        int kills = 0;
        int vendorTrips = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        std::uint32_t playerHealth = 0;
        std::uint32_t playerMaxHealth = 0;
        std::uint8_t playerPowerType = 0;
        std::uint32_t playerPowerRaw = 0;
        std::uint32_t playerMaxPowerRaw = 0;
        std::uint64_t targetGuid = 0;
        std::uint32_t targetHealth = 0;
        int grindState = 0;
        int combatState = 0;
        bool recoveryActive = false;
        bool recoveryNoSupplyStalled = false;
        bool firstAidActive = false;
        bool vendorActive = false;
        int vendorState = 0;
        int vendorProgressSerial = 0;
        int firstAidCraftsIssued = 0;
        bool postureValid = false;
        bool playerStanding = true;
    };

    struct RuntimeRobustnessEvent
    {
        RuntimeRobustnessEventKind kind = RuntimeRobustnessEventKind::None;
        std::uint64_t noProgressTicks = 0;
        int recoveryAttempt = 0;
        bool strategic = false;
        RuntimeRobustnessReason reason = RuntimeRobustnessReason::None;
        RuntimeActivityOwner owner = RuntimeActivityOwner::None;
        int ownerState = 0;
    };

    /*
     * Phase 14K.1.5
     *
     * High-level liveness watchdog. Existing movement/combat watchdogs catch
     * local stalls; this layer catches controller loops where state machines
     * remain alive but the bot does not accomplish useful work.
     *
     * Two global clocks are maintained:
     *   tactical liveness: movement, damage, healing, mana recovery, XP,
     *                      kills and completed vendor work
     *   strategic outcome: XP/level, kills, completed vendor trips
     *
     * 14K.1.1 adds bounded activity ownership on top of those global clocks.
     * 14K.1.4 keeps Grind acquisition as one epoch across Idle,
     * AcquiringTarget and WaitingForTargetSelection. Target GUID churn is not
     * progress; only a real movement/owner handoff or combat start ends the epoch.
     * 14K.1.5 adds a recovery containment boundary: once RecoveryController has
     * positively identified a no-food/no-bandage HP stall, the global supervisor
     * must not tear down that safety owner and restart acquisition at critical HP.
     * The recovery controller retains ownership, keeps probing supplies, and emits
     * bounded safe liveness pulses until genuine resource progress resumes.
     */
    class RuntimeRobustnessSupervisor
    {
    private:
        static constexpr float MeaningfulMovement = 3.0f;
        static constexpr float OwnerMeaningfulMovement = 2.0f;
        static constexpr std::uint32_t MeaningfulHeal = 5;
        static constexpr std::uint32_t MeaningfulManaRecovery = 5;

        static constexpr std::uint64_t WatchingTicks = 240;           // ~60 s
        static constexpr std::uint64_t RecoveryTicks = 480;           // ~120 s
        static constexpr std::uint64_t RecoveryCooldownTicks = 120;   // ~30 s
        static constexpr std::uint64_t StrategicOutcomeTicks = 1200;  // ~5 min
        static constexpr std::uint64_t StrategicCooldownTicks = 240;  // ~60 s

        static constexpr std::uint64_t IdleDeadlockTicks = 40;         // ~10 s
        static constexpr std::uint64_t IdleRecoveryCooldownTicks = 40; // ~10 s
        static constexpr std::uint64_t UnexpectedSeatedTicks = 8;      // ~2 s
        static constexpr std::uint64_t PostureRecoveryCooldownTicks = 20; // ~5 s

        static constexpr std::uint64_t RecoveryOwnerTimeoutTicks = 240; // ~60 s
        static constexpr std::uint64_t FirstAidOwnerTimeoutTicks = 96;  // ~24 s
        static constexpr std::uint64_t VendorOwnerTimeoutTicks = 240;   // ~60 s
        static constexpr std::uint64_t MovementOwnerTimeoutTicks = 120; // ~30 s
        static constexpr std::uint64_t CombatOwnerTimeoutTicks = 160;   // ~40 s
        static constexpr std::uint64_t OwnerRecoveryCooldownTicks = 40; // ~10 s

        static constexpr int ActivityLoopTransitionThreshold = 20;
        static constexpr std::uint64_t ActivityLoopMinimumTicks = 40;   // ~10 s
        static constexpr std::uint64_t ActivityLoopCooldownTicks = 80;  // ~20 s

        static constexpr int EscalationAttempt = 3;
        static constexpr int StrategicEscalationAttempt = 2;

        bool initialized_ = false;
        bool watchingLatched_ = false;
        RuntimeRobustnessSample anchor_{};
        std::uint64_t lastProgressTick_ = 0;
        std::uint64_t lastOutcomeTick_ = 0;
        std::uint64_t lastRecoveryTick_ = 0;
        std::uint64_t lastStrategicRecoveryTick_ = 0;
        std::uint64_t lastIdleRecoveryTick_ = 0;
        std::uint64_t lastPostureRecoveryTick_ = 0;
        std::uint64_t unexpectedSeatedSinceTick_ = 0;
        bool acquisitionEpochActive_ = false;
        std::uint64_t acquisitionEpochSinceTick_ = 0;

        RuntimeActivityOwner owner_ = RuntimeActivityOwner::None;
        int ownerState_ = 0;
        int ownerProgressSerial_ = 0;
        RuntimeRobustnessSample ownerAnchor_{};
        std::uint64_t ownerStartedTick_ = 0;
        std::uint64_t lastOwnerProgressTick_ = 0;
        std::uint64_t lastOwnerRecoveryTick_ = 0;

        bool activityInitialized_ = false;
        int lastActivityGrindState_ = 0;
        int lastActivityCombatState_ = 0;
        int lastActivityVendorState_ = 0;
        std::uint64_t lastActivityTargetGuid_ = 0;
        int activityTransitionsWithoutProgress_ = 0;
        std::uint64_t lastActivityLoopRecoveryTick_ = 0;

        int recoveriesWithoutProgress_ = 0;
        int strategicRecoveriesWithoutOutcome_ = 0;
        int watchingEvents_ = 0;
        int recoveryEvents_ = 0;
        int escalatedEvents_ = 0;
        int strategicEvents_ = 0;
        int idleDeadlockEvents_ = 0;
        int ownerTimeoutEvents_ = 0;
        int postureRecoveryEvents_ = 0;
        int repeatedActivityLoopEvents_ = 0;

        static float Distance3D(
            const RuntimeRobustnessSample& a,
            const RuntimeRobustnessSample& b)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float dz = b.z - a.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        static bool ManaRecoveryProgress(
            const RuntimeRobustnessSample& anchor,
            const RuntimeRobustnessSample& sample)
        {
            // Vanilla power type 0 is mana. Rage/energy changes are not recovery
            // progress and must never keep a stale controller alive.
            return sample.recoveryActive &&
                sample.playerPowerType == 0 &&
                anchor.playerPowerType == 0 &&
                sample.playerMaxPowerRaw > 0 &&
                sample.playerPowerRaw >
                    anchor.playerPowerRaw + MeaningfulManaRecovery;
        }

        bool HasOutcomeProgress(
            const RuntimeRobustnessSample& sample) const
        {
            if (sample.level != anchor_.level)
                return true;

            if (sample.xpValid && anchor_.xpValid &&
                sample.currentXp != anchor_.currentXp)
            {
                return true;
            }

            return sample.kills != anchor_.kills ||
                sample.vendorTrips != anchor_.vendorTrips;
        }

        bool HasTacticalProgress(
            const RuntimeRobustnessSample& sample) const
        {
            if (Distance3D(anchor_, sample) >= MeaningfulMovement)
                return true;

            if (anchor_.targetGuid != 0 &&
                sample.targetGuid == anchor_.targetGuid &&
                anchor_.targetHealth > 0 &&
                sample.targetHealth > 0 &&
                sample.targetHealth < anchor_.targetHealth)
            {
                return true;
            }

            if (sample.playerMaxHealth > 0 &&
                sample.playerHealth > anchor_.playerHealth + MeaningfulHeal)
            {
                return true;
            }

            return ManaRecoveryProgress(anchor_, sample);
        }

        void BaselineTactical(
            const RuntimeRobustnessSample& sample,
            std::uint64_t tick,
            bool clearRecoveryBudget)
        {
            anchor_ = sample;
            lastProgressTick_ = tick;
            watchingLatched_ = false;
            initialized_ = sample.active;
            if (clearRecoveryBudget)
                recoveriesWithoutProgress_ = 0;
        }

        static bool IsAcquisitionCombatState(int combatState)
        {
            // CombatState::Idle=0, AcquiringTarget=1,
            // WaitingForTargetSelection=2. All three belong to one acquisition
            // epoch. A transient target GUID or SetTarget retry is not a combat
            // handoff and must not mint a fresh liveness window.
            return combatState == 0 || combatState == 1 || combatState == 2;
        }

        static RuntimeActivityOwner ResolveOwner(
            const RuntimeRobustnessSample& sample)
        {
            if (sample.recoveryActive)
                return RuntimeActivityOwner::Recovery;

            if (sample.firstAidActive)
                return RuntimeActivityOwner::FirstAid;

            if (sample.vendorActive)
                return RuntimeActivityOwner::Vendor;

            // GrindModeState::ApproachingTarget=2, Roaming=3.
            if (sample.grindState == 2 || sample.grindState == 3)
                return RuntimeActivityOwner::GrindMovement;

            // Phase 14K.1.4: while GrindMode owns the loop, Idle,
            // AcquiringTarget and WaitingForTargetSelection are one acquisition
            // ownership family. Target GUID churn is deliberately ignored here:
            // only a real transition into opening/chase/fight ends acquisition.
            if (sample.grindState == 1 &&
                IsAcquisitionCombatState(sample.combatState))
            {
                return RuntimeActivityOwner::Acquisition;
            }

            // CombatState::Recovering=14, Failed=15. Every remaining active
            // combat state owns bounded tactical work.
            if (sample.combatState > 2 &&
                sample.combatState != 14 &&
                sample.combatState != 15)
            {
                return RuntimeActivityOwner::Combat;
            }

            return RuntimeActivityOwner::None;
        }

        static int OwnerStateToken(
            const RuntimeRobustnessSample& sample,
            RuntimeActivityOwner owner)
        {
            switch (owner)
            {
                case RuntimeActivityOwner::Vendor:
                    return sample.vendorState;
                case RuntimeActivityOwner::GrindMovement:
                    return sample.grindState;
                case RuntimeActivityOwner::Combat:
                case RuntimeActivityOwner::Recovery:
                    return sample.combatState;
                case RuntimeActivityOwner::FirstAid:
                case RuntimeActivityOwner::Acquisition:
                case RuntimeActivityOwner::None:
                default:
                    return 0;
            }
        }

        static int OwnerProgressSerial(
            const RuntimeRobustnessSample& sample,
            RuntimeActivityOwner owner)
        {
            switch (owner)
            {
                case RuntimeActivityOwner::Vendor:
                    return sample.vendorProgressSerial;
                case RuntimeActivityOwner::FirstAid:
                    return sample.firstAidCraftsIssued;
                default:
                    return 0;
            }
        }

        static bool HasOwnerProgress(
            const RuntimeRobustnessSample& anchor,
            const RuntimeRobustnessSample& sample)
        {
            if (Distance3D(anchor, sample) >= OwnerMeaningfulMovement)
                return true;

            if (anchor.targetGuid != 0 &&
                sample.targetGuid == anchor.targetGuid &&
                anchor.targetHealth > 0 &&
                sample.targetHealth > 0 &&
                sample.targetHealth < anchor.targetHealth)
            {
                return true;
            }

            if (sample.playerMaxHealth > 0 &&
                sample.playerHealth > anchor.playerHealth + MeaningfulHeal)
            {
                return true;
            }

            return ManaRecoveryProgress(anchor, sample);
        }

        void BaselineOwner(
            const RuntimeRobustnessSample& sample,
            std::uint64_t tick)
        {
            owner_ = ResolveOwner(sample);
            ownerState_ = OwnerStateToken(sample, owner_);
            ownerProgressSerial_ = OwnerProgressSerial(sample, owner_);
            ownerAnchor_ = sample;
            ownerStartedTick_ = tick;
            lastOwnerProgressTick_ = tick;
        }

        void UpdateOwnerTracking(
            const RuntimeRobustnessSample& sample,
            std::uint64_t tick,
            bool tacticalProgress)
        {
            const RuntimeActivityOwner owner = ResolveOwner(sample);
            const int state = OwnerStateToken(sample, owner);
            const int progressSerial = OwnerProgressSerial(sample, owner);

            if (owner != owner_ || state != ownerState_)
            {
                BaselineOwner(sample, tick);
                return;
            }

            if (tacticalProgress ||
                progressSerial != ownerProgressSerial_ ||
                HasOwnerProgress(ownerAnchor_, sample))
            {
                ownerProgressSerial_ = progressSerial;
                ownerAnchor_ = sample;
                lastOwnerProgressTick_ = tick;
                return;
            }

            // Target churn is explicitly not progress. Refresh its comparison
            // baseline so a newly selected target cannot inherit old HP data.
            if (sample.targetGuid != ownerAnchor_.targetGuid)
            {
                ownerAnchor_.targetGuid = sample.targetGuid;
                ownerAnchor_.targetHealth = sample.targetHealth;
            }
            else if (sample.targetHealth > ownerAnchor_.targetHealth)
            {
                ownerAnchor_.targetHealth = sample.targetHealth;
            }

            if (sample.playerHealth < ownerAnchor_.playerHealth)
                ownerAnchor_.playerHealth = sample.playerHealth;

            if (sample.playerPowerRaw < ownerAnchor_.playerPowerRaw)
                ownerAnchor_.playerPowerRaw = sample.playerPowerRaw;
        }

        static std::uint64_t OwnerTimeoutTicks(RuntimeActivityOwner owner)
        {
            switch (owner)
            {
                case RuntimeActivityOwner::Recovery:
                    return RecoveryOwnerTimeoutTicks;
                case RuntimeActivityOwner::FirstAid:
                    return FirstAidOwnerTimeoutTicks;
                case RuntimeActivityOwner::Vendor:
                    return VendorOwnerTimeoutTicks;
                case RuntimeActivityOwner::GrindMovement:
                    return MovementOwnerTimeoutTicks;
                case RuntimeActivityOwner::Combat:
                    return CombatOwnerTimeoutTicks;
                default:
                    return 0;
            }
        }

        static RuntimeRobustnessReason OwnerTimeoutReason(
            RuntimeActivityOwner owner)
        {
            switch (owner)
            {
                case RuntimeActivityOwner::Recovery:
                    return RuntimeRobustnessReason::RecoveryOwnerTimeout;
                case RuntimeActivityOwner::FirstAid:
                    return RuntimeRobustnessReason::FirstAidOwnerTimeout;
                case RuntimeActivityOwner::Vendor:
                    return RuntimeRobustnessReason::VendorOwnerTimeout;
                case RuntimeActivityOwner::GrindMovement:
                    return RuntimeRobustnessReason::MovementOwnerTimeout;
                case RuntimeActivityOwner::Combat:
                    return RuntimeRobustnessReason::CombatOwnerTimeout;
                default:
                    return RuntimeRobustnessReason::None;
            }
        }

        void BaselineActivitySignature(
            const RuntimeRobustnessSample& sample)
        {
            lastActivityGrindState_ = sample.grindState;
            lastActivityCombatState_ = sample.combatState;
            lastActivityVendorState_ = sample.vendorState;
            lastActivityTargetGuid_ = sample.targetGuid;
            activityInitialized_ = true;
        }

        void UpdateActivityLoopTracking(
            const RuntimeRobustnessSample& sample,
            bool tacticalProgress)
        {
            if (!activityInitialized_)
            {
                BaselineActivitySignature(sample);
                return;
            }

            if (tacticalProgress)
            {
                activityTransitionsWithoutProgress_ = 0;
                BaselineActivitySignature(sample);
                return;
            }

            const bool changed =
                sample.grindState != lastActivityGrindState_ ||
                sample.combatState != lastActivityCombatState_ ||
                sample.vendorState != lastActivityVendorState_ ||
                sample.targetGuid != lastActivityTargetGuid_;

            if (!changed)
                return;

            ++activityTransitionsWithoutProgress_;
            BaselineActivitySignature(sample);
        }

        RuntimeRobustnessEvent EmitTacticalRecovery(
            const RuntimeRobustnessSample& sample,
            std::uint64_t tick,
            std::uint64_t age,
            RuntimeRobustnessReason reason,
            RuntimeActivityOwner owner,
            int ownerState)
        {
            ++recoveriesWithoutProgress_;
            ++recoveryEvents_;

            anchor_ = sample;
            lastProgressTick_ = tick;
            watchingLatched_ = false;
            BaselineOwner(sample, tick);
            activityTransitionsWithoutProgress_ = 0;
            BaselineActivitySignature(sample);

            const bool escalated =
                recoveriesWithoutProgress_ >= EscalationAttempt;
            if (escalated)
                ++escalatedEvents_;

            return {
                escalated
                    ? RuntimeRobustnessEventKind::EscalatedRecovery
                    : RuntimeRobustnessEventKind::Recovery,
                age,
                recoveriesWithoutProgress_,
                false,
                reason,
                owner,
                ownerState
            };
        }

    public:
        RuntimeRobustnessEvent Update(
            const RuntimeRobustnessSample& sample,
            std::uint64_t tick)
        {
            if (!sample.active)
            {
                Reset();
                return {};
            }

            if (!initialized_)
            {
                anchor_ = sample;
                lastProgressTick_ = tick;
                lastOutcomeTick_ = tick;
                initialized_ = true;
                BaselineOwner(sample, tick);
                BaselineActivitySignature(sample);
                return {};
            }

            const bool outcomeProgress = HasOutcomeProgress(sample);
            const bool tacticalProgress =
                outcomeProgress || HasTacticalProgress(sample);

            if (outcomeProgress)
            {
                lastOutcomeTick_ = tick;
                strategicRecoveriesWithoutOutcome_ = 0;
            }

            if (tacticalProgress)
                BaselineTactical(sample, tick, true);

            // Target/state churn is not global progress, but refresh comparison
            // data without granting a new liveness window.
            if (!tacticalProgress)
            {
                if (sample.targetGuid != anchor_.targetGuid)
                {
                    anchor_.targetGuid = sample.targetGuid;
                    anchor_.targetHealth = sample.targetHealth;
                }
                else if (sample.targetHealth > anchor_.targetHealth)
                {
                    anchor_.targetHealth = sample.targetHealth;
                }

                if (sample.playerHealth < anchor_.playerHealth)
                    anchor_.playerHealth = sample.playerHealth;

                if (sample.playerPowerRaw < anchor_.playerPowerRaw)
                    anchor_.playerPowerRaw = sample.playerPowerRaw;

                anchor_.grindState = sample.grindState;
                anchor_.combatState = sample.combatState;
                anchor_.vendorState = sample.vendorState;
            }

            UpdateOwnerTracking(sample, tick, tacticalProgress);
            UpdateActivityLoopTracking(sample, tacticalProgress);

            const std::uint64_t tacticalAge =
                tick >= lastProgressTick_ ? tick - lastProgressTick_ : 0;
            const std::uint64_t strategicAge =
                tick >= lastOutcomeTick_ ? tick - lastOutcomeTick_ : 0;
            const std::uint64_t ownerAge =
                tick >= lastOwnerProgressTick_
                    ? tick - lastOwnerProgressTick_
                    : 0;

            const bool legitimateStationaryOwner =
                sample.recoveryActive ||
                sample.firstAidActive ||
                sample.vendorActive;

            // Phase 14K.1.4: acquisition liveness is one continuous epoch
            // across Idle -> AcquiringTarget -> WaitingForTargetSelection churn.
            // A transient target GUID is not progress. This closes the owner
            // boundary where repeated SetTarget attempts could reset the 10 s
            // watchdog forever without ever starting combat. Legitimate stationary
            // owners and real movement/combat states still end the epoch.
            const bool acquisitionEpoch =
                !legitimateStationaryOwner &&
                sample.grindState == 1 &&
                IsAcquisitionCombatState(sample.combatState);

            if (acquisitionEpoch)
            {
                if (!acquisitionEpochActive_ ||
                    tick < acquisitionEpochSinceTick_)
                {
                    acquisitionEpochActive_ = true;
                    acquisitionEpochSinceTick_ = tick;
                }
            }
            else
            {
                acquisitionEpochActive_ = false;
                acquisitionEpochSinceTick_ = 0;
            }

            const std::uint64_t acquisitionEpochAge =
                acquisitionEpochActive_ &&
                tick >= acquisitionEpochSinceTick_
                    ? tick - acquisitionEpochSinceTick_
                    : 0;

            const bool idleDeadlock =
                acquisitionEpochActive_ &&
                acquisitionEpochAge >= IdleDeadlockTicks;

            // A seated player in any acquisition state is not a legitimate
            // steady-state owner. Recovery/First Aid/Vendor remain excluded.
            const bool unexpectedSeated =
                sample.postureValid &&
                !sample.playerStanding &&
                !legitimateStationaryOwner &&
                sample.grindState == 1 &&
                IsAcquisitionCombatState(sample.combatState);

            if (unexpectedSeated)
            {
                if (unexpectedSeatedSinceTick_ == 0 ||
                    tick < unexpectedSeatedSinceTick_)
                {
                    unexpectedSeatedSinceTick_ = tick;
                }
            }
            else
            {
                unexpectedSeatedSinceTick_ = 0;
            }

            if (unexpectedSeated &&
                tick >= unexpectedSeatedSinceTick_ &&
                tick - unexpectedSeatedSinceTick_ >= UnexpectedSeatedTicks &&
                (lastPostureRecoveryTick_ == 0 ||
                 tick < lastPostureRecoveryTick_ ||
                 tick - lastPostureRecoveryTick_ >= PostureRecoveryCooldownTicks))
            {
                lastPostureRecoveryTick_ = tick;
                ++postureRecoveryEvents_;
                return EmitTacticalRecovery(
                    sample,
                    tick,
                    tick - unexpectedSeatedSinceTick_,
                    RuntimeRobustnessReason::UnexpectedSeatedIdle,
                    RuntimeActivityOwner::Acquisition,
                    sample.combatState);
            }

            if (idleDeadlock &&
                (lastIdleRecoveryTick_ == 0 ||
                 tick < lastIdleRecoveryTick_ ||
                 tick - lastIdleRecoveryTick_ >= IdleRecoveryCooldownTicks))
            {
                lastIdleRecoveryTick_ = tick;
                ++idleDeadlockEvents_;
                // Start a fresh acquisition epoch after the bounded reset.
                // If recovery fails to hand off, a new deadlock requires a full
                // IdleDeadlockTicks window instead of immediately retriggering.
                acquisitionEpochActive_ = true;
                acquisitionEpochSinceTick_ = tick;
                return EmitTacticalRecovery(
                    sample,
                    tick,
                    acquisitionEpochAge,
                    RuntimeRobustnessReason::IdleDeadlock,
                    RuntimeActivityOwner::Acquisition,
                    sample.combatState);
            }

            /*
             * Phase 14K.1.5: a positively identified no-supply recovery stall is
             * deliberately not converted into a global ownership reset. The old
             * path reset CombatController after ~60 s, which immediately re-entered
             * recovery at the same critical HP and created an endless
             * Recovering -> reset -> Recovering loop. RecoveryController now owns
             * the bounded stand/CTM-hold pulse and supply re-probe while this flag
             * is active. Genuine HP progress clears the flag automatically.
             */
            if (sample.recoveryActive && sample.recoveryNoSupplyStalled)
                return {};

            // State/target churn cannot be used as a heartbeat. A high number of
            // transitions with no physical/resource/combat progress is itself a
            // bounded liveness failure.
            if (activityTransitionsWithoutProgress_ >=
                    ActivityLoopTransitionThreshold &&
                tacticalAge >= ActivityLoopMinimumTicks &&
                (lastActivityLoopRecoveryTick_ == 0 ||
                 tick < lastActivityLoopRecoveryTick_ ||
                 tick - lastActivityLoopRecoveryTick_ >= ActivityLoopCooldownTicks))
            {
                lastActivityLoopRecoveryTick_ = tick;
                ++repeatedActivityLoopEvents_;
                return EmitTacticalRecovery(
                    sample,
                    tick,
                    tacticalAge,
                    RuntimeRobustnessReason::RepeatedActivityLoop,
                    owner_,
                    ownerState_);
            }

            const std::uint64_t ownerTimeout = OwnerTimeoutTicks(owner_);
            if (ownerTimeout > 0 &&
                ownerAge >= ownerTimeout &&
                (lastOwnerRecoveryTick_ == 0 ||
                 tick < lastOwnerRecoveryTick_ ||
                 tick - lastOwnerRecoveryTick_ >= OwnerRecoveryCooldownTicks))
            {
                lastOwnerRecoveryTick_ = tick;
                ++ownerTimeoutEvents_;
                return EmitTacticalRecovery(
                    sample,
                    tick,
                    ownerAge,
                    OwnerTimeoutReason(owner_),
                    owner_,
                    ownerState_);
            }

            // Strategic loop detection remains active even while the character
            // is physically moving. A grind bot walking for five minutes with
            // no XP, kill, level or completed vendor trip is not productive.
            if (strategicAge >= StrategicOutcomeTicks &&
                (lastStrategicRecoveryTick_ == 0 ||
                 tick < lastStrategicRecoveryTick_ ||
                 tick - lastStrategicRecoveryTick_ >= StrategicCooldownTicks))
            {
                lastStrategicRecoveryTick_ = tick;
                lastOutcomeTick_ = tick;
                ++strategicRecoveriesWithoutOutcome_;
                ++recoveryEvents_;
                ++strategicEvents_;

                const bool escalated =
                    strategicRecoveriesWithoutOutcome_ >=
                    StrategicEscalationAttempt;
                if (escalated)
                    ++escalatedEvents_;

                return {
                    escalated
                        ? RuntimeRobustnessEventKind::EscalatedRecovery
                        : RuntimeRobustnessEventKind::Recovery,
                    strategicAge,
                    strategicRecoveriesWithoutOutcome_,
                    true,
                    RuntimeRobustnessReason::StrategicNoOutcome,
                    owner_,
                    ownerState_
                };
            }

            if (!watchingLatched_ && tacticalAge >= WatchingTicks)
            {
                watchingLatched_ = true;
                ++watchingEvents_;
                return {
                    RuntimeRobustnessEventKind::Watching,
                    tacticalAge,
                    recoveriesWithoutProgress_,
                    false,
                    RuntimeRobustnessReason::TacticalNoProgress,
                    owner_,
                    ownerState_
                };
            }

            if (tacticalAge < RecoveryTicks)
                return {};

            if (lastRecoveryTick_ != 0 &&
                tick >= lastRecoveryTick_ &&
                tick - lastRecoveryTick_ < RecoveryCooldownTicks)
            {
                return {};
            }

            lastRecoveryTick_ = tick;
            return EmitTacticalRecovery(
                sample,
                tick,
                tacticalAge,
                RuntimeRobustnessReason::TacticalNoProgress,
                owner_,
                ownerState_);
        }

        void Reset()
        {
            initialized_ = false;
            watchingLatched_ = false;
            anchor_ = {};
            lastProgressTick_ = 0;
            lastOutcomeTick_ = 0;
            lastRecoveryTick_ = 0;
            lastStrategicRecoveryTick_ = 0;
            lastIdleRecoveryTick_ = 0;
            lastPostureRecoveryTick_ = 0;
            unexpectedSeatedSinceTick_ = 0;
            acquisitionEpochActive_ = false;
            acquisitionEpochSinceTick_ = 0;

            owner_ = RuntimeActivityOwner::None;
            ownerState_ = 0;
            ownerProgressSerial_ = 0;
            ownerAnchor_ = {};
            ownerStartedTick_ = 0;
            lastOwnerProgressTick_ = 0;
            lastOwnerRecoveryTick_ = 0;

            activityInitialized_ = false;
            lastActivityGrindState_ = 0;
            lastActivityCombatState_ = 0;
            lastActivityVendorState_ = 0;
            lastActivityTargetGuid_ = 0;
            activityTransitionsWithoutProgress_ = 0;
            lastActivityLoopRecoveryTick_ = 0;

            recoveriesWithoutProgress_ = 0;
            strategicRecoveriesWithoutOutcome_ = 0;
        }

        std::uint64_t ProgressAgeTicks(std::uint64_t tick) const
        {
            if (!initialized_ || tick < lastProgressTick_)
                return 0;
            return tick - lastProgressTick_;
        }

        std::uint64_t OutcomeAgeTicks(std::uint64_t tick) const
        {
            if (!initialized_ || tick < lastOutcomeTick_)
                return 0;
            return tick - lastOutcomeTick_;
        }

        std::uint64_t OwnerAgeTicks(std::uint64_t tick) const
        {
            if (!initialized_ || tick < lastOwnerProgressTick_)
                return 0;
            return tick - lastOwnerProgressTick_;
        }

        bool AcquisitionEpochActive() const
        {
            return acquisitionEpochActive_;
        }

        std::uint64_t AcquisitionEpochAgeTicks(std::uint64_t tick) const
        {
            if (!acquisitionEpochActive_ || tick < acquisitionEpochSinceTick_)
                return 0;
            return tick - acquisitionEpochSinceTick_;
        }

        RuntimeActivityOwner Owner() const { return owner_; }
        int OwnerState() const { return ownerState_; }
        int RecoveriesWithoutProgress() const { return recoveriesWithoutProgress_; }
        int WatchingEvents() const { return watchingEvents_; }
        int RecoveryEvents() const { return recoveryEvents_; }
        int EscalatedEvents() const { return escalatedEvents_; }
        int StrategicEvents() const { return strategicEvents_; }
        int IdleDeadlockEvents() const { return idleDeadlockEvents_; }
        int OwnerTimeoutEvents() const { return ownerTimeoutEvents_; }
        int PostureRecoveryEvents() const { return postureRecoveryEvents_; }
        int RepeatedActivityLoopEvents() const { return repeatedActivityLoopEvents_; }
    };
}
