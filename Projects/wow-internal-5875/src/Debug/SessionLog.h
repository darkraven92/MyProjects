#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace Debug
{
    // Diagnostic identity only. Never owns or resets a bot/controller.
    // Process creation time distinguishes PID reuse; thread/start distinguish
    // DLL reloads in the same process even when the module base is reused.
    struct LogSessionIdentity
    {
        std::uint32_t pid = 0;
        std::uint64_t processCreated = 0;
        std::uint64_t startedMs = 0;
        std::uint32_t threadId = 0;
        std::uintptr_t moduleBase = 0;
        std::string process;

        std::string Fields() const
        {
            std::ostringstream out;
            out << "pid=" << pid << " session=" << pid << '.'
                << processCreated << '.' << startedMs << '.' << threadId
                << " dllBase=0x" << std::hex << moduleBase << std::dec
                << " process=\"" << process << '"';
            return out.str();
        }
    };

    inline bool AppendSessionLog(const std::filesystem::path& path,
        const std::string& sessionFields, const std::string& message)
    {
        std::ofstream log(path, std::ios::out | std::ios::app);
        if (!log)
            return false;
        log.seekp(0, std::ios::end);
        // Rotation/deletion restores the SAME logger identity. It does not
        // mean a new DLL attachment or a new planner session.
        if (log.tellp() == std::streampos(0))
            log << "[INFO] LOGGER SESSION " << sessionFields
                << " openMode=append file=\"" << path.string()
                << "\" reason=empty_file runtimeReset=no\n";
        log << "[INFO] " << message << '\n';
        log.flush();
        return static_cast<bool>(log);
    }
}
