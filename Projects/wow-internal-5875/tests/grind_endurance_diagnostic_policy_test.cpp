#include "../src/Bot/GrindEnduranceDiagnosticPolicy.h"

#include <cassert>
#include <string_view>

int main()
{
    using namespace Bot;
    using Reason = GrindInterruptionReason;
    GrindInterruptionFacts facts{};

    facts.manualVendorWait = true;
    facts.watchdogReason = RuntimeRobustnessReason::IdleDeadlock;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::ManualVendorRequired);
    assert(!GrindEnduranceDiagnosticPolicy::CountsAsUnexpectedIdle(
        GrindEnduranceDiagnosticPolicy::Classify(facts)));

    facts = {};
    facts.deathRecoveryFailed = true;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::DeathRecoveryFailure);
    facts = {};
    facts.recoveryBlocked = true;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::RecoveryBlocked);
    facts = {};
    facts.lootFailed = true;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::LootFailure);
    facts = {};
    facts.navigationFailure = true;
    facts.navigationReason = Navigation::NavigationPlanFailure::SurfaceRecoveryExhausted;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::LocalRecoveryExhausted);
    facts.navigationReason = Navigation::NavigationPlanFailure::NoPath;
    facts.approachFailure = true;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::TargetUnreachable);
    facts.approachFailure = false;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::NavigationFailure);
    facts.navigationReason = Navigation::NavigationPlanFailure::None;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::OtherUnknown);
    facts = {};
    facts.watchdogReason = RuntimeRobustnessReason::CombatOwnerTimeout;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::CombatHardStall);
    facts = {};
    facts.noTarget = true;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::NoTarget);
    facts = {};
    facts.intentionalWait = true;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::IntentionalWait);
    facts.watchdogReason = RuntimeRobustnessReason::MovementOwnerTimeout;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::IntentionalWait);
    facts = {};
    facts.watchdogReason = RuntimeRobustnessReason::IdleDeadlock;
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::UnexpectedIdle);
    assert(GrindEnduranceDiagnosticPolicy::CountsAsUnexpectedIdle(
        GrindEnduranceDiagnosticPolicy::Classify(facts)));
    facts = {};
    assert(GrindEnduranceDiagnosticPolicy::Classify(facts) == Reason::OtherUnknown);
    assert(std::string_view(GrindEnduranceDiagnosticPolicy::Name(Reason::NoTarget)) == "no_target");
    assert(std::string_view(GrindEnduranceDiagnosticPolicy::WatchdogName(
        RuntimeRobustnessReason::CombatOwnerTimeout)) == "combat_owner_timeout");

    GrindEnduranceEdgeTracker tracker;
    auto edges = tracker.Observe(false, false, false);
    assert(!edges.combatStarted && !edges.deathStarted && !edges.manualVendorEntered);
    edges = tracker.Observe(true, false, false);
    assert(edges.combatStarted);
    edges = tracker.Observe(true, false, false);
    assert(!edges.combatStarted);
    edges = tracker.Observe(false, true, true);
    assert(edges.deathStarted && edges.manualVendorEntered);
    edges = tracker.Observe(false, true, true);
    assert(!edges.deathStarted && !edges.manualVendorEntered);
    edges = tracker.Observe(false, false, false);
    assert(edges.manualVendorCleared);
    edges = tracker.Observe(false, false, false);
    assert(!edges.manualVendorCleared);
    edges = tracker.Observe(false, true, false);
    assert(edges.deathStarted);
}
