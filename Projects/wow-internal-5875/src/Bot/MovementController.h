#pragma once

#include "ClickToMoveController.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"
#include "../Objects/UnitSnapshot.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    struct ApproachPlan
    {
        bool valid = false;

        std::uint64_t targetGuid = 0;

        float targetX = 0.0f;
        float targetY = 0.0f;
        float targetZ = 0.0f;

        float destinationX = 0.0f;
        float destinationY = 0.0f;
        float destinationZ = 0.0f;

        float targetDistance = 0.0f;
        float standOffDistance = 0.0f;
    };

    class MovementController
    {
    private:
        static std::string Float(
            float value)
        {
            std::ostringstream stream;

            stream
                << std::fixed
                << std::setprecision(3)
                << value;

            return stream.str();
        }

        static float HorizontalDistance(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx =
                bx - ax;

            const float dy =
                by - ay;

            return std::sqrt(
                dx * dx +
                dy * dy
            );
        }

    public:
        static constexpr float DefaultStandOffDistance =
            3.5f;

        static bool BuildApproachPlan(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            float standOffDistance,
            ApproachPlan& plan)
        {
            plan =
                ApproachPlan{};

            if (!player.valid)
            {
                Debug::Logger::Info(
                    "MovementController: "
                    "invalid player."
                );

                return false;
            }

            if (!target.valid)
            {
                Debug::Logger::Info(
                    "MovementController: "
                    "invalid target."
                );

                return false;
            }

            if (
                target.guid == 0 ||
                target.health == 0)
            {
                Debug::Logger::Info(
                    "MovementController: "
                    "target is dead or has no GUID."
                );

                return false;
            }

            if (
                !std::isfinite(
                    standOffDistance
                ) ||
                standOffDistance <= 0.0f ||
                standOffDistance > 20.0f)
            {
                Debug::Logger::Info(
                    "MovementController: "
                    "invalid stand-off distance."
                );

                return false;
            }

            /*
             * Direction from mob -> player.
             *
             * This means our destination stays on
             * the side of the mob that the player
             * is already approaching from.
             */
            const float dx =
                player.x - target.x;

            const float dy =
                player.y - target.y;

            const float horizontalDistance =
                std::sqrt(
                    dx * dx +
                    dy * dy
                );

            if (
                !std::isfinite(
                    horizontalDistance
                ) ||
                horizontalDistance < 0.001f)
            {
                Debug::Logger::Info(
                    "MovementController: "
                    "player and target have "
                    "indistinguishable XY position."
                );

                return false;
            }

            const float nx =
                dx / horizontalDistance;

            const float ny =
                dy / horizontalDistance;

            plan.valid =
                true;

            plan.targetGuid =
                target.guid;

            plan.targetX =
                target.x;

            plan.targetY =
                target.y;

            plan.targetZ =
                target.z;

            /*
             * Destination = target position +
             * direction toward player * standOff.
             */
            plan.destinationX =
                target.x +
                nx * standOffDistance;

            plan.destinationY =
                target.y +
                ny * standOffDistance;

            /*
             * For this first controlled probe
             * we use target ground height.
             */
            plan.destinationZ =
                target.z;

            plan.targetDistance =
                target.distance;

            plan.standOffDistance =
                standOffDistance;

            Debug::Logger::Info(
                "MovementController approach plan:"
            );

            Debug::Logger::Info(
                "Target distance: " +
                Float(
                    target.distance
                )
            );

            Debug::Logger::Info(
                "Stand-off: " +
                Float(
                    standOffDistance
                )
            );

            Debug::Logger::Info(
                "Target position: (" +
                Float(target.x) +
                "," +
                Float(target.y) +
                "," +
                Float(target.z) +
                ")"
            );

            Debug::Logger::Info(
                "Approach destination: (" +
                Float(
                    plan.destinationX
                ) +
                "," +
                Float(
                    plan.destinationY
                ) +
                "," +
                Float(
                    plan.destinationZ
                ) +
                ")"
            );

            const float calculatedDistance =
                HorizontalDistance(
                    plan.destinationX,
                    plan.destinationY,
                    target.x,
                    target.y
                );

            Debug::Logger::Info(
                "Calculated destination "
                "stand-off: " +
                Float(
                    calculatedDistance
                )
            );

            return true;
        }

        static bool HoldPosition(
            const Objects::PlayerState& player)
        {
            if (!player.valid || player.address == 0)
            {
                Debug::Logger::Info(
                    "COMBAT POSITIONING 14G.3.2: hold rejected - invalid player."
                );

                return false;
            }

            /*
             * Use the already verified CTM Move path instead of the client's
             * unverified CTM Stop opcode. A Move request to the live player
             * position replaces any stale chase destination and therefore
             * neutralizes forward overshoot before facing/attacking.
             */
            Debug::Logger::Info(
                "COMBAT POSITIONING 14G.3.2: MELEE HOLD - neutralizing stale CTM at live player position."
            );

            return
                ClickToMoveController::MoveTo(
                    player,
                    player.x,
                    player.y,
                    player.z,
                    0.35f
                );
        }

        static bool MoveAwayFromTarget(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            float separationDistance)
        {
            if (
                !player.valid ||
                player.address == 0 ||
                !target.valid ||
                target.guid == 0 ||
                target.health == 0 ||
                !std::isfinite(separationDistance) ||
                separationDistance <= 0.0f ||
                separationDistance > 4.0f)
            {
                Debug::Logger::Info(
                    "COMBAT POSITIONING 14G.3.2: separation rejected - invalid input."
                );

                return false;
            }

            float dx =
                player.x - target.x;

            float dy =
                player.y - target.y;

            float length =
                std::sqrt(dx * dx + dy * dy);

            if (!std::isfinite(length))
            {
                return false;
            }

            /*
             * At almost identical XY coordinates, target->player direction is
             * numerically unstable. Fall back to a point behind the player's
             * current heading to create separation first; normal facing is
             * re-established after the bounded movement window.
             */
            if (length < 0.05f)
            {
                dx = -std::cos(player.rotation);
                dy = -std::sin(player.rotation);
                length = 1.0f;
            }

            const float nx =
                dx / length;

            const float ny =
                dy / length;

            const float destinationX =
                player.x + nx * separationDistance;

            const float destinationY =
                player.y + ny * separationDistance;

            Debug::Logger::Info(
                "COMBAT POSITIONING 14G.3.2: SEPARATION STEP destination=(" +
                Float(destinationX) + "," +
                Float(destinationY) + "," +
                Float(player.z) + ") distance=" +
                Float(separationDistance)
            );

            return
                ClickToMoveController::MoveTo(
                    player,
                    destinationX,
                    destinationY,
                    player.z,
                    0.35f
                );
        }

        static bool MoveToApproachPoint(
            const Objects::PlayerState& player,
            const ApproachPlan& plan)
        {
            if (!plan.valid)
            {
                Debug::Logger::Info(
                    "MovementController: "
                    "approach plan invalid."
                );

                return false;
            }

            /*
             * Low CTM precision here because
             * stand-off is handled by the
             * destination itself.
             *
             * We do NOT rely on Vanilla CTM's
             * precision argument for melee range.
             */
            constexpr float ctmPrecision =
                0.75f;

            Debug::Logger::Info(
                "MovementController: "
                "issuing approach CTM."
            );

            return
                ClickToMoveController::
                    MoveTo(
                        player,

                        plan.destinationX,
                        plan.destinationY,
                        plan.destinationZ,

                        ctmPrecision
                    );
        }
    };
}
