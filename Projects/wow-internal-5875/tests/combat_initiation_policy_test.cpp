#include "../src/Bot/CombatInitiationPolicy.h"
#include "../src/Bot/CombatBootstrapInputScript.h"
#include "../src/Bot/CombatInputProbePolicy.h"

#include <cassert>
#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    using namespace Bot;
    if (argc==2 && std::string_view(argv[1])=="--lua")
    { std::cout<<CombatBootstrapInputScript; return 0; }
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
    assert(d.stateProgressAllowed && !d.offensiveInputAllowed);
    assert(!CombatInitiationPolicy::ResumeChase(3.5f));
    assert(CombatInitiationPolicy::ResumeChase(8.0f));
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

    // A point-blank target needs neither Charge nor prior damage to become
    // eligible for one guarded melee bootstrap. Unknown input is never safe.
    e={}; e.targetFresh=e.selectionKnown=e.selectionMatches=true;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(CombatInitiationPolicy::EnterMelee(0.3880f));
    assert(d.stateProgressAllowed && !d.offensiveInputAllowed);
    e.alternateKnown=e.alternateInputSafe=true;
    d=CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e);
    assert(d.stateProgressAllowed && d.offensiveInputAllowed);
    e.selectionMatches=false;
    assert(!CombatInitiationPolicy::Decide(CombatInitiationPhase::Melee,e).offensiveInputAllowed);

    for (const auto reason: {"unknown_ui_parent","unknown_life_api",
        "unknown_casting_frame","unknown_spell_targeting",
        "unknown_enumerate_frames","unknown_frame_visibility_api",
        "unknown_frame_object_type","unknown_frame_keyboard_api",
        "unknown_frame_script_api","unknown_frame_iteration_limit",
        "unknown_attack_action_api","unknown_attack_cooldown_api",
        "unknown_modal_lookup_api","unknown_readback","unknown_readback_address",
        "unknown_readback_dispatch","unknown_readback_thread",
        "unknown_readback_execute","unknown_readback_text",
        "unknown_script_error"})
        assert(CombatInputProbePolicy::Classify(reason)==CombatInputProbeState::Unknown);
    for (const auto reason: {"blocked_modal_frame","blocked_editbox",
        "blocked_keyboard_handler","blocked_spell_targeting",
        "blocked_player_dead","blocked_target_invalid","blocked_execution_guard"})
        assert(CombatInputProbePolicy::Classify(reason)==CombatInputProbeState::Blocked);
    for (const auto reason: {"waiting_cast","waiting_cast_or_gcd"})
        assert(CombatInputProbePolicy::Classify(reason)==CombatInputProbeState::Wait);
    assert(CombatInputProbePolicy::Classify("ready")==CombatInputProbeState::Ready);
}
