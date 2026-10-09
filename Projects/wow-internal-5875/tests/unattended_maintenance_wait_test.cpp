#include "../src/Bot/UnattendedMaintenanceWaitPolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include "../src/Bot/AfkQualificationHold.h"
#include "../src/Bot/DisconnectDiagnosticPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using namespace Bot;
    using D = MaintenanceWaitDecision;
    UnattendedMaintenanceWaitPolicy p;
    static_assert(UnattendedMaintenanceWaitPolicy::MaximumRetries == 2);
    p.FailedTrip(480);
    auto read = [&](unsigned tick, bool fresh, bool known, int slots,
                    bool safe = true, bool automatic = true)
    { return p.Observe(tick, fresh, known, slots, 1, safe, automatic); };
    assert(read(1, true, true, 0) == D::Wait);
    assert(read(480, true, true, 0, false) == D::Wait); // combat/death owner
    assert(p.Retries() == 0);
    assert(read(480, false, true, 0) == D::Wait);
    assert(read(480, true, true, 0) == D::RetryVendor);
    assert(p.Retries() == 1);
    p.FailedTrip(960);
    assert(read(481, true, false, 16) == D::Wait); // unknown is not space
    assert(read(960, true, true, 0) == D::RetryVendor);
    p.FailedTrip(1440);
    assert(read(1440, true, true, 0) == D::MaintenanceBlocked);
    assert(read(100000, true, true, 0) == D::MaintenanceBlocked);
    assert(p.Retries() == 2); // no retry spam/reset through FailedTrip
    assert(read(100001, true, true, 12) == D::Wait);
    assert(read(100002, false, true, 12) == D::Wait); // cached read not fresh
    assert(read(100009, true, false, 12) == D::MaintenanceBlocked);
    assert(read(100017, true, true, 12) == D::Wait);
    assert(read(100025, true, true, 12) == D::Resume);
    p.Reset(); assert(!p.Active() && p.Retries() == 0);
    p.FailedTrip(100010);
    assert(read(0, true, true, 12) == D::Wait);
    p.InvalidateSpaceProof(); // owner/world boundary cannot carry one old read
    assert(read(8, true, true, 12) == D::Wait);
    assert(read(16, true, true, 12) == D::Resume);
    p.Reset();
    p.FailedTrip(0);
    assert(read(0, true, true, 0, true, false) == D::MaintenanceBlocked);
    assert(p.Retries() == 0); // explicit GUI manual mode never overridden

    // A terminal retry read must not release the active episode, even when
    // the trip succeeded or a failed return trip left some free slots.
    for (int terminalSlots : {1, 2, 12})
    {
        p.Reset();
        p.FailedTrip(480);
        assert(read(480, true, true, 0) == D::RetryVendor);
        p.FailedTrip(960); // failed retry returns to the same wait episode
        assert(p.Active() && p.Retries() == 1);
        assert(p.RetryAt() == 960 && p.SpaceObservations() == 0);
        assert(read(488, false, true, terminalSlots) != D::Resume);
        assert(read(496, true, true, terminalSlots) != D::Resume);
        assert(p.SpaceObservations() == (terminalSlots > 1 ? 1u : 0u));
        assert(read(497, false, true, terminalSlots) != D::Resume);
        p.InvalidateSpaceProof(); // successful retry / world gap discards proof
        assert(p.Active() && p.Retries() == 1);
        assert(read(504, true, true, 2) == D::Wait);
        assert(read(512, true, false, 2) == D::Wait);
        assert(p.SpaceObservations() == 0);
        assert(read(520, true, true, 2) == D::Wait);
        assert(read(528, true, true, 2) == D::Resume);
    }

    AfkSafety safe; safe.healthyIdle = true;
    AfkObservation o{true, false, false, 300051, 0, 300000};
    AfkProductionPolicy afk;
    assert(afk.Update(o, safe, 1, false, false).action == AfkAction::InputPulse);
    for (auto field : {&AfkSafety::vendor, &AfkSafety::combat,
            &AfkSafety::death, &AfkSafety::navigation, &AfkSafety::dialog,
            &AfkSafety::recovery, &AfkSafety::water, &AfkSafety::fault})
    {
        auto blocked = safe; blocked.*field = true;
        assert(afk.Update(o, blocked, 1, false, false).action == AfkAction::None);
    }
    assert(afk.Update({}, safe, 1, false, false).action == AfkAction::None);
    assert(AfkWorkloadSafetyPolicy::LandMovementAllowed(0, false));
    assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(1, false));
    assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(0x02000000, false));
    assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(0x00200000, false));
    assert(afk.Update(o, safe, 1, false, false).action == AfkAction::InputPulse);
    afk.Issued(AfkAction::InputPulse, o, 1);
    assert(afk.Update(o, safe, 2, false, false).result == AfkResult::Pending);
    o.lastInput = o.clientNow;
    assert(afk.Update(o, safe, 3, false, false).result == AfkResult::Confirmed);

    // Existing diagnostics must not relabel an OM gap as a server disconnect.
    DisconnectDiagnosticPolicy diagnostic;
    assert(diagnostic.Update(true, 0) == DisconnectDiagnosticEvent::Healthy);
    assert(diagnostic.Update(false, 1) == DisconnectDiagnosticEvent::SnapshotLost);
    assert(diagnostic.Update(true, 250) == DisconnectDiagnosticEvent::SnapshotRecovered);
    std::ifstream file("src/Bot/GrindModeController.h");
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    assert(source.find("unattendedMaintenanceWait_.Observe(") != std::string::npos);
    assert(source.find("reason=two_fresh_bag_space_observations") != std::string::npos);
    assert(source.find("AdoptExactTargetForDefense(world, aggressor->guid, tick)") != std::string::npos);
    // Integration contracts: both vendor terminal paths must preserve an
    // existing episode. The policy alone cannot catch these owner bypasses.
    const auto failedStart = source.find("if (vendor_.Failed())");
    const auto doneStart = source.find("if (vendor_.IsDone())", failedStart);
    assert(failedStart != std::string::npos && doneStart != std::string::npos);
    const auto failed = source.substr(failedStart, doneStart - failedStart);
    assert(failed.find("const bool holdForFullBags =\n"
                       "                        unattendedMaintenanceWait_.Active() ||") != std::string::npos);
    const auto doneEnd = source.find("SetState(GrindModeState::Grinding);", doneStart);
    assert(doneEnd != std::string::npos);
    const auto done = source.substr(doneStart, doneEnd - doneStart);
    assert(done.find("unattendedMaintenanceWait_.Reset()") == std::string::npos);
    const auto active = done.find("if (unattendedMaintenanceWait_.Active())");
    assert(active != std::string::npos);
    const auto confirm = done.substr(active);
    assert(confirm.find("unattendedMaintenanceWait_.FailedTrip(tick + BagPressureVendorRetryBackoffTicks)") != std::string::npos);
    assert(confirm.find("nextBagProbeTick_ = tick + BagProbeIntervalTicks") != std::string::npos);
    assert(confirm.find("SetState(GrindModeState::WaitingForManualVendor)") != std::string::npos);
    assert(confirm.find("return;") != std::string::npos);

    std::ifstream monitorFile("src/Bot/WorldMonitor.h");
    assert(monitorFile);
    const std::string monitor{std::istreambuf_iterator<char>(monitorFile), {}};
    assert(monitor.find("afkSafety.vendor=grindMode.State()==GrindModeState::Vendoring;") != std::string::npos);
    assert(monitor.find("grindMode.State()==GrindModeState::WaitingForManualVendor) &&") != std::string::npos);
    const auto gap = monitor.find("if (!snapshotValid)");
    const auto invalidate = monitor.find("grindMode.InvalidateMaintenanceEvidenceOnWorldGap();", gap);
    assert(gap != std::string::npos && invalidate != std::string::npos);
    assert(invalidate < monitor.find("continue;", gap));
}
