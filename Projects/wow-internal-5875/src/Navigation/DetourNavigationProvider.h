#pragma once

#include "../Debug/Logger.h"

#include "DetourAlloc.h"
#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"
#include "IncrementalTileInitializationPolicy.h"
#include "MapNavMeshSessionCache.h"
#include "LivingWaterTraversalPolicy.h"
#include "TerrainTransitionPolicy.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Navigation
{
    struct NavPoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct NavSurfaceRayTrace
    {
        std::uint64_t startPoly = 0;
        std::uint64_t lastVisitedPoly = 0;
        NavPoint hitPoint{};
        int visitedCount = 0;
        bool complete = false;
    };

    struct NavPathResult
    {
        bool success = false;
        bool partial = false;
        bool corridorConnected = false;

        int loadedTiles = 0;
        int polygonCount = 0;

        std::uint64_t startPoly = 0;
        std::uint64_t endPoly = 0;
        std::uint64_t lastPoly = 0;
        std::uint64_t meshGeneration = 0;

        std::uint32_t findPathStatus = 0;
        std::uint32_t findStraightPathStatus = 0;
        bool findPathOutOfNodes = false;
        bool findPathBufferTooSmall = false;
        bool findPathPartialResult = false;
        bool findStraightPathBufferTooSmall = false;

        int queryNodePoolSize = 0;
        int maximumPolygons = 0;
        int maximumStraightPoints = 0;

        // Preserve the exact Detour terrain filter used for this route.
        // Ground remains ahead of water; 14O.1 excludes steep first and
        // identifies a steep-enabled fallback for pre-movement validation.
        unsigned short includeFlags = 0;
        unsigned short excludeFlags = 0;
        bool waterFallbackAttempted = false;
        bool waterAwareRoute = false;
        int waterPolygonCount = 0;
        bool steepFallback = false;
        int steepPolygonCount = 0;

        NavPoint projectedStart{};
        NavPoint projectedDestination{};
        NavPoint corridorEnd{};

        // Phase 13D.1 keeps the actual Detour polygon corridor instead of
        // throwing it away after straight-path generation. pointPolys uses
        // Detour's documented semantics: the polygon entered at each
        // straight-path point. This lets the follower identify the exact
        // local corridor transition associated with a physical stall.
        std::vector<std::uint64_t> corridorPolys;
        std::vector<std::uint64_t> pointPolys;

        bool avoidanceActive = false;
        int avoidanceRequestedCount = 0;
        int avoidanceAppliedCount = 0;
        std::vector<std::uint64_t> avoidedPolygons;

        std::vector<NavPoint> points;
        std::string error;
    };

    struct NavTerrainValidation
    {
        bool valid = true;
        std::uint64_t fromPoly = 0;
        std::uint64_t toPoly = 0;
        unsigned short fromFlags = 0;
        unsigned short toFlags = 0;
        NavPoint portalA{};
        NavPoint portalB{};
        bool portalKnown = false;
        bool tileSeamKnown = false;
        bool tileSeam = false;
        TerrainTransitionPolicy::Geometry geometry{};
        std::size_t pointIndex = 0;
        const char* reason = "none";
    };

    class DetourNavigationProvider
    {
    private:
        static constexpr std::uint32_t MmapMagic =
            0x4D4D4150;

        static constexpr std::uint32_t MmapVersion =
            6;

        static constexpr unsigned short NavGround =
            TerrainTransitionPolicy::GroundFlag;

        static constexpr unsigned short NavWater =
            TerrainTransitionPolicy::WaterFlag;

        static constexpr unsigned short PlayerNavFlags =
            static_cast<unsigned short>(NavGround | NavWater);

        WaterTraversalMode waterTraversalMode_ =
            WaterTraversalMode::AvoidUntilQualified;

        unsigned short QueryIncludeFlags() const
        { return LivingWaterTraversalPolicy::Include(waterTraversalMode_); }

        unsigned short QueryExcludeFlags(unsigned short existing = 0) const
        { return LivingWaterTraversalPolicy::Exclude(waterTraversalMode_, existing); }

        // Phase 13D.7.1 long-route capacity:
        // live Sen'jin -> Echo Isles routing exhausted the previous 32768-node
        // pool while returning a connected, strongly improving prefix. This
        // Detour fork stores node indices in an unsigned short and accepts up
        // to DT_NULL_IDX (65535) nodes. Use the maximum supported pool so a
        // long route gets the best chance of completing in one query; the
        // follower still supports bounded out-of-nodes prefix staging when
        // even the maximum-capacity query cannot reach the destination.
        static constexpr int QueryNodePoolSize =
            65535;

        static constexpr int MaximumPolygons =
            512;

        static constexpr int MaximumStraightPoints =
            512;

        // WoW continent terrain uses a 64x64 ADT grid. Loading all 704
        // Kalimdor mmtile files through Wine dominated planner startup
        // (~39.7 s in the Phase 11B.7 trace), while findPath itself was
        // effectively 0 ms. Route-scoped loading keeps only the ADTs around
        // the start->destination rectangle and falls back in bounded stages.
        static constexpr float WorldGridSize =
            533.333333f;

        static constexpr float WorldGridOrigin =
            32.0f * WorldGridSize;

        struct MmapTileHeader
        {
            std::uint32_t mmapMagic;
            std::uint32_t dtVersion;
            std::uint32_t mmapVersion;
            std::uint32_t size;
            std::uint32_t usesLiquids;
        };

        static_assert(
            sizeof(MmapTileHeader) == 20,
            "Unexpected MmapTileHeader layout."
        );

        struct CachedTopology
        {
            dtNavMesh* mesh = nullptr;
            ~CachedTopology() { if (mesh) dtFreeNavMesh(mesh); }
        };
        using SessionCache = MapNavMeshSessionCache<CachedTopology>;
        SessionCache::Handle session_;
        static SessionCache& Cache()
        {
            static SessionCache cache;
            return cache;
        }
        bool CurrentTopology() const { return Cache().IsCurrent(session_); }
        static bool FileIdentity(const std::string& path, NavMeshTileIdentity& identity)
        {
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) ||
                (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
            identity.stamp = (static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                data.ftLastWriteTime.dwLowDateTime;
            identity.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            return true;
        }

        dtNavMesh* mesh_ =
            nullptr;

        dtNavMeshQuery* query_ =
            nullptr;

        std::uint32_t mapId_ =
            0;

        int loadedTiles_ =
            0;

        std::string directory_;

        using ProfileClock = std::chrono::steady_clock;

        IncrementalTileInitializationPolicy incrementalCursor_{};
        std::vector<std::string> incrementalTiles_{};
        std::string incrementalMode_{};
        ProfileClock::time_point incrementalStarted_{};
        double incrementalWorkMs_ = 0.0;
        bool incrementalActive_ = false;
        std::size_t requestedTiles_ = 0;
        std::size_t reusedTiles_ = 0;
        std::size_t incrementalFinalTotal_ = 0;
        std::size_t incrementalFinalProcessed_ = 0;
        double incrementalFinalElapsedMs_ = 0.0;

        struct InitProfile
        {
            int tilesDiscovered = 0;
            int tilesOpened = 0;
            int tilesLoaded = 0;
            int tilesFailed = 0;
            int cacheHits = 0;
            int addTileCalls = 0;
            double enumerationMs = 0.0;
            double tileReadMs = 0.0;
            double addTileMs = 0.0;
            double queryInitMs = 0.0;
            double mapHeaderMs = 0.0;
            double cacheLookupMs = 0.0;
            double slowestTileMs = 0.0;
            std::string slowestTileName;
        } initProfile_{};

        static double ProfileMs(ProfileClock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(
                ProfileClock::now() - start).count();
        }

        void LogInitProfile(const char* mode, ProfileClock::time_point start,
                            bool success, double workMs = -1.0) const
        {
            const double totalMs = ProfileMs(start);
            if (workMs < 0.0)
                workMs = totalMs;
            const double accounted = initProfile_.enumerationMs +
                initProfile_.tileReadMs + initProfile_.addTileMs +
                initProfile_.queryInitMs + initProfile_.mapHeaderMs +
                initProfile_.cacheLookupMs;
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3)
                << "NAV 14N.3 INIT PROFILE: mode=" << mode
                << " threadId=" << GetCurrentThreadId()
                << " tilesDiscovered=" << initProfile_.tilesDiscovered
                << " tilesOpened=" << initProfile_.tilesOpened
                << " tilesLoaded=" << initProfile_.tilesLoaded
                << " tilesFailed=" << initProfile_.tilesFailed
                << " diskLoads=" << initProfile_.tilesOpened
                << " cacheHits=" << initProfile_.cacheHits
                << " addTileCalls=" << initProfile_.addTileCalls
                << " reusedTiles=" << initProfile_.cacheHits
                << " enumerationMs=" << initProfile_.enumerationMs
                << " tileReadMs=" << initProfile_.tileReadMs
                << " addTileMs=" << initProfile_.addTileMs
                << " queryInitMs=" << initProfile_.queryInitMs
                << " mapHeaderMs=" << initProfile_.mapHeaderMs
                << " cacheLookupMs=" << initProfile_.cacheLookupMs
                << " otherMs=" << std::max(0.0, workMs - accounted)
                << " workMs=" << workMs
                << " totalMs=" << totalMs
                << " slowestTileMs=" << initProfile_.slowestTileMs
                << " slowestTile="
                << (initProfile_.slowestTileName.empty()
                    ? "none" : initProfile_.slowestTileName)
                << " success=" << (success ? "yes" : "no");
            Debug::Logger::Info(stream.str());
        }

        struct InitProfileScope
        {
            const DetourNavigationProvider& provider;
            const char* mode;
            ProfileClock::time_point start = ProfileClock::now();
            bool success = false;

            ~InitProfileScope()
            {
                provider.LogInitProfile(mode, start, success);
            }
        };

        struct TileProfileScope
        {
            DetourNavigationProvider& provider;
            const std::string& path;
            ProfileClock::time_point start = ProfileClock::now();
            bool success = false;

            ~TileProfileScope()
            {
                const double tileMs = ProfileMs(start);
                if (tileMs > provider.initProfile_.slowestTileMs)
                {
                    provider.initProfile_.slowestTileMs = tileMs;
                    const auto separator = path.find_last_of("\\/");
                    provider.initProfile_.slowestTileName =
                        path.substr(separator == std::string::npos
                            ? 0 : separator + 1);
                }
                if (!success)
                    ++provider.initProfile_.tilesFailed;
            }
        };

        static std::string MapName(
            std::uint32_t mapId,
            const char* extension)
        {
            std::ostringstream stream;

            stream
                << std::setfill('0')
                << std::setw(3)
                << mapId
                << extension;

            return stream.str();
        }

        static std::string JoinPath(
            const std::string& directory,
            const std::string& name)
        {
            if (directory.empty())
            {
                return name;
            }

            const char last =
                directory.back();

            if (
                last == '\\' ||
                last == '/')
            {
                return
                    directory +
                    name;
            }

            return
                directory +
                "\\" +
                name;
        }

        static bool HasSuffix(
            const std::string& value,
            const std::string& suffix)
        {
            return
                value.size() >=
                    suffix.size() &&
                value.compare(
                    value.size() -
                        suffix.size(),
                    suffix.size(),
                    suffix
                ) == 0;
        }

        static bool IsTileName(
            const std::string& name,
            std::uint32_t mapId)
        {
            const std::string prefix =
                MapName(
                    mapId,
                    ""
                );

            static const std::string suffix =
                ".mmtile";

            /*
             * VMaNGOS map tiles are:
             *
             *   MMMXXYY.mmtile
             *
             * where MMM is the zero-padded map ID.
             */
            return
                name.size() == 14 &&
                name.compare(
                    0,
                    prefix.size(),
                    prefix
                ) == 0 &&
                HasSuffix(
                    name,
                    suffix
                );
        }

        static int WorldToTile(float coordinate)
        {
            const float value =
                (WorldGridOrigin - coordinate) /
                WorldGridSize;

            return static_cast<int>(
                std::floor(value)
            );
        }

        static std::string TileName(
            std::uint32_t mapId,
            int tileY,
            int tileX)
        {
            std::ostringstream stream;

            stream
                << std::setfill('0')
                << std::setw(3)
                << mapId
                << std::setw(2)
                << tileY
                << std::setw(2)
                << tileX
                << ".mmtile";

            return stream.str();
        }

        bool LoadRouteTiles(
            const NavPoint& start,
            const NavPoint& destination,
            int marginTiles,
            std::string& error)
        {
            /*
             * IMPORTANT: VMaNGOS/Recast horizontal axes are swapped relative
             * to WoW world coordinates:
             *
             *   Recast X = WoW Y
             *   Recast Z = WoW X
             *
             * MapBuilder therefore treats tileX as the WoW-Y grid coordinate
             * and tileY as the WoW-X grid coordinate. The .mmtile filename is
             * written as MMM + tileY + tileX.
             *
             * Example near Valley of Trials:
             *   WoW (-629, -4228) -> tileX=39, tileY=33
             *   filename prefix   -> 0013339
             */
            const int startX = WorldToTile(start.y);
            const int startY = WorldToTile(start.x);
            const int endX = WorldToTile(destination.y);
            const int endY = WorldToTile(destination.x);

            const int minX = std::max(
                0,
                std::min(startX, endX) - marginTiles
            );

            const int maxX = std::min(
                63,
                std::max(startX, endX) + marginTiles
            );

            const int minY = std::max(
                0,
                std::min(startY, endY) - marginTiles
            );

            const int maxY = std::min(
                63,
                std::max(startY, endY) + marginTiles
            );

            Debug::Logger::Info(
                "NAVMESH 11B.8.2: ADT axis mapping start tileX=" +
                std::to_string(startX) +
                " tileY=" +
                std::to_string(startY) +
                " file=" +
                TileName(mapId_, startY, startX) +
                " destination tileX=" +
                std::to_string(endX) +
                " tileY=" +
                std::to_string(endY) +
                " file=" +
                TileName(mapId_, endY, endX)
            );

            Debug::Logger::Info(
                "NAVMESH 11B.8.2: route tile window tileX=" +
                std::to_string(minX) + ".." +
                std::to_string(maxX) +
                " tileY=" +
                std::to_string(minY) + ".." +
                std::to_string(maxY) +
                " margin=" +
                std::to_string(marginTiles)
            );

            int existingFiles = 0;

            for (int tileY = minY; tileY <= maxY; ++tileY)
            {
                for (int tileX = minX; tileX <= maxX; ++tileX)
                {
                    const std::string path =
                        JoinPath(
                            directory_,
                            TileName(
                                mapId_,
                                tileY,
                                tileX
                            )
                        );

                    const auto lookupStarted = ProfileClock::now();
                    const DWORD attributes =
                        GetFileAttributesA(
                            path.c_str()
                        );
                    initProfile_.enumerationMs += ProfileMs(lookupStarted);

                    if (
                        attributes == INVALID_FILE_ATTRIBUTES ||
                        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    {
                        continue;
                    }

                    ++existingFiles;
                    ++initProfile_.tilesDiscovered;

                    if (!LoadTile(path, error))
                    {
                        return false;
                    }
                }
            }

            if (loadedTiles_ == 0)
            {
                error =
                    "No route-scoped mmtile files were found in the computed ADT window.";
                return false;
            }

            Debug::Logger::Info(
                "NAVMESH 11B.8.2: route-scoped tiles loaded=" +
                std::to_string(loadedTiles_) +
                " existingFiles=" +
                std::to_string(existingFiles)
            );

            return true;
        }

        static std::string NormalizeWinePath(
            std::string path)
        {
            if (path.empty())
            {
                return path;
            }

            /*
             * A Linux path inherited through Wine can be
             * addressed through Wine's Z: drive.
             */
            if (path.front() == '/')
            {
                path =
                    "Z:" +
                    path;
            }

            for (char& character : path)
            {
                if (character == '/')
                {
                    character = '\\';
                }
            }

            while (
                path.size() > 3 &&
                path.back() == '\\')
            {
                path.pop_back();
            }

            return path;
        }

        static void ToDetour(
            const NavPoint& wow,
            float* detour)
        {
            /*
             * VMaNGOS/Detour coordinate order:
             *
             *   WoW    = X, Y, Z
             *   Detour = Y, Z, X
             */
            detour[0] =
                wow.y;

            detour[1] =
                wow.z;

            detour[2] =
                wow.x;
        }

        static NavPoint ToWow(
            const float* detour)
        {
            return NavPoint{
                detour[2],
                detour[0],
                detour[1]
            };
        }

        bool ReadMapParameters(
            const std::string& path,
            dtNavMeshParams& parameters,
            std::string& error)
        {
            std::ifstream input(
                path,
                std::ios::binary
            );

            if (!input)
            {
                error =
                    "Could not open mmap file: " +
                    path;

                return false;
            }

            input.read(
                reinterpret_cast<char*>(
                    &parameters
                ),
                sizeof(parameters)
            );

            if (
                input.gcount() !=
                    static_cast<std::streamsize>(
                        sizeof(parameters)
                    ))
            {
                error =
                    "Invalid or truncated mmap file: " +
                    path;

                return false;
            }

            return true;
        }

        bool LoadTile(
            const std::string& path,
            std::string& error)
        {
            if (!CurrentTopology()) { error = "Navmesh cache generation invalidated."; return false; }
            NavMeshTileIdentity identity{};
            if (!FileIdentity(path, identity)) { error = "Could not inspect tile: " + path; return false; }
            const auto cached = session_->tiles.find(path);
            if (cached != session_->tiles.end())
            {
                if (cached->second.identity != identity ||
                    !mesh_->getTileByRef(static_cast<dtTileRef>(cached->second.reference)))
                {
                    InvalidateSessionCache("tile_metadata_or_reference_changed");
                    error = "Cached tile metadata/reference changed: " + path;
                    return false;
                }
                ++initProfile_.cacheHits;
                ++session_->cacheHits;
                loadedTiles_ = static_cast<int>(session_->tiles.size());
                return true;
            }
            TileProfileScope tileProfile{*this, path};
            const auto openStarted = ProfileClock::now();
            std::ifstream input(
                path,
                std::ios::binary
            );
            initProfile_.tileReadMs += ProfileMs(openStarted);

            if (!input)
            {
                error =
                    "Could not open mmtile file: " +
                    path;

                return false;
            }
            ++initProfile_.tilesOpened;

            MmapTileHeader header{};

            const auto headerReadStarted = ProfileClock::now();
            input.read(
                reinterpret_cast<char*>(
                    &header
                ),
                sizeof(header)
            );
            initProfile_.tileReadMs += ProfileMs(headerReadStarted);

            if (
                input.gcount() !=
                    static_cast<std::streamsize>(
                        sizeof(header)
                    ))
            {
                error =
                    "Invalid or truncated tile header: " +
                    path;

                return false;
            }

            if (
                header.mmapMagic !=
                    MmapMagic)
            {
                error =
                    "Bad MMAP magic in tile: " +
                    path;

                return false;
            }

            if (
                header.dtVersion !=
                    static_cast<std::uint32_t>(
                        DT_NAVMESH_VERSION
                    ))
            {
                error =
                    "Detour version mismatch in tile: " +
                    path;

                return false;
            }

            if (
                header.mmapVersion !=
                    MmapVersion)
            {
                error =
                    "MMAP version mismatch in tile: " +
                    path;

                return false;
            }

            if (header.size == 0)
            {
                error =
                    "Tile contains no navmesh data: " +
                    path;

                return false;
            }

            auto* data =
                static_cast<unsigned char*>(
                    dtAlloc(
                        header.size,
                        DT_ALLOC_PERM
                    )
                );

            if (data == nullptr)
            {
                error =
                    "Detour allocation failed for tile: " +
                    path;

                return false;
            }

            const auto dataReadStarted = ProfileClock::now();
            input.read(
                reinterpret_cast<char*>(
                    data
                ),
                header.size
            );
            initProfile_.tileReadMs += ProfileMs(dataReadStarted);

            if (
                input.gcount() !=
                    static_cast<std::streamsize>(
                        header.size
                    ))
            {
                dtFree(
                    data
                );

                error =
                    "Invalid or truncated tile data: " +
                    path;

                return false;
            }

            dtTileRef tileReference =
                0;

            const auto addTileStarted = ProfileClock::now();
            ++initProfile_.addTileCalls;
            const dtStatus status =
                mesh_->addTile(
                    data,
                    static_cast<int>(
                        header.size
                    ),
                    DT_TILE_FREE_DATA,
                    0,
                    &tileReference
                );
            initProfile_.addTileMs += ProfileMs(addTileStarted);

            if (
                dtStatusFailed(
                    status
                ) ||
                tileReference == 0)
            {
                dtFree(
                    data
                );

                error =
                    "Detour rejected tile: " +
                    path;

                return false;
            }

            session_->tiles.emplace(path, SessionCache::Tile{
                identity, static_cast<std::uint64_t>(tileReference)});
            ++session_->coldTileLoads;
            loadedTiles_ = static_cast<int>(session_->tiles.size());
            ++initProfile_.tilesLoaded;
            tileProfile.success = true;

            return true;
        }

        bool LoadTiles(
            std::string& error)
        {
            const auto enumerationStarted = ProfileClock::now();
            const std::string pattern =
                JoinPath(
                    directory_,
                    MapName(
                        mapId_,
                        "*.mmtile"
                    )
                );

            WIN32_FIND_DATAA data{};

            HANDLE search =
                FindFirstFileA(
                    pattern.c_str(),
                    &data
                );

            if (
                search ==
                    INVALID_HANDLE_VALUE)
            {
                initProfile_.enumerationMs += ProfileMs(enumerationStarted);
                error =
                    "No mmtile files found for map " +
                    std::to_string(
                        mapId_
                    ) +
                    " in " +
                    directory_;

                return false;
            }

            std::vector<std::string> tiles;

            do
            {
                if (
                    (data.dwFileAttributes &
                        FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    continue;
                }

                const std::string name =
                    data.cFileName;

                if (
                    IsTileName(
                        name,
                        mapId_
                    ))
                {
                    tiles.push_back(
                        name
                    );
                }
            }
            while (
                FindNextFileA(
                    search,
                    &data
                )
            );

            FindClose(
                search
            );

            std::sort(
                tiles.begin(),
                tiles.end()
            );
            initProfile_.enumerationMs += ProfileMs(enumerationStarted);
            initProfile_.tilesDiscovered = static_cast<int>(tiles.size());

            if (tiles.empty())
            {
                error =
                    "No valid mmtile filenames found for map " +
                    std::to_string(
                        mapId_
                    ) +
                    " in " +
                    directory_;

                return false;
            }

            for (
                const auto& tile :
                tiles)
            {
                if (!LoadTile(
                        JoinPath(
                            directory_,
                            tile
                        ),
                        error
                    ))
                {
                    return false;
                }
            }

            return true;
        }

        bool InitializeMeshAndQuery(
            const std::string& mmapPath,
            const dtNavMeshParams& parameters,
            std::string& error)
        {
            NavMeshTileIdentity metadata{};
            if (!FileIdentity(mmapPath, metadata))
            { error = "Could not inspect mmap metadata: " + mmapPath; return false; }
            const NavMeshCacheIdentity identity{mapId_, directory_,
                std::string(reinterpret_cast<const char*>(&parameters), sizeof(parameters)),
                metadata.stamp, metadata.size};
            const auto current = Cache().Current();
            if (current && current->identity != identity)
                InvalidateSessionCache(current->identity.map != mapId_
                    ? "map_changed" : "incompatible_mesh_metadata");
            session_ = Cache().Acquire(identity);
            if (session_->topology.mesh)
            {
                mesh_ = session_->topology.mesh;
                loadedTiles_ = static_cast<int>(session_->tiles.size());
                return true;
            }
            mesh_ =
                dtAllocNavMesh();
            session_->topology.mesh = mesh_;

            if (
                mesh_ == nullptr ||
                dtStatusFailed(
                    mesh_->init(
                        &parameters
                    )
                ))
            {
                error =
                    "Could not initialize Detour navmesh from " +
                    mmapPath;
                InvalidateSessionCache("mesh_initialization_failed");
                return false;
            }

            return true;
        }

        bool InitializeQuery(std::string& error)
        {
            if (!CurrentTopology()) { error = "Navmesh cache generation invalidated."; return false; }
            // Node pools and mutable query state are private to this provider.
            if (query_) { dtFreeNavMeshQuery(query_); query_ = nullptr; }
            query_ =
                dtAllocNavMeshQuery();

            if (query_ == nullptr)
            {
                error =
                    "Could not allocate Detour navmesh query.";
                return false;
            }

            const dtStatus queryStatus =
                query_->init(
                    mesh_,
                    QueryNodePoolSize
                );

            if (
                dtStatusFailed(
                    queryStatus
                ))
            {
                std::ostringstream message;

                message
                    << "Could not initialize Detour query: status=0x"
                    << std::hex
                    << static_cast<unsigned int>(
                        queryStatus
                    )
                    << std::dec
                    << ", nodes="
                    << QueryNodePoolSize;

                error = message.str();
                return false;
            }

            return true;
        }

        void FinishIncremental(bool success, bool cancelled = false)
        {
            if (!incrementalActive_)
                return;
            incrementalFinalTotal_ = requestedTiles_;
            incrementalFinalProcessed_ = reusedTiles_ + incrementalCursor_.Processed();
            incrementalFinalElapsedMs_ = ProfileMs(incrementalStarted_);
            LogInitProfile(incrementalMode_.c_str(), incrementalStarted_,
                           success, incrementalWorkMs_);
            Debug::Logger::Info("NAV CACHE READY map=" + std::to_string(mapId_) +
                " generation=" + std::to_string(session_ ? session_->generation : 0) +
                " mode=" + incrementalMode_ +
                " loadedTotal=" + std::to_string(LoadedTiles()) +
                " newLoads=" + std::to_string(initProfile_.tilesLoaded) +
                " reused=" + std::to_string(initProfile_.cacheHits) +
                " elapsedMs=" + std::to_string(incrementalFinalElapsedMs_) +
                " workMs=" + std::to_string(incrementalWorkMs_) +
                " success=" + (success ? "yes" : "no"));
            incrementalActive_ = false;
            if (cancelled)
                incrementalCursor_.Cancel();
            else if (success)
                incrementalCursor_.MarkReady();
            else
                incrementalCursor_.Fail();
            incrementalTiles_.clear();
        }

        bool BeginIncrementalCommon(const std::string& directory,
                                    std::uint32_t mapId, const char* mode,
                                    std::string& error)
        {
            Shutdown();
            initProfile_ = InitProfile{};
            incrementalStarted_ = ProfileClock::now();
            incrementalWorkMs_ = 0.0;
            incrementalFinalTotal_ = 0;
            incrementalFinalProcessed_ = 0;
            incrementalFinalElapsedMs_ = 0.0;
            incrementalMode_ = mode;
            incrementalCursor_ = IncrementalTileInitializationPolicy{};
            incrementalTiles_.clear();
            requestedTiles_ = reusedTiles_ = 0;
            incrementalActive_ = true;
            directory_ = NormalizeWinePath(directory);
            mapId_ = mapId;
            // Revoke the previous world even if the new map's metadata is
            // absent. A failed map switch must not leave old refs usable.
            const auto previous = Cache().Current();
            if (previous && (previous->identity.map != mapId_ ||
                             previous->identity.directory != directory_))
                InvalidateSessionCache(previous->identity.map != mapId_
                    ? "map_changed" : "incompatible_mesh_directory");
            const std::string mmapPath = JoinPath(directory_, MapName(mapId_, ".mmap"));
            dtNavMeshParams parameters{};
            const auto headerStarted = ProfileClock::now();
            const bool headerOk = ReadMapParameters(mmapPath, parameters, error);
            initProfile_.mapHeaderMs += ProfileMs(headerStarted);
            if (!headerOk) InvalidateSessionCache("mesh_metadata_unavailable");
            if (!headerOk || !InitializeMeshAndQuery(mmapPath, parameters, error))
            {
                incrementalWorkMs_ += ProfileMs(incrementalStarted_);
                FinishIncremental(false);
                Shutdown();
                return false;
            }
            return true;
        }

        bool PrepareIncrementalCache(std::string& error)
        {
            struct LookupProfile
            {
                DetourNavigationProvider& provider;
                ProfileClock::time_point started = ProfileClock::now();
                ~LookupProfile()
                {
                    const double elapsed = ProfileMs(started);
                    provider.initProfile_.cacheLookupMs += elapsed;
                    provider.incrementalWorkMs_ += elapsed;
                }
            } lookupProfile{*this};
            requestedTiles_ = incrementalTiles_.size();
            std::vector<std::string> missing;
            for (const auto& path : incrementalTiles_)
            {
                const auto existing = session_->tiles.find(path);
                if (existing == session_->tiles.end()) { missing.push_back(path); continue; }
                NavMeshTileIdentity identity{};
                if (!FileIdentity(path, identity) || existing->second.identity != identity ||
                    !mesh_->getTileByRef(static_cast<dtTileRef>(existing->second.reference)))
                {
                    InvalidateSessionCache("tile_metadata_or_reference_changed");
                    error = "Cached tile metadata/reference changed: " + path;
                    return false;
                }
                ++reusedTiles_;
            }
            initProfile_.cacheHits = static_cast<int>(reusedTiles_);
            session_->cacheHits += reusedTiles_;
            incrementalTiles_ = std::move(missing);
            incrementalCursor_.Begin(incrementalTiles_.size());
            Debug::Logger::Info("NAV CACHE map=" + std::to_string(mapId_) +
                " generation=" + std::to_string(session_->generation) +
                " requestedTiles=" + std::to_string(requestedTiles_) +
                " alreadyLoaded=" + std::to_string(reusedTiles_) +
                " newLoads=" + std::to_string(incrementalTiles_.size()) +
                " cacheHitPercent=" + std::to_string(requestedTiles_ ? 100.0 * reusedTiles_ / requestedTiles_ : 0.0) +
                " mode=" + incrementalMode_);
            return true;
        }

        bool CollectIncrementalRouteTiles(const NavPoint& start,
                                          const NavPoint& destination,
                                          int marginTiles, std::string& error)
        {
            // Keep the same WoW/Recast axis mapping, window and tile order as
            // LoadRouteTiles; only the addTile work is deferred.
            const int startX = WorldToTile(start.y);
            const int startY = WorldToTile(start.x);
            const int endX = WorldToTile(destination.y);
            const int endY = WorldToTile(destination.x);
            const int minX = std::max(0, std::min(startX, endX) - marginTiles);
            const int maxX = std::min(63, std::max(startX, endX) + marginTiles);
            const int minY = std::max(0, std::min(startY, endY) - marginTiles);
            const int maxY = std::min(63, std::max(startY, endY) + marginTiles);
            Debug::Logger::Info(
                "NAVMESH 11B.8.2: ADT axis mapping start tileX=" +
                std::to_string(startX) + " tileY=" + std::to_string(startY) +
                " file=" + TileName(mapId_, startY, startX) +
                " destination tileX=" + std::to_string(endX) +
                " tileY=" + std::to_string(endY) +
                " file=" + TileName(mapId_, endY, endX));
            Debug::Logger::Info(
                "NAVMESH 11B.8.2: route tile window tileX=" +
                std::to_string(minX) + ".." + std::to_string(maxX) +
                " tileY=" + std::to_string(minY) + ".." +
                std::to_string(maxY) + " margin=" + std::to_string(marginTiles));
            for (int tileY = minY; tileY <= maxY; ++tileY)
            {
                for (int tileX = minX; tileX <= maxX; ++tileX)
                {
                    const std::string path = JoinPath(
                        directory_, TileName(mapId_, tileY, tileX));
                    const auto lookupStarted = ProfileClock::now();
                    const DWORD attributes = GetFileAttributesA(path.c_str());
                    initProfile_.enumerationMs += ProfileMs(lookupStarted);
                    if (attributes != INVALID_FILE_ATTRIBUTES &&
                        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                        incrementalTiles_.push_back(path);
                }
            }
            initProfile_.tilesDiscovered =
                static_cast<int>(incrementalTiles_.size());
            if (incrementalTiles_.empty())
            {
                error = "No route-scoped mmtile files were found in the computed ADT window.";
                return false;
            }
            return true;
        }

        bool CollectIncrementalFullMapTiles(std::string& error)
        {
            const auto enumerationStarted = ProfileClock::now();
            const std::string pattern = JoinPath(
                directory_, MapName(mapId_, "*.mmtile"));
            WIN32_FIND_DATAA data{};
            HANDLE search = FindFirstFileA(pattern.c_str(), &data);
            if (search == INVALID_HANDLE_VALUE)
            {
                initProfile_.enumerationMs += ProfileMs(enumerationStarted);
                error = "No mmtile files found for map " +
                    std::to_string(mapId_) + " in " + directory_;
                return false;
            }
            std::vector<std::string> names;
            do
            {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                    IsTileName(data.cFileName, mapId_))
                    names.emplace_back(data.cFileName);
            }
            while (FindNextFileA(search, &data));
            FindClose(search);
            std::sort(names.begin(), names.end());
            initProfile_.enumerationMs += ProfileMs(enumerationStarted);
            initProfile_.tilesDiscovered = static_cast<int>(names.size());
            if (names.empty())
            {
                error = "No valid mmtile filenames found for map " +
                    std::to_string(mapId_) + " in " + directory_;
                return false;
            }
            for (const auto& name : names)
                incrementalTiles_.push_back(JoinPath(directory_, name));
            return true;
        }

    public:
        static void InvalidateSessionCache(const char* reason)
        {
            std::lock_guard lock(Cache().Mutex());
            const auto current = Cache().Current();
            if (current)
                Debug::Logger::Info("NAV CACHE INVALIDATE map=" + std::to_string(current->identity.map) +
                    " generation=" + std::to_string(current->generation) + " reason=" + reason);
            Cache().Invalidate();
        }
        bool CacheGenerationCurrent() const
        {
            std::lock_guard lock(Cache().Mutex());
            return CurrentTopology();
        }
        struct CacheStatistics
        {
            std::uint64_t generation = 0;
            int loadedTotal = 0;
            int diskLoads = 0;
            int cacheHits = 0;
            int addTileCalls = 0;
        };
        CacheStatistics CacheStats() const
        {
            std::lock_guard lock(Cache().Mutex());
            return {CurrentTopology() ? session_->generation : 0,
                    LoadedTiles(), initProfile_.tilesOpened,
                    initProfile_.cacheHits, initProfile_.addTileCalls};
        }
        // Keep a command-time validity check atomic with dispatch. It does
        // not transfer movement ownership or share route state.
        template<class Command>
        bool WithCurrentTopology(Command&& command) const
        {
            std::lock_guard lock(Cache().Mutex());
            return CurrentTopology() && command();
        }
        enum class IncrementalStatus { Pending, Ready, Failed };

        struct IncrementalProgress
        {
            std::size_t total = 0;
            std::size_t processed = 0;
            int loaded = 0;
            int failed = 0;
            double elapsedMs = 0.0;
            double workMs = 0.0;
        };

        IncrementalProgress InitializationProgress() const
        {
            std::lock_guard lock(Cache().Mutex());
            return {incrementalActive_ ? requestedTiles_ : incrementalFinalTotal_,
                    incrementalActive_ ? reusedTiles_ + incrementalCursor_.Processed() : incrementalFinalProcessed_,
                    initProfile_.tilesLoaded + initProfile_.cacheHits, initProfile_.tilesFailed,
                    incrementalActive_ ? ProfileMs(incrementalStarted_) : incrementalFinalElapsedMs_,
                    incrementalWorkMs_};
        }

        bool BeginIncrementalForRoute(const std::string& directory,
                                      std::uint32_t mapId,
                                      const NavPoint& start,
                                      const NavPoint& destination,
                                      int marginTiles, std::string& error,
                                      const char* mode = "route")
        {
            std::lock_guard lock(Cache().Mutex());
            if (!BeginIncrementalCommon(directory, mapId, mode, error))
                return false;
            const bool found = CollectIncrementalRouteTiles(
                start, destination, marginTiles, error);
            incrementalWorkMs_ += ProfileMs(incrementalStarted_);
            if (!found)
            {
                FinishIncremental(false);
                Shutdown();
                return false;
            }
            if (!PrepareIncrementalCache(error))
            { FinishIncremental(false); Shutdown(); return false; }
            error.clear();
            return true;
        }

        bool BeginIncrementalFullMap(const std::string& directory,
                                     std::uint32_t mapId, std::string& error)
        {
            std::lock_guard lock(Cache().Mutex());
            if (!BeginIncrementalCommon(directory, mapId, "full_map", error))
                return false;
            const bool found = CollectIncrementalFullMapTiles(error);
            incrementalWorkMs_ += ProfileMs(incrementalStarted_);
            if (!found)
            {
                FinishIncremental(false);
                Shutdown();
                return false;
            }
            if (!PrepareIncrementalCache(error))
            { FinishIncremental(false); Shutdown(); return false; }
            error.clear();
            return true;
        }

        IncrementalStatus StepIncremental(std::string& error)
        {
            std::lock_guard lock(Cache().Mutex());
            if (!CurrentTopology())
            {
                error = "Navmesh cache generation invalidated.";
                FinishIncremental(false);
                Shutdown();
                return IncrementalStatus::Failed;
            }
            if (!incrementalActive_)
            {
                error = "No incremental navmesh initialization is pending.";
                return IncrementalStatus::Failed;
            }
            const auto stepStarted = ProfileClock::now();
            incrementalCursor_.BeginStep();
            std::size_t index = 0;
            while (incrementalCursor_.Next(index))
            {
                if (!LoadTile(incrementalTiles_[index], error))
                {
                    incrementalWorkMs_ += ProfileMs(stepStarted);
                    FinishIncremental(false);
                    Shutdown();
                    return IncrementalStatus::Failed;
                }
            }
            if (!incrementalCursor_.Complete())
            {
                incrementalWorkMs_ += ProfileMs(stepStarted);
                return IncrementalStatus::Pending;
            }
            const auto queryStarted = ProfileClock::now();
            const bool queryOk = InitializeQuery(error);
            initProfile_.queryInitMs += ProfileMs(queryStarted);
            incrementalWorkMs_ += ProfileMs(stepStarted);
            FinishIncremental(queryOk);
            if (!queryOk)
            {
                Shutdown();
                return IncrementalStatus::Failed;
            }
            error.clear();
            return IncrementalStatus::Ready;
        }

        DetourNavigationProvider() =
            default;

        void SetWaterTraversalMode(WaterTraversalMode mode)
        { waterTraversalMode_ = mode; }

        ~DetourNavigationProvider()
        {
            Shutdown();
        }

        DetourNavigationProvider(
            const DetourNavigationProvider&) =
            delete;

        DetourNavigationProvider& operator=(
            const DetourNavigationProvider&) =
            delete;

        static std::string ResolveMmapsDirectory()
        {
            const char* environment =
                std::getenv(
                    "WOW_NAV_MMAPS"
                );

            if (
                environment != nullptr &&
                environment[0] != '\0')
            {
                return
                    NormalizeWinePath(
                        environment
                    );
            }

            return
                "Z:\\home\\ludvig\\Games\\WoW-NavData\\mmaps";
        }

        bool ProjectToNavMesh(
            const NavPoint& point,
            NavPoint& projected,
            std::uint64_t& polyRef,
            float horizontalExtent = 3.0f,
            float verticalExtent = 5.0f) const
        {
            std::lock_guard lock(Cache().Mutex());
            projected = point;
            polyRef = 0;
            if (!CurrentTopology() || query_ == nullptr || horizontalExtent <= 0.0f ||
                verticalExtent <= 0.0f)
            {
                return false;
            }

            float detourPoint[3]{};
            ToDetour(point, detourPoint);
            const float extents[3] =
            {
                horizontalExtent,
                verticalExtent,
                horizontalExtent
            };
            dtQueryFilter filter;
            filter.setIncludeFlags(QueryIncludeFlags());
            filter.setExcludeFlags(QueryExcludeFlags());
            dtPolyRef reference = 0;
            float closest[3]{};
            const dtStatus status = query_->findNearestPoly(
                detourPoint, extents, &filter, &reference, closest);
            if (dtStatusFailed(status) || reference == 0)
                return false;

            projected = ToWow(closest);
            polyRef = static_cast<std::uint64_t>(reference);
            return std::isfinite(projected.x) &&
                std::isfinite(projected.y) &&
                std::isfinite(projected.z);
        }

        bool InitializeForRoute(
            const std::string& directory,
            std::uint32_t mapId,
            const NavPoint& start,
            const NavPoint& destination,
            int marginTiles,
            std::string& error,
            const char* profileMode = "route")
        {
            // Each bounded step locks independently; a blocking/offline
            // caller must not monopolize topology across the whole load.
            if (!BeginIncrementalForRoute(directory, mapId, start, destination,
                    marginTiles, error, profileMode)) return false;
            IncrementalStatus status;
            do { status = StepIncremental(error); } while (status == IncrementalStatus::Pending);
            return status == IncrementalStatus::Ready;
        }

        bool Initialize(
            const std::string& directory,
            std::uint32_t mapId,
            std::string& error)
        {
            if (!BeginIncrementalFullMap(directory, mapId, error)) return false;
            IncrementalStatus status;
            do { status = StepIncremental(error); } while (status == IncrementalStatus::Pending);
            return status == IncrementalStatus::Ready;
        }

        void Shutdown()
        {
            std::lock_guard lock(Cache().Mutex());
            if (incrementalActive_)
                FinishIncremental(false, true);
            if (query_ != nullptr)
            {
                dtFreeNavMeshQuery(
                    query_
                );

                query_ =
                    nullptr;
            }

            // Detach route-local query/work only. Compatible topology and
            // completed tiles remain owned by the world-session cache.
            mesh_ = nullptr;
            session_.reset();

            loadedTiles_ =
                0;

            mapId_ =
                0;

            directory_.clear();
        }

        int LoadedTiles() const
        {
            std::lock_guard lock(Cache().Mutex());
            return CurrentTopology() ? static_cast<int>(session_->tiles.size()) : 0;
        }

        // Phase 12B.10 helper for bounded local recovery. This deliberately
        // uses a caller-supplied tight vertical extent so a probe from the
        // lower side of a ramp/cliff transition cannot silently snap to a
        // much higher stacked polygon. It does not alter the route corridor
        // or the Phase 11B.8.2 tile-axis mapping.
        bool ProjectGroundNear(
            const NavPoint& probe,
            float horizontalExtent,
            float verticalExtent,
            NavPoint& projected) const
        {
            std::lock_guard lock(Cache().Mutex());
            projected = NavPoint{};

            if (
                !CurrentTopology() || query_ == nullptr ||
                horizontalExtent <= 0.0f ||
                verticalExtent <= 0.0f)
            {
                return false;
            }

            float detourProbe[3]{};
            ToDetour(
                probe,
                detourProbe
            );

            const float extents[3]
            {
                horizontalExtent,
                verticalExtent,
                horizontalExtent
            };

            dtQueryFilter filter;
            filter.setIncludeFlags(NavGround);
            filter.setExcludeFlags(QueryExcludeFlags());

            dtPolyRef reference = 0;
            float closest[3]{};

            const dtStatus status =
                query_->findNearestPoly(
                    detourProbe,
                    extents,
                    &filter,
                    &reference,
                    closest
                );

            if (
                dtStatusFailed(status) ||
                reference == 0)
            {
                return false;
            }

            projected = ToWow(closest);

            return
                std::isfinite(projected.x) &&
                std::isfinite(projected.y) &&
                std::isfinite(projected.z);
        }

        // Phase 14I.1: measure horizontal clearance from a NavMesh point to
        // the nearest Detour wall. This lets the follower keep CTM steering
        // points away from wall/rock corners and detect when the live player
        // is physically pinned against local collision geometry.
        bool FindWallDistance(
            const NavPoint& point,
            float maximumRadius,
            float& wallDistance,
            NavPoint& wallPoint) const
        {
            std::lock_guard lock(Cache().Mutex());
            wallDistance = maximumRadius;
            wallPoint = point;

            if (!CurrentTopology() || query_ == nullptr || maximumRadius <= 0.0f)
                return false;

            float detourPoint[3]{};
            ToDetour(point, detourPoint);

            const float extents[3] = { 2.0f, 4.0f, 2.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(QueryIncludeFlags());
            filter.setExcludeFlags(QueryExcludeFlags());

            dtPolyRef reference = 0;
            float closest[3]{};
            const dtStatus nearestStatus = query_->findNearestPoly(
                detourPoint, extents, &filter, &reference, closest);

            if (dtStatusFailed(nearestStatus) || reference == 0)
                return false;

            float hitPosition[3]{};
            float hitNormal[3]{};
            float hitDistance = maximumRadius;
            const dtStatus wallStatus = query_->findDistanceToWall(
                reference,
                closest,
                maximumRadius,
                &filter,
                &hitDistance,
                hitPosition,
                hitNormal);

            if (dtStatusFailed(wallStatus) || !std::isfinite(hitDistance))
                return false;

            wallDistance = hitDistance;
            wallPoint = ToWow(hitPosition);
            return
                std::isfinite(wallPoint.x) &&
                std::isfinite(wallPoint.y) &&
                std::isfinite(wallPoint.z);
        }

        // Return the exact directed, possibly clipped Detour portal. This is
        // evidence for local steering only; it never authorizes crossing an
        // otherwise rejected terrain transition.
        bool GetDirectedPortal(std::uint64_t fromRef,
            std::uint64_t toRef, NavPoint& a, NavPoint& b) const
        {
            std::lock_guard lock(Cache().Mutex());
            a={}; b={};
            if (!CurrentTopology() || !mesh_ || !fromRef || !toRef || fromRef==toRef) return false;
            const dtMeshTile* tile=nullptr;
            const dtPoly* poly=nullptr;
            if (dtStatusFailed(mesh_->getTileAndPolyByRef(
                    static_cast<dtPolyRef>(fromRef),&tile,&poly)) ||
                !tile || !poly) return false;
            for (unsigned i=poly->firstLink;i!=DT_NULL_LINK;
                 i=tile->links[i].next)
            {
                const dtLink& link=tile->links[i];
                if (link.ref!=toRef || link.edge>=poly->vertCount) continue;
                const float* first=&tile->verts[poly->verts[link.edge]*3];
                const float* second=&tile->verts[
                    poly->verts[(link.edge+1)%poly->vertCount]*3];
                float left[3]{},right[3]{};
                if(link.side==0xff)
                {
                    dtVcopy(left,first);dtVcopy(right,second);
                }
                else
                {
                    if(link.bmin>link.bmax) return false;
                    dtVlerp(left,first,second,
                        static_cast<float>(link.bmin)/255.0f);
                    dtVlerp(right,first,second,
                        static_cast<float>(link.bmax)/255.0f);
                }
                a=ToWow(left);b=ToWow(right);
                return std::isfinite(a.x) && std::isfinite(a.y) &&
                    std::isfinite(a.z) && std::isfinite(b.x) &&
                    std::isfinite(b.y) && std::isfinite(b.z) &&
                    std::hypot(a.x-b.x,a.y-b.y)>0.01f;
            }
            return false;
        }

        // A route-local steering candidate inside one already selected poly.
        // Topology is unchanged, and a living query cannot sample excluded
        // water polygons through this helper. Terrain/steep validation remains
        // the follower's separate pre-movement gate.
        bool GetFilteredPolygonInterior(std::uint64_t reference,
            NavPoint& interior) const
        {
            std::lock_guard lock(Cache().Mutex());
            interior = {};
            if (!CurrentTopology() || !query_ || !mesh_ || !reference)
                return false;
            const dtMeshTile* tile = nullptr;
            const dtPoly* poly = nullptr;
            if (dtStatusFailed(mesh_->getTileAndPolyByRef(
                    static_cast<dtPolyRef>(reference), &tile, &poly)) ||
                !tile || !poly || !poly->vertCount ||
                (poly->flags & QueryIncludeFlags()) == 0 ||
                (poly->flags & QueryExcludeFlags()) != 0)
                return false;
            NavPoint center{};
            for (unsigned vertex = 0; vertex < poly->vertCount; ++vertex)
            {
                const NavPoint point = ToWow(&tile->verts[
                    poly->verts[vertex] * 3]);
                center.x += point.x;
                center.y += point.y;
                center.z += point.z;
            }
            const float count = static_cast<float>(poly->vertCount);
            center.x /= count;
            center.y /= count;
            center.z /= count;
            float desired[3]{}, closest[3]{};
            ToDetour(center, desired);
            if (dtStatusFailed(query_->closestPointOnPoly(
                    static_cast<dtPolyRef>(reference), desired, closest,
                    nullptr)))
                return false;
            interior = ToWow(closest);
            return std::isfinite(interior.x) && std::isfinite(interior.y) &&
                std::isfinite(interior.z);
        }

        // Phase 13C.1: validate a local steering segment against the
        // currently loaded Detour surface before handing it to WoW CTM.
        // A point can be on NavMesh yet still be separated from the live
        // player by local geometry; raycast catches that class of false
        // "directly reachable" steering target.
        bool IsSurfaceSegmentReachable(
            const NavPoint& start,
            const NavPoint& destination,
            float& reachableFraction,
            NavPoint& reachablePoint,
            NavSurfaceRayTrace* trace = nullptr) const
        {
            std::lock_guard lock(Cache().Mutex());
            reachableFraction = 0.0f;
            reachablePoint = start;
            if (trace) *trace = {};

            if (!CurrentTopology() || query_ == nullptr)
                return false;

            float startPosition[3]{};
            float destinationPosition[3]{};
            ToDetour(start, startPosition);
            ToDetour(destination, destinationPosition);

            const float extents[3] = { 2.0f, 4.0f, 2.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(QueryIncludeFlags());
            filter.setExcludeFlags(QueryExcludeFlags());

            dtPolyRef startReference = 0;
            float closestStart[3]{};
            const dtStatus nearestStatus = query_->findNearestPoly(
                startPosition,
                extents,
                &filter,
                &startReference,
                closestStart);

            if (dtStatusFailed(nearestStatus) || startReference == 0)
                return false;

            float t = 0.0f;
            float hitNormal[3]{};
            dtPolyRef visited[64]{};
            int visitedCount = 0;

            const dtStatus rayStatus = query_->raycast(
                startReference,
                closestStart,
                destinationPosition,
                &filter,
                &t,
                hitNormal,
                visited,
                &visitedCount,
                64);

            if (dtStatusFailed(rayStatus) || !std::isfinite(t))
                return false;

            reachableFraction = std::max(0.0f, std::min(1.0f, t));

            float reached[3]
            {
                closestStart[0] +
                    (destinationPosition[0] - closestStart[0]) * reachableFraction,
                closestStart[1] +
                    (destinationPosition[1] - closestStart[1]) * reachableFraction,
                closestStart[2] +
                    (destinationPosition[2] - closestStart[2]) * reachableFraction
            };

            reachablePoint = ToWow(reached);
            if (trace)
            {
                trace->startPoly=static_cast<std::uint64_t>(startReference);
                trace->lastVisitedPoly=visitedCount>0
                    ?static_cast<std::uint64_t>(visited[visitedCount-1]):0;
                trace->visitedCount=visitedCount;
                trace->complete=(rayStatus & DT_BUFFER_TOO_SMALL)==0;
                trace->hitPoint=reachablePoint;
            }
            return true;
        }

        // Phase 13C.1 local obstacle recovery. moveAlongSurface keeps the
        // result connected to the live start polygon, unlike a nearest-poly
        // projection that may snap to walkable ground on the far side of an
        // impassable rock, wall or cliff.
        bool MoveAlongSurface(
            const NavPoint& start,
            const NavPoint& desiredDestination,
            NavPoint& reached) const
        {
            std::lock_guard lock(Cache().Mutex());
            reached = start;

            if (!CurrentTopology() || query_ == nullptr)
                return false;

            float startPosition[3]{};
            float destinationPosition[3]{};
            ToDetour(start, startPosition);
            ToDetour(desiredDestination, destinationPosition);

            const float extents[3] = { 2.0f, 4.0f, 2.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(QueryIncludeFlags());
            filter.setExcludeFlags(QueryExcludeFlags());

            dtPolyRef startReference = 0;
            float closestStart[3]{};
            const dtStatus nearestStatus = query_->findNearestPoly(
                startPosition,
                extents,
                &filter,
                &startReference,
                closestStart);

            if (dtStatusFailed(nearestStatus) || startReference == 0)
                return false;

            float resultPosition[3]{};
            dtPolyRef visited[64]{};
            int visitedCount = 0;

            const dtStatus moveStatus = query_->moveAlongSurface(
                startReference,
                closestStart,
                destinationPosition,
                &filter,
                resultPosition,
                visited,
                &visitedCount,
                64);

            if (dtStatusFailed(moveStatus) || visitedCount <= 0)
                return false;

            reached = ToWow(resultPosition);
            return
                std::isfinite(reached.x) &&
                std::isfinite(reached.y) &&
                std::isfinite(reached.z);
        }

        // Phase 13D.3/13D.4: Detour's default query filter cannot reject a
        // single directed edge in this build because DT_VIRTUAL_QUERYFILTER
        // is disabled. Use a bounded route-scoped approximation instead:
        // temporarily remove NavGround from the target polygons of transitions
        // proven physically non-traversable, run the normal Detour query, then
        // restore every polygon flag immediately. The live start and final
        // destination polygons are never masked. No mesh data is persisted.
        bool FindPathAvoidingPolygons(
            const NavPoint& start,
            const NavPoint& destination,
            const std::vector<std::uint64_t>& blockedPolygons,
            NavPathResult& result)
        {
            std::lock_guard lock(Cache().Mutex());
            if (blockedPolygons.empty())
                return FindPath(start, destination, result);

            result = NavPathResult{};
            result.avoidanceActive = true;
            result.avoidanceRequestedCount =
                static_cast<int>(blockedPolygons.size());

            if (!CurrentTopology() || mesh_ == nullptr || query_ == nullptr)
            {
                result.error = session_ && !CurrentTopology()
                    ? "Navmesh cache generation invalidated."
                    : "Detour provider is not initialized.";
                return false;
            }

            float startPosition[3]{};
            float destinationPosition[3]{};
            ToDetour(start, startPosition);
            ToDetour(destination, destinationPosition);

            const float extents[3] = { 5.0f, 10.0f, 5.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(QueryIncludeFlags());
            filter.setExcludeFlags(QueryExcludeFlags());

            dtPolyRef liveStartRef = 0;
            dtPolyRef liveEndRef = 0;
            float closestStart[3]{};
            float closestEnd[3]{};

            const dtStatus startStatus = query_->findNearestPoly(
                startPosition, extents, &filter, &liveStartRef, closestStart);
            const dtStatus endStatus = query_->findNearestPoly(
                destinationPosition, extents, &filter, &liveEndRef, closestEnd);

            if (dtStatusFailed(startStatus) || liveStartRef == 0 ||
                dtStatusFailed(endStatus) || liveEndRef == 0)
            {
                result.error =
                    "Could not resolve start/end polygon for blocked-route query.";
                return false;
            }

            struct SavedFlags
            {
                dtPolyRef ref = 0;
                unsigned short flags = 0;
            };

            std::vector<SavedFlags> saved;
            saved.reserve(blockedPolygons.size());
            // The mesh outlives the query. Restore masks even on an exception;
            // the surrounding cache lock excludes every other query/addTile.
            struct FlagRestoration
            {
                dtNavMesh* mesh;
                const std::vector<SavedFlags>& saved;
                bool restored = false;
                void Restore()
                {
                    if (restored) return;
                    for (auto it = saved.rbegin(); it != saved.rend(); ++it)
                        mesh->setPolyFlags(it->ref, it->flags);
                    restored = true;
                }
                ~FlagRestoration() { Restore(); }
            } restoration{mesh_, saved};

            for (const std::uint64_t rawRef : blockedPolygons)
            {
                const dtPolyRef ref = static_cast<dtPolyRef>(rawRef);
                if (ref == 0 || ref == liveStartRef || ref == liveEndRef)
                    continue;

                bool duplicate = false;
                for (const SavedFlags& existing : saved)
                {
                    if (existing.ref == ref)
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate)
                    continue;

                unsigned short flags = 0;
                if (dtStatusFailed(mesh_->getPolyFlags(ref, &flags)))
                    continue;

                const unsigned short maskedFlags =
                    static_cast<unsigned short>(flags & ~NavGround);
                if (maskedFlags == flags)
                    continue;

                if (dtStatusFailed(mesh_->setPolyFlags(ref, maskedFlags)))
                    continue;

                saved.push_back(SavedFlags{ ref, flags });
            }

            const bool ok = FindPath(start, destination, result);

            restoration.Restore();

            result.avoidanceActive = true;
            result.avoidanceRequestedCount =
                static_cast<int>(blockedPolygons.size());
            result.avoidanceAppliedCount =
                static_cast<int>(saved.size());
            result.avoidedPolygons.clear();
            result.avoidedPolygons.reserve(saved.size());
            for (const SavedFlags& item : saved)
                result.avoidedPolygons.push_back(
                    static_cast<std::uint64_t>(item.ref));

            return ok;
        }

        bool FindPathWithFlags(
            const NavPoint& start,
            const NavPoint& destination,
            NavPathResult& result,
            unsigned short includeFlags,
            unsigned short excludeFlags,
            bool diagnosticWaterQuery = false)
        {
            std::lock_guard lock(Cache().Mutex());
            if (!diagnosticWaterQuery)
                excludeFlags = QueryExcludeFlags(excludeFlags);
            result =
                NavPathResult{};

            result.includeFlags =
                includeFlags;

            result.excludeFlags =
                excludeFlags;

            result.waterAwareRoute =
                (includeFlags & NavWater) != 0 &&
                (excludeFlags & NavWater) == 0;

            result.loadedTiles =
                LoadedTiles();
            result.meshGeneration = session_ ? session_->generation : 0;

            result.queryNodePoolSize =
                QueryNodePoolSize;

            result.maximumPolygons =
                MaximumPolygons;

            result.maximumStraightPoints =
                MaximumStraightPoints;

            if (
                !CurrentTopology() || mesh_ == nullptr ||
                query_ == nullptr)
            {
                result.error =
                    session_ && !CurrentTopology()
                        ? "Navmesh cache generation invalidated."
                        : "Detour provider is not initialized.";

                return false;
            }

            float startPosition[3]{};
            float destinationPosition[3]{};

            ToDetour(
                start,
                startPosition
            );

            ToDetour(
                destination,
                destinationPosition
            );

            /*
             * X/Z are horizontal in Detour's coordinate
             * system; Y is elevation after WoW->Detour
             * conversion.
             */
            const float extents[3] =
            {
                5.0f,
                10.0f,
                5.0f
            };

            dtQueryFilter filter;

            filter.setIncludeFlags(
                includeFlags
            );

            filter.setExcludeFlags(
                excludeFlags
            );

            dtPolyRef startReference =
                0;

            dtPolyRef endReference =
                0;

            float closestStart[3]{};
            float closestEnd[3]{};

            dtStatus status =
                query_->findNearestPoly(
                    startPosition,
                    extents,
                    &filter,
                    &startReference,
                    closestStart
                );

            if (
                dtStatusFailed(
                    status
                ) ||
                startReference == 0)
            {
                result.error =
                    "No ground polygon was found near the start position.";

                return false;
            }

            status =
                query_->findNearestPoly(
                    destinationPosition,
                    extents,
                    &filter,
                    &endReference,
                    closestEnd
                );

            if (
                dtStatusFailed(
                    status
                ) ||
                endReference == 0)
            {
                result.error =
                    "No ground polygon was found near the destination.";

                result.startPoly =
                    static_cast<std::uint64_t>(
                        startReference
                    );

                return false;
            }

            result.startPoly =
                static_cast<std::uint64_t>(
                    startReference
                );

            result.endPoly =
                static_cast<std::uint64_t>(
                    endReference
                );

            result.projectedStart =
                ToWow(
                    closestStart
                );

            result.projectedDestination =
                ToWow(
                    closestEnd
                );

            dtPolyRef polygons[
                MaximumPolygons
            ]{};

            int polygonCount =
                0;

            status =
                query_->findPath(
                    startReference,
                    endReference,
                    closestStart,
                    closestEnd,
                    &filter,
                    polygons,
                    &polygonCount,
                    MaximumPolygons
                );

            result.findPathStatus =
                static_cast<std::uint32_t>(
                    status
                );

            result.findPathOutOfNodes =
                dtStatusDetail(
                    status,
                    DT_OUT_OF_NODES
                );

            result.findPathBufferTooSmall =
                dtStatusDetail(
                    status,
                    DT_BUFFER_TOO_SMALL
                );

            result.findPathPartialResult =
                dtStatusDetail(
                    status,
                    DT_PARTIAL_RESULT
                );

            if (
                dtStatusFailed(
                    status
                ) ||
                polygonCount <= 0)
            {
                result.error =
                    "Detour could not build a polygon path.";

                return false;
            }

            result.polygonCount =
                polygonCount;

            result.corridorPolys.reserve(
                static_cast<std::size_t>(polygonCount));
            for (int index = 0; index < polygonCount; ++index)
            {
                result.corridorPolys.push_back(
                    static_cast<std::uint64_t>(polygons[index]));
                unsigned short flags = 0;
                if (dtStatusSucceed(mesh_->getPolyFlags(
                        polygons[index], &flags)))
                {
                    if (TerrainTransitionPolicy::UsesSteep(flags))
                        ++result.steepPolygonCount;
                    if ((flags & NavWater) != 0)
                        ++result.waterPolygonCount;
                }
            }

            // findPath() normally guarantees this, but keep the invariant
            // explicit before a partial prefix is handed to live movement.
            result.corridorConnected = true;
            for (int index = 0; index + 1 < polygonCount; ++index)
            {
                const dtMeshTile* tile = nullptr;
                const dtPoly* poly = nullptr;
                if (dtStatusFailed(mesh_->getTileAndPolyByRef(
                        polygons[index], &tile, &poly)) ||
                    tile == nullptr || poly == nullptr)
                {
                    result.corridorConnected = false;
                    break;
                }

                bool linked = false;
                for (unsigned int link = poly->firstLink;
                     link != DT_NULL_LINK;
                     link = tile->links[link].next)
                {
                    if (tile->links[link].ref == polygons[index + 1])
                    {
                        linked = true;
                        break;
                    }
                }
                if (!linked)
                {
                    result.corridorConnected = false;
                    break;
                }
            }

            result.lastPoly =
                static_cast<std::uint64_t>(
                    polygons[
                        polygonCount - 1
                    ]
                );

            result.partial =
                polygons[
                    polygonCount - 1
                ] !=
                endReference;

            if (result.partial)
            {
                status =
                    query_->closestPointOnPoly(
                        polygons[
                            polygonCount - 1
                        ],
                        closestEnd,
                        closestEnd,
                        nullptr
                    );

                if (
                    dtStatusFailed(
                        status
                    ))
                {
                    result.error =
                        "Could not resolve the end of a partial path.";

                    return false;
                }
            }

            result.corridorEnd =
                ToWow(
                    closestEnd
                );

            float straightPath[
                MaximumStraightPoints *
                3
            ]{};

            unsigned char straightFlags[
                MaximumStraightPoints
            ]{};

            dtPolyRef straightRefs[
                MaximumStraightPoints
            ]{};

            int pointCount =
                0;

            /*
             * Phase 10B.2 deliberately asks Detour for every polygon
             * crossing instead of only funnel corners.
             *
             * WoW 1.12 CTM is a local movement primitive, not a full
             * NavMesh follower. Feeding it only sparse funnel corners
             * allowed a direct CTM segment to run into world geometry.
             * ALL_CROSSINGS gives us a dense sequence of corridor
             * steering points that remains tied to the Detour path.
             */
            status =
                query_->findStraightPath(
                    closestStart,
                    closestEnd,
                    polygons,
                    polygonCount,
                    straightPath,
                    straightFlags,
                    straightRefs,
                    &pointCount,
                    MaximumStraightPoints,
                    DT_STRAIGHTPATH_ALL_CROSSINGS
                );

            result.findStraightPathStatus = static_cast<std::uint32_t>(status);
            result.findStraightPathBufferTooSmall =
                dtStatusDetail(status, DT_BUFFER_TOO_SMALL);

            if (
                dtStatusFailed(
                    status
                ) ||
                pointCount < 2)
            {
                result.error =
                    "Detour could not build a straight point path.";

                return false;
            }

            result.points.reserve(
                static_cast<std::size_t>(
                    pointCount
                )
            );
            result.pointPolys.reserve(
                static_cast<std::size_t>(pointCount));

            for (
                int index = 0;
                index < pointCount;
                ++index)
            {
                result.points.push_back(
                    ToWow(
                        straightPath +
                        index * 3
                    )
                );
                result.pointPolys.push_back(
                    static_cast<std::uint64_t>(straightRefs[index]));
            }

            result.success =
                true;

            result.error.clear();

            return true;
        }

        // ALL_CROSSINGS supplies a movement point for each traversed portal.
        // Check those actual player-facing legs before a follower may issue
        // CTM. Polygon centers alone can exaggerate a wide sloped polygon.
        NavTerrainValidation ValidateTerrainRoute(
            const NavPoint& start, const NavPathResult& path) const
        {
            std::lock_guard lock(Cache().Mutex());
            NavTerrainValidation result{};
            if (!CurrentTopology() || mesh_ == nullptr ||
                path.meshGeneration != session_->generation || path.points.size() < 2 ||
                path.pointPolys.size() != path.points.size() ||
                path.corridorPolys.empty())
            {
                result.valid = false;
                result.reason = "missing_corridor_geometry";
                return result;
            }

            // Inspect every Detour adjacency, including polygons that funnel
            // simplification omitted from the straight-point list. Center
            // geometry is only a rejection signal on a steep-enabled route
            // crossing a flagged steep polygon; ordinary wide polygons may
            // have misleading center-to-center slopes.
            for (std::size_t i = 1; i < path.corridorPolys.size(); ++i)
            {
                const dtMeshTile* fromTile = nullptr;
                const dtMeshTile* toTile = nullptr;
                const dtPoly* fromPoly = nullptr;
                const dtPoly* toPoly = nullptr;
                const dtPolyRef fromRef =
                    static_cast<dtPolyRef>(path.corridorPolys[i - 1]);
                const dtPolyRef toRef =
                    static_cast<dtPolyRef>(path.corridorPolys[i]);
                if (dtStatusFailed(mesh_->getTileAndPolyByRef(
                        fromRef, &fromTile, &fromPoly)) ||
                    dtStatusFailed(mesh_->getTileAndPolyByRef(
                        toRef, &toTile, &toPoly)) ||
                    !fromTile || !toTile || !fromPoly || !toPoly)
                {
                    result.valid = false;
                    result.reason = "missing_corridor_polygon";
                    return result;
                }
                const dtLink* portal = nullptr;
                for (unsigned linkIndex = fromPoly->firstLink;
                     linkIndex != DT_NULL_LINK;
                     linkIndex = fromTile->links[linkIndex].next)
                    if (fromTile->links[linkIndex].ref == toRef)
                    {
                        portal = &fromTile->links[linkIndex];
                        break;
                    }
                if (!portal)
                {
                    result.valid = false;
                    result.reason = "unlinked_corridor_transition";
                    return result;
                }
                if (!path.steepFallback ||
                    !TerrainTransitionPolicy::UsesSteep(
                        fromPoly->flags | toPoly->flags) ||
                    fromPoly->vertCount == 0 || toPoly->vertCount == 0)
                    continue;

                const auto center = [](const dtMeshTile* tile,
                                       const dtPoly* poly)
                {
                    NavPoint value{};
                    for (unsigned vertex = 0; vertex < poly->vertCount;
                         ++vertex)
                    {
                        const NavPoint point = ToWow(&tile->verts[
                            poly->verts[vertex] * 3]);
                        value.x += point.x;
                        value.y += point.y;
                        value.z += point.z;
                    }
                    const float count = static_cast<float>(poly->vertCount);
                    value.x /= count;
                    value.y /= count;
                    value.z /= count;
                    return value;
                };
                const auto geometry = TerrainTransitionPolicy::Assess(
                    center(fromTile, fromPoly), center(toTile, toPoly));
                if (!geometry.rejected)
                    continue;
                result.valid = false;
                result.fromPoly = path.corridorPolys[i - 1];
                result.toPoly = path.corridorPolys[i];
                result.fromFlags = fromPoly->flags;
                result.toFlags = toPoly->flags;
                result.tileSeamKnown = true;
                result.tileSeam = fromTile != toTile;
                result.geometry = geometry;
                result.reason = "unsafe_steep_corridor_transition";
                if (portal->edge < fromPoly->vertCount)
                {
                    const float* a = &fromTile->verts[
                        fromPoly->verts[portal->edge] * 3];
                    const float* b = &fromTile->verts[
                        fromPoly->verts[(portal->edge + 1) %
                            fromPoly->vertCount] * 3];
                    result.portalA = ToWow(a);
                    result.portalB = ToWow(b);
                    result.portalKnown = true;
                }
                return result;
            }

            std::size_t corridorIndex = 0;
            for (std::size_t i = 1; i < path.points.size(); ++i)
            {
                // This Detour fork reports ref=0 for the final straight-path
                // endpoint. It still lies on the corridor's final polygon.
                const std::uint64_t enteredRef =
                    TerrainTransitionPolicy::EnteredRef(
                        path.pointPolys[i], i + 1 == path.points.size(),
                        path.corridorPolys.back());
                std::size_t entered = corridorIndex;
                while (entered < path.corridorPolys.size() &&
                       path.corridorPolys[entered] != enteredRef)
                    ++entered;
                if (entered == path.corridorPolys.size())
                {
                    result.valid = false;
                    result.pointIndex = i;
                    result.reason = "straight_point_outside_corridor";
                    return result;
                }

                const NavPoint& from = i == 1 ? start : path.points[i - 1];
                const auto geometry = TerrainTransitionPolicy::Assess(
                    from, path.points[i]);
                if (geometry.rejected)
                {
                    result.valid = false;
                    result.geometry = geometry;
                    result.pointIndex = i;
                    result.reason = "unsafe_vertical_portal";
                    // A skipped polygon crossing cannot safely identify just
                    // one directed edge for the existing avoidance policy.
                    if (entered != corridorIndex + 1)
                        return result;
                    result.fromPoly = path.corridorPolys[corridorIndex];
                    result.toPoly = path.corridorPolys[entered];
                    mesh_->getPolyFlags(
                        static_cast<dtPolyRef>(result.fromPoly),
                        &result.fromFlags);
                    mesh_->getPolyFlags(
                        static_cast<dtPolyRef>(result.toPoly),
                        &result.toFlags);

                    const dtMeshTile* toTile = nullptr;
                    const dtPoly* toPoly = nullptr;
                    const dtMeshTile* fromTile = nullptr;
                    const dtPoly* fromPoly = nullptr;
                    if (dtStatusSucceed(mesh_->getTileAndPolyByRef(
                            static_cast<dtPolyRef>(result.fromPoly),
                            &fromTile,&fromPoly)) &&
                        dtStatusSucceed(mesh_->getTileAndPolyByRef(
                            static_cast<dtPolyRef>(result.toPoly),
                            &toTile,&toPoly)) && fromTile && toTile)
                    {
                        result.tileSeamKnown = true;
                        result.tileSeam = fromTile != toTile;
                    }

                    const dtMeshTile* tile = nullptr;
                    const dtPoly* poly = nullptr;
                    if (dtStatusSucceed(mesh_->getTileAndPolyByRef(
                            static_cast<dtPolyRef>(result.fromPoly),
                            &tile, &poly)) && tile && poly)
                    {
                        for (unsigned linkIndex = poly->firstLink;
                             linkIndex != DT_NULL_LINK;
                             linkIndex = tile->links[linkIndex].next)
                        {
                            const dtLink& link = tile->links[linkIndex];
                            if (link.ref != result.toPoly ||
                                link.edge >= poly->vertCount)
                                continue;
                            const float* a = &tile->verts[
                                poly->verts[link.edge] * 3];
                            const float* b = &tile->verts[
                                poly->verts[(link.edge + 1) %
                                    poly->vertCount] * 3];
                            float left[3] = {a[0], a[1], a[2]};
                            float right[3] = {b[0], b[1], b[2]};
                            if (link.side != 0xff)
                            {
                                dtVlerp(left, a, b,
                                    static_cast<float>(link.bmin) / 255.0f);
                                dtVlerp(right, a, b,
                                    static_cast<float>(link.bmax) / 255.0f);
                            }
                            result.portalA = ToWow(left);
                            result.portalB = ToWow(right);
                            result.portalKnown = true;
                            break;
                        }
                    }
                    return result;
                }
                corridorIndex = entered;
            }
            return result;
        }

        // Excluding steep must not silently project a steep start/end onto a
        // different nearby height layer and call that a complete route to
        // the requested location. Compare polygon identity using the same
        // search extents with only the steep exclusion removed.
        bool PreferredEndpointsMatchUnrestricted(
            const NavPoint& start, const NavPoint& destination,
            unsigned short includeFlags,
            const NavPathResult& preferred) const
        {
            std::lock_guard lock(Cache().Mutex());
            if (!CurrentTopology() || !query_ ||
                preferred.meshGeneration != session_->generation || preferred.startPoly == 0 ||
                preferred.endPoly == 0)
                return false;
            const float extents[3] = {5.0f, 10.0f, 5.0f};
            float startPosition[3]{}, destinationPosition[3]{};
            float closestStart[3]{}, closestDestination[3]{};
            ToDetour(start, startPosition);
            ToDetour(destination, destinationPosition);
            dtQueryFilter filter;
            filter.setIncludeFlags(includeFlags);
            filter.setExcludeFlags(QueryExcludeFlags());
            dtPolyRef startRef = 0, endRef = 0;
            return dtStatusSucceed(query_->findNearestPoly(
                       startPosition, extents, &filter,
                       &startRef, closestStart)) &&
                dtStatusSucceed(query_->findNearestPoly(
                    destinationPosition, extents, &filter,
                    &endRef, closestDestination)) &&
                TerrainTransitionPolicy::SameEndpointPolygons(
                    preferred.startPoly, preferred.endPoly,
                    startRef, endRef);
        }

        // Keep ground before water, but first ask each terrain class for a
        // complete route without NAV_STEEP_SLOPES. A steep-enabled result is
        // only a fallback; the follower validates its movement geometry
        // before it can enter Moving/issue CTM.
        bool FindPath(
            const NavPoint& start,
            const NavPoint& destination,
            NavPathResult& result)
        {
            std::lock_guard lock(Cache().Mutex());
            NavPathResult preferredGround{};
            const bool preferredGroundOk = FindPathWithFlags(
                start, destination, preferredGround, NavGround,
                TerrainTransitionPolicy::SteepFlag);
            const bool preferredQueryComplete = TerrainTransitionPolicy::Complete(
                preferredGroundOk, preferredGround.success,
                preferredGround.partial);
            const bool groundProjectionMatches = preferredQueryComplete &&
                PreferredEndpointsMatchUnrestricted(
                    start, destination, NavGround, preferredGround);
            const bool preferredComplete = preferredQueryComplete &&
                groundProjectionMatches;
            Debug::Logger::Info(
                "NAV 14O.1 ROUTE POLICY preferred=non_steep result=" +
                std::string(preferredComplete ? "complete" : "incomplete") +
                " polygons=" + std::to_string(preferredGround.polygonCount) +
                " projectionMatch=" +
                std::string(groundProjectionMatches ? "yes" : "no"));
            if (preferredComplete)
            {
                result = std::move(preferredGround);
                return true;
            }

            NavPathResult groundFallback{};
            const bool groundOk = FindPathWithFlags(
                start, destination, groundFallback, NavGround, 0);
            const bool groundComplete = TerrainTransitionPolicy::Complete(
                groundOk, groundFallback.success, groundFallback.partial);
            Debug::Logger::Info(
                "NAV 14O.1 FALLBACK reason=no_complete_non_steep_route "
                "terrain=ground result=" +
                std::string(groundComplete ? "complete" : "incomplete") +
                " polygons=" + std::to_string(groundFallback.polygonCount));
            if (groundComplete)
            {
                groundFallback.steepFallback = true;
                result = std::move(groundFallback);
                return true;
            }

            if (waterTraversalMode_ == WaterTraversalMode::AvoidUntilQualified)
            {
                // A read-only diagnostic query proves a complete alternative
                // uses water; its corridor is never returned to the follower.
                NavPathResult diagnostic{};
                bool diagnosticOk = FindPathWithFlags(start, destination,
                    diagnostic, PlayerNavFlags,
                    TerrainTransitionPolicy::SteepFlag, true);
                bool complete = TerrainTransitionPolicy::Complete(
                    diagnosticOk, diagnostic.success, diagnostic.partial);
                if (!complete)
                {
                    diagnosticOk = FindPathWithFlags(start, destination,
                        diagnostic, PlayerNavFlags, 0, true);
                    complete = TerrainTransitionPolicy::Complete(
                        diagnosticOk, diagnostic.success, diagnostic.partial);
                }
                if (complete && diagnostic.waterPolygonCount > 0)
                {
                    groundFallback.error =
                        "Living-player water traversal is disabled.";
                    Debug::Logger::Info(
                        "NAV WATER REJECT reason=water_traversal_disabled"
                        " policy=avoid_until_qualified waterPolygons=" +
                        std::to_string(diagnostic.waterPolygonCount));
                }
                result = std::move(groundFallback);
                return groundOk;
            }

            // Preserve 13D.7.2's water fallback only after ground queries.
            NavPathResult preferredWater{};
            const bool preferredWaterOk = FindPathWithFlags(
                start, destination, preferredWater, PlayerNavFlags,
                TerrainTransitionPolicy::SteepFlag);
            preferredWater.waterFallbackAttempted = true;
            if (TerrainTransitionPolicy::Complete(preferredWaterOk,
                    preferredWater.success, preferredWater.partial) &&
                PreferredEndpointsMatchUnrestricted(
                    start, destination, PlayerNavFlags, preferredWater))
            {
                Debug::Logger::Info(
                    "NAVMESH 13D.7.2: WATER-AWARE COMPLETE CORRIDOR RECOVERED "
                    "includeFlags=0x09 excludeFlags=0x10");
                result = std::move(preferredWater);
                return true;
            }

            NavPathResult waterFallback{};
            const bool waterOk = FindPathWithFlags(
                start, destination, waterFallback, PlayerNavFlags, 0);
            waterFallback.waterFallbackAttempted = true;
            if (TerrainTransitionPolicy::Complete(waterOk,
                    waterFallback.success, waterFallback.partial))
            {
                waterFallback.steepFallback = true;
                Debug::Logger::Info(
                    "NAVMESH 13D.7.2: WATER-AWARE COMPLETE CORRIDOR RECOVERED "
                    "includeFlags=0x09 excludeFlags=0x00");
                result = std::move(waterFallback);
                return true;
            }

            // No complete route was found. Preserve 13D.7's original
            // ground-prefix choice and let its existing bounded partial
            // staging policy decide whether that prefix is usable.
            groundFallback.waterFallbackAttempted = true;
            groundFallback.steepFallback = groundOk && groundFallback.success;
            result = std::move(groundFallback);
            return groundOk;
        }
    };
}
