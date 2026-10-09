#pragma once
#include <cstdint>
#include <map>

namespace Bot
{
    // Session-local, caller tick domain; never persisted or made permanent.
    class ServiceHubBackoffPolicy
    {
        std::map<std::uint32_t, std::uint64_t> until_;
    public:
        void Prune(std::uint64_t tick)
        {
            for (auto it = until_.begin(); it != until_.end();)
                if (tick >= it->second) it = until_.erase(it); else ++it;
        }
        bool Blocked(std::uint32_t entry, std::uint64_t tick) const
        {
            const auto it = until_.find(entry);
            return it != until_.end() && tick < it->second;
        }
        void Reject(std::uint32_t entry, std::uint64_t tick, std::uint64_t cooldown)
        {
            Prune(tick);
            if (entry) until_[entry] = tick + cooldown;
        }
    };
}
