#pragma once

#include "../Debug/Logger.h"
#include "../Wow5875/Client.h"
#include "../Wow5875/Offsets.h"
#include "GameThreadDispatcher.h"

#include <windows.h>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Bot
{
    class TargetController
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

        static bool IsExecutable(
            std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};

            const SIZE_T result =
                VirtualQuery(
                    reinterpret_cast<LPCVOID>(
                        address
                    ),
                    &info,
                    sizeof(info)
                );

            if (result == 0)
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

            if ((info.Protect & PAGE_NOACCESS) != 0)
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

    public:
        static std::uintptr_t FunctionAddress()
        {
            /*
             * WoW.exe base:
             *
             *     0x00400000
             *
             * SetTarget RVA:
             *
             *     0x00093540
             *
             * Result:
             *
             *     0x00493540
             */
            return
                Wow5875::Client::Base() +
                Wow5875::Offsets::
                    Functions::
                    SetTargetRva;
        }

        static bool Validate()
        {
            const std::uintptr_t address =
                FunctionAddress();

            Debug::Logger::Info(
                "SetTarget function address: " +
                Hex32(address)
            );

            const std::uintptr_t expected =
                Wow5875::Client::Base() +
                0x00093540;

            if (address != expected)
            {
                Debug::Logger::Info(
                    "SetTarget validation FAILED: "
                    "unexpected address."
                );

                return false;
            }

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "SetTarget validation FAILED: "
                    "address is not executable."
                );

                return false;
            }

            Debug::Logger::Info(
                "SetTarget validation PASS."
            );

            return true;
        }

        static bool SetTarget(
            std::uint64_t guid)
        {
            if (guid == 0)
            {
                Debug::Logger::Info(
                    "SetTarget rejected: "
                    "GUID is zero."
                );

                return false;
            }

            const std::uintptr_t address =
                FunctionAddress();

            if (!IsExecutable(address))
            {
                Debug::Logger::Info(
                    "SetTarget rejected: "
                    "function address invalid."
                );

                return false;
            }

            Debug::Logger::Info(
                "SetTarget request: guid=" +
                Hex64(guid)
            );

            Debug::Logger::Info(
                "SetTarget caller thread: " +
                std::to_string(
                    GetCurrentThreadId()
                )
            );

            using SetTargetFunction =
                void (__stdcall*)(
                    std::uint64_t
                );

            const auto function =
                reinterpret_cast<
                    SetTargetFunction
                >(address);

            bool nativeCallReturned =
                false;

            const bool dispatched =
                GameThreadDispatcher::Invoke(
                    [function, guid, &nativeCallReturned]()
                    {
                        Debug::Logger::Info(
                            "TARGET 14G.1.3: executing SetTarget on WoW window thread."
                        );

                        Debug::Logger::Info(
                            "TARGET 14G.1.3: execution thread=" +
                            std::to_string(
                                GetCurrentThreadId()
                            ) +
                            " gameThread=" +
                            std::string(
                                GameThreadDispatcher::IsGameThread()
                                    ? "yes"
                                    : "no"
                            )
                        );

                        function(guid);

                        nativeCallReturned =
                            true;

                        Debug::Logger::Info(
                            "TARGET 14G.1.3: native SetTarget returned on game thread."
                        );
                    }
                );

            if (!dispatched)
            {
                Debug::Logger::Info(
                    "TARGET 14G.1.3: SetTarget dispatch failed."
                );

                return false;
            }

            if (!nativeCallReturned)
            {
                Debug::Logger::Info(
                    "TARGET 14G.1.3: dispatch completed without native SetTarget return evidence."
                );

                return false;
            }

            Debug::Logger::Info(
                "TARGET 14G.1.3: SetTarget dispatched successfully."
            );

            return true;
        }
    };
}
