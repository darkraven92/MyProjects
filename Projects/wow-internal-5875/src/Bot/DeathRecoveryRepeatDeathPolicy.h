#pragma once

#include <cmath>
#include <cstdint>

namespace Bot
{
    // A circuit breaker for an observed outcome, not a hostile detector or a
    // safe-point predicate. Missing evidence leaves existing reclaim unchanged.
    class DeathRecoveryRepeatDeathPolicy
    {
    public:
        struct Point { float x = 0, y = 0, z = 0; };
        // Engineering bounds for "recent / near the confirmed resurrection".
        // Neither is a measured aggro radius or runtime-qualified safety limit.
        static constexpr std::uint64_t RecentReclaimMs = 120000;
        static constexpr float RepeatDeathDistance = 8.0f;

        void Reset() { *this = {}; }

        void RecordAutomaticAlive(std::uint64_t guid, std::uint32_t map,
            Point position, std::uint64_t nowMs)
        {
            Reset();
            if (guid == 0 || !Finite(position)) return;
            guid_ = guid;
            map_ = map;
            position_ = position;
            confirmedMs_ = nowMs;
        }

        void BeginDeath(std::uint64_t guid, std::uint32_t map,
            std::uint64_t nowMs)
        {
            eligible_ = false;
            blocked_ = false;
            ageAtDeathMs_ = 0;
            separation_ = 0;
            if (guid_ == 0 || guid != guid_ || map != map_ ||
                nowMs < confirmedMs_ || nowMs - confirmedMs_ > RecentReclaimMs)
            {
                Reset();
                return;
            }
            // Freeze time eligibility at death, not after release/routing.
            ageAtDeathMs_ = nowMs - confirmedMs_;
            eligible_ = true;
        }

        // Caller supplies only an observed dead body or current server corpse,
        // never a Ghost's position or a persisted/last-healthy routing anchor.
        bool ObserveCorpse(Point corpse)
        {
            if (!eligible_ || blocked_ || !Finite(corpse)) return false;
            const double dx = double(corpse.x) - position_.x;
            const double dy = double(corpse.y) - position_.y;
            const double dz = double(corpse.z) - position_.z;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance > RepeatDeathDistance) return false;
            separation_ = static_cast<float>(distance);
            blocked_ = true;
            return true;
        }

        bool ShouldBlock(bool freshConfirmedGhost) const
        {
            return blocked_ && freshConfirmedGhost;
        }
        bool Latched() const { return blocked_; }
        std::uint64_t AgeAtDeathMs() const { return ageAtDeathMs_; }
        float Separation() const { return separation_; }
        Point ConfirmedPosition() const { return position_; }

    private:
        static bool Finite(Point p)
        {
            return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
        }
        std::uint64_t guid_ = 0;
        std::uint32_t map_ = 0;
        Point position_{};
        std::uint64_t confirmedMs_ = 0;
        std::uint64_t ageAtDeathMs_ = 0;
        float separation_ = 0;
        bool eligible_ = false;
        bool blocked_ = false;
    };
}
