#include "../src/Bot/LivingWaterEmergencyPolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include "../src/Navigation/LivingWaterTraversalPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

using namespace Bot;
using A = WaterEgressAction;

static WaterEgressSample Sample(std::uint64_t now, bool swimming = false)
{
    return {true, true, false, true, swimming, true, 123, 456, 789, now,
        swimming ? 2.0f : 0.0f, 0, 0};
}
static std::string Read(const char* path)
{
    std::ifstream file(path); assert(file);
    return {std::istreambuf_iterator<char>(file), {}};
}

int main()
{
    static_assert(LivingWaterEmergencyPolicy::MaximumAttempts == 2);
    static_assert(Navigation::LocalRecoveryLimits::MaximumSurfaceRecoveryAttempts == 4);
    static_assert(LivingWaterBlockPolicy::ExitSamples == 3);
    LivingWaterEmergencyPolicy p;
    assert(p.Update(Sample(1000)).action == A::None);
    assert(p.AnchorKnown() && !p.Blocked());
    auto d = p.Update(Sample(1250, true));
    assert(d.entered && d.action == A::Backtrack && p.Blocked());
    assert(p.Attempts() == 1 && p.Destination().x == 0);
    auto command = Sample(1300, true);
    assert(p.CanDispatch(command));
    command.guid++;
    assert(!p.CanDispatch(command));
    command = Sample(1300, true); command.swimmingKnown = false;
    assert(!p.CanDispatch(command));
    command = Sample(1300, true); command.ownerSafe = false;
    assert(!p.CanDispatch(command));
    command = Sample(1300, true); command.living = false;
    assert(!p.CanDispatch(command));
    assert(p.Update(Sample(1500)).proof == 1);
    assert(!p.Update(Sample(1500)).recovered); // same observation is not proof two
    assert(p.Update(Sample(1750)).proof == 2);
    assert(p.Blocked());
    d = p.Update(Sample(2000));
    assert(d.recovered && d.proof == 3 && d.action == A::Stop && !p.Blocked());
    assert(!p.AnchorKnown()); // recovered snapshot is not a new destination

    for (bool death : {false, true})
    {
        LivingWaterEmergencyPolicy q;
        auto unknown = Sample(1000); unknown.swimmingKnown = false;
        assert(q.Update(unknown).action == A::None && !q.AnchorKnown());
        auto wet = Sample(1250, true); wet.deathOwns = death;
        wet.living = !death;
        auto result = q.Update(wet);
        assert(result.action != A::Backtrack);
        assert(death ? !q.Blocked() : q.Blocked());
    }
    for (int invalid = 0; invalid < 9; ++invalid)
    {
        LivingWaterEmergencyPolicy q;
        q.Update(Sample(1000));
        auto wet = Sample(1250, true);
        switch (invalid)
        {
        case 0: wet.living = false; break;
        case 1: wet.ownerSafe = false; break;
        case 2: wet.manager++; break;
        case 3: wet.guid++; break;
        case 4: wet.player++; break;
        case 5: wet.nowMs += 5000; break;
        case 6: wet.x = 13; break;
        case 7: wet.x = 0; wet.z = -2; break; // no vertical-only egress
        case 8: wet.z = -5; break;
        }
        assert(q.Update(wet).action != A::Backtrack);
        assert(q.Attempts() == 0);
    }
    LivingWaterEmergencyPolicy invalidPosition;
    auto bad = Sample(1000); bad.x = std::numeric_limits<float>::quiet_NaN();
    invalidPosition.Update(bad);
    assert(!invalidPosition.AnchorKnown());
    bad.nowMs = 1250; bad.swimming = true;
    d = invalidPosition.Update(bad);
    assert(d.entered && d.action == A::Stop && invalidPosition.Blocked());
    bad.nowMs = 1500; bad.deathOwns = true;
    assert(invalidPosition.Update(bad).deathHandoff); // coordinates cannot block DeathRecovery
    LivingWaterEmergencyPolicy invalidExit;
    invalidExit.Update(Sample(1000)); invalidExit.Update(Sample(1250, true));
    invalidExit.Update(Sample(1500)); invalidExit.Update(Sample(1750));
    bad = Sample(2000); bad.z = std::numeric_limits<float>::quiet_NaN();
    assert(!invalidExit.Update(bad).recovered && invalidExit.Blocked());

    LivingWaterEmergencyPolicy transient;
    transient.Update(Sample(1000)); transient.Update(Sample(1250, true));
    assert(transient.Update(Sample(1500)).proof == 1);
    assert(transient.Update(Sample(1750)).proof == 2);
    assert(transient.Update(Sample(2000, true)).action == A::Backtrack);
    assert(transient.Attempts() == 2 && transient.Blocked());
    assert(transient.Update(Sample(2250)).proof == 1);
    assert(transient.Update(Sample(2500, true)).action == A::Stop);
    for (unsigned t = 2750; t < 20000; t += 250)
        assert(transient.Update(Sample(t, true)).action == A::None);
    assert(transient.Attempts() == 2 && transient.Blocked()); // terminal, no spin

    LivingWaterEmergencyPolicy stalled;
    stalled.Update(Sample(1000)); stalled.Update(Sample(1250, true));
    assert(stalled.Update(Sample(5749, true)).action == A::None);
    d = stalled.Update(Sample(5750, true));
    assert(d.action == A::Backtrack && std::string(d.reason) == "no_progress");
    d = stalled.Update(Sample(10250, true));
    assert(d.action == A::Stop && std::string(d.reason) == "episode_deadline");
    assert(stalled.Update(Sample(10500, true)).action == A::None);

    LivingWaterEmergencyPolicy progress;
    progress.Update(Sample(1000)); progress.Update(Sample(1250, true));
    auto closer = Sample(1500, true); closer.x = 1;
    assert(std::string(progress.Update(closer).event) == "progress");
    closer.nowMs = 1750; closer.x = 0.5f;
    assert(progress.Update(closer).event == nullptr); // sparse, one progress log per attempt
    closer.nowMs = 5750;
    assert(progress.Update(closer).action == A::Backtrack);
    closer.nowMs = 10250; closer.x = 0.2f;
    assert(progress.Update(closer).action == A::Stop); // progress cannot refund time/budget

    for (bool gap : {false, true})
    {
        LivingWaterEmergencyPolicy q;
        q.Update(Sample(1000)); q.Update(Sample(1250, true));
        assert(q.Update(Sample(1500)).proof == 1);
        if (gap) q.InvalidateWorld();
        else
        {
            auto unknown = Sample(1750); unknown.swimmingKnown = false;
            assert(q.Update(unknown).action == A::Stop);
        }
        assert(!q.AnchorKnown());
        assert(!q.Update(Sample(2000)).recovered);
        assert(!q.Update(Sample(2250)).recovered);
        assert(q.Update(Sample(2500)).recovered);
    }
    LivingWaterEmergencyPolicy death;
    death.Update(Sample(1000)); death.Update(Sample(1250, true));
    auto ghost = Sample(1500, true); ghost.living = false; ghost.deathOwns = true;
    d = death.Update(ghost);
    assert(d.deathHandoff && d.action == A::None && !death.Blocked() && !death.AnchorKnown());
    LivingWaterEmergencyPolicy dispatch;
    dispatch.Update(Sample(1000)); dispatch.Update(Sample(1250, true));
    dispatch.DispatchFailed();
    assert(dispatch.Update(Sample(1500, true)).action == A::Stop);
    assert(dispatch.Update(Sample(1750, true)).action == A::None);

    // Egress is recovery, never authority to deliver an AFK pulse.
    AfkSafety safety; safety.recovery = true; safety.water = true;
    AfkProductionPolicy afk;
    AfkObservation o{true, false, false, 240000, 0, 300000};
    assert(AfkProductionPolicy::Band(o) == AfkProductionBand::Due);
    assert(afk.Update(o, safety, 1).action == AfkAction::None);
    o.clientNow = 270000;
    assert(AfkProductionPolicy::Band(o) == AfkProductionBand::Overdue);
    o.clientNow = 300000;
    assert(AfkProductionPolicy::Band(o) == AfkProductionBand::ThresholdCrossed);
    assert(afk.Update(o, safety, 2).action == AfkAction::None);
    using namespace Navigation;
    static_assert(!LivingWaterTraversalPolicy::Traversable(0x08, WaterTraversalMode::AvoidUntilQualified));
    static_assert(!LivingWaterTraversalPolicy::Traversable(0x09, WaterTraversalMode::AvoidUntilQualified));

    const auto monitor = Read("src/Bot/WorldMonitor.h");
    const auto entry = monitor.find("if (waterDecision.entered)");
    const auto move = monitor.find("if (waterDecision.action == WaterEgressAction::Backtrack)");
    const auto blocked = monitor.find("if (waterEmergency.Blocked())");
    assert(entry < move && move < blocked);
    const auto handoff = monitor.substr(entry, move - entry);
    assert(handoff.find("HoldPosition(world.player)") != std::string::npos);
    assert(handoff.find("ReleaseNavigationForLivingWater()") != std::string::npos);
    assert(handoff.find("if (waterHandoffSafe) combat.InvalidateIntentForLivingWater(tick)") != std::string::npos);
    const auto recovery = monitor.substr(move, blocked - move);
    assert(recovery.find("commandWater.movementKnown") != std::string::npos);
    assert(recovery.find("WorldStateReader::Read(commandWorld)") != std::string::npos);
    assert(recovery.find("waterEmergency.CanDispatch(commandSample)") != std::string::npos);
    assert(recovery.find("destination.x, destination.y, destination.z") != std::string::npos);
    assert(recovery.find("continue;") != std::string::npos); // fresh world before work resumes
    const auto blockedBody = monitor.substr(blocked, monitor.find("continue;", blocked) - blocked);
    assert(blockedBody.find("waterAfkSafety.recovery = true") != std::string::npos);
    assert(blockedBody.find("\"LivingWaterBlocked\"") != std::string::npos);
    for (const auto* forbidden : {"InputPulse", "Jump(", "SetWaterTraversalMode", "DefaultServerLogin"})
        assert(recovery.find(forbidden) == std::string::npos);
    const auto gapStart = monitor.find("if (!snapshotValid)");
    assert(monitor.find("waterEmergency.InvalidateWorld();", gapStart) < monitor.find("continue;", gapStart));
    const auto combat = Read("src/Bot/CombatController.h");
    const auto clearStart = combat.find("void InvalidateIntentForLivingWater(");
    const auto clear = combat.substr(clearStart, combat.find("void SuspendForDeathRecovery", clearStart) - clearStart);
    for (const auto* required : {"loot_.Reset()", "ResetTargetState()", "deferredCorpses_.clear()", "CombatState::AcquiringTarget"})
        assert(clear.find(required) != std::string::npos);
    const auto policy = Read("src/Bot/LivingWaterEmergencyPolicy.h");
    for (const auto* forbidden : {"ClickToMove", "Jump(", "AfkClient", "SetWaterTraversalMode", "DefaultServerLogin"})
        assert(policy.find(forbidden) == std::string::npos);
    const auto adapter = Read("src/Bot/AfkClient5875.h");
    assert(adapter.find("return \"swimming\"") != std::string::npos);
    const auto follower = Read("src/Navigation/GenericNavMeshPathFollower.h");
    assert(follower.find("MaximumSurfaceRecoveryAttempts = 4") != std::string::npos);
    assert(follower.find("MaximumLastSafeBacktracks = 2") != std::string::npos);
}
