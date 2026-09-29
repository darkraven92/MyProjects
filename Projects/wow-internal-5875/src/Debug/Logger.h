#pragma once

#include <windows.h>

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

            std::ofstream log(
                Path(),
                std::ios::out | std::ios::app
            );

            if (!log)
                return;

            log
                << "[INFO] "
                << message
                << '\n';
        }

    private:
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
