#pragma once

#include "LivingWaterBlockPolicy.h"
#include "../Navigation/LocalRecoveryLimits.h"
#include <cmath>
#include <cstdint>

namespace Bot
{
    struct WaterEgressSample
    {
        bool valid = false, living = false, deathOwns = false;
        bool swimmingKnown = false, swimming = false, ownerSafe = false;
        std::uint64_t guid = 0, manager = 0, player = 0, nowMs = 0;
        float x = 0, y = 0, z = 0;
    };

    enum class WaterEgressAction { None, Backtrack, Stop };
    struct WaterEgressDecision
    {
        WaterEgressAction action = WaterEgressAction::None;
        const char* event = nullptr;
        const char* reason = "none";
        bool entered = false, recovered = false, deathHandoff = false;
        unsigned proof = 0;
    };

    // A local return to an observed non-swimming position, never a water route.
    // Non-swimming does not prove ground contact, surface or breath safety.
    class LivingWaterEmergencyPolicy
    {
        LivingWaterBlockPolicy block_;
        WaterEgressSample anchor_{};
        bool anchorKnown_ = false, started_ = false, failed_ = false;
        bool moving_ = false, stopPending_ = false, attemptProgress_ = false;
        unsigned attempts_ = 0;
        std::uint64_t startedMs_ = 0, attemptMs_ = 0, lastSampleMs_ = 0;
        float bestDistance_ = 0;
        const char* failure_ = "none";

        static bool Finite(const WaterEgressSample& s)
        { return std::isfinite(s.x) && std::isfinite(s.y) && std::isfinite(s.z); }
        bool SameWorld(const WaterEgressSample& s) const
        { return s.guid == anchor_.guid && s.manager == anchor_.manager && s.player == anchor_.player; }
        float Distance(const WaterEgressSample& s) const
        { return std::hypot(s.x - anchor_.x, s.y - anchor_.y); }
        WaterEgressDecision Fail(const char* reason)
        {
            failed_ = true;
            failure_ = reason;
            moving_ = false;
            return {WaterEgressAction::Stop, "failed", reason};
        }
    public:
        static constexpr unsigned MaximumAttempts = Navigation::LocalRecoveryLimits::MaximumLastSafeBacktracks;
        // Existing local backtrack stall window: 18 * 250 ms. An absolute
        // deadline also bounds continuously moving but unsuccessful attempts.
        static constexpr std::uint64_t AttemptMs = 4500;
        static constexpr std::uint64_t MaximumEpisodeMs = MaximumAttempts * AttemptMs;
        static constexpr std::uint64_t MaximumAnchorAgeMs = AttemptMs;
        // Existing surface-recovery local distance/vertical limits.
        static constexpr float MaximumDistance = 12.0f, MaximumVerticalDelta = 4.0f;
        static constexpr float MinimumHorizontalDistance = 0.1f;
        static constexpr float ProgressDistance = 0.35f;

        bool Blocked() const { return block_.Blocked(); }
        bool AnchorKnown() const { return anchorKnown_; }
        unsigned Attempts() const { return attempts_; }
        const WaterEgressSample& Destination() const { return anchor_; }
        bool CanDispatch(const WaterEgressSample& s) const
        {
            return block_.Blocked() && started_ && !failed_ && moving_ &&
                s.valid && s.living && !s.deathOwns && s.swimmingKnown &&
                s.swimming && s.ownerSafe && anchorKnown_ && SameWorld(s) && Finite(s) &&
                s.nowMs >= startedMs_ && s.nowMs - startedMs_ < MaximumEpisodeMs &&
                Distance(s) >= MinimumHorizontalDistance && Distance(s) <= MaximumDistance &&
                std::fabs(s.z - anchor_.z) <= MaximumVerticalDelta;
        }
        void InvalidateWorld()
        {
            anchorKnown_ = false;
            block_.InvalidateProof();
            if (started_ && !failed_)
            {
                failed_ = true;
                failure_ = "world_gap_or_identity_change";
                stopPending_ = true; // use a new valid player, never a stale pointer
            }
        }
        void DispatchFailed()
        {
            failed_ = true;
            failure_ = "backtrack_dispatch_failed";
            stopPending_ = true;
        }

        WaterEgressDecision Update(const WaterEgressSample& s)
        {
            if (!s.valid || !s.guid || !s.manager || !s.player)
            { InvalidateWorld(); return {}; }
            if (anchorKnown_ && !SameWorld(s)) InvalidateWorld();
            if (s.deathOwns)
            {
                const bool held = block_.Blocked();
                block_.Observe(true, false, false);
                anchorKnown_ = started_ = failed_ = moving_ = stopPending_ = false;
                attempts_ = 0;
                return {WaterEgressAction::None, held ? "death_handoff" : nullptr,
                    "death_recovery_authoritative", false, false, held};
            }
            // Duplicate observations cannot advance the exit proof.
            if (s.nowMs <= lastSampleMs_) return {};
            lastSampleMs_ = s.nowMs;
            const auto beforeProof = block_.NonSwimmingSamples();
            const auto event = block_.Observe(false,
                s.swimmingKnown && (s.swimming || (s.living && Finite(s))), s.swimming);
            if (!Finite(s))
            {
                InvalidateWorld();
                // Bad coordinates cannot turn a known SWIMMING bit into
                // permission for ordinary work. The adapter will fail its
                // neutralization guard rather than command an invalid point.
                if (block_.Blocked())
                {
                    auto invalid = Fail("position_unknown");
                    stopPending_ = false;
                    invalid.entered = event == LivingWaterBlockEvent::Entered;
                    return invalid;
                }
                return {};
            }
            if (event == LivingWaterBlockEvent::ExitedNonSwimming)
            {
                const char* reason = failed_ ? "manual_non_swimming_confirmed" : "three_non_swimming_observations";
                started_ = moving_ = failed_ = stopPending_ = anchorKnown_ = false;
                return {WaterEgressAction::Stop, "recovered", reason, false, true, false,
                    LivingWaterBlockPolicy::ExitSamples};
            }
            if (!block_.Blocked())
            {
                if (s.living && s.swimmingKnown && !s.swimming)
                { anchor_ = s; anchorKnown_ = true; }
                else anchorKnown_ = false;
                return {};
            }
            const bool entered = event == LivingWaterBlockEvent::Entered;
            if (entered)
            {
                started_ = true; failed_ = moving_ = false; attempts_ = 0;
                startedMs_ = s.nowMs;
            }
            WaterEgressDecision d;
            if (stopPending_)
            {
                stopPending_ = false;
                d = Fail(failure_);
            }
            else if (failed_) // terminal until externally verified exit/death; no retry spin
            {
                if (block_.NonSwimmingSamples() != beforeProof && block_.NonSwimmingSamples())
                    d = {WaterEgressAction::None, "non_swimming_candidate", "manual_recovery",
                        false, false, false, block_.NonSwimmingSamples()};
            }
            else if (!s.living || !s.swimmingKnown)
            { anchorKnown_ = false; d = Fail("living_or_swimming_unknown"); }
            else if (!s.ownerSafe) d = Fail("higher_priority_owner_or_unknown_threat");
            else if (!anchorKnown_ || !SameWorld(s)) d = Fail("no_same_world_non_swimming_anchor");
            else if (entered && (s.nowMs < anchor_.nowMs || s.nowMs - anchor_.nowMs > MaximumAnchorAgeMs))
                d = Fail("anchor_expired");
            else if (Distance(s) > MaximumDistance || std::fabs(s.z - anchor_.z) > MaximumVerticalDelta)
                d = Fail("anchor_outside_local_backtrack_bounds");
            else if (s.swimming && Distance(s) < MinimumHorizontalDistance)
                d = Fail("no_horizontal_backtrack"); // never attempt vertical-only swimming
            else if (!s.swimming)
            {
                d = {moving_ ? WaterEgressAction::Stop : WaterEgressAction::None,
                    "non_swimming_candidate", "await_three_observations", false, false, false,
                    block_.NonSwimmingSamples()};
                moving_ = false;
            }
            else if (s.nowMs - startedMs_ >= MaximumEpisodeMs) d = Fail("episode_deadline");
            else if (!moving_ || s.nowMs - attemptMs_ >= AttemptMs)
            {
                if (attempts_ >= MaximumAttempts) d = Fail("backtrack_budget_exhausted");
                else
                {
                    const char* reason = attempts_ == 0 ? "observed_non_swimming_backtrack" :
                        attemptProgress_ ? "progress_without_exit" : "no_progress";
                    ++attempts_; moving_ = true; attemptMs_ = s.nowMs; attemptProgress_ = false;
                    bestDistance_ = Distance(s);
                    d = {WaterEgressAction::Backtrack, "attempt", reason};
                }
            }
            else if (Distance(s) + ProgressDistance < bestDistance_)
            {
                bestDistance_ = Distance(s);
                if (!attemptProgress_)
                    d = {WaterEgressAction::None, "progress", "distance_decreased"};
                attemptProgress_ = true;
            }
            d.entered = entered;
            return d;
        }
    };
}
