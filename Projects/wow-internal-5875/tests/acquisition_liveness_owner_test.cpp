#include "../src/Bot/RuntimeRobustnessSupervisor.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

static std::string Read(const char* file)
{
    std::ifstream input(file);
    assert(input.good());
    return std::string(std::istreambuf_iterator<char>(input), {});
}

int main()
{
    using namespace Bot;
    RuntimeRobustnessSample sample{};
    sample.active = true;
    sample.grindState = 1;
    sample.combatState = 1;
    sample.playerHealth = sample.playerMaxHealth = 100;
    RuntimeRobustnessSupervisor supervisor;
    supervisor.Update(sample, 0);
    supervisor.Update(sample, 1);
    assert(supervisor.AcquisitionEpochActive());

    // Reproduce the live epoch spanning a real, long navigation initializer.
    sample.grindState = 3;
    sample.navigationInitializationPending = true;
    for (std::uint64_t tick = 2; tick <= 428; ++tick)
    {
        assert(supervisor.Update(sample, tick).kind == RuntimeRobustnessEventKind::None);
        assert(!supervisor.AcquisitionEpochActive());
        assert(supervisor.AcquisitionEpochAgeTicks(tick) == 0);
    }
    sample.grindState = 1;
    sample.navigationInitializationPending = false;
    assert(supervisor.Update(sample, 429).kind == RuntimeRobustnessEventKind::None);
    assert(supervisor.AcquisitionEpochAgeTicks(429) == 0);
    assert(supervisor.Update(sample, 468).kind == RuntimeRobustnessEventKind::None);
    assert(supervisor.Update(sample, 469).reason == RuntimeRobustnessReason::IdleDeadlock);

    // Explicit follower ownership wins over lagging high-level acquisition
    // state, for both initialization and already-issued NavMesh movement.
    for (bool initialization : {false, true})
    {
        RuntimeRobustnessSupervisor navigation;
        sample = {};
        sample.active = true;
        sample.grindState = sample.combatState = 1;
        sample.navigationOwned = true;
        sample.navigationInitializationPending = initialization;
        for (std::uint64_t tick = 0; tick < 430; ++tick)
        {
            if (!initialization) sample.x += 3.0f;
            assert(navigation.Update(sample, tick).kind == RuntimeRobustnessEventKind::None);
            assert(!navigation.AcquisitionEpochActive());
        }
        sample.navigationOwned = sample.navigationInitializationPending = false;
        assert(navigation.Update(sample, 430).kind == RuntimeRobustnessEventKind::None);
        assert(navigation.AcquisitionEpochAgeTicks(430) == 0);
        assert(std::string_view(navigation.AcquisitionDecision()) == "resume");
        assert(navigation.Update(sample, 469).kind == RuntimeRobustnessEventKind::None);
        const auto deadlock = navigation.Update(sample, 470);
        assert(deadlock.reason == RuntimeRobustnessReason::IdleDeadlock);
        assert(deadlock.noProgressTicks == 40);
        assert(navigation.AcquisitionDecisionAgeTicks() == 40);
    }

    // No competing owner is acquisition inactivity. On release, each grants
    // exactly one new acquisition epoch, not a continuously renewed heartbeat.
    for (bool RuntimeRobustnessSample::* owner : {
             &RuntimeRobustnessSample::deathRecoveryActive,
             &RuntimeRobustnessSample::vendorActive,
             &RuntimeRobustnessSample::recoveryActive,
             &RuntimeRobustnessSample::firstAidActive,
             &RuntimeRobustnessSample::dialogActive})
    {
        RuntimeRobustnessSupervisor otherOwner;
        sample = {};
        sample.active = true;
        sample.grindState = sample.combatState = 1;
        otherOwner.Update(sample, 0);
        sample.*owner = true;
        for (std::uint64_t tick = 1; tick <= 45; ++tick)
        {
            assert(otherOwner.Update(sample, tick).kind == RuntimeRobustnessEventKind::None);
            assert(!otherOwner.AcquisitionEpochActive());
        }
        sample.*owner = false;
        assert(otherOwner.Update(sample, 46).kind == RuntimeRobustnessEventKind::None);
        assert(otherOwner.AcquisitionEpochAgeTicks(46) == 0);
        assert(otherOwner.Update(sample, 86).reason == RuntimeRobustnessReason::IdleDeadlock);
    }

    // Target-selection churn, transient GUIDs and physical displacement within
    // actual acquisition still cannot mint endless acquisition budgets.
    RuntimeRobustnessSupervisor churn;
    sample = {};
    sample.active = true;
    sample.grindState = 1;
    for (std::uint64_t tick = 0; tick <= 40; ++tick)
    {
        sample.combatState = static_cast<int>(tick % 3);
        sample.targetGuid = tick;
        sample.x += 3.0f;
        const auto event = churn.Update(sample, tick);
        assert(event.reason == (tick == 40 ? RuntimeRobustnessReason::IdleDeadlock
                                          : RuntimeRobustnessReason::None));
    }
    sample.combatState = 11; // Current CombatState::Fighting, verified from source.
    churn.Update(sample, 41);
    assert(!churn.AcquisitionEpochActive());
    assert(std::string_view(churn.AcquisitionDecision()) == "reset");
    sample.active = false; // World/death gap is already reset by WorldMonitor.
    churn.Update(sample, 42);
    assert(!churn.AcquisitionEpochActive());

    // Outcome debt survives legitimate planning and movement. Terminal nav
    // handoff gets the existing 40-tick acquisition opportunity, not a reset
    // of the 1200-tick strategic clock or a fabricated kill/XP outcome.
    RuntimeRobustnessSupervisor strategic;
    sample = {};
    sample.active = true;
    sample.grindState = 3;
    sample.combatState = 1;
    sample.navigationInitializationPending = true;
    for (std::uint64_t tick = 0; tick <= 1345; ++tick)
        assert(strategic.Update(sample, tick).kind == RuntimeRobustnessEventKind::None);
    sample.grindState = 1;
    sample.navigationInitializationPending = false;
    assert(strategic.Update(sample, 1346).kind == RuntimeRobustnessEventKind::None);
    assert(strategic.Update(sample, 1347).kind == RuntimeRobustnessEventKind::None);
    assert(strategic.OutcomeAgeTicks(1347) == 1347);
    // Legitimate fighting handoff avoids idle debt, but does not hide five
    // minutes of no outcome. Physical work alone is not strategic success.
    sample.combatState = 11;
    for (std::uint64_t tick = 1348; tick < 1386; ++tick)
    {
        sample.x += 3.0f;
        assert(strategic.Update(sample, tick).kind == RuntimeRobustnessEventKind::None);
    }
    assert(strategic.Update(sample, 1386).reason == RuntimeRobustnessReason::StrategicNoOutcome);

    // Any actual bounded repair gets its existing cooldown before strategic
    // escalation; it cannot immediately duplicate the just-issued repair.
    RuntimeRobustnessSupervisor sequential;
    sample = {};
    sample.active = true;
    sample.grindState = 1;
    sample.combatState = 1;
    sequential.Update(sample, 0);
    assert(sequential.Update(sample, 1346).reason == RuntimeRobustnessReason::IdleDeadlock);
    sample.combatState = 11;
    sample.x += 3.0f;
    assert(sequential.Update(sample, 1347).kind == RuntimeRobustnessEventKind::None);
    assert(sequential.OutcomeAgeTicks(1347) == 1347);
    assert(sequential.Update(sample, 1386).reason == RuntimeRobustnessReason::StrategicNoOutcome);

    // Production wiring and budgets are protected without importing Win32
    // controller headers into this deterministic policy replay.
    const auto monitor = Read("src/Bot/WorldMonitor.h");
    assert(monitor.find("grindMode.NavigationOwnsMovement() || navMeshReturn.OwnsMovement()") != std::string::npos);
    assert(monitor.find("runtimeRobustness.AcquisitionDecision()") != std::string::npos);
    assert(monitor.find("else\n                {\n                    runtimeRobustness.Reset();") != std::string::npos);
    const auto follower = Read("src/Navigation/GenericNavMeshPathFollower.h");
    assert(follower.find("MaximumPathLength =\n            2000.0f") != std::string::npos);
    assert(follower.find("MaximumSurfaceRecoveryAttempts = 4") != std::string::npos);
    assert(follower.find("MaximumLastSafeBacktracks = 2") != std::string::npos);
}
