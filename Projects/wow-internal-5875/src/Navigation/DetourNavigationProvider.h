#pragma once

#include "../Debug/Logger.h"

#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"

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
#include <vector>

namespace Navigation
{
    struct NavPoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
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

        std::uint32_t findPathStatus = 0;
        bool findPathOutOfNodes = false;
        bool findPathBufferTooSmall = false;
        bool findPathPartialResult = false;

        int queryNodePoolSize = 0;
        int maximumPolygons = 0;

        // Phase 13D.7.2: preserve the Detour terrain flags used to build
        // the route. Ground remains the primary query; NAV_WATER is enabled
        // only as a fallback for player routes that cross legitimate swim
        // connections such as mainland-to-island travel.
        unsigned short includeFlags = 0;
        bool waterFallbackAttempted = false;
        bool waterAwareRoute = false;

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

    class DetourNavigationProvider
    {
    private:
        static constexpr std::uint32_t MmapMagic =
            0x4D4D4150;

        static constexpr std::uint32_t MmapVersion =
            6;

        static constexpr unsigned short NavGround =
            0x01;

        static constexpr unsigned short NavWater =
            0x08;

        static constexpr unsigned short PlayerNavFlags =
            static_cast<unsigned short>(NavGround | NavWater);

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

        struct InitProfile
        {
            int tilesDiscovered = 0;
            int tilesOpened = 0;
            int tilesLoaded = 0;
            int tilesFailed = 0;
            double enumerationMs = 0.0;
            double tileReadMs = 0.0;
            double addTileMs = 0.0;
            double queryInitMs = 0.0;
            double mapHeaderMs = 0.0;
            double slowestTileMs = 0.0;
            std::string slowestTileName;
        } initProfile_{};

        static double ProfileMs(ProfileClock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(
                ProfileClock::now() - start).count();
        }

        void LogInitProfile(const char* mode, ProfileClock::time_point start,
                            bool success) const
        {
            const double totalMs = ProfileMs(start);
            const double accounted = initProfile_.enumerationMs +
                initProfile_.tileReadMs + initProfile_.addTileMs +
                initProfile_.queryInitMs + initProfile_.mapHeaderMs;
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3)
                << "NAV 14N.3 INIT PROFILE: mode=" << mode
                << " threadId=" << GetCurrentThreadId()
                << " tilesDiscovered=" << initProfile_.tilesDiscovered
                << " tilesOpened=" << initProfile_.tilesOpened
                << " tilesLoaded=" << initProfile_.tilesLoaded
                << " tilesFailed=" << initProfile_.tilesFailed
                << " enumerationMs=" << initProfile_.enumerationMs
                << " tileReadMs=" << initProfile_.tileReadMs
                << " addTileMs=" << initProfile_.addTileMs
                << " queryInitMs=" << initProfile_.queryInitMs
                << " mapHeaderMs=" << initProfile_.mapHeaderMs
                << " otherMs=" << std::max(0.0, totalMs - accounted)
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

            ++loadedTiles_;
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
            mesh_ =
                dtAllocNavMesh();

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

                return false;
            }

            return true;
        }

        bool InitializeQuery(std::string& error)
        {
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

    public:
        DetourNavigationProvider() =
            default;

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
            projected = point;
            polyRef = 0;
            if (query_ == nullptr || horizontalExtent <= 0.0f ||
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
            filter.setIncludeFlags(PlayerNavFlags);
            filter.setExcludeFlags(0);
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
            InitProfileScope profileScope{*this, profileMode};
            Shutdown();
            initProfile_ = InitProfile{};

            directory_ =
                NormalizeWinePath(
                    directory
                );

            mapId_ = mapId;

            dtNavMeshParams parameters{};

            const std::string mmapPath =
                JoinPath(
                    directory_,
                    MapName(
                        mapId_,
                        ".mmap"
                    )
                );

            const auto mapHeaderStarted = ProfileClock::now();
            const bool mapHeaderRead = ReadMapParameters(
                    mmapPath,
                    parameters,
                    error);
            initProfile_.mapHeaderMs += ProfileMs(mapHeaderStarted);
            if (!mapHeaderRead)
            {
                Shutdown();
                return false;
            }

            if (!InitializeMeshAndQuery(
                    mmapPath,
                    parameters,
                    error
                ))
            {
                Shutdown();
                return false;
            }

            if (!LoadRouteTiles(
                    start,
                    destination,
                    marginTiles,
                    error
                ))
            {
                Shutdown();
                return false;
            }

            const auto queryInitStarted = ProfileClock::now();
            const bool queryInitialized = InitializeQuery(error);
            initProfile_.queryInitMs += ProfileMs(queryInitStarted);
            if (!queryInitialized)
            {
                Shutdown();
                return false;
            }

            error.clear();
            profileScope.success = true;
            return true;
        }

        bool Initialize(
            const std::string& directory,
            std::uint32_t mapId,
            std::string& error)
        {
            InitProfileScope profileScope{*this, "full_map"};
            Shutdown();
            initProfile_ = InitProfile{};

            directory_ =
                NormalizeWinePath(
                    directory
                );

            mapId_ =
                mapId;

            dtNavMeshParams parameters{};

            const std::string mmapPath =
                JoinPath(
                    directory_,
                    MapName(
                        mapId_,
                        ".mmap"
                    )
                );

            const auto mapHeaderStarted = ProfileClock::now();
            const bool mapHeaderRead = ReadMapParameters(
                    mmapPath,
                    parameters,
                    error);
            initProfile_.mapHeaderMs += ProfileMs(mapHeaderStarted);
            if (!mapHeaderRead)
            {
                Shutdown();

                return false;
            }

            mesh_ =
                dtAllocNavMesh();

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

                Shutdown();

                return false;
            }

            if (!LoadTiles(
                    error))
            {
                Shutdown();

                return false;
            }

            const auto queryInitStarted = ProfileClock::now();
            query_ =
                dtAllocNavMeshQuery();

            if (query_ == nullptr)
            {
                initProfile_.queryInitMs += ProfileMs(queryInitStarted);
                error =
                    "Could not allocate Detour navmesh query.";

                Shutdown();

                return false;
            }

            const dtStatus queryStatus =
                query_->init(
                    mesh_,
                    QueryNodePoolSize
                );
            initProfile_.queryInitMs += ProfileMs(queryInitStarted);

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

                error =
                    message.str();

                Shutdown();

                return false;
            }

            error.clear();
            profileScope.success = true;

            return true;
        }

        void Shutdown()
        {
            if (query_ != nullptr)
            {
                dtFreeNavMeshQuery(
                    query_
                );

                query_ =
                    nullptr;
            }

            if (mesh_ != nullptr)
            {
                dtFreeNavMesh(
                    mesh_
                );

                mesh_ =
                    nullptr;
            }

            loadedTiles_ =
                0;

            mapId_ =
                0;

            directory_.clear();
        }

        int LoadedTiles() const
        {
            return
                loadedTiles_;
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
            projected = NavPoint{};

            if (
                query_ == nullptr ||
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
            filter.setExcludeFlags(0);

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
            wallDistance = maximumRadius;
            wallPoint = point;

            if (query_ == nullptr || maximumRadius <= 0.0f)
                return false;

            float detourPoint[3]{};
            ToDetour(point, detourPoint);

            const float extents[3] = { 2.0f, 4.0f, 2.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(PlayerNavFlags);
            filter.setExcludeFlags(0);

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

        // Phase 13C.1: validate a local steering segment against the
        // currently loaded Detour surface before handing it to WoW CTM.
        // A point can be on NavMesh yet still be separated from the live
        // player by local geometry; raycast catches that class of false
        // "directly reachable" steering target.
        bool IsSurfaceSegmentReachable(
            const NavPoint& start,
            const NavPoint& destination,
            float& reachableFraction,
            NavPoint& reachablePoint) const
        {
            reachableFraction = 0.0f;
            reachablePoint = start;

            if (query_ == nullptr)
                return false;

            float startPosition[3]{};
            float destinationPosition[3]{};
            ToDetour(start, startPosition);
            ToDetour(destination, destinationPosition);

            const float extents[3] = { 2.0f, 4.0f, 2.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(PlayerNavFlags);
            filter.setExcludeFlags(0);

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
            reached = start;

            if (query_ == nullptr)
                return false;

            float startPosition[3]{};
            float destinationPosition[3]{};
            ToDetour(start, startPosition);
            ToDetour(desiredDestination, destinationPosition);

            const float extents[3] = { 2.0f, 4.0f, 2.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(PlayerNavFlags);
            filter.setExcludeFlags(0);

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
            if (blockedPolygons.empty())
                return FindPath(start, destination, result);

            result = NavPathResult{};
            result.avoidanceActive = true;
            result.avoidanceRequestedCount =
                static_cast<int>(blockedPolygons.size());

            if (mesh_ == nullptr || query_ == nullptr)
            {
                result.error = "Detour provider is not initialized.";
                return false;
            }

            float startPosition[3]{};
            float destinationPosition[3]{};
            ToDetour(start, startPosition);
            ToDetour(destination, destinationPosition);

            const float extents[3] = { 5.0f, 10.0f, 5.0f };
            dtQueryFilter filter;
            filter.setIncludeFlags(PlayerNavFlags);
            filter.setExcludeFlags(0);

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

            for (auto it = saved.rbegin(); it != saved.rend(); ++it)
                mesh_->setPolyFlags(it->ref, it->flags);

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
            unsigned short includeFlags)
        {
            result =
                NavPathResult{};

            result.includeFlags =
                includeFlags;

            result.waterAwareRoute =
                (includeFlags & NavWater) != 0;

            result.loadedTiles =
                loadedTiles_;

            result.queryNodePoolSize =
                QueryNodePoolSize;

            result.maximumPolygons =
                MaximumPolygons;

            if (
                mesh_ == nullptr ||
                query_ == nullptr)
            {
                result.error =
                    "Detour provider is not initialized.";

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
                0
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

        // Phase 13D.7.2: prefer a normal ground-only player route. If the
        // ground graph cannot reach the destination, retry once with the
        // water terrain flag enabled. This avoids making swimming a shortcut
        // preference while still allowing legitimate mainland/island travel.
        bool FindPath(
            const NavPoint& start,
            const NavPoint& destination,
            NavPathResult& result)
        {
            NavPathResult groundResult{};
            const bool groundOk =
                FindPathWithFlags(
                    start,
                    destination,
                    groundResult,
                    NavGround
                );

            if (groundOk && groundResult.success && !groundResult.partial)
            {
                result = groundResult;
                return true;
            }

            Debug::Logger::Info(
                "NAVMESH 13D.7.2: GROUND-ONLY ROUTE INCOMPLETE "
                "ok=" + std::string(groundOk ? "yes" : "no") +
                " partial=" + std::string(groundResult.partial ? "yes" : "no") +
                " outOfNodes=" + std::string(groundResult.findPathOutOfNodes ? "yes" : "no") +
                " pathCount=" + std::to_string(groundResult.polygonCount) +
                " includeFlags=0x01"
            );

            Debug::Logger::Info(
                "NAVMESH 13D.7.2: PLAYER WATER FALLBACK START includeFlags=0x09"
            );

            NavPathResult waterResult{};
            const bool waterOk =
                FindPathWithFlags(
                    start,
                    destination,
                    waterResult,
                    PlayerNavFlags
                );

            waterResult.waterFallbackAttempted = true;

            if (waterOk && waterResult.success && !waterResult.partial)
            {
                Debug::Logger::Info(
                    "NAVMESH 13D.7.2: WATER-AWARE COMPLETE CORRIDOR RECOVERED "
                    "pathCount=" + std::to_string(waterResult.polygonCount) +
                    " includeFlags=0x09"
                );
                result = waterResult;
                return true;
            }

            Debug::Logger::Info(
                "NAVMESH 13D.7.2: WATER FALLBACK DID NOT RECOVER COMPLETE CORRIDOR "
                "ok=" + std::string(waterOk ? "yes" : "no") +
                " partial=" + std::string(waterResult.partial ? "yes" : "no") +
                " outOfNodes=" + std::string(waterResult.findPathOutOfNodes ? "yes" : "no") +
                " pathCount=" + std::to_string(waterResult.polygonCount)
            );

            groundResult.waterFallbackAttempted = true;
            result = groundResult;
            return groundOk;
        }
    };
}
