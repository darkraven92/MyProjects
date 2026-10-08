#pragma once
#include "CombatLivenessPolicy.h"
#include <cstdint>

namespace Bot
{
    enum class CombatTerminalAction { Observe, StopAndClearOwnTarget, ClearOwnSelection, Abandoned, OwnerFailure, HostileReturned, SystemFail };
    struct CombatTerminalSample
    {
        std::uint64_t nowMs=0, targetGuid=0, selectedGuid=0, serverVictimGuid=0;
        std::uint32_t playerHp=0;
        bool optionalGrind=false, mandatoryObjective=false, known=false, inputSafe=false;
        bool selectionInputSafe=false, episodeAttackOwnershipEstablished=false;
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
        bool observing_=false, issued_=false, postContainment_=false, selectionOnly_=false;
    public:
        static constexpr std::uint64_t VerificationMs=CombatLivenessPolicy::StructuralVerificationMs;
        static constexpr std::uint64_t PostContainmentReleaseMs=VerificationMs*4;
        void Reset() { *this=CombatTerminalPolicy{}; }
        bool Issued() const { return issued_; }
        void Dispatched(std::uint64_t now,
            CombatTerminalAction action=CombatTerminalAction::StopAndClearOwnTarget)
        {
            issued_=true; dispatched_=now;
            selectionOnly_=action==CombatTerminalAction::ClearOwnSelection;
        }
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
            // A target selected before this combat episode is not proof that
            // this controller ever owned Attack. The independent selection
            // probe must still prove input is safe before a guarded clear.
            const bool attackOwned=s.episodeAttackOwnershipEstablished ||
                (s.attackKnown && s.attackActive);
            const bool releaseInputSafe=postContainment_ && !attackOwned
                ? (s.selectionInputSafe || s.inputSafe) : s.inputSafe;
            if (!releaseInputSafe)
                return postContainment_
                    ? CombatTerminalDecision{CombatTerminalAction::Observe,"awaiting_release_input_evidence"}
                    : CombatTerminalDecision{CombatTerminalAction::SystemFail,"terminal_action_input_conflict"};
            // Defensive escape may have stopped and cleared our own Attack
            // before this terminal verifier ran. Confirming all three native
            // ownership fields clear is stronger than dispatching a redundant
            // target-clear command, and still never counts as a kill.
            if (!issued_ && !s.selectedGuid && !s.serverVictimGuid &&
                (!postContainment_ || attackOwned ? (s.attackKnown && !s.attackActive) : true))
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
                if (postContainment_ && s.selectedGuid && s.selectedGuid!=guid_)
                    return {CombatTerminalAction::SystemFail,"selection_changed_to_unrelated"};
                if (selectionOnly_ && s.attackKnown && s.attackActive)
                    return {CombatTerminalAction::SystemFail,"attack_active_after_selection_clear"};
                if (s.nowMs>dispatched_ && !s.selectedGuid && !s.serverVictimGuid &&
                    (selectionOnly_ ? !s.attackKnown || !s.attackActive :
                        s.attackKnown && !s.attackActive))
                    return s.mandatoryObjective
                        ? CombatTerminalDecision{CombatTerminalAction::OwnerFailure,
                            "mandatory_combat_liveness_exhausted"}
                        : CombatTerminalDecision{CombatTerminalAction::Abandoned,
                            selectionOnly_ ? "selection_clear_victim_clear" :
                                "own_attack_and_selection_verified_clear"};
                if (s.nowMs-dispatched_>=VerificationMs)
                    return {CombatTerminalAction::SystemFail,
                        selectionOnly_ ? (s.selectedGuid ? "selection_clear_failed" :
                            "selection_clear_victim_not_confirmed") :
                            "target_abandon_not_confirmed"};
                return {CombatTerminalAction::Observe,
                    selectionOnly_ && !s.selectedGuid && s.serverVictimGuid
                        ? "selection_clear_victim_stale" : "verifying_own_target_release"};
            }
            if (postContainment_ && !attackOwned)
            {
                if (s.selectedGuid && s.selectedGuid!=guid_)
                    return {CombatTerminalAction::SystemFail,"post_containment_unrelated_selection"};
                if (!s.selectedGuid)
                    return {CombatTerminalAction::Observe,"selection_clear_victim_stale"};
                if (s.serverVictimGuid && s.serverVictimGuid!=guid_)
                    return {CombatTerminalAction::SystemFail,"post_containment_victim_conflict"};
                if (s.nowMs-started_>=VerificationMs)
                    return {CombatTerminalAction::ClearOwnSelection,
                        "no_episode_attack_guarded_selection_clear"};
                return {CombatTerminalAction::Observe,"verifying_selection_clear_eligibility"};
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
