#pragma once

#include "CombatController.h"
#include "IObjectiveExecutor.h"
#include "ValleyOfTrialsProfiles.h"

#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    class CollectItemFromMobExecutor :
        public IObjectiveExecutor
    {
    private:
        static constexpr float LiveTargetHandoffDistance =
            55.0f;

        static constexpr std::uint64_t MaximumExecutingTicks =
            720;

        ObjectiveExecutorState state_ =
            ObjectiveExecutorState::Idle;

        const QuestProfile* profile_ =
            nullptr;

        Navigation::GenericNavMeshPathFollower
            navigator_{};

        std::uint64_t executingStartTick_ =
            0;

        bool plannerTargetEnabled_ =
            false;

        bool defensiveCombatActive_ =
            false;

        bool waitingForDefensiveRecovery_ =
            false;

        bool defensiveTargetAcquisitionPending_ =
            false;

        std::uint64_t defensiveTargetAcquisitionDeadlineTick_ =
            0;

        std::uint64_t defensiveGuid_ =
            0;

        std::uint32_t navigationLastHealth_ =
            0;

        std::uint64_t navigationTargetBaseline_ =
            0;

        static constexpr float DefensiveResumeHealthPercent =
            95.0f;

        // If WoW has already selected a new live unit that is essentially in
        // melee range while planner navigation owns CTM, hand control to the
        // existing combat stack before waiting for the next HP-loss sample.
        // This is deliberately short-range and exact-GUID only.
        static constexpr float ProactiveSelectedThreatDistance =
            8.0f;

        static constexpr float DirectAggressorMaximumDistance =
            35.0f;

        // WorldMonitor pulses every 250 ms. Give WoW a short bounded window
        // to populate the client target after incoming damage before treating
        // defensive acquisition as unavailable.
        static constexpr std::uint64_t DefensiveTargetAcquisitionTicks =
            24;

        // WoW 1.12.1.5875 ObjectManager. PlayerState intentionally does not
        // duplicate the local player GUID; read the authoritative active GUID
        // directly from ObjectManager instead.
        static constexpr std::uintptr_t ObjectManagerRootRva =
            0x00741414;

        static constexpr std::uintptr_t ActivePlayerGuidOffset =
            0x000000C0;

        template <typename T>
        static bool ReadValue(
            std::uintptr_t address,
            T& value)
        {
            MEMORY_BASIC_INFORMATION info{};

            if (
                address == 0 ||
                VirtualQuery(
                    reinterpret_cast<LPCVOID>(address),
                    &info,
                    sizeof(info)
                ) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            std::memcpy(
                &value,
                reinterpret_cast<const void*>(address),
                sizeof(T)
            );

            return true;
        }

        static std::uint64_t ActivePlayerGuid()
        {
            std::uint32_t manager = 0;

            if (!ReadValue(
                    Wow5875::Client::Base() + ObjectManagerRootRva,
                    manager) ||
                manager == 0 ||
                (manager & 1u) != 0)
            {
                return 0;
            }

            std::uint64_t guid = 0;

            if (!ReadValue(
                    static_cast<std::uintptr_t>(manager) +
                        ActivePlayerGuidOffset,
                    guid))
            {
                return 0;
            }

            return guid;
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
                << std::setprecision(2)
                << value;

            return stream.str();
        }

        void LogThreatSnapshot(
            const Objects::WorldState& world) const
        {
            Debug::Logger::Info(
                "THREAT SNAPSHOT 11B.5: player target=" +
                Hex64(
                    world.player.targetGuid
                ) +
                " hp=" +
                std::to_string(
                    world.player.health
                ) +
                "/" +
                std::to_string(
                    world.player.maxHealth
                )
            );

            int emitted =
                0;

            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.health == 0 ||
                    unit.maxHealth == 0 ||
                    unit.distance < 0.0f ||
                    unit.distance > 35.0f)
                {
                    continue;
                }

                Debug::Logger::Info(
                    "THREAT SNAPSHOT 11B.5: unit guid=" +
                    Hex64(
                        unit.guid
                    ) +
                    " entry=" +
                    std::to_string(
                        unit.entryId
                    ) +
                    " hp=" +
                    std::to_string(
                        unit.health
                    ) +
                    "/" +
                    std::to_string(
                        unit.maxHealth
                    ) +
                    " distance=" +
                    Float(
                        unit.distance
                    ) +
                    " targetGuid=" +
                    Hex64(
                        unit.targetGuid
                    )
                );

                ++emitted;

                if (emitted >= 12)
                {
                    break;
                }
            }
        }

        static const Objects::UnitState* FindDirectAggressor(
            const Objects::WorldState& world)
        {
            const std::uint64_t playerGuid =
                ActivePlayerGuid();

            if (playerGuid == 0)
            {
                return nullptr;
            }

            const Objects::UnitState* best =
                nullptr;

            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.health == 0 ||
                    unit.maxHealth == 0 ||
                    unit.targetGuid != playerGuid ||
                    unit.distance < 0.0f ||
                    unit.distance > DirectAggressorMaximumDistance)
                {
                    continue;
                }

                if (
                    best == nullptr ||
                    unit.distance < best->distance)
                {
                    best = &unit;
                }
            }

            return best;
        }

        bool TryBeginDirectAggressorDefense(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            const char* context,
            bool pauseNavigation)
        {
            const auto* attacker =
                FindDirectAggressor(world);

            if (attacker == nullptr)
            {
                return false;
            }

            Debug::Logger::Info(
                "================================"
            );
            Debug::Logger::Info(
                "OBJECTIVE 11B.8: DIRECT AGGRESSOR PREEMPTION"
            );
            Debug::Logger::Info(
                std::string("Context: ") + context
            );
            Debug::Logger::Info(
                "Attacker GUID: " + Hex64(attacker->guid) +
                " entry=" + std::to_string(attacker->entryId) +
                " distance=" + Float(attacker->distance) +
                " unit.targetGuid=" + Hex64(attacker->targetGuid)
            );
            Debug::Logger::Info(
                "Player GUID: " + Hex64(ActivePlayerGuid())
            );
            Debug::Logger::Info(
                "================================"
            );

            if (pauseNavigation)
            {
                navigator_.PauseForCombat(
                    world.player,
                    "live unit targetGuid matches local player GUID"
                );
            }

            navigationLastHealth_ =
                world.player.health;

            defensiveTargetAcquisitionPending_ =
                false;

            if (!combat.AdoptExactTargetForDefense(
                    world,
                    attacker->guid,
                    tick))
            {
                return false;
            }

            defensiveGuid_ =
                attacker->guid;

            defensiveCombatActive_ =
                true;

            waitingForDefensiveRecovery_ =
                false;

            Debug::Logger::Info(
                "OBJECTIVE 11B.8: exact direct aggressor handed to CombatController."
            );

            return true;
        }

        bool RestartNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (profile_ == nullptr)
            {
                return false;
            }

            if (!navigator_.ResumeAfterCombat(
                    world.player,
                    tick))
            {
                return false;
            }

            navigationLastHealth_ =
                world.player.health;

            navigationTargetBaseline_ =
                world.player.targetGuid;

            Debug::Logger::Info(
                "OBJECTIVE 11B.7: defensive recovery complete; "
                "NavMesh resumed from the preserved corridor/cache."
            );

            return true;
        }

        bool TryAdoptDefensiveTarget(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            const std::uint64_t selectedGuid =
                world.player.targetGuid;

            const bool targetChanged =
                selectedGuid != 0 &&
                selectedGuid !=
                    navigationTargetBaseline_;

            const auto* selected =
                TargetSelector::FindByGuid(
                    world,
                    selectedGuid
                );

            if (
                !targetChanged ||
                selected == nullptr ||
                !selected->valid ||
                selected->health == 0 ||
                selected->maxHealth == 0)
            {
                return false;
            }

            if (!combat.AdoptSelectedTargetForDefense(
                    world,
                    tick))
            {
                return false;
            }

            defensiveGuid_ =
                selectedGuid;

            defensiveCombatActive_ =
                true;

            defensiveTargetAcquisitionPending_ =
                false;

            waitingForDefensiveRecovery_ =
                false;

            Debug::Logger::Info(
                "OBJECTIVE 11B.6: defensive combat acquired exact GUID " +
                Hex64(
                    defensiveGuid_
                )
            );

            return true;
        }

        bool TryBeginProactiveSelectedDefense(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick,
            const char* context)
        {
            const std::uint64_t selectedGuid =
                world.player.targetGuid;

            if (
                selectedGuid == 0 ||
                selectedGuid == navigationTargetBaseline_)
            {
                return false;
            }

            const auto* selected =
                TargetSelector::FindByGuid(
                    world,
                    selectedGuid
                );

            if (
                selected == nullptr ||
                !selected->valid ||
                selected->health == 0 ||
                selected->maxHealth == 0 ||
                selected->distance < 0.0f ||
                selected->distance >
                    ProactiveSelectedThreatDistance)
            {
                return false;
            }

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "OBJECTIVE 11B.7: PROACTIVE CLOSE-TARGET PREEMPTION"
            );

            Debug::Logger::Info(
                std::string("Context: ") + context
            );

            Debug::Logger::Info(
                "Exact client GUID: " +
                Hex64(selectedGuid) +
                " entry=" +
                std::to_string(selected->entryId) +
                " distance=" +
                Float(selected->distance)
            );

            Debug::Logger::Info(
                "================================"
            );

            navigator_.PauseForCombat(
                world.player,
                "new exact client target entered <=8 yd during planner ownership"
            );

            navigationLastHealth_ =
                world.player.health;

            defensiveTargetAcquisitionPending_ =
                true;

            defensiveTargetAcquisitionDeadlineTick_ =
                tick + DefensiveTargetAcquisitionTicks;

            return
                TryAdoptDefensiveTarget(
                    world,
                    combat,
                    tick
                );
        }

        bool BeginDefensiveCombat(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            const std::uint64_t selectedGuid =
                world.player.targetGuid;

            const bool targetChanged =
                selectedGuid != 0 &&
                selectedGuid !=
                    navigationTargetBaseline_;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "OBJECTIVE 11B.6: INCOMING DAMAGE DURING NAVIGATION"
            );

            Debug::Logger::Info(
                "Health: " +
                std::to_string(
                    navigationLastHealth_
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

            Debug::Logger::Info(
                "Navigation target baseline: " +
                Hex64(
                    navigationTargetBaseline_
                )
            );

            Debug::Logger::Info(
                "Current client target: " +
                Hex64(
                    selectedGuid
                )
            );

            Debug::Logger::Info(
                "Target changed with damage: " +
                std::string(
                    targetChanged
                        ? "yes"
                        : "no"
                )
            );

            Debug::Logger::Info(
                "================================"
            );

            LogThreatSnapshot(
                world
            );

            navigator_.PauseForCombat(
                world.player,
                "player HP dropped during planner-owned navigation"
            );

            navigationLastHealth_ =
                world.player.health;

            defensiveTargetAcquisitionPending_ =
                true;

            defensiveTargetAcquisitionDeadlineTick_ =
                tick +
                DefensiveTargetAcquisitionTicks;

            if (TryAdoptDefensiveTarget(
                    world,
                    combat,
                    tick))
            {
                return true;
            }

            Debug::Logger::Info(
                "OBJECTIVE 11B.6: attacker target not synchronized yet; "
                "CTM remains paused while waiting up to 6.0 seconds "
                "for an exact live client target."
            );

            return false;
        }

        void UpdateDefensiveTargetAcquisition(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (world.player.health == 0)
            {
                defensiveTargetAcquisitionPending_ =
                    false;

                Fail(
                    combat,
                    "player died while waiting for a defensive target; "
                    "corpse recovery is not implemented."
                );

                return;
            }

            navigationLastHealth_ =
                world.player.health;

            if (TryBeginDirectAggressorDefense(
                    world,
                    combat,
                    tick,
                    "defensive acquisition",
                    false))
            {
                return;
            }

            if (TryAdoptDefensiveTarget(
                    world,
                    combat,
                    tick))
            {
                return;
            }

            if (
                tick <
                    defensiveTargetAcquisitionDeadlineTick_)
            {
                return;
            }

            LogThreatSnapshot(
                world
            );

            const float hp =
                HealthPercent(world.player);

            if (hp >= DefensiveResumeHealthPercent)
            {
                defensiveTargetAcquisitionPending_ =
                    false;

                waitingForDefensiveRecovery_ =
                    true;

                Debug::Logger::Info(
                    "OBJECTIVE 11B.8: no attacker signal remained and player "
                    "recovered to >=95%; treating the transient threat as cleared."
                );
                return;
            }

            defensiveTargetAcquisitionDeadlineTick_ =
                tick + DefensiveTargetAcquisitionTicks;

            Debug::Logger::Info(
                "OBJECTIVE 11B.8: no exact attacker signal yet; defensive hold "
                "remains active for another bounded 6.0-second window. "
                "Navigation stays stopped; the objective is not failed merely "
                "because client target synchronization lagged."
            );
        }

        void UpdateDefensiveCombat(
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick)
        {
            if (world.player.health == 0)
            {
                combat.ClearPlannerQuestTarget();
                defensiveCombatActive_ = false;

                Fail(
                    combat,
                    "player died during defensive navigation combat; "
                    "corpse recovery is not implemented."
                );

                return;
            }

            const auto* defender =
                TargetSelector::FindByGuid(
                    world,
                    defensiveGuid_
                );

            if (
                combat.State() == CombatState::Failed ||
                combat.Failed())
            {
                combat.ClearPlannerQuestTarget();
                defensiveCombatActive_ = false;

                Fail(
                    combat,
                    "CombatController failed during defensive "
                    "navigation combat."
                );

                return;
            }

            if (
                combat.LockedGuid() == 0 &&
                (
                    combat.State() == CombatState::PostKillDelay ||
                    combat.State() == CombatState::AcquiringTarget
                ))
            {
                if (
                    defender != nullptr &&
                    defender->valid &&
                    defender->health > 0)
                {
                    combat.ClearPlannerQuestTarget();
                    defensiveCombatActive_ = false;

                    Fail(
                        combat,
                        "defensive combat lost the exact attacker "
                        "while it was still alive."
                    );

                    return;
                }

                combat.ClearPlannerQuestTarget();

                defensiveCombatActive_ =
                    false;

                defensiveGuid_ =
                    0;

                waitingForDefensiveRecovery_ =
                    true;

                navigationLastHealth_ =
                    world.player.health;

                navigationTargetBaseline_ =
                    world.player.targetGuid;

                Debug::Logger::Info(
                    "OBJECTIVE 11B.5: defensive target resolved; "
                    "waiting for >=95% HP before route resume."
                );

                return;
            }

            combat.Update(
                world,
                tick
            );
        }

        static const char* StateNameInternal(
            ObjectiveExecutorState state)
        {
            switch (state)
            {
                case ObjectiveExecutorState::Idle:
                    return "Idle";

                case ObjectiveExecutorState::Navigating:
                    return "Navigating";

                case ObjectiveExecutorState::Executing:
                    return "Executing";

                case ObjectiveExecutorState::ReadyForTurnIn:
                    return "ReadyForTurnIn";

                case ObjectiveExecutorState::Failed:
                    return "Failed";

                default:
                    return "Unknown";
            }
        }

        void SetState(
            ObjectiveExecutorState next)
        {
            if (state_ == next)
            {
                return;
            }

            Debug::Logger::Info(
                std::string(
                    "CollectItemFromMobExecutor state: "
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
        }

        const PlannerQuestLogEntry* FindLiveQuest(
            const QuestPlannerSnapshot& snapshot) const
        {
            if (profile_ == nullptr)
            {
                return nullptr;
            }

            for (const auto& entry : snapshot.quests)
            {
                const auto* mapped =
                    ValleyOfTrialsProfiles::Find(
                        entry,
                        snapshot.classToken
                    );

                if (
                    mapped != nullptr &&
                    mapped->questId ==
                        profile_->questId)
                {
                    return
                        &entry;
                }
            }

            return nullptr;
        }

        const Objects::UnitState* FindObjectiveTarget(
            const Objects::WorldState& world) const
        {
            if (profile_ == nullptr)
            {
                return nullptr;
            }

            const Objects::UnitState* best =
                nullptr;

            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    unit.health == 0 ||
                    unit.maxHealth == 0 ||
                    unit.entryId !=
                        profile_->objective.targetEntry)
                {
                    continue;
                }

                if (
                    best == nullptr ||
                    unit.distance <
                        best->distance)
                {
                    best =
                        &unit;
                }
            }

            return best;
        }

        void EnablePlannerTarget(
            CombatController& combat)
        {
            if (
                plannerTargetEnabled_ ||
                profile_ == nullptr)
            {
                return;
            }

            combat.SetPlannerQuestTarget(
                profile_->objective.targetEntry,
                profile_->objective.targetName
            );

            plannerTargetEnabled_ =
                true;
        }

        void DisablePlannerTarget(
            CombatController& combat)
        {
            if (!plannerTargetEnabled_)
            {
                return;
            }

            combat.ClearPlannerQuestTarget();

            plannerTargetEnabled_ =
                false;
        }

        void Complete(
            CombatController& combat)
        {
            DisablePlannerTarget(
                combat
            );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "OBJECTIVE 11B: COMPLETE"
            );

            Debug::Logger::Info(
                "Quest: " +
                std::to_string(
                    profile_->questId
                ) +
                " " +
                profile_->title
            );

            if (profile_->objective.type == QuestObjectiveType::KillMob)
            {
                Debug::Logger::Info(
                    "Objective step complete: kill entry " +
                    std::to_string(profile_->objective.targetEntry));
            }
            else
            {
                Debug::Logger::Info(
                    "Objective step complete: collected item " +
                    std::to_string(profile_->objective.itemId) +
                    " from entry " +
                    std::to_string(profile_->objective.targetEntry));
            }

            Debug::Logger::Info(
                "Planner will re-evaluate the quest; another objective step may remain."
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                ObjectiveExecutorState::
                    ReadyForTurnIn
            );
        }

        void Fail(
            CombatController& combat,
            const std::string& reason)
        {
            DisablePlannerTarget(
                combat
            );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "OBJECTIVE 11B: FAILED"
            );

            Debug::Logger::Info(
                "Reason: " +
                reason
            );

            Debug::Logger::Info(
                "================================"
            );

            SetState(
                ObjectiveExecutorState::
                    Failed
            );
        }

    public:
        bool Supports(
            const QuestProfile& profile) const override
        {
            const bool collect =
                profile.objective.type ==
                    QuestObjectiveType::CollectItemFromMob &&
                profile.objective.itemId != 0;

            const bool kill =
                profile.objective.type ==
                    QuestObjectiveType::KillMob;

            return
                (collect || kill) &&
                profile.objective.targetEntry != 0 &&
                profile.destination.valid;
        }

        bool Start(
            const QuestProfile& profile,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) override
        {
            if (
                state_ !=
                    ObjectiveExecutorState::Idle ||
                !Supports(
                    profile
                ))
            {
                return false;
            }

            profile_ =
                &profile;

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "OBJECTIVE EXECUTOR 11B: START"
            );

            Debug::Logger::Info(
                "Quest: " +
                std::to_string(
                    profile.questId
                ) +
                " " +
                profile.title
            );

            Debug::Logger::Info(
                std::string("Type: ") +
                (profile.objective.type == QuestObjectiveType::KillMob
                    ? "KillMob"
                    : "CollectItemFromMob")
            );

            Debug::Logger::Info(
                "Target: " +
                std::string(
                    profile.objective.targetName
                ) +
                " entry=" +
                std::to_string(
                    profile.objective.targetEntry
                )
            );

            if (profile.objective.type == QuestObjectiveType::CollectItemFromMob)
            {
                Debug::Logger::Info(
                    "Item: " +
                    std::to_string(profile.objective.itemId) +
                    " required=" +
                    std::to_string(profile.objective.requiredCount));
            }
            else
            {
                Debug::Logger::Info(
                    "Kills required=" +
                    std::to_string(profile.objective.requiredCount));
            }

            Debug::Logger::Info(
                "================================"
            );

            /*
             * If the objective target is already loaded and
             * close enough, do not navigate to a stale spawn
             * seed. Use the live WorldState immediately.
             */
            const auto* liveTarget =
                FindObjectiveTarget(
                    world
                );

            if (
                liveTarget != nullptr &&
                liveTarget->distance <=
                    LiveTargetHandoffDistance)
            {
                EnablePlannerTarget(
                    combat
                );

                executingStartTick_ =
                    tick;

                SetState(
                    ObjectiveExecutorState::
                        Executing
                );

                Debug::Logger::Info(
                    "OBJECTIVE 11B: live target already "
                    "within combat handoff range."
                );

                return true;
            }

            const Navigation::NavPoint destination
            {
                profile.destination.x,
                profile.destination.y,
                profile.destination.z
            };

            if (!navigator_.Start(
                    world.player,
                    tick,
                    destination,
                    profile.destination.mapId,
                    profile.destination.arrivalDistance,
                    profile.destination.label))
            {
                Fail(
                    combat,
                    "generic NavMesh route failed to start."
                );

                return false;
            }

            navigationLastHealth_ =
                world.player.health;

            navigationTargetBaseline_ =
                world.player.targetGuid;

            SetState(
                ObjectiveExecutorState::
                    Navigating
            );

            return true;
        }

        void Update(
            const QuestPlannerSnapshot& snapshot,
            const Objects::WorldState& world,
            CombatController& combat,
            std::uint64_t tick) override
        {
            if (
                state_ ==
                    ObjectiveExecutorState::Idle ||
                state_ ==
                    ObjectiveExecutorState::
                        ReadyForTurnIn ||
                state_ ==
                    ObjectiveExecutorState::Failed)
            {
                return;
            }

            const auto* liveQuest =
                FindLiveQuest(
                    snapshot
                );

            if (liveQuest == nullptr)
            {
                Fail(
                    combat,
                    "active quest disappeared before the "
                    "objective was observed complete."
                );

                return;
            }

            if (SelectedObjectiveComplete(*liveQuest, *profile_))
            {
                Complete(
                    combat
                );

                return;
            }

            if (defensiveCombatActive_)
            {
                UpdateDefensiveCombat(
                    world,
                    combat,
                    tick
                );

                return;
            }

            if (defensiveTargetAcquisitionPending_)
            {
                UpdateDefensiveTargetAcquisition(
                    world,
                    combat,
                    tick
                );

                return;
            }

            if (waitingForDefensiveRecovery_)
            {
                if (world.player.health == 0)
                {
                    Fail(
                        combat,
                        "player died while waiting for defensive recovery."
                    );

                    return;
                }

                if (TryBeginDirectAggressorDefense(
                        world,
                        combat,
                        tick,
                        "defensive recovery",
                        false))
                {
                    waitingForDefensiveRecovery_ =
                        false;

                    return;
                }

                if (TryBeginProactiveSelectedDefense(
                        world,
                        combat,
                        tick,
                        "defensive recovery"))
                {
                    waitingForDefensiveRecovery_ =
                        false;

                    return;
                }

                if (
                    world.player.health <
                        navigationLastHealth_)
                {
                    if (BeginDefensiveCombat(
                            world,
                            combat,
                            tick))
                    {
                        waitingForDefensiveRecovery_ =
                            false;
                    }

                    return;
                }

                navigationLastHealth_ =
                    world.player.health;

                const float hp =
                    HealthPercent(
                        world.player
                    );

                if (
                    hp <
                        DefensiveResumeHealthPercent)
                {
                    return;
                }

                waitingForDefensiveRecovery_ =
                    false;

                if (!RestartNavigation(
                        world,
                        tick))
                {
                    Fail(
                        combat,
                        "NavMesh route failed to restart after "
                        "defensive recovery."
                    );
                }

                return;
            }

            if (
                state_ ==
                    ObjectiveExecutorState::
                        Navigating)
            {
                /*
                 * Prefer real objective-target data over the static seed.
                 * This check intentionally runs before generic defensive
                 * preemption so the live objective target keeps normal objective semantics.
                 */
                const auto* liveTarget =
                    FindObjectiveTarget(
                        world
                    );

                if (
                    liveTarget != nullptr &&
                    liveTarget->distance <=
                        LiveTargetHandoffDistance)
                {
                    /*
                     * A previous CTM command remains active in the client
                     * until another movement command replaces it. Stop the
                     * route explicitly before combat takes ownership so the
                     * player cannot keep running through the live target.
                     */
                    navigator_.PauseForCombat(
                        world.player,
                        "live objective target entered combat handoff range"
                    );

                    EnablePlannerTarget(
                        combat
                    );

                    executingStartTick_ =
                        tick;

                    SetState(
                        ObjectiveExecutorState::
                            Executing
                    );

                    Debug::Logger::Info(
                        "OBJECTIVE 13A: live objective target acquired; "
                        "NavMesh ownership handed to combat."
                    );

                    return;
                }

                if (TryBeginDirectAggressorDefense(
                        world,
                        combat,
                        tick,
                        "planner navigation",
                        true))
                {
                    return;
                }

                if (TryBeginProactiveSelectedDefense(
                        world,
                        combat,
                        tick,
                        "planner navigation"))
                {
                    return;
                }

                if (
                    world.player.health <
                        navigationLastHealth_)
                {
                    BeginDefensiveCombat(
                        world,
                        combat,
                        tick
                    );

                    return;
                }

                navigationLastHealth_ =
                    world.player.health;

                navigator_.Update(
                    world.player,
                    tick
                );

                if (navigator_.Failed())
                {
                    Fail(
                        combat,
                        "generic NavMesh follower failed."
                    );

                    return;
                }

                if (navigator_.Arrived())
                {
                    EnablePlannerTarget(
                        combat
                    );

                    executingStartTick_ =
                        tick;

                    SetState(
                        ObjectiveExecutorState::
                            Executing
                    );

                    Debug::Logger::Info(
                        "OBJECTIVE 11B: objective area "
                        "reached; waiting for target and "
                        "using generic combat + loot."
                    );
                }

                return;
            }

            if (
                state_ ==
                    ObjectiveExecutorState::
                        Executing)
            {
                if (
                    tick >=
                        executingStartTick_ +
                        MaximumExecutingTicks)
                {
                    Fail(
                        combat,
                        "objective execution timeout reached."
                    );

                    return;
                }

                combat.Update(
                    world,
                    tick
                );
            }
        }

        bool OwnsControl() const override
        {
            return
                state_ ==
                    ObjectiveExecutorState::
                        Navigating ||
                state_ ==
                    ObjectiveExecutorState::
                        Executing ||
                state_ ==
                    ObjectiveExecutorState::
                        ReadyForTurnIn;
        }

        ObjectiveExecutorState State() const override
        {
            return
                state_;
        }

        const char* StateName() const override
        {
            return
                StateNameInternal(
                    state_
                );
        }

        const QuestProfile* Profile() const
        {
            return
                profile_;
        }

        const Navigation::GenericNavMeshPathFollower&
            Navigator() const
        {
            return
                navigator_;
        }
    };
}
