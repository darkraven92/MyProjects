#include "../src/Bot/CombatInitiationPolicy.h"

#include <cassert>

int main()
{
    using namespace Bot;
    CombatInitiationEvidence e{};
    e.targetFresh=e.selectionKnown=e.selectionMatches=true;
    e.actionKnown=e.actionInputSafe=true;

    // Delayed UI selection discards an earlier alignment; the first two
    // selected, fresh, aligned snapshots then authorize Charge.
    auto aligned=CombatInitiationPolicy::ConfirmChargeFacing(0,false,true);
    assert(aligned==0);
    aligned=CombatInitiationPolicy::ConfirmChargeFacing(aligned,true,true);
    assert(aligned==1);
    aligned=CombatInitiationPolicy::ConfirmChargeFacing(aligned,true,true);
    assert(aligned==CombatFacingPolicy::StableSnapshotsRequired);
    assert(CombatInitiationPolicy::ConfirmChargeFacing(aligned,true,false)==0);
    assert(CombatInitiationPolicy::ChargeRangeEligible(19.0f));
    assert(!CombatInitiationPolicy::ChargeRangeEligible(7.9f));
    assert(!CombatInitiationPolicy::ChargeRangeEligible(25.1f));

    // The melee watchdog's unknown Attack readback does not freeze the
    // Charge-facing timeout or the physical chase FSM. It cannot authorize
    // a Charge or Attack command by itself.
    e.actionKnown=e.actionInputSafe=false;
    auto d=CombatInitiationPolicy::Decide(CombatInitiationPhase::ChargeFacing,e);
    assert(d.stateProgressAllowed && !d.offensiveInputAllowed);
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Chase,e);
    assert(d.stateProgressAllowed && !d.offensiveInputAllowed);
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(!d.stateProgressAllowed && !d.offensiveInputAllowed);
    e.alternateKnown=e.alternateInputSafe=true;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::ChargeFacing,e);
    assert(d.stateProgressAllowed && d.offensiveInputAllowed);
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(d.stateProgressAllowed && d.offensiveInputAllowed);
    e.alternateInputSafe=false;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Chase,e);
    assert(!d.stateProgressAllowed && !d.offensiveInputAllowed);
    e.alternateWait=true;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::ChargeFacing,e);
    assert(d.reason==CombatInitiationReason::ActionWait);
    e.alternateWait=false;

    // A known cast, GCD, dialog, or input conflict may not be overridden by
    // an independent probe. A stale target or unknown selection also holds.
    e.actionKnown=true; e.actionInputSafe=true; e.actionWait=true;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::ChargeFacing,e);
    assert(!d.stateProgressAllowed && !d.offensiveInputAllowed);
    e.actionWait=false; e.actionInputSafe=false;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(!d.stateProgressAllowed && !d.offensiveInputAllowed);
    e.actionKnown=false; e.targetFresh=false;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Chase,e);
    assert(!d.stateProgressAllowed);
    e.targetFresh=true; e.selectionKnown=false;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(!d.stateProgressAllowed);
    e.selectionKnown=true; e.selectionMatches=false;
    e.alternateInputSafe=true;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(d.reason==CombatInitiationReason::SelectionMismatch &&
        !d.stateProgressAllowed && !d.offensiveInputAllowed);

    // Existing ChaseController 5/6-yard hysteresis is shared with this
    // testable policy: an unavailable Charge falls back to chase and a live
    // locked target enters melee without any supervisor recovery.
    assert(!CombatInitiationPolicy::EnterMelee(6.5f));
    assert(!CombatInitiationPolicy::EnterMelee(5.5f));
    assert(CombatInitiationPolicy::EnterMelee(4.5f));
    assert(CombatInitiationPolicy::EnterMelee(3.5f));
    assert(CombatInitiationPolicy::ResumeChase(6.5f));
    assert(!CombatInitiationPolicy::ResumeChase(5.5f));
}
