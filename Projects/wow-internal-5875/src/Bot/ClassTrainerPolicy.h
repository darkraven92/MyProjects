#pragma once
#include "AutonomousMaintenancePolicy.h"
#include <cstdint>

namespace Bot
{
    enum class TrainerDecision { Learn, WrongTrainer, MissingSource, KnownRank,
        PrerequisiteMissing, Unavailable, FrameMissing, InsufficientFunds, MaintenancePending };
    struct TrainerAbilityEvidence
    {
        bool sourceBacked=false, classTrainer=false, classMatches=false;
        bool frameOpen=false, liveAvailable=false, knownSameOrHigher=false, prerequisitesKnown=false;
        std::uint64_t money=0, cost=0, reserve=0;
        bool maintenancePending=false;
    };
    struct ClassTrainerPolicy
    {
        static constexpr std::uint64_t ProbeTicks=40, RetryTicks=2400;
        static constexpr std::uint64_t ConfirmationTicks=40, EpisodeTicks=900;
        static TrainerDecision Evaluate(const TrainerAbilityEvidence& e)
        {
            if (!e.classTrainer || !e.classMatches) return TrainerDecision::WrongTrainer;
            if (!e.sourceBacked) return TrainerDecision::MissingSource;
            if (e.knownSameOrHigher) return TrainerDecision::KnownRank;
            if (!e.prerequisitesKnown) return TrainerDecision::PrerequisiteMissing;
            if (!e.frameOpen) return TrainerDecision::FrameMissing;
            if (!e.liveAvailable) return TrainerDecision::Unavailable;
            if (e.maintenancePending) return TrainerDecision::MaintenancePending;
            if (e.money < e.reserve || e.cost > e.money-e.reserve) return TrainerDecision::InsufficientFunds;
            return TrainerDecision::Learn;
        }
        static std::uint64_t Reserve(std::uint32_t money, std::uint32_t repair, std::uint32_t supplies)
        {
            return std::max<std::uint64_t>(AutonomousMaintenancePolicy::ConsumableReserve(money),
                static_cast<std::uint64_t>(repair)+supplies);
        }
    };
    // Session-local audit state, not a persisted movement/UI owner.
    class TrainerNeedPolicy
    {
        unsigned level_=0, audited_=0;
        std::uint64_t retry_=0;
    public:
        bool ObserveLevel(unsigned level)
        {
            if (!level || level==level_) return false;
            level_=level; audited_=0; retry_=0; return true;
        }
        bool Pending(std::uint64_t tick) const { return level_ && audited_!=level_ && tick>=retry_; }
        void Complete() { audited_=level_; }
        void Defer(std::uint64_t tick) { retry_=tick+ClassTrainerPolicy::RetryTicks; }
        unsigned Level() const { return level_; }
        unsigned AuditedLevel() const { return audited_; }
    };
}
