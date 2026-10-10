#pragma once

#include "ProfileWorldEvidence.h"

#include <algorithm>
#include <compare>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Bot
{
    struct ProfileSegmentId
    {
        std::string profileId, segmentId;
        auto operator<=>(const ProfileSegmentId&) const = default;
    };

    struct ProfileGrindRegion
    {
        std::string label;
        ProfilePosition minimum, maximum; // Inclusive axis-aligned XYZ box.
        bool constrainSelection = false; // Otherwise informational metadata.
        bool Valid() const
        {
            return minimum.Finite() && maximum.Finite() &&
                minimum.x <= maximum.x && minimum.y <= maximum.y &&
                minimum.z <= maximum.z;
        }
        bool Contains(const ProfilePosition& p) const
        {
            return p.Finite() && p.x >= minimum.x && p.x <= maximum.x &&
                p.y >= minimum.y && p.y <= maximum.y &&
                p.z >= minimum.z && p.z <= maximum.z;
        }
        bool operator==(const ProfileGrindRegion&) const = default;
    };

    struct LevelingProfileSegment
    {
        std::string id;
        std::uint32_t minimumLevel = 0, maximumLevel = 0;
        std::optional<std::uint32_t> expectedMap;
        std::optional<std::uint32_t> expectedZone; // Reserved, fail closed.
        std::optional<std::uint32_t> allowedRaces, allowedClasses;
        std::optional<ProfileFactionGroup> expectedFactionGroup; // Reserved.
        std::vector<std::uint32_t> allowedTargetEntries;
        // Existing UnitClassification tokens, not creature types.
        std::vector<std::string> allowedTargetClassifications;
        std::optional<ProfileGrindRegion> grindRegion;
        std::vector<std::string> preparationRequirements;
        std::vector<std::uint32_t> questIds; // References only, no execution.
        // Authored hint only; never followed automatically or used as evidence
        // of completion. Fresh selection determines the actual next segment.
        std::optional<std::string> nextSegment;
        bool operator==(const LevelingProfileSegment&) const = default;
    };

    struct LevelingProfile
    {
        std::string id;
        std::vector<LevelingProfileSegment> segments;
        // Opt-in owner-supplied fact. No implicit level/quest completion.
        std::optional<std::string> completionFact;
        bool operator==(const LevelingProfile&) const = default;
    };

    class LevelingProfileValidation
    {
        template<class T>
        static bool Unique(const std::vector<T>& values)
        {
            return std::set<T>(values.begin(), values.end()).size() == values.size();
        }

    public:
        static bool Valid(const LevelingProfile& profile)
        {
            if (profile.id.empty() || profile.segments.empty() ||
                (profile.completionFact && profile.completionFact->empty())) return false;
            std::map<std::string, const LevelingProfileSegment*> byId;
            for (const auto& s : profile.segments)
            {
                if (s.id.empty() || !byId.emplace(s.id, &s).second ||
                    s.minimumLevel == 0 || s.maximumLevel > 60 ||
                    s.minimumLevel > s.maximumLevel ||
                    (s.allowedRaces && (*s.allowedRaces == 0 ||
                        (*s.allowedRaces & ~ProfileWorldEvidence::VanillaRaces))) ||
                    (s.allowedClasses && (*s.allowedClasses == 0 ||
                        (*s.allowedClasses & ~ProfileWorldEvidence::VanillaClasses))) ||
                    (s.expectedFactionGroup &&
                        *s.expectedFactionGroup != ProfileFactionGroup::Alliance &&
                        *s.expectedFactionGroup != ProfileFactionGroup::Horde) ||
                    (s.expectedZone && !s.expectedMap) ||
                    (s.grindRegion && s.grindRegion->constrainSelection && !s.expectedMap) ||
                    (s.grindRegion && !s.grindRegion->Valid()) ||
                    !Unique(s.allowedTargetEntries) || !Unique(s.questIds) ||
                    !Unique(s.allowedTargetClassifications) ||
                    !Unique(s.preparationRequirements)) return false;
                for (auto entry : s.allowedTargetEntries) if (!entry) return false;
                for (auto quest : s.questIds) if (!quest) return false;
                for (const auto& key : s.preparationRequirements)
                    if (key.empty()) return false;
                for (const auto& c : s.allowedTargetClassifications)
                    if (c != "normal" && c != "elite" && c != "rare" &&
                        c != "rareelite" && c != "worldboss") return false;
            }
            // Independent segments, multiple terminals and shared successors
            // are allowed. Optional links must resolve and cannot form cycles.
            for (const auto& s : profile.segments)
            {
                std::set<std::string> visited;
                const auto* current = &s;
                while (current)
                {
                    if (!visited.insert(current->id).second) return false;
                    if (!current->nextSegment) break;
                    const auto next = byId.find(*current->nextSegment);
                    if (next == byId.end()) return false;
                    current = next->second;
                }
            }
            return true;
        }

        static bool ValidCatalogue(const std::vector<LevelingProfile>& profiles)
        {
            std::set<std::string> ids;
            for (const auto& p : profiles)
                if (!ids.insert(p.id).second || !Valid(p)) return false;
            return true;
        }

        // Offline reference closure only. The caller must supply IDs from its
        // validated catalogue snapshot. Membership never proves eligibility,
        // completion, current location or execution support. Keep the existing
        // structural API available for synthetic/advisory profiles.
        struct MissingQuestReference
        {
            ProfileSegmentId segment;
            std::uint32_t questId = 0;
            auto operator<=>(const MissingQuestReference&) const = default;
        };

        static std::vector<MissingQuestReference> MissingQuestReferences(
            const std::vector<LevelingProfile>& profiles,
            const std::set<std::uint32_t>& catalogueQuestIds)
        {
            std::set<MissingQuestReference> missing;
            for (const auto& profile : profiles)
                for (const auto& segment : profile.segments)
                    for (const auto quest : segment.questIds)
                        if (!catalogueQuestIds.contains(quest))
                            missing.insert({{profile.id, segment.id}, quest});
            return {missing.begin(), missing.end()};
        }

        static bool ValidCatalogue(const std::vector<LevelingProfile>& profiles,
            const std::set<std::uint32_t>& catalogueQuestIds)
        {
            return ValidCatalogue(profiles) &&
                MissingQuestReferences(profiles, catalogueQuestIds).empty();
        }
    };
}
