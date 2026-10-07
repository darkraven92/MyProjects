#pragma once
#include <algorithm>
#include <cstdint>

namespace Bot
{
    enum class CombatStallClass
    {
        Healthy, TargetSelectionDesync, AutoattackLatchDesync, FacingBlock,
        RangeBlock, MovementOwnerConflict, LegitimateActionWait,
        OffensiveNoProgress, TargetEnded, UnknownOrStale
    };
    enum class CombatRecoveryAction { None, RestoreTarget, ReengageAttack, RefreshAttack, Fail };
    inline const char* CombatStallName(CombatStallClass c)
    {
        switch (c)
        {
            case CombatStallClass::Healthy: return "healthy";
            case CombatStallClass::TargetSelectionDesync: return "target_selection_desync";
            case CombatStallClass::AutoattackLatchDesync: return "autoattack_latch_desync";
            case CombatStallClass::FacingBlock: return "facing_block";
            case CombatStallClass::RangeBlock: return "range_block";
            case CombatStallClass::MovementOwnerConflict: return "movement_owner_conflict";
            case CombatStallClass::LegitimateActionWait: return "legitimate_action_wait";
            case CombatStallClass::OffensiveNoProgress: return "offensive_no_progress";
            case CombatStallClass::TargetEnded: return "target_invalid_or_dead";
            case CombatStallClass::UnknownOrStale: return "unknown_or_stale_evidence";
        }
        return "unknown";
    }
    inline const char* CombatRecoveryName(CombatRecoveryAction a)
    {
        switch (a)
        {
            case CombatRecoveryAction::None: return "none";
            case CombatRecoveryAction::RestoreTarget: return "restore_target";
            case CombatRecoveryAction::ReengageAttack: return "reengage_attack";
            case CombatRecoveryAction::RefreshAttack: return "hard_stall_refresh";
            case CombatRecoveryAction::Fail: return "bounded_failure";
        }
        return "unknown";
    }
    struct CombatLivenessSample
    {
        std::uint64_t nowMs=0, sampleTick=0, targetGuid=0, selectedGuid=0;
        std::uint32_t targetObject=0, targetHp=0, playerHp=0, attackPeriodMs=0;
        bool fresh=false, alive=false, targetValid=false, selectionKnown=false;
        bool melee=false, facing=false, inputSafe=false, actionWait=false;
        bool actionKnown=false, attackActive=false;
    };
    struct CombatLivenessDecision
    {
        CombatStallClass classification=CombatStallClass::Healthy;
        CombatRecoveryAction action=CombatRecoveryAction::None;
        CombatRecoveryAction verified=CombatRecoveryAction::None;
    };
    // Structural progress (selection/facing/latch) is NOT damage progress.
    // Existing 250 ms poll / 1 s latch probe / 4 s soft / 8 s hard-stall
    // windows anchor these limits. Slow weapons get two full swing periods.
    class CombatLivenessPolicy
    {
        std::uint64_t guid_=0, tick_=0, lastDamageMs_=0, eligibleSinceMs_=0, dispatchedMs_=0;
        std::uint32_t object_=0, hp_=0, playerHp_=0;
        unsigned repairs_=0;
        bool sampled_=false, eligible_=false, losingHealth_=false, refreshed_=false;
        CombatRecoveryAction pending_=CombatRecoveryAction::None;
    public:
        static constexpr unsigned MaximumRepairs=3;
        static constexpr std::uint64_t StructuralVerificationMs=1000;
        void Reset() { *this=CombatLivenessPolicy{}; }
        unsigned Repairs() const { return repairs_; }
        bool Pending() const { return pending_!=CombatRecoveryAction::None; }
        std::uint64_t NoDamageMs(std::uint64_t now) const
        { return sampled_ && now>=lastDamageMs_ ? now-lastDamageMs_ : 0; }
        void Dispatched(CombatRecoveryAction action, std::uint64_t now)
        {
            if (action==CombatRecoveryAction::None || action==CombatRecoveryAction::Fail) return;
            ++repairs_; pending_=action; dispatchedMs_=now;
            if (action==CombatRecoveryAction::RefreshAttack) refreshed_=true;
        }
        CombatLivenessDecision Observe(const CombatLivenessSample& s)
        {
            CombatLivenessDecision d{};
            if (!s.alive || !s.targetValid || !s.targetGuid)
            { Reset(); d.classification=CombatStallClass::TargetEnded; return d; }
            if (!s.fresh || !s.selectionKnown || (sampled_ && s.sampleTick<=tick_))
            {
                // A freshness gap discards timing, NOT spent recovery budget.
                sampled_=false; eligible_=false;
                d.classification=CombatStallClass::UnknownOrStale; return d;
            }
            if (!s.targetHp)
            { Reset(); d.classification=CombatStallClass::TargetEnded; return d; }
            if (guid_!=s.targetGuid || object_!=s.targetObject) Reset();
            const bool damage=sampled_ && s.targetHp<hp_;
            if (!sampled_ || damage)
            {
                lastDamageMs_=s.nowMs;
                if (damage)
                {
                    repairs_=0; refreshed_=false; losingHealth_=false;
                    if (pending_==CombatRecoveryAction::RefreshAttack ||
                        (pending_==CombatRecoveryAction::ReengageAttack && s.selectedGuid==s.targetGuid))
                    { d.verified=pending_; pending_=CombatRecoveryAction::None; }
                }
            }
            if (sampled_ && s.playerHp<playerHp_) losingHealth_=true;
            guid_=s.targetGuid; object_=s.targetObject; tick_=s.sampleTick;
            hp_=s.targetHp; playerHp_=s.playerHp; sampled_=true;
            const auto window=std::max<std::uint64_t>(losingHealth_ ? 4000 : 8000,
                (losingHealth_ ? 1u : 2u)*std::uint64_t(s.attackPeriodMs)+1000);
            if (!s.inputSafe) d.classification=CombatStallClass::MovementOwnerConflict;
            else if (s.actionWait) d.classification=CombatStallClass::LegitimateActionWait;
            else if (s.selectedGuid!=guid_) d.classification=CombatStallClass::TargetSelectionDesync;
            else if (!s.melee) d.classification=CombatStallClass::RangeBlock;
            else if (!s.facing) d.classification=CombatStallClass::FacingBlock;
            else if (s.actionKnown && !s.attackActive) d.classification=CombatStallClass::AutoattackLatchDesync;
            const bool allowed=s.inputSafe && !s.actionWait && s.melee;
            if (!allowed || !s.facing)
                eligible_=false;
            else if (!eligible_)
            { eligible_=true; eligibleSinceMs_=s.nowMs; }
            if (pending_!=CombatRecoveryAction::None)
            {
                const bool structural=(pending_==CombatRecoveryAction::RestoreTarget && s.selectedGuid==guid_) ||
                    (pending_==CombatRecoveryAction::ReengageAttack && s.actionKnown && s.attackActive);
                if (structural)
                { d.verified=pending_; pending_=CombatRecoveryAction::None; }
                else if (s.nowMs-dispatchedMs_ < (pending_==CombatRecoveryAction::RefreshAttack ? window : StructuralVerificationMs))
                    return d;
                else pending_=CombatRecoveryAction::None;
            }
            if (!s.inputSafe || s.actionWait) return d;
            if (d.classification==CombatStallClass::TargetSelectionDesync)
                d.action=CombatRecoveryAction::RestoreTarget;
            else if (allowed && s.facing && d.classification==CombatStallClass::AutoattackLatchDesync)
                d.action=CombatRecoveryAction::ReengageAttack;
            else if (s.facing && eligible_ && NoDamageMs(s.nowMs)>=window &&
                     s.nowMs-eligibleSinceMs_>=window)
            {
                d.classification=CombatStallClass::OffensiveNoProgress;
                d.action=refreshed_ ? CombatRecoveryAction::Fail : CombatRecoveryAction::RefreshAttack;
            }
            if (d.action!=CombatRecoveryAction::None && repairs_>=MaximumRepairs)
                d.action=CombatRecoveryAction::Fail;
            return d;
        }
    };
}
