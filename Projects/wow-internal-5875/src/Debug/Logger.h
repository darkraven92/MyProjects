#pragma once

#include <windows.h>
#include "SessionLog.h"

#include <fstream>
#include <mutex>
#include <string>

namespace Debug
{
    class Logger
    {
    public:
        static void SetModule(HMODULE module)
        {
            std::lock_guard<std::mutex> lock(Mutex());
            Identity().moduleBase = reinterpret_cast<std::uintptr_t>(
                module ? module : GetModuleHandleA(nullptr));
            char modulePath[MAX_PATH] = {};

            const DWORD length =
                GetModuleFileNameA(
                    module,
                    modulePath,
                    MAX_PATH
                );

            if (length == 0)
                return;

            std::string path(modulePath);

            const auto position =
                path.find_last_of("\\/");

            if (position == std::string::npos)
                return;

            Path() =
                path.substr(0, position + 1) +
                "wow-internal.log";
        }

        static void Info(const std::string& message)
        {
            std::lock_guard<std::mutex> lock(Mutex());
            AppendSessionLog(Path(), Identity().Fields(), message);
        }

        // Separate append-only lifecycle journal survives GUI Clear log on
        // start. GUI and DLL both identify their own PID/session on EVERY
        // journal entry; a shared file never implies a shared controller.
        static void Journal(const std::string& message)
        {
            std::lock_guard<std::mutex> lock(Mutex());
            const auto path = std::filesystem::path(Path()).parent_path() /
                "wow-internal.lifecycle.log";
            AppendSessionLog(path, Identity().Fields(),
                message + " " + Identity().Fields() +
                " monotonicMs=" + std::to_string(GetTickCount64()));
        }

        static void Event(const std::string& message)
        {
            Info(message + " " + Identity().Fields() +
                " monotonicMs=" + std::to_string(GetTickCount64()));
            Journal(message);
        }

    private:
        static LogSessionIdentity& Identity()
        {
            static LogSessionIdentity identity = [] {
                LogSessionIdentity value;
                value.pid = GetCurrentProcessId();
                value.startedMs = GetTickCount64();
                value.threadId = GetCurrentThreadId();
                FILETIME created{}, exited{}, kernel{}, user{};
                if (GetProcessTimes(GetCurrentProcess(), &created, &exited,
                        &kernel, &user))
                    value.processCreated =
                        (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) |
                        created.dwLowDateTime;
                char executable[MAX_PATH]{};
                GetModuleFileNameA(nullptr, executable, MAX_PATH);
                value.process = executable;
                return value;
            }();
            return identity;
        }

        static std::string& Path()
        {
            static std::string path =
                "wow-internal.log";

            return path;
        }

        static std::mutex& Mutex()
        {
            static std::mutex mutex;
            return mutex;
        }
    };
}
