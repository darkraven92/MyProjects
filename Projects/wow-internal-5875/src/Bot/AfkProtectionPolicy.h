#pragma once

#include <cstdint>

namespace Bot
{
    enum class AfkStatus { Unknown, Active, ApproachingThreshold, AfkDetected,
        ClearingAfk, VerifiedClear, Blocked, Fault };
    enum class AfkAction { None, InputPulse, ClearFlag, NativeAutoClear };
    enum class AfkAutoClearSetting { Unknown, Disabled, Enabled };
    enum class AfkResult { None, Pending, Confirmed, Failed };
    struct AfkObservation
    {
        bool known = false;
        bool serverAfk = false;
        bool clientAfk = false;
        std::uint32_t clientNow = 0;
        std::uint32_t lastInput = 0;
        // Read from the signature-validated client's actual idle check.
        // This is source evidence; it is not a measured server timeout.
        std::uint32_t thresholdMs = 0;
        const char* evidenceReason = "not_read";
    };
    struct AfkSafety
    {
        bool healthyIdle = false;
        bool combat = false, navigation = false, death = false, recovery = false;
        bool water = false, dialog = false, vendor = false, trainer = false;
        bool talents = false, equipment = false, loot = false, fault = false;
        bool Safe() const
        {
            return healthyIdle && !combat && !navigation && !death && !recovery &&
                !water && !dialog && !vendor && !trainer && !talents &&
                !equipment && !loot && !fault;
        }
    };
    struct AfkDecision
    {
        AfkStatus status = AfkStatus::Unknown;
        AfkAction action = AfkAction::None;
        AfkResult result = AfkResult::None;
        const char* reason = "observation_unknown";
    };

    // A failed candidate is latched for this session. Neither CTM nor physical
    // displacement resets this policy: only the client's input clock does.
    class AfkProtectionPolicy
    {
        AfkAction pending_ = AfkAction::None;
        std::uint32_t priorInput_ = 0;
        std::uint64_t issuedAt_ = 0;
        bool fault_ = false;
        bool inputVerified_ = false;
    public:
        static constexpr std::uint64_t VerificationMs = 3000;
        static bool Valid(const AfkObservation& o)
        {
            return o.known && o.thresholdMs >= 10000 && o.thresholdMs <= 3600000 &&
                std::uint32_t(o.clientNow-o.lastInput) < 0x80000000u;
        }
        static const char* ObservationReason(const AfkObservation& o)
        {
            if (!o.known) return o.evidenceReason;
            if (o.thresholdMs<10000 || o.thresholdMs>3600000) return "threshold_invalid";
            if (std::uint32_t(o.clientNow-o.lastInput)>=0x80000000u) return "input_clock_ahead_or_discontinuous";
            return "verified_native_read";
        }
        AfkDecision Update(const AfkObservation& o, const AfkSafety& safety,
            std::uint64_t now, bool observeOnly = false)
        {
            if (fault_) return {AfkStatus::Fault,AfkAction::None,AfkResult::Failed,
                "candidate_failed_session_latched"};
            if (pending_ != AfkAction::None)
            {
                const bool inputChanged = Valid(o) && o.lastInput != priorInput_ &&
                    std::uint32_t(o.clientNow-o.lastInput) <= VerificationMs;
                if (pending_ == AfkAction::InputPulse && inputChanged)
                {
                    pending_=AfkAction::None; inputVerified_=true;
                    return {o.clientAfk || o.serverAfk ? AfkStatus::AfkDetected :
                        AfkStatus::VerifiedClear,AfkAction::None,AfkResult::Confirmed,
                        "client_input_clock_advanced"};
                }
                if (pending_ == AfkAction::ClearFlag && Valid(o) &&
                    !o.clientAfk && !o.serverAfk)
                {
                    pending_=AfkAction::None;
                    return {AfkStatus::VerifiedClear,AfkAction::None,
                        AfkResult::Confirmed,"client_and_server_afk_clear"};
                }
                if (now < issuedAt_ || now-issuedAt_ >= VerificationMs)
                {
                    fault_=true; pending_=AfkAction::None;
                    return {AfkStatus::Fault,AfkAction::None,AfkResult::Failed,
                        "verification_timeout"};
                }
                return {AfkStatus::ClearingAfk,AfkAction::None,AfkResult::Pending,
                    "waiting_live_evidence"};
            }
            if (!Valid(o)) return {};
            const auto age=std::uint32_t(o.clientNow-o.lastInput);
            const bool due=age >= o.thresholdMs-o.thresholdMs/5;
            if (observeOnly)
                return {o.serverAfk || o.clientAfk ? AfkStatus::AfkDetected :
                    AfkStatus::Active,AfkAction::None,AfkResult::None,"observe_only"};
            if (!due && !o.serverAfk && !o.clientAfk)
                return {AfkStatus::Active,AfkAction::None,AfkResult::None,"input_clock_recent"};
            if (!safety.Safe())
                return {AfkStatus::Blocked,AfkAction::None,AfkResult::None,"active_owner_or_unsafe_idle"};
            // Clear only a locally-confirmed AFK flag: an empty AFK chat is a
            // toggle on the server. A mismatching state must never toggle it on.
            if (!due && inputVerified_ && o.clientAfk && o.serverAfk)
                return {AfkStatus::ClearingAfk,AfkAction::ClearFlag,
                    AfkResult::None,"verified_input_then_clear_existing_afk"};
            if (!due && inputVerified_)
                return {AfkStatus::Blocked,AfkAction::None,AfkResult::None,"client_server_afk_disagree"};
            return {AfkStatus::ApproachingThreshold,AfkAction::InputPulse,
                AfkResult::None,"client_input_age_due"};
        }
        void Issued(AfkAction action, const AfkObservation& o, std::uint64_t now)
        {
            pending_=action; priorInput_=o.lastInput; issuedAt_=now;
        }
        void DispatchFailed() { fault_=true; pending_=AfkAction::None; }
        AfkAction PendingAction() const { return pending_; }
        void Reset() { *this=AfkProtectionPolicy{}; }
    };
}
