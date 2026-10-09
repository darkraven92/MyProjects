#pragma once

#include "CombatController.h"
#include "TargetSelector.h"

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
    enum class ObjectiveDefenseUpdate
    {
        Clear,
        OwnsControl,
        Failed
    };

    // Shared defensive-combat preemption for planner-owned objectives that are
    // not themselves combat executors. It intentionally uses the strongest
    // available runtime signal: a live unit whose UnitTargetGuid equals the
    // ObjectManager active-player GUID. This avoids nearest-hostile guessing.
    class ObjectiveDefensiveCombatGuard
    {
    private:
        static constexpr std::uintptr_t ObjectManagerRootRva = 0x00741414;
        static constexpr std::uintptr_t ActivePlayerGuidOffset = 0x000000C0;
        static constexpr float DirectAggressorMaximumDistance = 35.0f;
        static constexpr float ResumeHealthPercent = 85.0f;
        static constexpr std::uint64_t AcquisitionWindowTicks = 24;

        bool initialized_ = false;
        bool acquisitionPending_ = false;
        bool combatActive_ = false;
        bool waitingForRecovery_ = false;
        bool navigationPaused_ = false;
        std::uint64_t defensiveGuid_ = 0;
        std::uint64_t acquisitionDeadlineTick_ = 0;
        std::uint32_t lastHealth_ = 0;
        std::string context_{};

        template <typename T>
        static bool ReadValue(std::uintptr_t address, T& value)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (address == 0 ||
                VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
            return true;
        }

        static std::uint64_t ActivePlayerGuid()
        {
            std::uint32_t manager = 0;
            if (!ReadValue(Wow5875::Client::Base() + ObjectManagerRootRva, manager) ||
                manager == 0 || (manager & 1u) != 0)
            {
                return 0;
            }

            std::uint64_t guid = 0;
            if (!ReadValue(
                    static_cast<std::uintptr_t>(manager) + ActivePlayerGuidOffset,
                    guid))
            {
                return 0;
            }

            return guid;
        }

        static std::string Hex64(std::uint64_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::uppercase << std::hex
                   << std::setw(16) << std::setfill('0') << value;
            return stream.str();
        }

        static float HealthPercent(const Objects::PlayerState& player)
        {
            if (player.maxHealth == 0)
                return 0.0f;
            return 100.0f * static_cast<float>(player.health) /
                   static_cast<float>(player.maxHealth);
        }

        static const Objects::UnitState* FindDirectAggressor(
            const Objects::WorldState& world)
        {
            const std::uint64_t playerGuid = ActivePlayerGuid();
            if (playerGuid == 0)
                return nullptr;

            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 || unit.health == 0 ||
                    unit.maxHealth == 0 || unit.targetGuid != playerGuid ||
                    unit.distance < 0.0f || unit.distance > DirectAggressorMaximumDistance)
                {
                    continue;
                }

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }
            return best;
        }

        static const Objects::UnitState* FindSelectedDefensiveTarget(
            const Objects::WorldState& world,
            std::uint64_t protectedObjectiveTargetGuid)
        {
            const std::uint64_t selectedGuid = world.player.targetGuid;
            if (selectedGuid == 0 || selectedGuid == protectedObjectiveTargetGuid)
                return nullptr;

            const auto* unit = TargetSelector::FindByGuid(world, selectedGuid);
            if (unit == nullptr || !unit->valid || unit->health == 0 ||
                unit->maxHealth == 0 || unit->distance < 0.0f ||
                unit->distance > DirectAggressorMaximumDistance)
            {
                return nullptr;
            }

            // Do not attack an arbitrary selected NPC merely because damage was
            // observed. A selected fallback is accepted only if the unit itself
            // reports the local player as its target.
            const std::uint64_t playerGuid = ActivePlayerGuid();
            if (playerGuid == 0 || unit->targetGuid != playerGuid)
                return nullptr;

            return unit;
        }

        bool PauseNavigation(
            const Objects::WorldState& world,
            Navigation::GenericNavMeshPathFollower* navigator,
            const std::string& reason)
        {
            if (navigator == nullptr || navigationPaused_)
                return true;

            if (!navigator->PauseForCombat(world.player, reason))
                return false;

            navigationPaused_ = true;
            return true;
        }

        bool Adopt(
            const Objects::WorldState& world,
            CombatController& combat,
            Navigation::GenericNavMeshPathFollower* navigator,
            const Objects::UnitState& attacker,
            std::uint64_t tick,
            const char* reason)
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info("OBJECTIVE DEFENSE 11D.2.2: DIRECT AGGRESSOR PREEMPTION");
            Debug::Logger::Info("Context: " + context_);
            Debug::Logger::Info(std::string("Reason: ") + reason);
            Debug::Logger::Info(
                "Attacker GUID=" + Hex64(attacker.guid) +
                " entry=" + std::to_string(attacker.entryId) +
                " distance=" + std::to_string(attacker.distance) +
                " unit.targetGuid=" + Hex64(attacker.targetGuid));
            Debug::Logger::Info("Player GUID=" + Hex64(ActivePlayerGuid()));
            Debug::Logger::Info("================================");

            // If the objective currently owns CTM, freeze it before combat.
            PauseNavigation(
                world,
                navigator,
                "planner-owned non-combat objective preempted by exact direct aggressor");

            lastHealth_ = world.player.health;
            acquisitionPending_ = false;
            waitingForRecovery_ = false;

            if (!combat.AdoptExactTargetForDefense(world, attacker.guid, tick))
            {
                Debug::Logger::Info(
                    "OBJECTIVE DEFENSE 11D.2.2: exact-GUID combat adoption not ready; defensive hold remains active.");
                acquisitionPending_ = true;
                acquisitionDeadlineTick_ = tick + AcquisitionWindowTicks;
                return false;
            }

            defensiveGuid_ = attacker.guid;
            combatActive_ = true;

            Debug::Logger::Info(
                "OBJECTIVE DEFENSE 11D.2.2: combat owns exact attacker " +
                Hex64(defensiveGuid_));
            return true;
        }

        ObjectiveDefenseUpdate UpdateActiveCombat(
            const Objects::WorldState& world,
            CombatController& combat,
            Navigation::GenericNavMeshPathFollower* navigator,
            std::uint64_t tick)
        {
            if (combat.Failed() || combat.State() == CombatState::Failed)
            {
                combat.ClearPlannerQuestTarget();
                combatActive_ = false;
                return ObjectiveDefenseUpdate::Failed;
            }

            combat.Update(world, tick);

            if (combat.LockedGuid() != 0)
                return ObjectiveDefenseUpdate::OwnsControl;

            if (combat.State() != CombatState::PostKillDelay &&
                combat.State() != CombatState::AcquiringTarget)
            {
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            const auto* oldAttacker = TargetSelector::FindByGuid(world, defensiveGuid_);
            if (oldAttacker != nullptr && oldAttacker->valid && oldAttacker->health > 0)
            {
                // The exact unit is still alive. Try to re-adopt rather than
                // letting the objective continue while under threat.
                if (Adopt(world, combat, navigator, *oldAttacker, tick, "exact attacker still alive"))
                    return ObjectiveDefenseUpdate::OwnsControl;
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            combat.ClearPlannerQuestTarget();
            combatActive_ = false;
            defensiveGuid_ = 0;
            lastHealth_ = world.player.health;

            // Chain-aggro: another unit targeting the player takes precedence
            // over recovery and objective resumption.
            if (const auto* next = FindDirectAggressor(world); next != nullptr)
            {
                Adopt(world, combat, navigator, *next, tick, "chained aggressor after kill");
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            waitingForRecovery_ = true;
            Debug::Logger::Info(
                "OBJECTIVE DEFENSE 11D.2.2: attacker resolved; waiting for >=85% HP before objective resume.");
            return ObjectiveDefenseUpdate::OwnsControl;
        }

        ObjectiveDefenseUpdate UpdateAcquisition(
            const Objects::WorldState& world,
            CombatController& combat,
            Navigation::GenericNavMeshPathFollower* navigator,
            std::uint64_t protectedObjectiveTargetGuid,
            std::uint64_t tick)
        {
            if (const auto* attacker = FindDirectAggressor(world); attacker != nullptr)
            {
                Adopt(world, combat, navigator, *attacker, tick, "direct targetGuid signal during acquisition");
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            if (const auto* selected = FindSelectedDefensiveTarget(
                    world, protectedObjectiveTargetGuid); selected != nullptr)
            {
                Adopt(world, combat, navigator, *selected, tick, "verified selected attacker during acquisition");
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            lastHealth_ = world.player.health;

            if (tick < acquisitionDeadlineTick_)
                return ObjectiveDefenseUpdate::OwnsControl;

            if (HealthPercent(world.player) >= ResumeHealthPercent)
            {
                acquisitionPending_ = false;
                waitingForRecovery_ = true;
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            acquisitionDeadlineTick_ = tick + AcquisitionWindowTicks;
            Debug::Logger::Info(
                "OBJECTIVE DEFENSE 11D.2.2: damage was observed but no exact attacker signal is synchronized yet; objective remains paused for another bounded acquisition window.");
            return ObjectiveDefenseUpdate::OwnsControl;
        }

        ObjectiveDefenseUpdate UpdateRecovery(
            const Objects::WorldState& world,
            CombatController& combat,
            Navigation::GenericNavMeshPathFollower* navigator,
            std::uint64_t tick)
        {
            if (const auto* attacker = FindDirectAggressor(world); attacker != nullptr)
            {
                waitingForRecovery_ = false;
                Adopt(world, combat, navigator, *attacker, tick, "new direct aggressor during recovery");
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            lastHealth_ = world.player.health;
            if (HealthPercent(world.player) < ResumeHealthPercent)
                return ObjectiveDefenseUpdate::OwnsControl;

            waitingForRecovery_ = false;

            if (navigationPaused_)
            {
                if (navigator == nullptr || !navigator->ResumeAfterCombat(world.player, tick))
                {
                    Debug::Logger::Info(
                        "OBJECTIVE DEFENSE 11D.2.2: cached objective navigation failed to resume after combat.");
                    return ObjectiveDefenseUpdate::Failed;
                }
                navigationPaused_ = false;
            }

            Debug::Logger::Info(
                "OBJECTIVE DEFENSE 11D.2.2: recovery complete; returning ownership to objective executor.");
            return ObjectiveDefenseUpdate::Clear;
        }

    public:
        static bool HasDirectAggressor(const Objects::WorldState& world)
        { return FindDirectAggressor(world) != nullptr; }

        void Reset(const Objects::WorldState& world, const std::string& context)
        {
            initialized_ = true;
            acquisitionPending_ = false;
            combatActive_ = false;
            waitingForRecovery_ = false;
            navigationPaused_ = false;
            defensiveGuid_ = 0;
            acquisitionDeadlineTick_ = 0;
            lastHealth_ = world.player.health;
            context_ = context;
        }

        ObjectiveDefenseUpdate Update(
            const Objects::WorldState& world,
            CombatController& combat,
            Navigation::GenericNavMeshPathFollower* navigator,
            std::uint64_t protectedObjectiveTargetGuid,
            std::uint64_t tick)
        {
            if (!initialized_)
                Reset(world, "planner objective");

            if (world.player.health == 0)
            {
                combat.ClearPlannerQuestTarget();
                return ObjectiveDefenseUpdate::Failed;
            }

            if (combatActive_)
                return UpdateActiveCombat(world, combat, navigator, tick);

            if (acquisitionPending_)
                return UpdateAcquisition(
                    world, combat, navigator, protectedObjectiveTargetGuid, tick);

            if (waitingForRecovery_)
                return UpdateRecovery(world, combat, navigator, tick);

            // Proactive exact threat signal. This catches attacks even before
            // the next player-health sample shows damage.
            if (const auto* attacker = FindDirectAggressor(world); attacker != nullptr)
            {
                Adopt(world, combat, navigator, *attacker, tick, "live unit is targeting local player");
                return ObjectiveDefenseUpdate::OwnsControl;
            }

            if (world.player.health < lastHealth_)
            {
                Debug::Logger::Info("================================");
                Debug::Logger::Info("OBJECTIVE DEFENSE 11D.2.2: INCOMING DAMAGE");
                Debug::Logger::Info(
                    "Health: " + std::to_string(lastHealth_) + " -> " +
                    std::to_string(world.player.health) + "/" +
                    std::to_string(world.player.maxHealth));
                Debug::Logger::Info("================================");

                PauseNavigation(
                    world,
                    navigator,
                    "player HP dropped during planner-owned non-combat objective");

                lastHealth_ = world.player.health;
                acquisitionPending_ = true;
                acquisitionDeadlineTick_ = tick + AcquisitionWindowTicks;

                // Re-check after pausing in case the target signal arrived in
                // this same WorldState snapshot.
                return UpdateAcquisition(
                    world, combat, navigator, protectedObjectiveTargetGuid, tick);
            }

            lastHealth_ = world.player.health;
            return ObjectiveDefenseUpdate::Clear;
        }

        bool OwnsControl() const
        {
            return acquisitionPending_ || combatActive_ || waitingForRecovery_;
        }
    };
}
