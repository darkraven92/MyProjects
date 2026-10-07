#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Navigation
{
    constexpr bool SameNavMeshGeneration(std::uint64_t evidence, std::uint64_t current)
    {
        return evidence != 0 && evidence == current;
    }

    struct NavMeshCacheIdentity
    {
        std::uint32_t map = 0;
        std::string directory;
        std::string mapParameters;
        std::uint64_t metadataStamp = 0;
        std::uint64_t metadataSize = 0;
        bool operator==(const NavMeshCacheIdentity&) const = default;
    };

    struct NavMeshTileIdentity
    {
        std::uint64_t stamp = 0;
        std::uint64_t size = 0;
        bool operator==(const NavMeshTileIdentity&) const = default;
    };

    // One current world/map topology, not a route cache. Entry handles own
    // storage but become unusable on invalidation, even if a route still holds
    // one. All entry access (including queries/temporary flag masks) is under
    // Mutex(). A private query/corridor belongs to each provider/follower.
    template<class Topology>
    class MapNavMeshSessionCache
    {
    public:
        struct Tile
        {
            NavMeshTileIdentity identity;
            std::uint64_t reference = 0;
        };
        struct Entry
        {
            NavMeshCacheIdentity identity;
            std::uint64_t generation = 0;
            bool valid = true;
            Topology topology{};
            std::unordered_map<std::string, Tile> tiles;
            std::uint64_t cacheHits = 0;
            std::uint64_t coldTileLoads = 0;
        };
        using Handle = std::shared_ptr<Entry>;

        std::recursive_mutex& Mutex() { return mutex_; }
        Handle Current() const { return current_; }
        bool IsCurrent(const Handle& handle) const
        {
            return handle && handle->valid && handle == current_;
        }
        Handle Acquire(const NavMeshCacheIdentity& identity)
        {
            std::lock_guard lock(mutex_);
            if (current_ && current_->identity == identity)
                return current_;
            Invalidate();
            current_ = std::make_shared<Entry>();
            current_->identity = identity;
            current_->generation = ++generation_;
            return current_;
        }
        void Invalidate()
        {
            std::lock_guard lock(mutex_);
            if (current_) current_->valid = false;
            current_.reset();
        }
    private:
        std::recursive_mutex mutex_;
        Handle current_;
        std::uint64_t generation_ = 0;
    };
}
