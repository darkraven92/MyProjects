#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t MmapMagic = 0x4d4d4150;
constexpr std::uint32_t MmapVersion = 6;
constexpr unsigned short NavGround = 0x01;
constexpr int MaximumPolygons = 256;
// This VMaNGOS Detour fork uses 16-bit node indices. 65536 therefore wraps
// the null/sentinel index and makes dtNavMeshQuery::init fail.
constexpr int QueryNodePoolSize = 2048;

struct MmapTileHeader
{
    std::uint32_t mmapMagic;
    std::uint32_t dtVersion;
    std::uint32_t mmapVersion;
    std::uint32_t size;
    std::uint32_t usesLiquids;
};

static_assert(sizeof(MmapTileHeader) == 20);

struct WowNavPoint
{
    float x;
    float y;
    float z;
};

struct NavHandle
{
    dtNavMesh* mesh = nullptr;
    dtNavMeshQuery* query = nullptr;
    int loadedTiles = 0;

    ~NavHandle()
    {
        if (query)
            dtFreeNavMeshQuery(query);
        if (mesh)
            dtFreeNavMesh(mesh);
    }
};

void writeError(char* destination, int capacity, const std::string& message)
{
    if (!destination || capacity <= 0)
        return;
    const std::size_t count = std::min(message.size(), static_cast<std::size_t>(capacity - 1));
    std::memcpy(destination, message.data(), count);
    destination[count] = '\0';
}

std::string mapName(std::uint32_t mapId, const char* extension)
{
    std::ostringstream value;
    value << std::setfill('0') << std::setw(3) << mapId << extension;
    return value.str();
}

bool hasTileName(const std::filesystem::path& path, std::uint32_t mapId)
{
    const std::string name = path.filename().string();
    const std::string prefix = mapName(mapId, "");
    constexpr const char* suffix = ".mmtile";
    constexpr std::size_t suffixLength = 7;
    return name.size() == 14 &&
           name.compare(0, prefix.size(), prefix) == 0 &&
           name.compare(name.size() - suffixLength, suffixLength, suffix) == 0;
}

bool readMapParameters(const std::filesystem::path& path,
                       dtNavMeshParams& parameters,
                       std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        error = "Could not open " + path.string();
        return false;
    }
    input.read(reinterpret_cast<char*>(&parameters), sizeof(parameters));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(parameters)))
    {
        error = "Invalid or truncated mmap file: " + path.string();
        return false;
    }
    return true;
}

bool loadTile(NavHandle& handle, const std::filesystem::path& path, std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        error = "Could not open " + path.string();
        return false;
    }

    MmapTileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(header)))
    {
        error = "Invalid or truncated tile header: " + path.string();
        return false;
    }
    if (header.mmapMagic != MmapMagic)
    {
        error = "Bad MMAP magic in " + path.string();
        return false;
    }
    if (header.dtVersion != static_cast<std::uint32_t>(DT_NAVMESH_VERSION))
    {
        error = "Detour version mismatch in " + path.string();
        return false;
    }
    if (header.mmapVersion != MmapVersion)
    {
        error = "MMAP version mismatch in " + path.string();
        return false;
    }
    if (header.size == 0)
    {
        error = "Tile contains no navmesh data: " + path.string();
        return false;
    }

    auto* data = static_cast<unsigned char*>(dtAlloc(header.size, DT_ALLOC_PERM));
    if (!data)
    {
        error = "Detour allocation failed for " + path.string();
        return false;
    }
    input.read(reinterpret_cast<char*>(data), header.size);
    if (input.gcount() != static_cast<std::streamsize>(header.size))
    {
        dtFree(data);
        error = "Invalid or truncated tile data: " + path.string();
        return false;
    }

    dtTileRef tileReference = 0;
    const dtStatus status = handle.mesh->addTile(
        data, static_cast<int>(header.size), DT_TILE_FREE_DATA, 0, &tileReference);
    if (dtStatusFailed(status) || tileReference == 0)
    {
        dtFree(data);
        error = "Detour rejected tile " + path.string();
        return false;
    }

    ++handle.loadedTiles;
    return true;
}

void toDetour(const WowNavPoint& wow, float* detour)
{
    detour[0] = wow.y;
    detour[1] = wow.z;
    detour[2] = wow.x;
}

WowNavPoint toWow(const float* detour)
{
    return WowNavPoint{detour[2], detour[0], detour[1]};
}
}

extern "C"
{
void* wow_nav_create(const char* mmapsDirectory,
                     std::uint32_t mapId,
                     char* errorBuffer,
                     int errorCapacity)
{
    if (!mmapsDirectory)
    {
        writeError(errorBuffer, errorCapacity, "The mmaps directory is null.");
        return nullptr;
    }

    try
    {
        const std::filesystem::path directory(mmapsDirectory);
        if (!std::filesystem::is_directory(directory))
        {
            writeError(errorBuffer, errorCapacity,
                       "The mmaps directory does not exist: " + directory.string());
            return nullptr;
        }

        auto handle = std::make_unique<NavHandle>();
        dtNavMeshParams parameters{};
        std::string error;
        if (!readMapParameters(directory / mapName(mapId, ".mmap"), parameters, error))
        {
            writeError(errorBuffer, errorCapacity, error);
            return nullptr;
        }

        handle->mesh = dtAllocNavMesh();
        if (!handle->mesh || dtStatusFailed(handle->mesh->init(&parameters)))
        {
            writeError(errorBuffer, errorCapacity, "Could not initialize the Detour navmesh.");
            return nullptr;
        }

        std::vector<std::filesystem::path> tiles;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
            if (entry.is_regular_file() && hasTileName(entry.path(), mapId))
                tiles.push_back(entry.path());
        std::sort(tiles.begin(), tiles.end());

        if (tiles.empty())
        {
            writeError(errorBuffer, errorCapacity,
                       "No mmtile files were found for map " + std::to_string(mapId) + ".");
            return nullptr;
        }
        for (const auto& tile : tiles)
        {
            if (!loadTile(*handle, tile, error))
            {
                writeError(errorBuffer, errorCapacity, error);
                return nullptr;
            }
        }

        handle->query = dtAllocNavMeshQuery();
        if (!handle->query)
        {
            writeError(errorBuffer, errorCapacity, "Could not allocate the Detour query.");
            return nullptr;
        }

        const dtStatus queryStatus = handle->query->init(handle->mesh, QueryNodePoolSize);
        if (dtStatusFailed(queryStatus))
        {
            std::ostringstream message;
            message << "Could not initialize the Detour query: status=0x"
                    << std::hex << static_cast<unsigned int>(queryStatus)
                    << std::dec << ", nodes=" << QueryNodePoolSize << ".";
            writeError(errorBuffer, errorCapacity, message.str());
            return nullptr;
        }

        writeError(errorBuffer, errorCapacity, "");
        return handle.release();
    }
    catch (const std::exception& exception)
    {
        writeError(errorBuffer, errorCapacity, exception.what());
        return nullptr;
    }
}

void wow_nav_destroy(void* rawHandle)
{
    delete static_cast<NavHandle*>(rawHandle);
}

int wow_nav_loaded_tiles(void* rawHandle)
{
    const auto* handle = static_cast<NavHandle*>(rawHandle);
    return handle ? handle->loadedTiles : 0;
}

int wow_nav_find_path(void* rawHandle,
                      const WowNavPoint* start,
                      const WowNavPoint* end,
                      WowNavPoint* outputPoints,
                      int maximumPoints,
                      int* outputPointCount,
                      int* outputPartial,
                      char* errorBuffer,
                      int errorCapacity)
{
    auto* handle = static_cast<NavHandle*>(rawHandle);
    if (!handle || !handle->query || !start || !end || !outputPoints || maximumPoints < 2 ||
        !outputPointCount || !outputPartial)
    {
        writeError(errorBuffer, errorCapacity, "Invalid path query arguments.");
        return 0;
    }

    *outputPointCount = 0;
    *outputPartial = 0;
    float startPosition[3];
    float endPosition[3];
    toDetour(*start, startPosition);
    toDetour(*end, endPosition);

    const float extents[3] = {5.0f, 10.0f, 5.0f};
    dtQueryFilter filter;
    filter.setIncludeFlags(NavGround);
    filter.setExcludeFlags(0);
    dtPolyRef startReference = 0;
    dtPolyRef endReference = 0;
    float closestStart[3];
    float closestEnd[3];

    dtStatus status = handle->query->findNearestPoly(
        startPosition, extents, &filter, &startReference, closestStart);
    if (dtStatusFailed(status) || startReference == 0)
    {
        writeError(errorBuffer, errorCapacity,
                   "No ground polygon was found near the start position.");
        return 0;
    }
    status = handle->query->findNearestPoly(
        endPosition, extents, &filter, &endReference, closestEnd);
    if (dtStatusFailed(status) || endReference == 0)
    {
        writeError(errorBuffer, errorCapacity,
                   "No ground polygon was found near the target position.");
        return 0;
    }

    dtPolyRef polygons[MaximumPolygons];
    int polygonCount = 0;
    status = handle->query->findPath(
        startReference, endReference, closestStart, closestEnd, &filter,
        polygons, &polygonCount, MaximumPolygons);
    if (dtStatusFailed(status) || polygonCount == 0)
    {
        writeError(errorBuffer, errorCapacity, "Detour could not build a polygon path.");
        return 0;
    }

    *outputPartial = polygons[polygonCount - 1] == endReference ? 0 : 1;
    if (*outputPartial)
    {
        status = handle->query->closestPointOnPoly(
            polygons[polygonCount - 1], closestEnd, closestEnd, nullptr);
        if (dtStatusFailed(status))
        {
            writeError(errorBuffer, errorCapacity,
                       "Could not resolve the end of a partial path.");
            return 0;
        }
    }

    std::vector<float> straightPath(static_cast<std::size_t>(maximumPoints) * 3);
    int pointCount = 0;
    status = handle->query->findStraightPath(
        closestStart, closestEnd, polygons, polygonCount, straightPath.data(),
        nullptr, nullptr, &pointCount, maximumPoints);
    if (dtStatusFailed(status) || pointCount < 2)
    {
        writeError(errorBuffer, errorCapacity,
                   "Detour could not build a straight point path.");
        return 0;
    }

    for (int index = 0; index < pointCount; ++index)
        outputPoints[index] = toWow(straightPath.data() + index * 3);

    *outputPointCount = pointCount;
    writeError(errorBuffer, errorCapacity, "");
    return 1;
}
}
