#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Navigation
{
    // Detour 5875's findPath expands bestPoly->firstLink and explicitly skips
    // a link with ref==0. Mask only links owned by the rejected FROM polygon;
    // an unrelated C->B link is left intact. The provider holds the session
    // cache lock and restores these values before releasing it.
    struct DirectedTransitionQueryPolicy
    {
        template<class Link>
        struct SavedLink
        {
            Link* link = nullptr;
            std::uint64_t ref = 0;
        };

        template<class Link>
        static bool Mask(Link* links, unsigned int first,
                         unsigned int linkCapacity, unsigned int nullLink,
                         std::uint64_t target,
                         std::vector<SavedLink<Link>>& saved)
        {
            if (!links || target == 0 || linkCapacity == 0)
                return false;
            unsigned int index = first;
            unsigned int visited = 0;
            bool found = false;
            while (index != nullLink)
            {
                if (index >= linkCapacity || ++visited > linkCapacity)
                    return false; // malformed/cyclic chain: caller restores
                Link* link = &links[index];
                if (static_cast<std::uint64_t>(link->ref) == target)
                {
                    saved.push_back({link,
                        static_cast<std::uint64_t>(link->ref)});
                    link->ref = 0;
                    found = true;
                }
                index = link->next;
            }
            return found;
        }

        template<class Link>
        static void Restore(std::vector<SavedLink<Link>>& saved)
        {
            for (auto it = saved.rbegin(); it != saved.rend(); ++it)
                it->link->ref = static_cast<decltype(it->link->ref)>(it->ref);
            saved.clear();
        }
    };
}
