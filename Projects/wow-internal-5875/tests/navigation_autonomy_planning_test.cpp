#include "../src/Bot/AutonomySupervisor.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using namespace Bot;
    AutonomySample s{};
    s.activity = AutonomyActivity::Movement;
    s.identity = 4;
    s.navigationInitialization = {true, 1, Navigation::NavigationInitTier::Route, 0, 25};
    AutonomySupervisor supervisor;
    // Reproduce stationary Roaming while tiles actually advance. Scoped ->
    // expanded -> full-map retain one intent, not fresh movement budgets.
    for (std::uint64_t tick = 0; tick < 180; ++tick)
    {
        s.monotonicMs = tick * 250;
        s.navigationInitialization.tier = tick < 13 ? Navigation::NavigationInitTier::Route
            : (tick < 60 ? Navigation::NavigationInitTier::Expanded
                         : Navigation::NavigationInitTier::FullMap);
        s.navigationInitialization.tilesProcessed = tick < 13 ? tick * 2
            : (tick < 60 ? (tick - 13) * 2 : (tick - 60) * 2);
        s.navigationInitialization.tilesTotal = tick < 13 ? 25 : (tick < 60 ? 92 : 704);
        assert(supervisor.Update(s, tick).kind == AutonomyEventKind::None);
    }
    assert(supervisor.HardEvents() == 0);
    assert(supervisor.SoftEvents() == 0);
    // Completion isn't movement success: one unchanged 48-tick execution
    // window is granted, then real no-displacement still hard-stalls.
    s.navigationInitialization.pending = false;
    assert(supervisor.Update(s, 180).kind == AutonomyEventKind::None);
    assert(supervisor.Update(s, 203).kind == AutonomyEventKind::None);
    assert(supervisor.Update(s, 204).kind == AutonomyEventKind::SoftStall);
    assert(supervisor.Update(s, 228).kind == AutonomyEventKind::HardStall);

    // A boolean pending flag or repeated route/tier churn cannot exempt a
    // frozen loader indefinitely. CTM/replan dispatch is not tile progress.
    AutonomySupervisor frozen;
    s.navigationInitialization = {true, 2, Navigation::NavigationInitTier::Expanded, 26, 82};
    s.monotonicMs = 0;
    assert(frozen.Update(s, 0).kind == AutonomyEventKind::None);
    s.monotonicMs = 47 * 250;
    assert(frozen.Update(s, 47).kind == AutonomyEventKind::None);
    s.monotonicMs = 48 * 250;
    assert(frozen.Update(s, 48).kind == AutonomyEventKind::HardStall);

    // Even a continuously advancing loader is bounded by the pre-existing
    // shared 4-minute initialization ceiling, not a larger movement timeout.
    AutonomySupervisor bounded;
    s.navigationInitialization = {true, 3, Navigation::NavigationInitTier::FullMap, 0, 2000};
    for (std::uint64_t tick = 0; tick < 960; ++tick)
    {
        s.monotonicMs = tick * 250;
        s.navigationInitialization.tilesProcessed = tick;
        assert(bounded.Update(s, tick).kind == AutonomyEventKind::None);
    }
    s.monotonicMs = 240000;
    ++s.navigationInitialization.tilesProcessed;
    assert(bounded.Update(s, 960).kind == AutonomyEventKind::HardStall);

    // Genuine owner objective change starts independent planning evidence.
    s.navigationInitialization.intent = 4;
    s.monotonicMs = 240250;
    assert(bounded.Update(s, 961).kind == AutonomyEventKind::None);

    // Physical displacement remains physical progress after planning; new
    // commands/route fingerprints are not exposed as earned progress.
    AutonomySupervisor moving;
    s.navigationInitialization = {};
    assert(moving.Update(s, 0).kind == AutonomyEventKind::None);
    s.x = 1.0f;
    assert(moving.Update(s, 47).kind == AutonomyEventKind::None);
    assert(moving.Update(s, 70).kind == AutonomyEventKind::None);

    // A regressing progress cursor cannot masquerade as fresh work.
    AutonomySupervisor regressed;
    s.navigationInitialization = {true, 5, Navigation::NavigationInitTier::Expanded, 26, 82};
    s.monotonicMs = 0;
    assert(regressed.Update(s, 0).kind == AutonomyEventKind::None);
    s.navigationInitialization.tilesProcessed = 0;
    const auto regression = regressed.Update(s, 1);
    assert(regression.kind == AutonomyEventKind::HardStall);
    assert(std::string(regression.classification) == "initialization_evidence_regressed");
    // Pending without actual provider evidence is not a blanket exemption.
    AutonomySupervisor unknown;
    s.navigationInitialization = {true, 0, Navigation::NavigationInitTier::None, 0, 0};
    assert(unknown.Update(s, 0).kind == AutonomyEventKind::None);
    assert(unknown.Update(s, 48).kind == AutonomyEventKind::HardStall);
    // Combat preempts planning; damage watchdog semantics remain unchanged.
    AutonomySupervisor preempted;
    s.navigationInitialization = {true, 6, Navigation::NavigationInitTier::Route, 0, 25};
    assert(preempted.Update(s, 0).kind == AutonomyEventKind::None);
    s.activity = AutonomyActivity::Combat;
    s.targetHealth = 100;
    assert(preempted.Update(s, 1).kind == AutonomyEventKind::None);
    s.targetHealth = 90;
    assert(preempted.Update(s, 30).kind == AutonomyEventKind::None);
    assert(preempted.Update(s, 46).kind == AutonomyEventKind::SoftStall);
    s.activity = AutonomyActivity::None;
    assert(preempted.Update(s, 47).kind == AutonomyEventKind::None);

    // Shared monitor/owner integration must feed actual provider tile counts.
    auto read = [](const char* file) {
        std::ifstream in(file);
        assert(in.good());
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    const auto monitor = read("src/Bot/WorldMonitor.h");
    const auto grind = read("src/Bot/GrindModeController.h");
    const auto follower = read("src/Navigation/GenericNavMeshPathFollower.h");
    assert(monitor.find("autonomySample.navigationInitialization =") != std::string::npos);
    assert(grind.find("NavigationInitializationObservation() const") != std::string::npos);
    assert(follower.find("progress.processed, progress.total") != std::string::npos);
    assert(follower.find("MaximumSurfaceRecoveryAttempts = 4") != std::string::npos);
    assert(follower.find("MaximumLastSafeBacktracks = 2") != std::string::npos);
    assert(follower.find("2000.0f") != std::string::npos);
}
