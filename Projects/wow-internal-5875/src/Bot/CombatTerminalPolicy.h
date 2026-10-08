#pragma once
#include "CombatLivenessPolicy.h"
#include <cstdint>

namespace Bot
{
    enum class CombatTerminalAction { Observe, StopAndClearOwnTarget, Abandoned, OwnerFailure, HostileReturned, SystemFail };
    struct CombatTerminalSample
    {
        std::uint64_t nowMs=0, targetGuid=0, selectedGuid=0, serverVictimGuid=0;
        std::uint32_t playerHp=0;
        bool optionalGrind=false, mandatoryObjective=false, known=false, inputSafe=false;
        bool hostileEngaged=true, attackKnown=false, attackActive=false;
    };
    struct CombatTerminalDecision
    {
        CombatTerminalAction action=CombatTerminalAction::Observe;
        const char* reason="observing_safe_disengagement";
    };
    // This is terminal verification, NOT another attack repair or a kill.
    // Unknown/combat/transaction evidence never permits optional abandonment.
    class CombatTerminalPolicy
    {
        std::uint64_t guid_=0, started_=0, dispatched_=0;
        std::uint32_t hp_=0;
        bool observing_=false, issued_=false, postContainment_=false;
    public:
        static constexpr std::uint64_t VerificationMs=CombatLivenessPolicy::StructuralVerificationMs;
        static constexpr std::uint64_t PostContainmentReleaseMs=VerificationMs*4;
        void Reset() { *this=CombatTerminalPolicy{}; }
        bool Issued() const { return issued_; }
        void Dispatched(std::uint64_t now) { issued_=true; dispatched_=now; }
        void BeginPostContainment(std::uint64_t now, std::uint64_t guid, std::uint32_t hp)
        {
            Reset(); observing_=true; postContainment_=true;
            started_=now; guid_=guid; hp_=hp;
        }
        CombatTerminalDecision Observe(const CombatTerminalSample& s)
        {
            if (!s.optionalGrind && !s.mandatoryObjective)
                return {CombatTerminalAction::SystemFail,"terminal_owner_not_recoverable"};
            if (!s.targetGuid || !s.playerHp)
                return {CombatTerminalAction::SystemFail,"hostile_engagement_unknown"};
            if (postContainment_ && (s.nowMs<started_ ||
                s.nowMs-started_>=PostContainmentReleaseMs))
                return {CombatTerminalAction::SystemFail,"post_containment_release_timeout"};
            if (!s.known)
                return postContainment_
                    ? CombatTerminalDecision{CombatTerminalAction::Observe,"awaiting_fresh_release_evidence"}
                    : CombatTerminalDecision{CombatTerminalAction::SystemFail,"hostile_engagement_unknown"};
            if (s.hostileEngaged)
                return postContainment_
                    ? CombatTerminalDecision{CombatTerminalAction::HostileReturned,"hostile_returned_during_release"}
                    : CombatTerminalDecision{CombatTerminalAction::SystemFail,"unresolved_hostile_after_bounded_repair"};
            if (observing_ && (guid_!=s.targetGuid || s.playerHp<hp_))
                return {CombatTerminalAction::SystemFail,"terminal_identity_or_health_changed"};
            if (!observing_)
            { observing_=true; guid_=s.targetGuid; started_=s.nowMs; hp_=s.playerHp; }
            hp_=s.playerHp; // a heal must not hide a later HP drop
            // Containment may end with a still-selected own target while the
            // UI action probe is transiently unknown. Wait boundedly for safe
            // input evidence; never treat unknown Attack as stopped.
            if (!s.inputSafe)
                return postContainment_
                    ? CombatTerminalDecision{CombatTerminalAction::Observe,"awaiting_release_input_evidence"}
                    : CombatTerminalDecision{CombatTerminalAction::SystemFail,"terminal_action_input_conflict"};
            // Defensive escape may have stopped and cleared our own Attack
            // before this terminal verifier ran. Confirming all three native
            // ownership fields clear is stronger than dispatching a redundant
            // target-clear command, and still never counts as a kill.
            if (!issued_ && !s.selectedGuid && !s.serverVictimGuid &&
                s.attackKnown && !s.attackActive)
            {
                if (s.nowMs-started_<VerificationMs)
                    return {CombatTerminalAction::Observe,
                        "verifying_already_clear_own_target"};
                return s.mandatoryObjective
                    ? CombatTerminalDecision{CombatTerminalAction::OwnerFailure,
                        "mandatory_combat_liveness_exhausted"}
                    : CombatTerminalDecision{CombatTerminalAction::Abandoned,
                        "own_attack_and_selection_verified_clear"};
            }
            if (issued_)
            {
                if (s.nowMs>dispatched_ && !s.selectedGuid && !s.serverVictimGuid &&
                    s.attackKnown && !s.attackActive)
                    return s.mandatoryObjective
                        ? CombatTerminalDecision{CombatTerminalAction::OwnerFailure,
                            "mandatory_combat_liveness_exhausted"}
                        : CombatTerminalDecision{CombatTerminalAction::Abandoned,
                            "own_attack_and_selection_verified_clear"};
                if (s.nowMs-dispatched_>=VerificationMs)
                    return {CombatTerminalAction::SystemFail,"target_abandon_not_confirmed"};
                return {CombatTerminalAction::Observe,"verifying_own_target_release"};
            }
            if (s.selectedGuid!=guid_ || (s.serverVictimGuid && s.serverVictimGuid!=guid_) || !s.attackKnown)
                return postContainment_
                    ? CombatTerminalDecision{CombatTerminalAction::Observe,"awaiting_own_target_release_evidence"}
                    : CombatTerminalDecision{CombatTerminalAction::SystemFail,"terminal_target_ownership_unknown"};
            if (s.nowMs-started_>=VerificationMs)
                return {CombatTerminalAction::StopAndClearOwnTarget,"target_safe_to_release"};
            return {};
        }
    };
}
