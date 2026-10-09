#pragma once
#include <cstdint>
#include <set>

namespace Bot
{
    // A live-log-confirmed acceptance, not a static candidate, earns one
    // same-actor reread. The Vanilla quest-log capacity bounds a whole sweep.
    class QuestGiverOfferCycle
    {
        std::set<int> accepted_{};
        std::uint64_t pendingGuid_=0;
    public:
        bool Accepted(int questId, std::uint64_t giverGuid)
        {
            if (questId<=0 || giverGuid==0 || accepted_.size()>=20 ||
                !accepted_.insert(questId).second) return false;
            pendingGuid_=giverGuid;
            return true;
        }
        std::uint64_t TakeReaudit()
        {
            const auto guid=pendingGuid_;
            pendingGuid_=0;
            return guid;
        }
    };
}
