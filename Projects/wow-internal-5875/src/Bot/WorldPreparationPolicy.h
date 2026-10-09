#pragma once

#include "LevelingProfile.h"

namespace Bot
{
    struct WorldPreparationResult
    {
        std::vector<std::string> unknown, unmet;
        bool Ready() const { return unknown.empty() && unmet.empty(); }
        bool operator==(const WorldPreparationResult&) const = default;
    };

    class WorldPreparationPolicy
    {
    public:
        static WorldPreparationResult Evaluate(const LevelingProfileSegment& segment,
                                               const ProfileWorldEvidence& evidence)
        {
            WorldPreparationResult result;
            for (const auto& key : segment.preparationRequirements)
            {
                const auto fact = evidence.preparationFacts.find(key);
                if (!evidence.QualifiedIdentity() ||
                    fact == evidence.preparationFacts.end() || !fact->second)
                    result.unknown.push_back(key);
                else if (!*fact->second) result.unmet.push_back(key);
            }
            std::sort(result.unknown.begin(), result.unknown.end());
            std::sort(result.unmet.begin(), result.unmet.end());
            return result;
        }
    };
}
