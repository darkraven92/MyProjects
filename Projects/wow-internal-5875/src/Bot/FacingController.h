#pragma once

#include "GameThreadDispatcher.h"

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"
#include "../Objects/UnitSnapshot.h"
#include "../Wow5875/Client.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    class FacingController
    {
    private:
        static constexpr float Pi =
            3.14159265358979323846f;

        static constexpr float TwoPi =
            Pi * 2.0f;

        static constexpr std::uintptr_t SetFacingRva =
            0x003C6F30;

        static constexpr std::uintptr_t SendMovementUpdateRva =
            0x00200A30;

        static constexpr std::uintptr_t MovementStructOffset =
            0x000009A8;

        static constexpr std::uint32_t MsgMoveSetFacing =
            0x000000DA;

        static std::string Hex32(
            std::uintptr_t value)
        {
            std::ostringstream stream;

            stream
                << "0x"
                << std::hex
                << std::uppercase
                << std::setw(8)
                << std::setfill('0')
                << static_cast<std::uint32_t>(
                    value
                );

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

        static bool IsExecutable(
            std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};

            if (VirtualQuery(
                    reinterpret_cast<LPCVOID>(
                        address
                    ),
                    &info,
                    sizeof(info)
                ) == 0)
            {
                return false;
            }

            if (info.State != MEM_COMMIT)
            {
                return false;
            }

            if ((info.Protect & PAGE_GUARD) != 0)
            {
                return false;
            }

            const DWORD protect =
                info.Protect & 0xFF;

            switch (protect)
            {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;

                default:
                    return false;
            }
        }

        static float NormalizeInternal(
            float angle)
        {
            while (angle < 0.0f)
            {
                angle += TwoPi;
            }

            while (angle >= TwoPi)
            {
                angle -= TwoPi;
            }

            return angle;
        }

    public:
        static std::uintptr_t
        SetFacingAddress()
        {
            return
                Wow5875::Client::Base() +
                SetFacingRva;
        }

        static std::uintptr_t
        SendMovementAddress()
        {
            return
                Wow5875::Client::Base() +
                SendMovementUpdateRva;
        }

        static std::uintptr_t
        MovementStructAddress(
            const Objects::PlayerState& player)
        {
            if (player.address == 0)
            {
                return 0;
            }

            return
                static_cast<std::uintptr_t>(
                    player.address
                ) +
                MovementStructOffset;
        }

        static bool Validate()
        {
            const auto setFacing =
                SetFacingAddress();

            const auto sendMovement =
                SendMovementAddress();

            Debug::Logger::Info(
                "SetFacing address: " +
                Hex32(setFacing)
            );

            Debug::Logger::Info(
                "SendMovementUpdate address: " +
                Hex32(sendMovement)
            );

            if (!IsExecutable(setFacing))
            {
                Debug::Logger::Info(
                    "Facing validation FAILED: "
                    "SetFacing not executable."
                );

                return false;
            }

            if (!IsExecutable(sendMovement))
            {
                Debug::Logger::Info(
                    "Facing validation FAILED: "
                    "SendMovementUpdate not executable."
                );

                return false;
            }

            Debug::Logger::Info(
                "Facing validation PASS."
            );

            return true;
        }

        static float Normalize(
            float angle)
        {
            return
                NormalizeInternal(angle);
        }

        static float CalculateFacing(
            const Objects::PlayerState& player,
            const Objects::UnitState& target)
        {
            const float dx =
                target.x - player.x;

            const float dy =
                target.y - player.y;

            return NormalizeInternal(
                std::atan2(
                    dy,
                    dx
                )
            );
        }

        static float AngularDifference(
            float a,
            float b)
        {
            float delta =
                std::fabs(
                    NormalizeInternal(a) -
                    NormalizeInternal(b)
                );

            if (delta > Pi)
            {
                delta =
                    TwoPi - delta;
            }

            return delta;
        }

        static bool Face(
            const Objects::PlayerState& player,
            const Objects::UnitState& target,
            float* desiredFacingOut = nullptr)
        {
            if (
                !player.valid ||
                player.address == 0)
            {
                Debug::Logger::Info(
                    "Facing rejected: "
                    "invalid player."
                );

                return false;
            }

            if (
                !target.valid ||
                target.guid == 0 ||
                target.health == 0)
            {
                Debug::Logger::Info(
                    "Facing rejected: "
                    "invalid/dead target."
                );

                return false;
            }

            if (!Validate())
            {
                return false;
            }

            const std::uintptr_t movementStruct =
                MovementStructAddress(
                    player
                );

            if (movementStruct == 0)
            {
                Debug::Logger::Info(
                    "Facing rejected: "
                    "invalid movement struct."
                );

                return false;
            }

            const float desired =
                CalculateFacing(
                    player,
                    target
                );

            if (desiredFacingOut != nullptr)
            {
                *desiredFacingOut =
                    desired;
            }

            const float initialDelta =
                AngularDifference(
                    player.rotation,
                    desired
                );

            Debug::Logger::Info(
                "================================"
            );

            Debug::Logger::Info(
                "Facing request:"
            );

            Debug::Logger::Info(
                "Player address: " +
                Hex32(
                    static_cast<std::uintptr_t>(
                        player.address
                    )
                )
            );

            Debug::Logger::Info(
                "Movement struct: " +
                Hex32(
                    movementStruct
                )
            );

            Debug::Logger::Info(
                "Current rotation: " +
                Float(
                    player.rotation
                )
            );

            Debug::Logger::Info(
                "Desired rotation: " +
                Float(
                    desired
                )
            );

            Debug::Logger::Info(
                "Angular delta: " +
                Float(
                    initialDelta
                )
            );

            using SetFacingFunction =
                void (__thiscall*)(
                    std::uint32_t,
                    float
                );

            using SendMovementFunction =
                void (__thiscall*)(
                    std::uint32_t,
                    std::uint32_t,
                    std::uint32_t,
                    std::uint32_t,
                    std::uint32_t
                );

            const auto setFacing =
                reinterpret_cast<
                    SetFacingFunction
                >(
                    SetFacingAddress()
                );

            const auto sendMovement =
                reinterpret_cast<
                    SendMovementFunction
                >(
                    SendMovementAddress()
                );

            const std::uint32_t movementThis =
                static_cast<std::uint32_t>(
                    movementStruct
                );

            const std::uint32_t playerThis =
                static_cast<std::uint32_t>(
                    player.address
                );

            const DWORD callerThread =
                GetCurrentThreadId();

            Debug::Logger::Info(
                "Facing caller thread: " +
                std::to_string(
                    callerThread
                )
            );

            bool executedOnGameThread =
                false;

            DWORD executionThread =
                0;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [=,
                     &executedOnGameThread,
                     &executionThread]()
                    {
                        executionThread =
                            GetCurrentThreadId();

                        executedOnGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            "Facing executing thread: " +
                            std::to_string(
                                executionThread
                            )
                        );

                        Debug::Logger::Info(
                            std::string(
                                "Facing on game thread: "
                            ) +
                            (
                                executedOnGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        setFacing(
                            movementThis,
                            desired
                        );

                        sendMovement(
                            playerThis,
                            static_cast<std::uint32_t>(
                                GetTickCount()
                            ),
                            MsgMoveSetFacing,
                            0,
                            0
                        );
                    }
                );

            if (!dispatched)
            {
                Debug::Logger::Info(
                    "Facing FAILED: "
                    "game-thread dispatch failed."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            if (!executedOnGameThread)
            {
                Debug::Logger::Info(
                    "Facing FAILED: "
                    "not executed on game thread."
                );

                Debug::Logger::Info(
                    "================================"
                );

                return false;
            }

            Debug::Logger::Info(
                "Facing dispatched successfully."
            );

            Debug::Logger::Info(
                "================================"
            );

            return true;
        }
    };
}
