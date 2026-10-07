#pragma once

#include "DeathRecoveryPolicy.h"

#include <cstdint>

namespace Bot
{
    // Death belongs to the bot lifecycle, independent of its current mode.
    // A one-HP snapshot may be a ghost on this client; Questing is held until
    // the existing Lua death probe resolves it rather than running an
    // objective against an unconfirmed player state.
    struct BotDeathOwnershipPolicy
    {
        static bool ShouldOwn(bool recoveryActive, bool recoveryFailed,
            bool playerValid, std::uint32_t health,
            std::uint32_t maxHealth, bool confirmedBootstrap)
        {
            return recoveryActive || recoveryFailed ||
                DeathRecoveryPolicy::CanStartFromDeath(
                    playerValid, health, maxHealth) || confirmedBootstrap;
        }

        static bool HoldQuestPlanner(bool deathOwned, bool playerValid,
            std::uint32_t health, std::uint32_t maxHealth,
            bool idleAliveConfirmed)
        {
            return deathOwned || (!idleAliveConfirmed &&
                DeathRecoveryPolicy::ShouldProbeIdleGhost(
                    playerValid, health, maxHealth));
        }

        // Normal mode is shared by Questing and Grind. A one-HP alive probe
        // only clears this gate for its own snapshot, and an empty unit view
        // at one HP remains insufficient for autonomous world decisions.
        static bool HoldNormalMode(bool deathOwned, bool playerValid,
            std::uint32_t health, std::uint32_t maxHealth,
            bool freshAliveProbe, bool localUnitsAvailable)
        {
            return deathOwned ||
                (DeathRecoveryPolicy::ShouldProbeIdleGhost(
                    playerValid, health, maxHealth) &&
                 (!freshAliveProbe || !localUnitsAvailable));
        }

        static bool BeginReconciliationHold(bool wasHolding, bool isHolding)
        {
            return !wasHolding && isHolding;
        }

    };
}
