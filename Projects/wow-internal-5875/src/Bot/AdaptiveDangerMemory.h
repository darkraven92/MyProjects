#pragma once

#include "AdaptiveDangerPolicy.h"
#include "VanillaQuestDatabase.h"

#include "../Debug/Logger.h"
#include "../Navigation/DetourNavigationProvider.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace Bot
{
    /*
     * Phase 14G.5
     *
     * Persistent, character-local danger memory.  It stores only runtime
     * observations; there are no hand-authored unsafe coordinates.  Nearby
     * deaths are merged into a hotspot.  Repeated deaths on the same player
     * level quarantine that hotspot until the character has gained a level.
     * Historical risk then decays sharply as the character out-levels it.
     */
    class AdaptiveDangerMemory
    {
    public:
        struct Hotspot
        {
            std::uint32_t mapId = 0;
            Navigation::NavPoint center{};
            float risk = 0.0f;
            int totalDeaths = 0;
            std::uint32_t levelBucket = 0;
            int deathsAtLevel = 0;
            std::uint32_t blockedAtLevel = 0;
        };

    private:
        static constexpr int FileVersion = 1;
        static constexpr float MergeRadius = 70.0f;
        static constexpr float InfluenceRadius = 130.0f;
        static constexpr float QuarantineRadius = 80.0f;
        static constexpr float DeathRisk = 5.0f;
        static constexpr float MinimumPersistedRisk = 0.05f;

        std::uint64_t characterGuid_ = 0;
        std::uint32_t lastObservedLevel_ = 0;
        std::filesystem::path statePath_{};
        std::vector<Hotspot> hotspots_{};
        int totalDeaths_ = 0;
        int quarantineEvents_ = 0;
        bool initialized_ = false;

        static float Distance2D(
            const Navigation::NavPoint& a,
            const Navigation::NavPoint& b)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            return std::sqrt(dx * dx + dy * dy);
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

            // .../<project>/data/questdb/runtime/durotar.tsv
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

            const auto dir = root / "data" / "state" / "danger_memory";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: failed to create state directory: " +
                    ec.message());
                return false;
            }

            statePath_ = dir / ("player_" + GuidHex(characterGuid_) + ".tsv");
            return true;
        }

        bool Load()
        {
            hotspots_.clear();
            totalDeaths_ = 0;

            std::ifstream input(statePath_);
            if (!input)
                return true;

            std::string line;
            int version = 0;
            while (std::getline(input, line))
            {
                if (line.empty() || line[0] == '#')
                    continue;

                if (line.rfind("version\t", 0) == 0)
                {
                    version = std::atoi(line.substr(8).c_str());
                    continue;
                }

                if (line.rfind("hotspot\t", 0) != 0)
                    continue;

                Hotspot hotspot{};
                unsigned mapId = 0;
                unsigned levelBucket = 0;
                unsigned blockedAtLevel = 0;
                if (std::sscanf(
                        line.c_str(),
                        "hotspot\t%u\t%f\t%f\t%f\t%f\t%d\t%u\t%d\t%u",
                        &mapId,
                        &hotspot.center.x,
                        &hotspot.center.y,
                        &hotspot.center.z,
                        &hotspot.risk,
                        &hotspot.totalDeaths,
                        &levelBucket,
                        &hotspot.deathsAtLevel,
                        &blockedAtLevel) != 9)
                {
                    continue;
                }

                if (!std::isfinite(hotspot.center.x) ||
                    !std::isfinite(hotspot.center.y) ||
                    !std::isfinite(hotspot.center.z) ||
                    !std::isfinite(hotspot.risk))
                {
                    continue;
                }

                hotspot.mapId = static_cast<std::uint32_t>(mapId);
                hotspot.levelBucket = static_cast<std::uint32_t>(levelBucket);
                hotspot.blockedAtLevel = static_cast<std::uint32_t>(blockedAtLevel);
                hotspot.risk = std::max(0.0f, hotspot.risk);
                hotspot.totalDeaths = std::max(0, hotspot.totalDeaths);
                hotspot.deathsAtLevel = std::max(0, hotspot.deathsAtLevel);
                totalDeaths_ += hotspot.totalDeaths;
                hotspots_.push_back(hotspot);
            }

            if (version != 0 && version != FileVersion)
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: unsupported state version; starting clean.");
                hotspots_.clear();
                totalDeaths_ = 0;
            }

            return true;
        }

        bool Save() const
        {
            if (statePath_.empty())
                return false;

            const auto tempPath = statePath_.string() + ".tmp";
            {
                std::ofstream output(tempPath, std::ios::trunc);
                if (!output)
                    return false;

                output << "# wow-internal adaptive danger memory\n";
                output << "version\t" << FileVersion << "\n";
                output << "guid\t" << GuidHex(characterGuid_) << "\n";
                output << std::fixed << std::setprecision(3);

                for (const auto& hotspot : hotspots_)
                {
                    if (hotspot.risk < MinimumPersistedRisk &&
                        hotspot.blockedAtLevel == 0)
                    {
                        continue;
                    }

                    output
                        << "hotspot\t" << hotspot.mapId
                        << '\t' << hotspot.center.x
                        << '\t' << hotspot.center.y
                        << '\t' << hotspot.center.z
                        << '\t' << hotspot.risk
                        << '\t' << hotspot.totalDeaths
                        << '\t' << hotspot.levelBucket
                        << '\t' << hotspot.deathsAtLevel
                        << '\t' << hotspot.blockedAtLevel
                        << '\n';
                }
            }

            std::error_code ec;
            std::filesystem::remove(statePath_, ec);
            ec.clear();
            std::filesystem::rename(tempPath, statePath_, ec);
            if (ec)
            {
                std::filesystem::remove(tempPath, ec);
                return false;
            }
            return true;
        }

        Hotspot* FindMergeTarget(
            std::uint32_t mapId,
            const Navigation::NavPoint& point)
        {
            Hotspot* best = nullptr;
            float bestDistance = MergeRadius;
            for (auto& hotspot : hotspots_)
            {
                if (hotspot.mapId != mapId)
                    continue;

                const float distance = Distance2D(hotspot.center, point);
                if (distance <= bestDistance)
                {
                    bestDistance = distance;
                    best = &hotspot;
                }
            }
            return best;
        }


    public:
        bool Initialize(std::uint64_t characterGuid)
        {
            if (initialized_ && characterGuid_ == characterGuid)
                return true;

            characterGuid_ = characterGuid;
            hotspots_.clear();
            totalDeaths_ = 0;
            quarantineEvents_ = 0;
            initialized_ = false;

            if (!BuildStatePath())
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: persistence path unavailable; runtime memory will still operate in-session.");
                initialized_ = true;
                return true;
            }

            Load();
            initialized_ = true;
            Debug::Logger::Info(
                "DANGER MEMORY 14G.5: initialized hotspots=" +
                std::to_string(hotspots_.size()) +
                " totalDeaths=" + std::to_string(totalDeaths_) +
                " path=" + statePath_.string());
            return true;
        }

        void ObserveLevel(std::uint32_t currentLevel)
        {
            if (currentLevel == 0)
                return;

            if (lastObservedLevel_ != 0 && currentLevel > lastObservedLevel_)
            {
                int reopened = 0;
                for (const auto& hotspot : hotspots_)
                {
                    if (hotspot.blockedAtLevel != 0 &&
                        hotspot.blockedAtLevel < currentLevel)
                    {
                        ++reopened;
                    }
                }

                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: LEVEL UP " +
                    std::to_string(lastObservedLevel_) + " -> " +
                    std::to_string(currentLevel) +
                    "; previously quarantined hotspots eligible for cautious retry=" +
                    std::to_string(reopened));
            }

            lastObservedLevel_ = currentLevel;
        }

        void RecordDeath(
            std::uint32_t mapId,
            const Navigation::NavPoint& point,
            std::uint32_t playerLevel)
        {
            if (!initialized_ || mapId == 0 || playerLevel == 0 ||
                !std::isfinite(point.x) || !std::isfinite(point.y) ||
                !std::isfinite(point.z))
            {
                return;
            }

            Hotspot* hotspot = FindMergeTarget(mapId, point);
            if (hotspot == nullptr)
            {
                Hotspot created{};
                created.mapId = mapId;
                created.center = point;
                created.levelBucket = playerLevel;
                hotspots_.push_back(created);
                hotspot = &hotspots_.back();
            }

            const int previousDeaths = hotspot->totalDeaths;
            const float weight = static_cast<float>(std::max(1, previousDeaths));
            if (previousDeaths > 0)
            {
                hotspot->center.x =
                    (hotspot->center.x * weight + point.x) / (weight + 1.0f);
                hotspot->center.y =
                    (hotspot->center.y * weight + point.y) / (weight + 1.0f);
                hotspot->center.z =
                    (hotspot->center.z * weight + point.z) / (weight + 1.0f);
            }
            else
            {
                hotspot->center = point;
            }

            ++hotspot->totalDeaths;
            ++totalDeaths_;
            hotspot->risk += DeathRisk;

            if (hotspot->levelBucket != playerLevel)
            {
                hotspot->levelBucket = playerLevel;
                hotspot->deathsAtLevel = 1;
            }
            else
            {
                ++hotspot->deathsAtLevel;
            }

            const bool wasQuarantined =
                hotspot->blockedAtLevel != 0 &&
                playerLevel <= hotspot->blockedAtLevel;

            if (AdaptiveDangerPolicy::ShouldQuarantine(hotspot->deathsAtLevel))
            {
                hotspot->blockedAtLevel = playerLevel;
                if (!wasQuarantined)
                    ++quarantineEvents_;
            }

            Debug::Logger::Info("================================");
            Debug::Logger::Info(
                "DANGER MEMORY 14G.5: DEATH RECORDED map=" +
                std::to_string(mapId) +
                " level=" + std::to_string(playerLevel) +
                " hotspotDeaths=" + std::to_string(hotspot->totalDeaths) +
                " deathsThisLevel=" + std::to_string(hotspot->deathsAtLevel) +
                " risk=" + std::to_string(hotspot->risk));
            if (hotspot->blockedAtLevel == playerLevel)
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: HOTSPOT QUARANTINED until player level exceeds " +
                    std::to_string(playerLevel));
            }
            Debug::Logger::Info("================================");

            if (!statePath_.empty() && !Save())
            {
                Debug::Logger::Info(
                    "DANGER MEMORY 14G.5: warning - persistent save failed; in-session memory remains active.");
            }
        }

        bool IsQuarantined(
            std::uint32_t mapId,
            const Navigation::NavPoint& point,
            std::uint32_t playerLevel) const
        {
            for (const auto& hotspot : hotspots_)
            {
                if (hotspot.mapId != mapId || hotspot.blockedAtLevel == 0)
                    continue;

                if (!AdaptiveDangerPolicy::RemainsQuarantined(
                        hotspot.blockedAtLevel, playerLevel))
                    continue;

                if (Distance2D(hotspot.center, point) <= QuarantineRadius)
                    return true;
            }
            return false;
        }

        float RiskAt(
            std::uint32_t mapId,
            const Navigation::NavPoint& point,
            std::uint32_t playerLevel) const
        {
            float risk = 0.0f;
            for (const auto& hotspot : hotspots_)
            {
                if (hotspot.mapId != mapId)
                    continue;

                const float distance = Distance2D(hotspot.center, point);
                if (distance >= InfluenceRadius)
                    continue;

                const float spatial = 1.0f - distance / InfluenceRadius;
                risk += hotspot.risk *
                    AdaptiveDangerPolicy::LevelRiskFactor(
                        hotspot.levelBucket, playerLevel) *
                    spatial;

                if (AdaptiveDangerPolicy::RemainsQuarantined(
                        hotspot.blockedAtLevel, playerLevel) &&
                    distance <= QuarantineRadius)
                {
                    risk += 1000.0f;
                }
            }
            return risk;
        }

        bool IsElevatedRisk(
            std::uint32_t mapId,
            const Navigation::NavPoint& point,
            std::uint32_t playerLevel) const
        {
            return RiskAt(mapId, point, playerLevel) >= 3.5f;
        }

        std::size_t HotspotCount() const { return hotspots_.size(); }
        int TotalDeaths() const { return totalDeaths_; }
        int QuarantineEvents() const { return quarantineEvents_; }

        int ActiveQuarantines(std::uint32_t playerLevel) const
        {
            int count = 0;
            for (const auto& hotspot : hotspots_)
            {
                if (AdaptiveDangerPolicy::RemainsQuarantined(
                        hotspot.blockedAtLevel, playerLevel))
                {
                    ++count;
                }
            }
            return count;
        }
    };
}
