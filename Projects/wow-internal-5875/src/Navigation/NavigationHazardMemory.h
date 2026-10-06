#pragma once

#include "DetourNavigationProvider.h"

#include "../Debug/Logger.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace Navigation
{
    /*
     * Phase 14I.0
     *
     * Persistent navigation hazard memory learned exclusively from runtime
     * stalls.  The memory is map-local and spatial: it stores quantized cells
     * rather than exact floating-point positions or unstable dtPolyRef values.
     *
     * A single stall is only soft evidence. Repeated independent stalls raise
     * the score until the cell becomes a hard avoidance region. Successful
     * traversal reduces the score again, and old failures decay on load.
     *
     * The file is bounded, corruption tolerant and replaced atomically. If
     * persistence is unavailable the in-session memory remains active.
     */
    class NavigationHazardMemory
    {
    public:
        struct Cell
        {
            std::uint32_t mapId = 0;
            std::int32_t cellX = 0;
            std::int32_t cellY = 0;
            std::int32_t cellZ = 0;
            float score = 0.0f;
            int failures = 0;
            int successes = 0;
            std::int64_t lastFailureUnix = 0;
            std::int64_t lastSuccessUnix = 0;

            // Runtime-only de-duplication. Not persisted.
            std::uint64_t lastFailureTick = 0;
            std::uint64_t lastSuccessTick = 0;
        };

    private:
        static constexpr int FileVersion = 1;
        static constexpr float CellSizeXY = 6.0f;
        static constexpr float CellSizeZ = 4.0f;
        static constexpr float HardScore = 4.0f;
        static constexpr float MaximumScore = 12.0f;
        static constexpr float SuccessCredit = 0.50f;
        static constexpr float MinimumPersistedScore = 0.05f;
        static constexpr float HardInfluenceRadius = 7.0f;
        static constexpr float SoftInfluenceRadius = 18.0f;
        static constexpr float MaximumVerticalInfluence = 7.0f;
        static constexpr std::uint64_t FailureDedupTicks = 12;
        static constexpr std::uint64_t SuccessDedupTicks = 40;
        static constexpr std::size_t MaximumCells = 256;
        static constexpr std::int64_t DecayIntervalSeconds = 24 * 60 * 60;
        static constexpr float DailyDecay = 0.50f;

        inline static int moduleAnchor_ = 0;

        mutable std::mutex mutex_{};
        std::uint32_t activeMapId_ = 0;
        std::filesystem::path statePath_{};
        std::vector<Cell> cells_{};
        bool initialized_ = false;
        bool persistenceAvailable_ = false;

        static bool Finite(const NavPoint& point)
        {
            return std::isfinite(point.x) &&
                std::isfinite(point.y) &&
                std::isfinite(point.z);
        }

        static float Distance2D(const NavPoint& a, const NavPoint& b)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        static std::int32_t Quantize(float value, float size)
        {
            return static_cast<std::int32_t>(std::floor(value / size));
        }

        static NavPoint Center(const Cell& cell)
        {
            return NavPoint{
                (static_cast<float>(cell.cellX) + 0.5f) * CellSizeXY,
                (static_cast<float>(cell.cellY) + 0.5f) * CellSizeXY,
                (static_cast<float>(cell.cellZ) + 0.5f) * CellSizeZ
            };
        }

        static std::int64_t UnixNow()
        {
            return static_cast<std::int64_t>(std::time(nullptr));
        }

        static std::filesystem::path ResolveProjectRoot()
        {
            HMODULE module = nullptr;
            const BOOL ok = GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&moduleAnchor_),
                &module);
            if (!ok || module == nullptr)
                return {};

            wchar_t buffer[MAX_PATH] = {};
            const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
                return {};

            std::filesystem::path modulePath(buffer);
            auto moduleDir = modulePath.parent_path();
            if (moduleDir.empty())
                return {};

            // Development layout: <project>/build/wow_internal.dll.
            // Portable layout: keep state beside the module if there is no
            // recognizable build directory.
            if (moduleDir.filename() == L"build")
                return moduleDir.parent_path();

            return moduleDir;
        }

        bool BuildStatePathLocked(std::uint32_t mapId, bool readOnly = false)
        {
            const auto root = ResolveProjectRoot();
            if (root.empty())
                return false;

            const auto dir = root / "data" / "state" / "navigation_hazards";
            std::error_code ec;
            if (!readOnly)
                std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: failed to create state directory: " +
                    ec.message());
                return false;
            }

            std::ostringstream name;
            name << "map_" << std::setfill('0') << std::setw(3) << mapId
                 << ".tsv";
            statePath_ = dir / name.str();
            return true;
        }

        Cell* FindExactLocked(
            std::uint32_t mapId,
            std::int32_t x,
            std::int32_t y,
            std::int32_t z)
        {
            for (auto& cell : cells_)
            {
                if (cell.mapId == mapId &&
                    cell.cellX == x &&
                    cell.cellY == y &&
                    cell.cellZ == z)
                {
                    return &cell;
                }
            }
            return nullptr;
        }

        Cell* FindNearestRiskCellLocked(
            std::uint32_t mapId,
            const NavPoint& point,
            float maximumDistance)
        {
            Cell* best = nullptr;
            float bestDistance = maximumDistance;
            for (auto& cell : cells_)
            {
                if (cell.mapId != mapId || cell.score <= 0.0f)
                    continue;

                const NavPoint center = Center(cell);
                if (std::fabs(center.z - point.z) > MaximumVerticalInfluence)
                    continue;

                const float distance = Distance2D(center, point);
                if (distance <= bestDistance)
                {
                    bestDistance = distance;
                    best = &cell;
                }
            }
            return best;
        }

        static bool IsHard(const Cell& cell)
        {
            return cell.score >= HardScore;
        }

        void ApplyDecayLocked()
        {
            const std::int64_t now = UnixNow();
            for (auto& cell : cells_)
            {
                if (cell.lastFailureUnix <= 0 || now <= cell.lastFailureUnix)
                    continue;

                const std::int64_t elapsed = now - cell.lastFailureUnix;
                const std::int64_t days = elapsed / DecayIntervalSeconds;
                if (days <= 0)
                    continue;

                cell.score = std::max(
                    0.0f,
                    cell.score - static_cast<float>(days) * DailyDecay);
                // Advance the decay checkpoint so repeated DLL unload/reload
                // cycles do not apply the same wall-clock decay more than once.
                cell.lastFailureUnix += days * DecayIntervalSeconds;
            }

            cells_.erase(
                std::remove_if(
                    cells_.begin(),
                    cells_.end(),
                    [](const Cell& cell)
                    {
                        return cell.score < MinimumPersistedScore &&
                            cell.failures <= cell.successes;
                    }),
                cells_.end());
        }

        void BoundSizeLocked()
        {
            if (cells_.size() <= MaximumCells)
                return;

            std::stable_sort(
                cells_.begin(),
                cells_.end(),
                [](const Cell& a, const Cell& b)
                {
                    if (IsHard(a) != IsHard(b))
                        return IsHard(a) > IsHard(b);
                    if (a.score != b.score)
                        return a.score > b.score;
                    return a.lastFailureUnix > b.lastFailureUnix;
                });
            cells_.resize(MaximumCells);
        }

        bool LoadLocked()
        {
            cells_.clear();
            if (statePath_.empty())
                return false;

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

                if (line.rfind("cell\t", 0) != 0)
                    continue;

                Cell cell{};
                unsigned mapId = 0;
                long long lastFailure = 0;
                long long lastSuccess = 0;
                if (std::sscanf(
                        line.c_str(),
                        "cell\t%u\t%d\t%d\t%d\t%f\t%d\t%d\t%lld\t%lld",
                        &mapId,
                        &cell.cellX,
                        &cell.cellY,
                        &cell.cellZ,
                        &cell.score,
                        &cell.failures,
                        &cell.successes,
                        &lastFailure,
                        &lastSuccess) != 9)
                {
                    continue;
                }

                if (mapId != activeMapId_ ||
                    !std::isfinite(cell.score) ||
                    cell.score < 0.0f)
                {
                    continue;
                }

                cell.mapId = static_cast<std::uint32_t>(mapId);
                cell.score = std::min(MaximumScore, cell.score);
                cell.failures = std::max(0, cell.failures);
                cell.successes = std::max(0, cell.successes);
                cell.lastFailureUnix = static_cast<std::int64_t>(lastFailure);
                cell.lastSuccessUnix = static_cast<std::int64_t>(lastSuccess);
                cells_.push_back(cell);
            }

            if (version != 0 && version != FileVersion)
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: unsupported state version; starting clean.");
                cells_.clear();
            }

            ApplyDecayLocked();
            BoundSizeLocked();
            return true;
        }

        bool SaveLocked() const
        {
            if (!persistenceAvailable_ || statePath_.empty())
                return false;

            std::filesystem::path tempPath = statePath_;
            tempPath += L".tmp";

            {
                std::ofstream output(tempPath, std::ios::trunc);
                if (!output)
                    return false;

                output << "# wow-internal persistent navigation hazard memory\n";
                output << "version\t" << FileVersion << "\n";
                output << "map\t" << activeMapId_ << "\n";
                output << std::fixed << std::setprecision(3);

                for (const auto& cell : cells_)
                {
                    if (cell.score < MinimumPersistedScore)
                        continue;

                    output
                        << "cell\t" << cell.mapId
                        << '\t' << cell.cellX
                        << '\t' << cell.cellY
                        << '\t' << cell.cellZ
                        << '\t' << cell.score
                        << '\t' << cell.failures
                        << '\t' << cell.successes
                        << '\t' << cell.lastFailureUnix
                        << '\t' << cell.lastSuccessUnix
                        << '\n';
                }

                output.flush();
                if (!output)
                    return false;
            }

            const std::wstring source = tempPath.wstring();
            const std::wstring destination = statePath_.wstring();
            if (!MoveFileExW(
                    source.c_str(),
                    destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::error_code ec;
                std::filesystem::remove(tempPath, ec);
                return false;
            }

            return true;
        }

        int HardCountLocked() const
        {
            int count = 0;
            for (const auto& cell : cells_)
            {
                if (cell.mapId == activeMapId_ && IsHard(cell))
                    ++count;
            }
            return count;
        }

    public:
        static NavigationHazardMemory& Instance()
        {
            static NavigationHazardMemory instance;
            return instance;
        }

        bool InitializeForMap(std::uint32_t mapId)
        {
            if (mapId == 0)
                return false;

            std::lock_guard<std::mutex> lock(mutex_);
            if (initialized_ && activeMapId_ == mapId)
                return true;

            activeMapId_ = mapId;
            statePath_.clear();
            cells_.clear();
            initialized_ = true;
            persistenceAvailable_ = BuildStatePathLocked(mapId);

            if (persistenceAvailable_)
            {
                LoadLocked();
                if (!SaveLocked())
                {
                    Debug::Logger::Info(
                        "NAV HAZARD 14I.0: warning - initial state flush failed; runtime memory remains active.");
                }
            }
            else
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: persistence path unavailable; in-session memory remains active.");
            }

            Debug::Logger::Info(
                "NAV HAZARD 14I.0: initialized map=" +
                std::to_string(mapId) +
                " cells=" + std::to_string(cells_.size()) +
                " hard=" + std::to_string(HardCountLocked()) +
                (statePath_.empty() ? std::string() :
                    " path=" + statePath_.string()));
            return true;
        }

        void RecordFailure(
            std::uint32_t mapId,
            const NavPoint& point,
            std::uint64_t tick,
            int severity,
            const char* reason)
        {
            if (mapId == 0 || !Finite(point) || severity <= 0)
                return;

            InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);

            const std::int32_t x = Quantize(point.x, CellSizeXY);
            const std::int32_t y = Quantize(point.y, CellSizeXY);
            const std::int32_t z = Quantize(point.z, CellSizeZ);
            Cell* cell = FindExactLocked(mapId, x, y, z);
            if (cell == nullptr)
            {
                cells_.push_back(Cell{});
                cell = &cells_.back();
                cell->mapId = mapId;
                cell->cellX = x;
                cell->cellY = y;
                cell->cellZ = z;
            }

            if (cell->lastFailureTick != 0 &&
                tick >= cell->lastFailureTick &&
                tick - cell->lastFailureTick < FailureDedupTicks)
            {
                return;
            }

            const bool wasHard = IsHard(*cell);
            cell->lastFailureTick = tick;
            cell->lastFailureUnix = UnixNow();
            ++cell->failures;
            cell->score = std::min(
                MaximumScore,
                cell->score + static_cast<float>(std::min(severity, 3)));
            const bool isHard = IsHard(*cell);

            const NavPoint center = Center(*cell);
            Debug::Logger::Info(
                "NAV HAZARD 14I.0: FAILURE learned cell=(" +
                std::to_string(center.x) + "," +
                std::to_string(center.y) + "," +
                std::to_string(center.z) + ") score=" +
                std::to_string(cell->score) +
                " failures=" + std::to_string(cell->failures) +
                " reason=" + (reason == nullptr ? std::string("unknown") : reason));

            if (!wasHard && isHard)
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: HARD CELL ACTIVATED; future normal routes will avoid this learned area.");
            }

            BoundSizeLocked();
            if (persistenceAvailable_ && !SaveLocked())
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: warning - persistent save failed; in-session hazard memory remains active.");
            }
        }

        void ObserveTraversalSuccess(
            std::uint32_t mapId,
            const NavPoint& point,
            std::uint64_t tick)
        {
            if (mapId == 0 || !Finite(point))
                return;

            InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);
            Cell* cell = FindNearestRiskCellLocked(
                mapId,
                point,
                HardInfluenceRadius);
            if (cell == nullptr)
                return;

            if (cell->lastSuccessTick != 0 &&
                tick >= cell->lastSuccessTick &&
                tick - cell->lastSuccessTick < SuccessDedupTicks)
            {
                return;
            }

            const bool wasHard = IsHard(*cell);
            cell->lastSuccessTick = tick;
            cell->lastSuccessUnix = UnixNow();
            ++cell->successes;
            cell->score = std::max(0.0f, cell->score - SuccessCredit);
            const bool isHard = IsHard(*cell);

            if (wasHard && !isHard)
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: HARD CELL REOPENED after successful traversal evidence.");
            }

            if (persistenceAvailable_ && !SaveLocked())
            {
                Debug::Logger::Info(
                    "NAV HAZARD 14I.0: warning - success decay save failed; runtime state remains active.");
            }
        }

        bool IsHardBlocked(
            std::uint32_t mapId,
            const NavPoint& point) const
        {
            if (mapId == 0 || !Finite(point))
                return false;

            const_cast<NavigationHazardMemory*>(this)->InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& cell : cells_)
            {
                if (cell.mapId != mapId || !IsHard(cell))
                    continue;

                const NavPoint center = Center(cell);
                if (std::fabs(center.z - point.z) > MaximumVerticalInfluence)
                    continue;

                if (Distance2D(center, point) <= HardInfluenceRadius)
                    return true;
            }
            return false;
        }

        float RiskAt(
            std::uint32_t mapId,
            const NavPoint& point) const
        {
            if (mapId == 0 || !Finite(point))
                return 0.0f;

            const_cast<NavigationHazardMemory*>(this)->InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);
            float risk = 0.0f;
            for (const auto& cell : cells_)
            {
                if (cell.mapId != mapId || cell.score <= 0.0f)
                    continue;

                const NavPoint center = Center(cell);
                if (std::fabs(center.z - point.z) > MaximumVerticalInfluence)
                    continue;

                const float distance = Distance2D(center, point);
                if (distance >= SoftInfluenceRadius)
                    continue;

                const float spatial = 1.0f - distance / SoftInfluenceRadius;
                risk += cell.score * spatial;
                if (IsHard(cell) && distance <= HardInfluenceRadius)
                    risk += 1000.0f;
            }
            return risk;
        }

        std::vector<NavPoint> HardCellCenters(std::uint32_t mapId) const
        {
            const_cast<NavigationHazardMemory*>(this)->InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<NavPoint> result;
            for (const auto& cell : cells_)
            {
                if (cell.mapId == mapId && IsHard(cell))
                    result.push_back(Center(cell));
            }
            return result;
        }

        // A route-cost probe must see the same hazards as execution without
        // initializing/flushing the runtime singleton or learning from a probe.
        std::vector<NavPoint> HardCellCentersForProbe(std::uint32_t mapId) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (initialized_ && activeMapId_ == mapId)
            {
                std::vector<NavPoint> result;
                for (const auto& cell : cells_)
                    if (cell.mapId == mapId && IsHard(cell))
                        result.push_back(Center(cell));
                return result;
            }
            NavigationHazardMemory snapshot;
            snapshot.activeMapId_ = mapId;
            if (mapId != 0 && snapshot.BuildStatePathLocked(mapId, true))
                snapshot.LoadLocked();
            std::vector<NavPoint> result;
            for (const auto& cell : snapshot.cells_)
                if (cell.mapId == mapId && IsHard(cell))
                    result.push_back(Center(cell));
            return result;
        }

        int HardCellCount(std::uint32_t mapId) const
        {
            const_cast<NavigationHazardMemory*>(this)->InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);
            int count = 0;
            for (const auto& cell : cells_)
            {
                if (cell.mapId == mapId && IsHard(cell))
                    ++count;
            }
            return count;
        }

        std::size_t CellCount(std::uint32_t mapId) const
        {
            const_cast<NavigationHazardMemory*>(this)->InitializeForMap(mapId);
            std::lock_guard<std::mutex> lock(mutex_);
            std::size_t count = 0;
            for (const auto& cell : cells_)
            {
                if (cell.mapId == mapId)
                    ++count;
            }
            return count;
        }
    };
}
