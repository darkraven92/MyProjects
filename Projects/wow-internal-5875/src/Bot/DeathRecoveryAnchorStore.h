#pragma once

#include "DeathRecoveryPolicy.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Bot
{
    struct DeathAnchorPosition
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // Only an observed body-death position is stored. A restart as a ghost
    // may reuse that same episode's anchor; stale/foreign records fail closed.
    struct DeathRecoveryAnchorPolicy
    {
        static constexpr std::int64_t MaximumAgeSeconds =
            DeathRecoveryLivenessPolicy::MaximumEpisodeAgeMs / 1000;

        static bool Eligible(std::uint64_t expectedGuid,
            std::uint32_t expectedMap, std::uint64_t storedGuid,
            std::uint32_t storedMap, std::int64_t ageSeconds,
            bool active, DeathAnchorPosition position)
        {
            return expectedGuid != 0 && expectedGuid == storedGuid &&
                expectedMap == storedMap && active && ageSeconds >= 0 &&
                ageSeconds <= MaximumAgeSeconds &&
                std::isfinite(position.x) && std::isfinite(position.y) &&
                std::isfinite(position.z);
        }
    };

    class DeathRecoveryAnchorStore
    {
    private:
        static std::filesystem::path ProjectRoot()
        {
#ifdef _WIN32
            static const int moduleAnchor = 0;
            HMODULE module = nullptr;
            if (!GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(&moduleAnchor), &module) ||
                module == nullptr)
                return {};
            char modulePath[MAX_PATH]{};
            const DWORD length = GetModuleFileNameA(
                module, modulePath, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
                return {};
            const auto root = std::filesystem::path(
                std::string(modulePath, length)).parent_path().parent_path();
            std::error_code error;
            if (!std::filesystem::exists(root / "AGENTS.md", error) || error ||
                !std::filesystem::exists(
                    root / "data" / "questdb" / "runtime" /
                        "durotar.tsv", error) || error)
                return {};
            return root;
#else
            return {};
#endif
        }

        static std::filesystem::path Path(std::uint64_t guid)
        {
            if (guid == 0)
                return {};
            const auto root = ProjectRoot();
            if (root.empty())
                return {};
            std::ostringstream name;
            name << "player_" << std::uppercase << std::hex
                 << std::setw(16) << std::setfill('0') << guid << ".txt";
            return root / "data" / "state" / "death_recovery" / name.str();
        }

        static std::int64_t NowSeconds()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

    public:
        static bool Write(std::uint64_t guid, std::uint32_t mapId,
            DeathAnchorPosition position, bool active)
        {
            const auto path = Path(guid);
            if (path.empty() || !std::isfinite(position.x) ||
                !std::isfinite(position.y) || !std::isfinite(position.z))
                return false;
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            if (error)
                return false;
            std::ofstream output(path, std::ios::trunc);
            if (!output)
                return false;
            output << 1 << ' ' << guid << ' ' << mapId << ' '
                   << NowSeconds() << ' ' << (active ? 1 : 0) << ' '
                   << std::setprecision(9) << position.x << ' '
                   << position.y << ' ' << position.z << '\n';
            return static_cast<bool>(output);
        }

        static bool Load(std::uint64_t guid, std::uint32_t mapId,
            DeathAnchorPosition& position)
        {
            position = {};
            const auto path = Path(guid);
            if (path.empty())
                return false;
            std::ifstream input(path);
            int version = 0;
            std::uint64_t storedGuid = 0;
            std::uint32_t storedMap = 0;
            std::int64_t storedAt = 0;
            int active = 0;
            DeathAnchorPosition stored{};
            if (!(input >> version >> storedGuid >> storedMap >> storedAt >>
                    active >> stored.x >> stored.y >> stored.z) ||
                version != 1 ||
                !DeathRecoveryAnchorPolicy::Eligible(
                    guid, mapId, storedGuid, storedMap,
                    NowSeconds() - storedAt, active == 1, stored))
                return false;
            position = stored;
            return true;
        }
    };
}
