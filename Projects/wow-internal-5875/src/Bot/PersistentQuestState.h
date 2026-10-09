#pragma once
#include "QuestOfferResolutionPolicy.h"

#include "VanillaQuestDatabase.h"
#include "../Debug/Logger.h"
#include "../Wow5875/Client.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Bot
{
    class PersistentQuestState
    {
    private:
        static constexpr int FormatVersion = 1;
        static constexpr std::uintptr_t ObjectManagerRootRva = 0x00741414;
        static constexpr std::uintptr_t ActivePlayerGuidOffset = 0x000000C0;

        bool loaded_ = false;
        bool loggedWaitingForGuid_ = false;
        std::uint64_t characterGuid_ = 0;
        int storedLevel_ = 0;
        std::filesystem::path statePath_{};
        std::set<int> lastSavedCompleted_{};
        std::set<std::uint32_t> lastSavedCheckedGivers_{};

        static std::uint64_t ParseGuid(const std::string& text)
        {
            if (text.empty())
                return 0;

            char* end = nullptr;
            const unsigned long long value = std::strtoull(text.c_str(), &end, 0);
            return end != text.c_str() ? static_cast<std::uint64_t>(value) : 0;
        }

#ifdef _WIN32
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
#endif

        static std::uint64_t ResolveCharacterGuid()
        {
            if (const char* overrideGuid = std::getenv("WOW_INTERNAL_CHARACTER_GUID"))
            {
                const auto parsed = ParseGuid(overrideGuid);
                if (parsed != 0)
                    return parsed;
            }

#ifdef _WIN32
            std::uint32_t manager = 0;
            if (!ReadValue(Wow5875::Client::Base() + ObjectManagerRootRva, manager) ||
                manager == 0 || (manager & 1u) != 0)
            {
                return 0;
            }

            std::uint64_t guid = 0;
            if (!ReadValue(static_cast<std::uintptr_t>(manager) + ActivePlayerGuidOffset, guid))
                return 0;
            return guid;
#else
            return 0;
#endif
        }

        static std::string GuidHex(std::uint64_t guid)
        {
            std::ostringstream stream;
            stream << std::hex << std::uppercase
                   << std::setw(16) << std::setfill('0') << guid;
            return stream.str();
        }

        static std::filesystem::path ResolveProjectRoot()
        {
            auto& db = VanillaQuestDatabase::Instance();
            if (!db.EnsureLoaded() || db.LoadedPath().empty())
                return {};

            std::error_code ec;
            std::filesystem::path path(db.LoadedPath());
            if (path.is_relative())
                path = std::filesystem::absolute(path, ec);
            if (ec)
                return {};

            // .../<project>/data/questdb/runtime/valley_of_trials.tsv
            for (int i = 0; i < 4; ++i)
            {
                if (!path.has_parent_path())
                    return {};
                path = path.parent_path();
            }
            return path;
        }

        bool BuildStatePath()
        {
            const auto root = ResolveProjectRoot();
            if (root.empty() || characterGuid_ == 0)
                return false;

            const auto dir = root / "data" / "state" / "quest_completion";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                Debug::Logger::Info(
                    "QUESTDB 12B.9: failed to create persistent state directory: " +
                    ec.message());
                return false;
            }

            statePath_ = dir / ("player_" + GuidHex(characterGuid_) + ".txt");
            return true;
        }

        bool LoadFile(
            std::set<int>& completedQuestIds,
            std::set<std::uint32_t>& checkedGiverEntries)
        {
            std::ifstream input(statePath_);
            if (!input)
                return true; // first run for this character

            int version = 0;
            int fileLevel = 0;
            int giverAuditVersion = 0;
            std::uint64_t fileGuid = 0;
            std::set<int> fileCompleted;
            std::set<std::uint32_t> fileChecked;

            std::string line;
            while (std::getline(input, line))
            {
                if (line.empty() || line[0] == '#')
                    continue;

                const auto equals = line.find('=');
                if (equals == std::string::npos)
                    continue;

                const std::string key = line.substr(0, equals);
                const std::string value = line.substr(equals + 1);

                if (key == "version")
                    version = std::atoi(value.c_str());
                else if (key == "guid")
                    fileGuid = ParseGuid(value);
                else if (key == "level")
                    fileLevel = std::atoi(value.c_str());
                else if (key == "giverAuditVersion")
                    giverAuditVersion = std::atoi(value.c_str());
                else if (key == "completed")
                {
                    const int questId = std::atoi(value.c_str());
                    if (questId > 0)
                        fileCompleted.insert(questId);
                }
                else if (key == "checkedGiver")
                {
                    const long entry = std::strtol(value.c_str(), nullptr, 10);
                    if (entry > 0)
                        fileChecked.insert(static_cast<std::uint32_t>(entry));
                }
            }

            if (version != 0 && version != FormatVersion)
            {
                Debug::Logger::Info(
                    "QUESTDB 12B.9: persistent state version mismatch; preserving file and starting empty session state.");
                return true;
            }

            if (fileGuid != 0 && fileGuid != characterGuid_)
            {
                Debug::Logger::Info(
                    "QUESTDB 12B.9: persistent state GUID mismatch; ignoring file.");
                return true;
            }

            completedQuestIds.insert(fileCompleted.begin(), fileCompleted.end());
            storedLevel_ = fileLevel;

            if (fileLevel > 0 && fileLevel == storedLevel_ &&
                QuestOfferResolutionPolicy::RestoreEmptyCache(giverAuditVersion))
                checkedGiverEntries.insert(fileChecked.begin(), fileChecked.end());
            else if(!fileChecked.empty())
                Debug::Logger::Info("QUEST GIVER CACHE result=reaudit reason=unverified_legacy_empty_cache completionLedger=preserved");

            return true;
        }

        bool WriteFile(
            int currentLevel,
            const std::set<int>& completedQuestIds,
            const std::set<std::uint32_t>& checkedGiverEntries)
        {
            if (statePath_.empty())
                return false;

            const auto tempPath = statePath_.string() + ".tmp";
            {
                std::ofstream output(tempPath, std::ios::trunc);
                if (!output)
                {
                    Debug::Logger::Info(
                        "QUESTDB 12B.9: failed to open persistent state temp file for writing.");
                    return false;
                }

                output << "# wow-internal persistent quest state v1\n";
                output << "version=" << FormatVersion << "\n";
                output << "guid=0x" << GuidHex(characterGuid_) << "\n";
                output << "level=" << currentLevel << "\n";
                output << "giverAuditVersion=" << QuestOfferResolutionPolicy::EmptyCacheVersion << "\n";
                for (const int questId : completedQuestIds)
                    output << "completed=" << questId << "\n";
                for (const auto giver : checkedGiverEntries)
                    output << "checkedGiver=" << giver << "\n";
                output.flush();
                if (!output)
                    return false;
            }

            std::error_code ec;
            std::filesystem::remove(statePath_, ec);
            ec.clear();
            std::filesystem::rename(tempPath, statePath_, ec);
            if (ec)
            {
                Debug::Logger::Info(
                    "QUESTDB 12B.9: failed to atomically replace persistent state: " + ec.message());
                std::filesystem::remove(tempPath, ec);
                return false;
            }

            storedLevel_ = currentLevel;
            lastSavedCompleted_ = completedQuestIds;
            lastSavedCheckedGivers_ = checkedGiverEntries;
            return true;
        }

    public:
        bool EnsureLoaded(
            int currentLevel,
            std::set<int>& completedQuestIds,
            std::set<std::uint32_t>& checkedGiverEntries)
        {
            if (!loaded_)
            {
                characterGuid_ = ResolveCharacterGuid();
                if (characterGuid_ == 0)
                {
                    if (!loggedWaitingForGuid_)
                    {
                        loggedWaitingForGuid_ = true;
                        Debug::Logger::Info(
                            "QUESTDB 12B.9: persistent state waiting for active player GUID.");
                    }
                    return false;
                }

                if (!BuildStatePath())
                    return false;

                if (!LoadFile(completedQuestIds, checkedGiverEntries))
                    return false;

                loaded_ = true;
                loggedWaitingForGuid_ = false;

                // checked-giver results are valid only for the level at which
                // they were audited. Completed quest IDs remain permanent.
                if (storedLevel_ != 0 && storedLevel_ != currentLevel)
                {
                    Debug::Logger::Info(
                        "QUESTDB 12B.9: player level changed from " +
                        std::to_string(storedLevel_) + " to " +
                        std::to_string(currentLevel) +
                        "; clearing persisted giver-audit cache while preserving completed quests.");
                    checkedGiverEntries.clear();
                }

                Debug::Logger::Info(
                    "QUESTDB 12B.9: PERSISTENT STATE LOADED path=" + statePath_.string() +
                    " guid=0x" + GuidHex(characterGuid_) +
                    " level=" + std::to_string(currentLevel) +
                    " completed=" + std::to_string(completedQuestIds.size()) +
                    " checkedGivers=" + std::to_string(checkedGiverEntries.size()));

                return WriteFile(currentLevel, completedQuestIds, checkedGiverEntries);
            }

            if (storedLevel_ != currentLevel)
            {
                Debug::Logger::Info(
                    "QUESTDB 12B.9: LEVEL INVALIDATION oldLevel=" +
                    std::to_string(storedLevel_) + " newLevel=" +
                    std::to_string(currentLevel) +
                    "; giver cache cleared, completed quests retained.");
                checkedGiverEntries.clear();
                return WriteFile(currentLevel, completedQuestIds, checkedGiverEntries);
            }

            return true;
        }

        void SaveIfChanged(
            int currentLevel,
            const std::set<int>& completedQuestIds,
            const std::set<std::uint32_t>& checkedGiverEntries,
            const char* reason)
        {
            if (!loaded_)
                return;

            if (storedLevel_ == currentLevel &&
                completedQuestIds == lastSavedCompleted_ &&
                checkedGiverEntries == lastSavedCheckedGivers_)
            {
                return;
            }

            if (WriteFile(currentLevel, completedQuestIds, checkedGiverEntries))
            {
                Debug::Logger::Info(
                    std::string("QUESTDB 12B.9: STATE CHECKPOINT reason=") +
                    (reason == nullptr ? "unspecified" : reason) +
                    " completed=" + std::to_string(completedQuestIds.size()) +
                    " checkedGivers=" + std::to_string(checkedGiverEntries.size()));
            }
        }

        bool Loaded() const { return loaded_; }
        std::uint64_t CharacterGuid() const { return characterGuid_; }
        const std::filesystem::path& StatePath() const { return statePath_; }
    };
}
