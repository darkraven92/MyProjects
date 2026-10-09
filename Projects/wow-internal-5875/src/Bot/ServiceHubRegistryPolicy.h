#pragma once

#include "ServiceHubSelectionPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace Bot
{
    // The file is scoped to map 001 by its caller. V1 rows mean a verified
    // merchant with unknown repair; V2 retains observed sell/repair knowledge.
    struct ServiceHubRegistryPolicy
    {
        static constexpr std::uint32_t Version = 2;

        static void Load(std::istream& input,
                         std::vector<ServiceHubCandidate>& hubs,
                         std::unordered_set<std::uint32_t>& verifiedEntries)
        {
            std::string line;
            while (std::getline(input, line))
            {
                std::istringstream row(line);
                std::string tag;
                std::uint32_t version = 0;
                ServiceHubCandidate hub{};
                unsigned sell = 0;
                unsigned repair = 0;
                if (!(row >> tag >> version >> hub.entry >> hub.x >> hub.y >> hub.z) ||
                    tag != "V" || (version != 1 && version != Version) ||
                    hub.entry == 0 || !std::isfinite(hub.x) ||
                    !std::isfinite(hub.y) || !std::isfinite(hub.z))
                    continue;
                if (version == Version)
                {
                    if (!(row >> sell >> repair) || sell > 2 || repair > 2)
                        continue;
                    hub.sell = static_cast<ServiceKnowledge>(sell);
                    hub.repair = static_cast<ServiceKnowledge>(repair);
                }
                else
                    hub.sell = ServiceKnowledge::Available;
                hub.positionKnown = true;
                hub.source = ServiceHubSource::Persisted;
                auto existing = std::find_if(hubs.begin(), hubs.end(),
                    [&](const auto& item) { return item.entry == hub.entry; });
                if (existing == hubs.end())
                    hubs.push_back(hub);
                else
                    *existing = hub;
                verifiedEntries.insert(hub.entry);
            }
        }

        static void Write(std::ostream& output,
                          const std::vector<ServiceHubCandidate>& hubs,
                          const std::unordered_set<std::uint32_t>& verifiedEntries)
        {
            for (const auto& hub : hubs)
            {
                if (!hub.positionKnown ||
                    verifiedEntries.find(hub.entry) == verifiedEntries.end())
                    continue;
                output << "V\t" << Version << '\t' << hub.entry << '\t'
                       << std::setprecision(9) << hub.x << '\t' << hub.y << '\t'
                       << hub.z << '\t' << static_cast<unsigned>(hub.sell)
                       << '\t' << static_cast<unsigned>(hub.repair) << '\n';
            }
        }
    };
}
