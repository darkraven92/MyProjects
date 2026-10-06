// Offline, read-only inspection of the exact VMaNGOS .mmap/.mmtile format
// consumed by DetourNavigationProvider. This tool never loads or contacts WoW.
#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourStatus.h"
#include "../src/Navigation/TerrainTransitionPolicy.h"
#include "NavmeshAuditGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float GridSize = 533.333333f;
constexpr float GridOrigin = 32.0f * GridSize;
constexpr unsigned short GroundFlag =
    Navigation::TerrainTransitionPolicy::GroundFlag;
constexpr unsigned short SteepFlag =
    Navigation::TerrainTransitionPolicy::SteepFlag;
constexpr std::uint32_t MmapMagic = 0x4D4D4150;
constexpr std::uint32_t MmapVersion = 6;
constexpr int MaximumPolygons = 512;
constexpr int MaximumStraightPoints = 512;
constexpr int NodePoolSize = 65535;

struct Point { float x = 0, y = 0, z = 0; };
struct TileHeader
{
    std::uint32_t magic, detourVersion, mmapVersion, size, usesLiquids;
};
static_assert(sizeof(TileHeader) == 20);

struct Options
{
    int map = -1;
    Point player{};
    std::optional<Point> destination;
    std::optional<Point> observedPortal;
    float radius = 200;
    std::filesystem::path mmaps;
    std::filesystem::path out = "debug/navmesh";
    bool haveX = false, haveY = false, haveZ = false;
};

std::string Ref(dtPolyRef ref)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << ref;
    return stream.str();
}

int Tile(float value)
{
    return static_cast<int>(std::floor((GridOrigin - value) / GridSize));
}

Point Wow(const float* detour) { return {detour[2], detour[0], detour[1]}; }
std::array<float, 3> Detour(Point wow) { return {wow.y, wow.z, wow.x}; }
float Dist2D(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
float Dist3D(Point a, Point b)
{
    return std::hypot(Dist2D(a, b), a.z - b.z);
}

bool ParseFloat(const std::string& value, float& out)
{
    try
    {
        std::size_t used = 0;
        out = std::stof(value, &used);
        return used == value.size() && std::isfinite(out);
    }
    catch (...) { return false; }
}

bool Parse(int argc, char** argv, Options& options)
{
    bool toX = false, toY = false, toZ = false;
    bool portalX = false, portalY = false, portalZ = false;
    Point destination{}, portal{};
    for (int i = 1; i < argc; i += 2)
    {
        if (i + 1 >= argc) return false;
        const std::string key = argv[i], value = argv[i + 1];
        if (key == "--map")
        {
            float parsed = 0;
            if (!ParseFloat(value, parsed) || parsed < 0 || parsed > 999 ||
                std::floor(parsed) != parsed) return false;
            options.map = static_cast<int>(parsed);
        }
        else if (key == "--x") options.haveX = ParseFloat(value, options.player.x);
        else if (key == "--y") options.haveY = ParseFloat(value, options.player.y);
        else if (key == "--z") options.haveZ = ParseFloat(value, options.player.z);
        else if (key == "--to-x") toX = ParseFloat(value, destination.x);
        else if (key == "--to-y") toY = ParseFloat(value, destination.y);
        else if (key == "--to-z") toZ = ParseFloat(value, destination.z);
        else if (key == "--portal-x") portalX = ParseFloat(value, portal.x);
        else if (key == "--portal-y") portalY = ParseFloat(value, portal.y);
        else if (key == "--portal-z") portalZ = ParseFloat(value, portal.z);
        else if (key == "--radius")
        {
            if (!ParseFloat(value, options.radius) || options.radius < 1 ||
                options.radius > 250) return false;
        }
        else if (key == "--mmaps") options.mmaps = value;
        else if (key == "--out") options.out = value;
        else return false;
    }
    if (toX || toY || toZ)
    {
        if (!(toX && toY && toZ)) return false;
        options.destination = destination;
    }
    if (portalX || portalY || portalZ)
    {
        if (!(portalX && portalY && portalZ)) return false;
        options.observedPortal = portal;
    }
    return options.map >= 0 && options.haveX && options.haveY &&
        options.haveZ && !options.mmaps.empty() && !options.out.empty();
}

std::string TileName(int map, int wowXTile, int wowYTile)
{
    std::ostringstream name;
    name << std::setfill('0') << std::setw(3) << map << std::setw(2)
         << wowXTile << std::setw(2) << wowYTile << ".mmtile";
    return name.str();
}

bool LoadTile(dtNavMesh& mesh, const std::filesystem::path& file,
              std::ostream& report)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) return false;
    TileHeader header{};
    if (!input.read(reinterpret_cast<char*>(&header), sizeof(header)) ||
        header.magic != MmapMagic || header.detourVersion != DT_NAVMESH_VERSION ||
        header.mmapVersion != MmapVersion || header.size == 0 ||
        header.size > 64u * 1024u * 1024u)
    {
        report << "invalid_tile " << file << '\n';
        return false;
    }
    auto* data = static_cast<unsigned char*>(dtAlloc(header.size, DT_ALLOC_PERM));
    if (!data) return false;
    if (!input.read(reinterpret_cast<char*>(data), header.size))
    {
        dtFree(data);
        report << "truncated_tile " << file << '\n';
        return false;
    }
    const dtStatus status = mesh.addTile(data, header.size, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(status))
    {
        dtFree(data);
        report << "add_tile_failed " << file << " status=" << status << '\n';
        return false;
    }
    report << "loaded_tile " << file.filename().string() << '\n';
    return true;
}

struct PolyView
{
    const dtMeshTile* tile = nullptr;
    const dtPoly* poly = nullptr;
    Point center{};
    float minZ = 0, maxZ = 0;
};

std::optional<PolyView> View(const dtNavMesh& mesh, dtPolyRef ref)
{
    const dtMeshTile* tile = nullptr;
    const dtPoly* poly = nullptr;
    if (!ref || dtStatusFailed(mesh.getTileAndPolyByRef(ref, &tile, &poly)) ||
        poly->vertCount == 0) return std::nullopt;
    PolyView view{tile, poly};
    view.minZ = std::numeric_limits<float>::infinity();
    view.maxZ = -view.minZ;
    for (unsigned i = 0; i < poly->vertCount; ++i)
    {
        const Point p = Wow(&tile->verts[poly->verts[i] * 3]);
        view.center.x += p.x;
        view.center.y += p.y;
        view.center.z += p.z;
        view.minZ = std::min(view.minZ, p.z);
        view.maxZ = std::max(view.maxZ, p.z);
    }
    const float n = static_cast<float>(poly->vertCount);
    view.center.x /= n; view.center.y /= n; view.center.z /= n;
    return view;
}

struct Portal
{
    bool linked = false;
    Point a{}, b{};
    unsigned edge = 0, side = 0, bmin = 0, bmax = 0;
};

Portal SharedPortal(const dtNavMesh& mesh, dtPolyRef from, dtPolyRef to)
{
    Portal result{};
    const auto view = View(mesh, from);
    if (!view) return result;
    for (unsigned linkIndex = view->poly->firstLink; linkIndex != DT_NULL_LINK;
         linkIndex = view->tile->links[linkIndex].next)
    {
        const dtLink& link = view->tile->links[linkIndex];
        if (link.ref != to || link.edge >= view->poly->vertCount) continue;
        const unsigned next = (link.edge + 1) % view->poly->vertCount;
        result.a = Wow(&view->tile->verts[view->poly->verts[link.edge] * 3]);
        result.b = Wow(&view->tile->verts[view->poly->verts[next] * 3]);
        result.edge = link.edge;
        result.side = link.side;
        result.bmin = link.bmin;
        result.bmax = link.bmax;
        const auto clipped = NavmeshAudit::ClipPortal(
            result.a, result.b, result.side, result.bmin, result.bmax);
        if (!clipped) return result;
        result.a = clipped->first;
        result.b = clipped->second;
        result.linked = true;
        return result;
    }
    return result;
}

std::string PointText(Point p)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << p.x << ',' << p.y << ',' << p.z;
    return out.str();
}

void EmitObjPoly(std::ostream& obj, const PolyView& view)
{
    const unsigned n = view.poly->vertCount;
    if (n < 3) return;
    // OBJ Y is vertical; WoW Z is vertical.
    for (unsigned i = 0; i < n; ++i)
    {
        const Point p = Wow(&view.tile->verts[view.poly->verts[i] * 3]);
        obj << "v " << p.x << ' ' << p.z << ' ' << p.y << '\n';
    }
    for (unsigned i = 1; i + 1 < n; ++i)
        obj << "f -" << n << " -" << n - i << " -" << n - i - 1 << '\n';
}

struct Route
{
    std::vector<dtPolyRef> polys;
    std::vector<Point> straight;
    std::vector<dtPolyRef> straightPolys;
    dtStatus status = 0;
    bool reachedEnd = false;
};

Route Query(dtNavMeshQuery& query, Point start, Point end,
            unsigned short excluded, std::ostream& report)
{
    Route route{};
    const auto startD = Detour(start), endD = Detour(end);
    const float extents[3] = {5, 10, 5}; // runtime FindPathWithFlags
    dtQueryFilter filter;
    filter.setIncludeFlags(GroundFlag);
    filter.setExcludeFlags(excluded);
    dtPolyRef startRef = 0, endRef = 0;
    float projectedStart[3]{}, projectedEnd[3]{};
    query.findNearestPoly(startD.data(), extents, &filter,
                          &startRef, projectedStart);
    query.findNearestPoly(endD.data(), extents, &filter,
                          &endRef, projectedEnd);
    report << "query exclude=" << excluded << " startRef=" << Ref(startRef)
           << " endRef=" << Ref(endRef) << " startProjection="
           << PointText(Wow(projectedStart)) << " endProjection="
           << PointText(Wow(projectedEnd)) << '\n';
    if (!startRef || !endRef) return route;
    std::array<dtPolyRef, MaximumPolygons> refs{};
    int count = 0;
    route.status = query.findPath(startRef, endRef, projectedStart,
        projectedEnd, &filter, refs.data(), &count, MaximumPolygons);
    if (dtStatusFailed(route.status) || count <= 0) return route;
    route.polys.assign(refs.begin(), refs.begin() + count);
    route.reachedEnd = refs[count - 1] == endRef &&
        !(route.status & DT_PARTIAL_RESULT);
    float straightEnd[3] = {
        projectedEnd[0], projectedEnd[1], projectedEnd[2]};
    if (!route.reachedEnd)
        query.closestPointOnPoly(refs[count - 1], projectedEnd,
                                 straightEnd, nullptr);
    std::array<float, MaximumStraightPoints * 3> rawStraight{};
    std::array<unsigned char, MaximumStraightPoints> straightFlags{};
    std::array<dtPolyRef, MaximumStraightPoints> straightRefs{};
    int straightCount = 0;
    const dtStatus straightStatus = query.findStraightPath(
        projectedStart, straightEnd, refs.data(), count,
        rawStraight.data(), straightFlags.data(), straightRefs.data(),
        &straightCount, MaximumStraightPoints, DT_STRAIGHTPATH_ALL_CROSSINGS);
    if (dtStatusSucceed(straightStatus))
        for (int i = 0; i < straightCount; ++i)
        {
            route.straight.push_back(Wow(&rawStraight[i * 3]));
            route.straightPolys.push_back(straightRefs[i]);
        }
    report << "route exclude=" << excluded << " polygons=" << count
           << " reachedEnd=" << (route.reachedEnd ? "yes" : "no")
           << " partial=" << ((route.status & DT_PARTIAL_RESULT) ? "yes" : "no")
           << " capacityHit=" << (count == MaximumPolygons ? "yes" : "no")
           << " straightPoints=" << route.straight.size() << '\n';
    return route;
}

// Read-only evidence, not a new projection/steering policy. In particular,
// a clear 2D ray is never reported here as proof of matching height layers.
void AuditProjections(dtNavMeshQuery& query, const dtNavMesh& mesh,
                      Point point, const char* label, const Route& route,
                      std::ostream& out)
{
    const auto position = Detour(point);
    struct Scope { const char* name; float horizontal, vertical; unsigned short flags; };
    const Scope scopes[] = {{"route_ground", 5, 10, GroundFlag},
                            {"route_water_fallback", 5, 10, 0x09},
                            {"local_surface", 2, 4, 0x09}};
    for (const auto& scope : scopes)
    {
        const float extents[] = {scope.horizontal, scope.vertical, scope.horizontal};
        dtQueryFilter filter;
        filter.setIncludeFlags(scope.flags);
        filter.setExcludeFlags(0);
        dtPolyRef selected = 0;
        float nearest[3]{};
        const auto nearestStatus = query.findNearestPoly(
            position.data(), extents, &filter, &selected, nearest);
        std::array<dtPolyRef, 64> refs{};
        int count = 0;
        const auto status = query.queryPolygons(position.data(), extents,
            &filter, refs.data(), &count, static_cast<int>(refs.size()));
        out << "projectionProbe label=" << label << " scope=" << scope.name
            << " position=" << PointText(point) << " status=" << status
            << " nearestStatus=" << nearestStatus << " candidates=" << count
            << " truncated=" << ((status & DT_BUFFER_TOO_SMALL) ? "yes" : "no")
            << " selected=" << Ref(selected) << '\n';
        if (dtStatusFailed(status)) continue;
        std::sort(refs.begin(), refs.begin() + count);
        for (int i = 0; i < count; ++i)
        {
            const auto view = View(mesh, refs[i]);
            float closest[3]{};
            bool over = false;
            if (!view || dtStatusFailed(query.closestPointOnPoly(
                    refs[i], position.data(), closest, &over))) continue;
            const auto projected = Wow(closest);
            const auto inCorridor = std::find(route.polys.begin(), route.polys.end(), refs[i]);
            out << "projectionCandidate label=" << label << " scope=" << scope.name
                << " poly=" << Ref(refs[i]) << " selected=" << (refs[i] == selected)
                << " overPoly=" << over << " projected=" << PointText(projected)
                << " horizontal=" << Dist2D(point, projected)
                << " deltaZ=" << projected.z - point.z
                << " flags=" << view->poly->flags
                << " area=" << static_cast<unsigned>(view->poly->getArea())
                << " tileLayer=" << view->tile->header->layer
                << " corridorIndex=" << (inCorridor == route.polys.end() ? -1 :
                    static_cast<int>(inCorridor - route.polys.begin())) << '\n';
        }
    }
}

void AuditLocalRay(dtNavMeshQuery& query, Point start, Point destination,
                   std::size_t index, std::ostream& out)
{
    const auto from = Detour(start), to = Detour(destination);
    const float extents[] = {2, 4, 2}; // runtime IsSurfaceSegmentReachable
    dtQueryFilter filter;
    filter.setIncludeFlags(0x09);
    dtPolyRef startRef = 0;
    float projected[3]{};
    const auto nearestStatus = query.findNearestPoly(
        from.data(), extents, &filter, &startRef, projected);
    if (dtStatusFailed(nearestStatus) || !startRef)
    {
        out << "localRay index=" << index << " result=no_start_projection\n";
        return;
    }
    float t = 0, normal[3]{};
    dtPolyRef visited[64]{};
    int count = 0;
    const auto status = query.raycast(startRef, projected, to.data(), &filter,
        &t, normal, visited, &count, 64);
    float endHeight = 0;
    const bool heightKnown = dtStatusSucceed(status) && count > 0 && t >= 1 &&
        !(status & DT_BUFFER_TOO_SMALL) && dtStatusSucceed(
            query.getPolyHeight(visited[count - 1], to.data(), &endHeight));
    out << "localRay index=" << index << " from=" << PointText(start)
        << " target=" << PointText(destination) << " startPoly=" << Ref(startRef)
        << " status=" << status << " fraction=" << std::clamp(t, 0.0f, 1.0f)
        << " visited=" << count << " lastPoly=" << Ref(count ? visited[count - 1] : 0)
        << " heightKnown=" << heightKnown;
    if (heightKnown) out << " surfaceZ=" << endHeight
                         << " targetMinusSurfaceZ=" << destination.z - endHeight;
    out << " interpretation=horizontal_raycast_only\n";
}

void AuditCorridor(dtNavMeshQuery& query, const dtNavMesh& mesh,
                   const Route& route, std::ostream& out)
{
    out << "index,from,to,linked,width2D,portalA,portalB,fromZ,toZ,flagsFrom,flagsTo,areaFrom,areaTo,midpointClearanceKnown,midpointClearance\n";
    dtQueryFilter filter;
    filter.setIncludeFlags(0x09);
    for (std::size_t i = 1; i < route.polys.size(); ++i)
    {
        const auto from = View(mesh, route.polys[i - 1]), to = View(mesh, route.polys[i]);
        if (!from || !to) continue;
        const auto portal = SharedPortal(mesh, route.polys[i - 1], route.polys[i]);
        const Point midpoint{(portal.a.x + portal.b.x) / 2,
                             (portal.a.y + portal.b.y) / 2,
                             (portal.a.z + portal.b.z) / 2};
        const auto probe = Detour(midpoint);
        float distance = 0, hit[3]{}, normal[3]{};
        const bool known = portal.linked && dtStatusSucceed(query.findDistanceToWall(
            route.polys[i - 1], probe.data(), 3, &filter, &distance, hit, normal));
        out << i << ',' << Ref(route.polys[i - 1]) << ',' << Ref(route.polys[i])
            << ',' << portal.linked << ',' << Dist2D(portal.a, portal.b)
            << ",\"" << PointText(portal.a) << "\",\"" << PointText(portal.b)
            << "\"," << from->center.z << ',' << to->center.z
            << ',' << from->poly->flags << ',' << to->poly->flags
            << ',' << static_cast<unsigned>(from->poly->getArea())
            << ',' << static_cast<unsigned>(to->poly->getArea()) << ',' << known << ',';
        if (known) out << distance;
        out << '\n';
    }
}
} // namespace

int main(int argc, char** argv)
{
    Options options{};
    if (!Parse(argc, argv, options))
    {
        std::cerr << "Usage: navmesh_debug --map ID --x X --y Y --z Z "
            "--mmaps DIR [--radius 1..250] [--to-x X --to-y Y --to-z Z] "
            "[--portal-x X --portal-y Y --portal-z Z] [--out DIR]\n";
        return 2;
    }
    std::error_code ec;
    std::filesystem::create_directories(options.out, ec);
    if (ec) { std::cerr << ec.message() << '\n'; return 1; }
    std::ofstream report(options.out / "report.txt");
    const auto mmap = options.mmaps /
        ([&] { std::ostringstream n; n << std::setfill('0') << std::setw(3)
             << options.map << ".mmap"; return n.str(); }());
    dtNavMeshParams params{};
    std::ifstream mapInput(mmap, std::ios::binary);
    if (!mapInput.read(reinterpret_cast<char*>(&params), sizeof(params)))
    { std::cerr << "Cannot read " << mmap << '\n'; return 1; }
    std::unique_ptr<dtNavMesh, decltype(&dtFreeNavMesh)> mesh(
        dtAllocNavMesh(), &dtFreeNavMesh);
    if (!mesh || dtStatusFailed(mesh->init(&params)))
    { std::cerr << "Detour map init failed\n"; return 1; }
    report << "mmap=" << mmap << " player=" << PointText(options.player)
           << " radius=" << options.radius << '\n';
    const Point destination = options.destination.value_or(options.player);
    const float minX = std::min(options.player.x - options.radius, destination.x);
    const float maxX = std::max(options.player.x + options.radius, destination.x);
    const float minY = std::min(options.player.y - options.radius, destination.y);
    const float maxY = std::max(options.player.y + options.radius, destination.y);
    const int tileYMin = std::max(0, Tile(maxX) - 1);
    const int tileYMax = std::min(63, Tile(minX) + 1);
    const int tileXMin = std::max(0, Tile(maxY) - 1);
    const int tileXMax = std::min(63, Tile(minY) + 1);
    if ((tileYMax - tileYMin + 1) * (tileXMax - tileXMin + 1) > 36)
    { std::cerr << "Route tile window exceeds 36 tiles\n"; return 1; }
    int loaded = 0;
    for (int tileY = tileYMin; tileY <= tileYMax; ++tileY)
        for (int tileX = tileXMin; tileX <= tileXMax; ++tileX)
            loaded += LoadTile(*mesh,
                options.mmaps / TileName(options.map, tileY, tileX), report);
    report << "playerADT_WoWX=" << Tile(options.player.x)
           << " WoWY=" << Tile(options.player.y)
           << " file=" << TileName(options.map, Tile(options.player.x),
                                    Tile(options.player.y))
           << " loadedTiles=" << loaded << '\n';
    std::unique_ptr<dtNavMeshQuery, decltype(&dtFreeNavMeshQuery)> query(
        dtAllocNavMeshQuery(), &dtFreeNavMeshQuery);
    if (!query || dtStatusFailed(query->init(mesh.get(), NodePoolSize)))
    { std::cerr << "Detour query init failed\n"; return 1; }
    const auto playerD = Detour(options.player);
    const float extents[3] = {5, 10, 5};
    dtQueryFilter filter;
    filter.setIncludeFlags(GroundFlag);
    filter.setExcludeFlags(0);
    dtPolyRef playerRef = 0;
    float projected[3]{};
    query->findNearestPoly(playerD.data(), extents, &filter,
                           &playerRef, projected);
    report << "playerPoly=" << Ref(playerRef)
           << " projection=" << PointText(Wow(projected)) << '\n';
    if (const auto playerPoly = View(*mesh, playerRef))
    {
        report << "playerPolyTile=" << playerPoly->tile->header->x << ','
               << playerPoly->tile->header->y << ','
               << playerPoly->tile->header->layer
               << " center=" << PointText(playerPoly->center)
               << " zRange=" << playerPoly->minZ << ',' << playerPoly->maxZ
               << " flags=" << playerPoly->poly->flags
               << " tileWalkableHeight=" << playerPoly->tile->header->walkableHeight
               << " tileWalkableRadius=" << playerPoly->tile->header->walkableRadius
               << " tileWalkableClimb=" << playerPoly->tile->header->walkableClimb
               << " vertices=";
        for (unsigned j = 0; j < playerPoly->poly->vertCount; ++j)
            report << ' ' << PointText(Wow(&playerPoly->tile->verts[
                playerPoly->poly->verts[j] * 3]));
        report << '\n';
    }
    Route route{}, safeRoute{};
    if (options.destination)
    {
        route = Query(*query, options.player, *options.destination, 0, report);
        safeRoute = Query(*query, options.player, *options.destination,
                          SteepFlag, report);
    }

    std::ofstream polygons(options.out / "polygons.csv");
    std::ofstream adjacency(options.out / "adjacency.csv");
    std::ofstream corridor(options.out / "corridor.csv");
    std::ofstream safeCorridor(options.out / "nonsteep_corridor.csv");
    std::ofstream straight(options.out / "straight.csv");
    std::ofstream safeStraight(options.out / "nonsteep_straight.csv");
    std::ofstream obj(options.out / "local.obj");
    polygons << "ref,tileX,tileY,layer,area,flags,cx,cy,cz,minZ,maxZ,vertices\n";
    adjacency << "from,to,edge,side,bmin,bmax,fromTileX,fromTileY,toTileX,toTileY,portalA,portalB,centerHorizontal,centerDeltaZ,centerRiseRun\n";
    corridor << "index,from,to,fromCenter,toCenter,horizontal,deltaZ,riseRun,portalA,portalB,linked,fromFlags,toFlags\n";
    safeCorridor << "index,from,to,fromCenter,toCenter,horizontal,deltaZ,riseRun,portalA,portalB,linked,fromFlags,toFlags\n";
    straight << "index,point,enteredPoly\n";
    safeStraight << "index,point,enteredPoly\n";
    obj << "# OBJ coordinate order: X=WoW X, Y=WoW Z (up), Z=WoW Y\n"
           "g local_polygons\n";
    std::size_t localCount = 0;
    const dtNavMesh& constMesh = *mesh;
    for (int i = 0; i < mesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = constMesh.getTile(i);
        if (!tile || !tile->header) continue;
        const dtPolyRef base = mesh->getPolyRefBase(tile);
        for (int j = 0; j < tile->header->polyCount; ++j)
        {
            const dtPolyRef ref = base | static_cast<dtPolyRef>(j);
            const auto view = View(*mesh, ref);
            if (!view || view->poly->getType() != DT_POLYTYPE_GROUND ||
                Dist2D(options.player, view->center) > options.radius) continue;
            ++localCount;
            polygons << Ref(ref) << ',' << tile->header->x << ','
                     << tile->header->y << ',' << tile->header->layer << ','
                     << static_cast<int>(view->poly->getArea()) << ','
                     << view->poly->flags << ',' << view->center.x << ','
                     << view->center.y << ',' << view->center.z << ','
                     << view->minZ << ',' << view->maxZ << ",\"";
            for (unsigned v = 0; v < view->poly->vertCount; ++v)
            {
                if (v) polygons << ';';
                polygons << PointText(Wow(&tile->verts[
                    view->poly->verts[v] * 3]));
            }
            polygons << "\"\n";
            EmitObjPoly(obj, *view);
            for (unsigned linkIndex = view->poly->firstLink;
                 linkIndex != DT_NULL_LINK;
                 linkIndex = tile->links[linkIndex].next)
            {
                const dtPolyRef to = tile->links[linkIndex].ref;
                const auto other = View(*mesh, to);
                if (!other) continue;
                const Portal portal = SharedPortal(*mesh, ref, to);
                const float horizontal = Dist2D(view->center, other->center);
                const float dz = other->center.z - view->center.z;
                adjacency << Ref(ref) << ',' << Ref(to) << ',' << portal.edge
                          << ',' << portal.side << ',' << portal.bmin << ','
                          << portal.bmax << ',' << tile->header->x << ','
                          << tile->header->y << ',' << other->tile->header->x
                          << ',' << other->tile->header->y << ",\""
                          << PointText(portal.a) << "\",\""
                          << PointText(portal.b) << "\"," << horizontal
                          << ',' << dz << ',' << (horizontal > 0 ? dz / horizontal : 0)
                          << '\n';
            }
        }
    }
    report << "localPolygons=" << localCount << '\n';
    obj << "g observed_corridor\n";
    std::size_t suspiciousIndex = 0;
    float bestPortalDistance = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < route.polys.size(); ++i)
    {
        const auto current = View(*mesh, route.polys[i]);
        if (current && Dist2D(options.player, current->center) <= options.radius)
            EmitObjPoly(obj, *current);
        if (i == 0 || !current) continue;
        const auto previous = View(*mesh, route.polys[i - 1]);
        if (!previous) continue;
        const Portal portal = SharedPortal(*mesh, route.polys[i - 1], route.polys[i]);
        const float horizontal = Dist2D(previous->center, current->center);
        const float dz = current->center.z - previous->center.z;
        corridor << i << ',' << Ref(route.polys[i - 1]) << ','
                 << Ref(route.polys[i]) << ",\""
                 << PointText(previous->center) << "\",\""
                 << PointText(current->center) << "\"," << horizontal << ','
                 << dz << ',' << (horizontal > 0 ? dz / horizontal : 0)
                 << ",\"" << PointText(portal.a) << "\",\""
                 << PointText(portal.b) << "\"," << (portal.linked ? 1 : 0)
                 << ',' << previous->poly->flags << ',' << current->poly->flags
                 << '\n';
        if (options.observedPortal && portal.linked)
        {
            const Point midpoint{(portal.a.x + portal.b.x) / 2,
                                 (portal.a.y + portal.b.y) / 2,
                                 (portal.a.z + portal.b.z) / 2};
            const float distance = std::min({
                Dist3D(portal.a, *options.observedPortal),
                Dist3D(portal.b, *options.observedPortal),
                Dist3D(midpoint, *options.observedPortal)});
            if (distance < bestPortalDistance)
            { bestPortalDistance = distance; suspiciousIndex = i; }
        }
    }
    for (std::size_t i = 0; i < route.straight.size(); ++i)
        straight << i << ",\"" << PointText(route.straight[i]) << "\","
                 << Ref(route.straightPolys[i]) << '\n';
    obj << "g nonsteep_corridor\n";
    for (std::size_t i = 0; i < safeRoute.polys.size(); ++i)
    {
        const auto current = View(*mesh, safeRoute.polys[i]);
        if (current && Dist2D(options.player, current->center) <= options.radius)
            EmitObjPoly(obj, *current);
        if (i == 0 || !current) continue;
        const auto previous = View(*mesh, safeRoute.polys[i - 1]);
        if (!previous) continue;
        const Portal portal = SharedPortal(*mesh, safeRoute.polys[i - 1],
                                           safeRoute.polys[i]);
        const float horizontal = Dist2D(previous->center, current->center);
        const float dz = current->center.z - previous->center.z;
        safeCorridor << i << ',' << Ref(safeRoute.polys[i - 1]) << ','
                     << Ref(safeRoute.polys[i]) << ",\""
                     << PointText(previous->center) << "\",\""
                     << PointText(current->center) << "\"," << horizontal
                     << ',' << dz << ','
                     << (horizontal > 0 ? dz / horizontal : 0)
                     << ",\"" << PointText(portal.a) << "\",\""
                     << PointText(portal.b) << "\"," << (portal.linked ? 1 : 0)
                     << ',' << previous->poly->flags << ','
                     << current->poly->flags << '\n';
    }
    for (std::size_t i = 0; i < safeRoute.straight.size(); ++i)
        safeStraight << i << ",\"" << PointText(safeRoute.straight[i])
                     << "\"," << Ref(safeRoute.straightPolys[i]) << '\n';
    if (options.observedPortal && suspiciousIndex > 0)
    {
        report << "closestCorridorPortalIndex=" << suspiciousIndex
               << " portalDistance3D=" << bestPortalDistance
               << " from=" << Ref(route.polys[suspiciousIndex - 1])
               << " to=" << Ref(route.polys[suspiciousIndex]) << '\n';
        obj << "g suspicious_transition\n";
        for (const std::size_t index : {suspiciousIndex - 1, suspiciousIndex})
            if (const auto view = View(*mesh, route.polys[index]))
                EmitObjPoly(obj, *view);
    }
    report << "standardCorridor=";
    for (dtPolyRef ref : route.polys) report << ' ' << Ref(ref);
    report << "\nnonSteepCorridor=";
    for (dtPolyRef ref : safeRoute.polys) report << ' ' << Ref(ref);
    report << '\n';
    const auto straightLength = [](const Route& selected)
    {
        float length = 0;
        for (std::size_t i = 1; i < selected.straight.size(); ++i)
            length += Dist3D(selected.straight[i - 1], selected.straight[i]);
        return length;
    };
    const auto steepCount = [&](const Route& selected)
    {
        std::size_t count = 0;
        for (dtPolyRef ref : selected.polys)
            if (const auto view = View(*mesh, ref))
                count += (view->poly->flags & SteepFlag) != 0;
        return count;
    };
    report << "standardStraightLength3D=" << straightLength(route)
           << " steepPolygons=" << steepCount(route) << '\n';
    report << "nonSteepStraightLength3D=" << straightLength(safeRoute)
           << " steepPolygons=" << steepCount(safeRoute) << '\n';
    if (options.destination)
    {
        const bool safeQueryComplete = Navigation::TerrainTransitionPolicy::Complete(
            !safeRoute.polys.empty(), safeRoute.reachedEnd,
            (safeRoute.status & DT_PARTIAL_RESULT) != 0);
        const bool projectionIdentityMatches = safeQueryComplete &&
            route.reachedEnd && !route.polys.empty() &&
            Navigation::TerrainTransitionPolicy::SameEndpointPolygons(
                safeRoute.polys.front(), safeRoute.polys.back(),
                route.polys.front(), route.polys.back());
        const bool safeComplete = safeQueryComplete &&
            projectionIdentityMatches;
        const Route& selected = safeComplete ? safeRoute : route;
        report << "auditScope=offline_static_geometry live_position_verified=no "
                  "persistent_hazards_replayed=no steering_insets_replayed=no\n";
        AuditProjections(*query, *mesh, options.player, "player", selected, report);
        AuditProjections(*query, *mesh, *options.destination, "destination", selected, report);
        if (options.observedPortal)
            AuditProjections(*query, *mesh, *options.observedPortal, "observed_portal", selected, report);
        std::ofstream contract(options.out / "corridor_contract.csv");
        AuditCorridor(*query, *mesh, selected, contract);
        // First local candidates only; do not imply that a ray from the
        // original player position should reach the entire distant route.
        for (std::size_t i = 1; i < selected.straight.size() && i <= 8; ++i)
            AuditLocalRay(*query, options.player, selected.straight[i], i, report);
        std::size_t firstUnsafeSteepCorridor = 0;
        if (!safeComplete)
            for (std::size_t i = 1; i < selected.polys.size(); ++i)
            {
                const auto from = View(*mesh, selected.polys[i - 1]);
                const auto to = View(*mesh, selected.polys[i]);
                if (from && to &&
                    Navigation::TerrainTransitionPolicy::UsesSteep(
                        from->poly->flags | to->poly->flags) &&
                    Navigation::TerrainTransitionPolicy::Assess(
                        from->center, to->center).rejected)
                {
                    firstUnsafeSteepCorridor = i;
                    break;
                }
            }
        std::size_t firstUnsafe = 0;
        for (std::size_t i = 1; i < selected.straight.size(); ++i)
        {
            const Point from = i == 1 ? options.player : selected.straight[i - 1];
            if (Navigation::TerrainTransitionPolicy::Assess(
                    from, selected.straight[i]).rejected)
            {
                firstUnsafe = i;
                break;
            }
        }
        report << "14O1PreferredSelection="
               << (safeComplete ? "non_steep" : "steep_enabled_fallback")
               << " projectionIdentityMatch="
               << (projectionIdentityMatches ? "yes" : "no")
               << " selectedPolygons=" << selected.polys.size()
               << " selectedSteepPolygons=" << steepCount(selected)
               << " firstUnsafeSteepCorridor=" << firstUnsafeSteepCorridor
               << " firstUnsafeStraightSegment=" << firstUnsafe << '\n';
    }
    std::cout << "Offline navmesh inspection: " << options.out << '\n';
    std::cout << "player poly " << Ref(playerRef) << ", route "
              << route.polys.size() << " polys, non-steep route "
              << safeRoute.polys.size() << " polys\n";
    return 0;
}
