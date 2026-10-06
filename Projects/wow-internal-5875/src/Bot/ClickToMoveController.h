#pragma once

#include "GameThreadDispatcher.h"
#include "MovementCommandTrace.h"
#include <source_location>

#include "../Debug/Logger.h"
#include "../Objects/PlayerSnapshot.h"

#include "../Wow5875/Client.h"
#include "../Wow5875/Offsets.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    enum class ClickToMoveType :
        std::uint32_t
    {
        FaceTarget = 0x01,
        Face = 0x02,

        /*
         * Vi använder inte Stop = 0x03 ännu.
         */
        Move = 0x04,

        NpcInteract = 0x05,
        Loot = 0x06,
        ObjectInteract = 0x07,

        FaceOther = 0x08,
        Skin = 0x09,

        AttackPosition = 0x0A,
        AttackGuid = 0x0B,

        ConstantFace = 0x0C,

        None = 0x0D,

        Attack = 0x10,
        Idle = 0x13
    };

    struct ClickToMovePosition
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    class ClickToMoveController
    {
    private:
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
                << std::setprecision(3)
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

            if ((info.Protect &
                 PAGE_GUARD) != 0)
            {
                return false;
            }

            if ((info.Protect &
                 PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD protection =
                info.Protect & 0xFF;

            switch (protection)
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

    public:
        static const MovementCommandTrace& LastCommand() { return commandTrace_; }
    private:
        inline static MovementCommandTrace commandTrace_{};
    public:
        static std::uintptr_t FunctionAddress()
        {
            return
                Wow5875::Client::Base() +
                Wow5875::Offsets::
                    Functions::
                    ClickToMoveRva;
        }

        static bool Validate()
        {
            const std::uintptr_t address =
                FunctionAddress();

            Debug::Logger::Info(
                "ClickToMove function address: " +
                Hex32(address)
            );

            const std::uintptr_t expected =
                Wow5875::Client::Base() +
                0x00211130;

            if (address != expected)
            {
                Debug::Logger::Info(
                    "ClickToMove validation FAILED: "
                    "unexpected function address."
                );

                return false;
            }

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "ClickToMove validation FAILED: "
                    "address is not executable."
                );

                return false;
            }

            Debug::Logger::Info(
                "ClickToMove validation PASS."
            );

            /*
             * This only resolves WoW's HWND and
             * window-thread ID.
             *
             * No persistent WndProc hook is
             * installed here.
             */
            if (!GameThreadDispatcher::Initialize())
            {
                Debug::Logger::Info(
                    "ClickToMove validation FAILED: "
                    "dispatcher could not resolve "
                    "WoW window."
                );

                return false;
            }

            Debug::Logger::Info(
                "GameThreadDispatcher ready "
                "(no persistent hook)."
            );

            return true;
        }

        static bool MoveTo(
            const Objects::PlayerState& player,
            float x,
            float y,
            float z,
            float precision,
            const std::source_location origin = std::source_location::current())
        {
            if (!player.valid ||
                player.address == 0)
            {
                Debug::Logger::Info(
                    "ClickToMove rejected: "
                    "invalid player."
                );

                return false;
            }

            if (!std::isfinite(x) ||
                !std::isfinite(y) ||
                !std::isfinite(z) ||
                !std::isfinite(precision))
            {
                Debug::Logger::Info(
                    "ClickToMove rejected: "
                    "invalid coordinates."
                );

                return false;
            }

            if (precision <= 0.0f ||
                precision > 100.0f)
            {
                Debug::Logger::Info(
                    "ClickToMove rejected: "
                    "invalid precision."
                );

                return false;
            }

            const std::uintptr_t address =
                FunctionAddress();

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "ClickToMove rejected: "
                    "function address invalid."
                );

                return false;
            }

            ClickToMovePosition position{};

            position.x = x;
            position.y = y;
            position.z = z;

            std::uint64_t zeroGuid =
                0;

            Debug::Logger::Info(
                "ClickToMove request:"
            );

            Debug::Logger::Info(
                "Player: " +
                Hex32(
                    player.address
                )
            );

            Debug::Logger::Info(
                "Destination: (" +
                Float(position.x) +
                "," +
                Float(position.y) +
                "," +
                Float(position.z) +
                ")"
            );

            Debug::Logger::Info(
                "Precision: " +
                Float(precision)
            );

            Debug::Logger::Info(
                "CTM type: Move (0x04)"
            );

            Debug::Logger::Info(
                "Caller thread: " +
                std::to_string(
                    GetCurrentThreadId()
                )
            );

            /*
             * WoW 1.12.1 build 5875.
             */
            using ClickToMoveFunction =
                void (__thiscall*)(
                    std::uint32_t,
                    std::uint32_t,
                    std::uint64_t*,
                    ClickToMovePosition*,
                    float
                );

            const auto function =
                reinterpret_cast<
                    ClickToMoveFunction
                >(address);

            bool executedOnGameThread =
                false;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [&]()
                    {
                        executedOnGameThread =
                            GameThreadDispatcher::
                                IsGameThread();

                        Debug::Logger::Info(
                            "ClickToMove executing thread: " +
                            std::to_string(
                                GetCurrentThreadId()
                            )
                        );

                        Debug::Logger::Info(
                            std::string(
                                "ClickToMove on game thread: "
                            ) +
                            (
                                executedOnGameThread
                                    ? "yes"
                                    : "no"
                            )
                        );

                        function(
                            player.address,
                            static_cast<
                                std::uint32_t
                            >(
                                ClickToMoveType::Move
                            ),
                            &zeroGuid,
                            &position,
                            precision
                        );
                    }
                );

            if (!dispatched)
            {
                Debug::Logger::Info(
                    "ClickToMove FAILED: "
                    "game-thread dispatch failed."
                );

                return false;
            }

            if (!executedOnGameThread)
            {
                Debug::Logger::Info(
                    "ClickToMove FAILED: "
                    "wrong execution thread."
                );

                return false;
            }

            Debug::Logger::Info(
                "ClickToMove dispatched successfully."
            );

            if (commandTrace_.writer != origin.function_name())
                Debug::Logger::Info("MOVEMENT COMMAND WRITER previous=" + commandTrace_.writer +
                    " next=" + origin.function_name() + " sourceLine=" + std::to_string(origin.line()));
            commandTrace_.Observe(origin.function_name(), GetTickCount64(), x, y, z);

            return true;
        }
    };
}
