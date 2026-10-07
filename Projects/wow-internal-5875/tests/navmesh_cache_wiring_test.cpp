#include "../src/Navigation/NavigationInitTelemetryPolicy.h"
#include "../src/Navigation/IncrementalTileInitializationPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path);
    assert(file);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main()
{
    using namespace Navigation;
    static_assert(IncrementalTileInitializationPolicy::MaxTilesPerStep == 2);
    const auto provider = Read("src/Navigation/DetourNavigationProvider.h");
    const auto follower = Read("src/Navigation/GenericNavMeshPathFollower.h");
    const auto world = Read("src/Bot/WorldMonitor.h");
    // Guards complement the executable real-Detour probe, not mock runtime success.
    assert(provider.find("session_->tiles.emplace(path") != std::string::npos);
    assert(provider.find("incrementalTiles_ = std::move(missing)") != std::string::npos);
    assert(provider.find("incrementalCursor_.Begin(incrementalTiles_.size())") != std::string::npos);
    const auto shutdown = provider.substr(provider.find("void Shutdown()"),
        provider.find("int LoadedTiles()") - provider.find("void Shutdown()"));
    assert(shutdown.find("dtFreeNavMesh(") == std::string::npos);
    assert(shutdown.find("session_.reset()") != std::string::npos);
    assert(provider.find("dtFreeNavMeshQuery(query_)") != std::string::npos);
    assert(provider.find("~CachedTopology() { if (mesh) dtFreeNavMesh(mesh); }") != std::string::npos);
    assert(provider.find("path.meshGeneration != session_->generation") != std::string::npos);
    assert(provider.find("~FlagRestoration() { Restore(); }") != std::string::npos);
    assert(provider.find("restoration.Restore();") != std::string::npos);
    assert(provider.find("tile_metadata_or_reference_changed") != std::string::npos);
    assert(provider.find("incompatible_mesh_metadata") != std::string::npos);
    assert(world.find("world_unload_or_snapshot_gap") != std::string::npos);
    assert(world.find("~NavMeshSessionLifetime()") != std::string::npos);
    assert(world.find("session_teardown") != std::string::npos);
    assert(follower.find("provider_.WithCurrentTopology([&]") != std::string::npos);
    assert(follower.find("RejectInvalidatedTopology()") != std::string::npos);
    assert(follower.find("SameNavMeshGeneration(options.initialTransitionMeshGeneration") != std::string::npos);
    const auto death = Read("src/Bot/DeathRecoveryController.h");
    assert(death.find("SameNavMeshGeneration(priorGeneration, evidence.meshGeneration)") != std::string::npos);
    assert(death.find("lastRouteEvidence_.meshGeneration,") != std::string::npos);
    assert(death.find("ghostConfirmedThisRecovery_\n") != std::string::npos);
    assert(follower.find("MaximumPathLength =\n            2000.0f") != std::string::npos);
    assert(follower.find("MaximumSurfaceRecoveryAttempts = 4") != std::string::npos);
    assert(follower.find("MaximumLastSafeBacktracks = 2") != std::string::npos);
    assert(follower.find("HardCellCenters") != std::string::npos);
    assert(follower.find("provider_.ValidateTerrainRoute") != std::string::npos);
    assert(NavigationInitTelemetryPolicy::ReasonName(NavigationPlanFailure::MeshCacheInvalidated)
        == std::string("mesh_cache_invalidated"));
    // Mandatory/optional fallback decision remains exactly the existing policy.
    assert(NavigationInitTelemetryPolicy::NextTier(NavigationInitTier::Expanded, true)
        == NavigationInitTier::FullMap);
    assert(NavigationInitTelemetryPolicy::NextTier(NavigationInitTier::Expanded, false)
        == NavigationInitTier::None);
}
