// Offline topology/query integration test. Never attaches to or controls WoW.
#include "../src/Navigation/DetourNavigationProvider.h"
#include <iostream>
#include <stdexcept>
#include <thread>

using Navigation::DetourNavigationProvider;
using Navigation::NavPathResult;
using Navigation::NavPoint;

static void Require(bool valid, const std::string& reason)
{
    if (!valid) throw std::runtime_error(reason);
}

int main(int argc, char** argv)
{
    Debug::Logger::SetModule(nullptr);
    try
    {
        const auto directory = argc > 1 ? std::string(argv[1]) :
            DetourNavigationProvider::ResolveMmapsDirectory();
        const NavPoint start{584.566f, -2543.18f, 95.7873f};
        const NavPoint destination{330.008f, -2288.622f, 95.7873f};
        std::string error;
        DetourNavigationProvider::InvalidateSessionCache("offline_probe_start");
        DetourNavigationProvider first;
        Require(first.InitializeForRoute(directory, 1, start, destination, 2, error), error);
        const auto cold = first.CacheStats();
        Require(cold.diskLoads > 0 && cold.cacheHits == 0, "first route must be cold");
        NavPoint projected{};
        std::uint64_t reference = 0;
        Require(first.ProjectToNavMesh(start, projected, reference), "start projection failed");
        const auto originalReference = reference;

        auto route = [&](DetourNavigationProvider& provider, int margin, const char* mode) {
            Require(provider.BeginIncrementalForRoute(directory, 1, start, destination,
                margin, error, mode), error);
            auto status = DetourNavigationProvider::IncrementalStatus::Pending;
            while (status == DetourNavigationProvider::IncrementalStatus::Pending)
                status = provider.StepIncremental(error);
            Require(status == DetourNavigationProvider::IncrementalStatus::Ready, error);
            return provider.CacheStats();
        };
        DetourNavigationProvider second;
        auto warm = route(second, 2, "route");
        Require(warm.generation == cold.generation && warm.diskLoads == 0 &&
            warm.addTileCalls == 0 && warm.cacheHits == cold.loadedTotal, "warm route reloaded tiles");
        const auto expanded = route(second, 4, "expanded");
        Require(expanded.cacheHits == cold.loadedTotal &&
            expanded.diskLoads == expanded.loadedTotal - cold.loadedTotal,
            "expanded fallback must load only missing tiles");
        Require(second.Initialize(directory, 1, error), error);
        const auto full = second.CacheStats();
        Require(full.cacheHits == expanded.loadedTotal &&
            full.diskLoads == full.loadedTotal - expanded.loadedTotal, "full-map reload detected");
        Require(first.ProjectToNavMesh(start, projected, reference) &&
            reference == originalReference, "additive loads changed existing poly refs");
        first.Shutdown(); // Detach one route/query, not the topology.
        warm = route(first, 2, "route");
        Require(warm.diskLoads == 0 && warm.addTileCalls == 0 &&
            warm.loadedTotal == full.loadedTotal, "new intent did not retain topology");
        const auto warmExpanded = route(first, 4, "expanded");
        Require(warmExpanded.diskLoads == 0 && warmExpanded.addTileCalls == 0 &&
            warmExpanded.cacheHits == expanded.loadedTotal, "warm expanded rebuild");
        Require(first.Initialize(directory, 1, error), error);
        Require(first.CacheStats().diskLoads == 0 &&
            first.CacheStats().cacheHits == full.loadedTotal, "warm full-map rebuild");

        // Query-mode changes must never rebuild or destructively filter the
        // session topology. Ghost retains the old water-capable query; the
        // living query excludes water-tagged (including mixed 0x09) polys.
        const auto topologyBeforeWaterModes = first.CacheStats();
        NavPathResult ghostModeRoute{};
        first.SetWaterTraversalMode(Navigation::WaterTraversalMode::GhostDeathRecovery);
        first.FindPath(start, destination, ghostModeRoute);
        NavPathResult livingModeRoute{};
        first.SetWaterTraversalMode(Navigation::WaterTraversalMode::AvoidUntilQualified);
        first.FindPath(start, destination, livingModeRoute);
        const auto topologyAfterWaterModes = first.CacheStats();
        Require(topologyAfterWaterModes.generation == topologyBeforeWaterModes.generation &&
            topologyAfterWaterModes.loadedTotal == topologyBeforeWaterModes.loadedTotal &&
            topologyAfterWaterModes.diskLoads == topologyBeforeWaterModes.diskLoads &&
            topologyAfterWaterModes.addTileCalls == topologyBeforeWaterModes.addTileCalls,
            "water query mode changed cached topology");
        Require(!livingModeRoute.success ||
            (livingModeRoute.excludeFlags & Navigation::TerrainTransitionPolicy::WaterFlag),
            "living route lost water exclusion");

        // Find a real multi-poly local corridor, then exercise scoped hazard
        // masking concurrently with another provider's unrestricted queries.
        NavPathResult baseline{};
        NavPoint end{};
        bool found = false;
        for (float radius : {25.0f, 50.0f, 100.0f})
        {
            for (int axis = 0; axis < 4 && !found; ++axis)
            {
                end = start;
                if (axis == 0) end.x += radius;
                if (axis == 1) end.x -= radius;
                if (axis == 2) end.y += radius;
                if (axis == 3) end.y -= radius;
                found = first.FindPath(start, end, baseline) && baseline.success &&
                    !baseline.partial && baseline.corridorPolys.size() >= 3;
            }
            if (found) break;
        }
        Require(found, "no fixture corridor for hazard verification");
        const auto terrain = first.ValidateTerrainRoute(start, baseline);
        const std::vector<std::uint64_t> blocked{
            baseline.corridorPolys[baseline.corridorPolys.size() / 2]};
        bool masksApplied = false;
        std::thread masks([&] {
            for (int i = 0; i < 20; ++i)
            {
                NavPathResult avoided;
                second.FindPathAvoidingPolygons(start, end, blocked, avoided);
                masksApplied = masksApplied || avoided.avoidanceAppliedCount > 0;
            }
        });
        bool unchanged = true;
        for (int i = 0; i < 20; ++i)
        {
            NavPathResult after;
            unchanged = first.FindPath(start, end, after) && unchanged &&
                after.corridorPolys == baseline.corridorPolys;
        }
        masks.join();
        Require(masksApplied && unchanged, "temporary hazard mask leaked between queries");
        Require(first.ValidateTerrainRoute(start, baseline).valid == terrain.valid,
            "terrain validation changed after hazard query");

        DetourNavigationProvider::InvalidateSessionCache("world_unload_probe");
        bool commandCalled = false;
        Require(!first.WithCurrentTopology([&] { commandCalled = true; return true; }) &&
            !commandCalled, "stale topology dispatched a command");
        Require(!first.ProjectToNavMesh(start, projected, reference) && reference == 0,
            "stale generation projected a polygon");
        Require(!first.ValidateTerrainRoute(start, baseline).valid,
            "stale corridor validated against another generation");
        // Map switch without loading unrelated topology also revokes all old handles.
        route(second, 2, "route");
        Require(second.CacheStats().generation > cold.generation, "generation did not advance");
        DetourNavigationProvider otherMap;
        // This data installation need not contain map 0. Even a failed
        // initialization of a different map must invalidate map 1 handles.
        otherMap.BeginIncrementalFullMap(directory, 0, error);
        Require(!second.CacheGenerationCurrent(), "map 1 cache used by map 0");
        DetourNavigationProvider::InvalidateSessionCache("offline_probe_teardown");
        std::cout << "PASS real Detour cache: cold=" << cold.diskLoads
            << " expandedNew=" << expanded.diskLoads << " fullNew=" << full.diskLoads
            << " fullTotal=" << full.loadedTotal << " warmDiskLoads=0 warmAddTileCalls=0"
            << " hazardMasksRestored=yes staleGenerationsRejected=yes\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        DetourNavigationProvider::InvalidateSessionCache("offline_probe_failed");
        return 1;
    }
}
