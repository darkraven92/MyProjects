#pragma once

#include "WorldPreparationPolicy.h"

namespace Bot
{
    enum class LevelingSelectionStatus
    {
        InvalidProfile, InvalidEvidence, UnknownProfile, EvidenceUnavailable,
        NoMatch, Ambiguous, PreparationPending, Selected, Complete
    };

    enum class ProfileCandidateMatch { No, Unknown, Yes };

    struct ProfileCandidateResult
    {
        ProfileSegmentId id;
        ProfileCandidateMatch match = ProfileCandidateMatch::Yes;
        // Stable reason codes, sorted independently of catalogue input order.
        std::vector<std::string> reasons;
        bool operator==(const ProfileCandidateResult&) const = default;
    };

    struct LevelingSelectionResult
    {
        LevelingSelectionStatus status = LevelingSelectionStatus::NoMatch;
        std::string reason = "no_matching_segment";
        std::optional<ProfileSegmentId> selected, nextSegment;
        std::vector<ProfileCandidateResult> candidates;
        std::vector<std::string> completedProfiles;
        WorldPreparationResult preparation;
        bool operator==(const LevelingSelectionResult&) const = default;
    };

    class LevelingProfileSelector
    {
        static ProfileCandidateResult Evaluate(const LevelingProfile& profile,
                                               const LevelingProfileSegment& s,
                                               const ProfileWorldEvidence& e)
        {
            ProfileCandidateResult result;
            result.id = {profile.id, s.id};
            const auto reject = [&](const char* reason) {
                result.match = ProfileCandidateMatch::No;
                result.reasons.emplace_back(reason);
            };
            const auto unknown = [&](const char* reason) {
                if (result.match != ProfileCandidateMatch::No)
                    result.match = ProfileCandidateMatch::Unknown;
                result.reasons.emplace_back(reason);
            };
            if (*e.playerLevel < s.minimumLevel || *e.playerLevel > s.maximumLevel)
                reject("level_outside_range");
            if (s.expectedMap)
            {
                if (!e.mapId) unknown("map_unknown");
                else if (e.mapId != s.expectedMap) reject("map_mismatch");
            }
            // Until separately qualified, even caller-supplied values do not
            // enable zone or faction-group matching.
            if (s.expectedZone) unknown("zone_reader_unqualified");
            if (s.expectedFactionGroup) unknown("faction_group_reader_unqualified");
            const auto identity = [&](const auto& required, const auto& observed,
                                      std::uint32_t mask, const char* missing,
                                      const char* mismatch) {
                if (!required) return;
                if (!ProfileWorldEvidence::KnownIdentityMask(observed, mask))
                    unknown(missing);
                else if (!(*required & *observed)) reject(mismatch);
            };
            identity(s.allowedRaces, e.raceMask, ProfileWorldEvidence::VanillaRaces,
                     "race_unknown", "race_mismatch");
            identity(s.allowedClasses, e.classMask, ProfileWorldEvidence::VanillaClasses,
                     "class_unknown", "class_mismatch");
            if (s.grindRegion && s.grindRegion->constrainSelection)
            {
                if (!e.position || !e.position->Finite()) unknown("position_unavailable");
                else if (!e.mapId || !e.positionMapId) unknown("position_map_unknown");
                else if (e.positionMapId != e.mapId) unknown("position_map_incoherent");
                else if (e.mapId == s.expectedMap && !s.grindRegion->Contains(*e.position))
                    reject("outside_region");
            }
            if (profile.completionFact)
            {
                const auto fact = e.preparationFacts.find(*profile.completionFact);
                if (fact == e.preparationFacts.end() || !fact->second)
                    unknown("completion_unknown");
            }
            // A known contradiction dominates unknown constraints; unknown is
            // never silently converted into an eligible or rejected candidate.
            std::sort(result.reasons.begin(), result.reasons.end());
            return result;
        }

    public:
        // No retained progress, I/O, callbacks, controller or client access.
        // Optional scope is configuration only; it never proves eligibility.
        static LevelingSelectionResult Select(
            const std::vector<LevelingProfile>& profiles,
            const ProfileWorldEvidence& evidence,
            const std::optional<std::string>& profileId = std::nullopt)
        {
            LevelingSelectionResult result;
            const auto finish = [&](LevelingSelectionStatus status, const char* reason) {
                result.status = status;
                result.reason = reason;
                return result;
            };
            if (!LevelingProfileValidation::ValidCatalogue(profiles))
                return finish(LevelingSelectionStatus::InvalidProfile, "invalid_catalogue");
            std::vector<const LevelingProfile*> scope;
            for (const auto& p : profiles)
                if (!profileId || p.id == *profileId) scope.push_back(&p);
            if (scope.empty())
                return profileId
                    ? finish(LevelingSelectionStatus::UnknownProfile, "unknown_profile")
                    : finish(LevelingSelectionStatus::NoMatch, "empty_catalogue");
            if (!evidence.QualifiedIdentity())
                return finish(LevelingSelectionStatus::EvidenceUnavailable, "identity_or_freshness_unknown");
            if (!evidence.playerLevel)
                return finish(LevelingSelectionStatus::EvidenceUnavailable, "level_unknown");
            if (*evidence.playerLevel == 0 || *evidence.playerLevel > 60)
                return finish(LevelingSelectionStatus::InvalidEvidence, "level_invalid");

            const LevelingProfileSegment* selected = nullptr;
            std::optional<ProfileSegmentId> selectedId;
            std::size_t matches = 0, unknown = 0;
            for (const auto* p : scope)
            {
                if (p->completionFact)
                {
                    const auto fact = evidence.preparationFacts.find(*p->completionFact);
                    if (fact != evidence.preparationFacts.end() && fact->second && *fact->second)
                    {
                        result.completedProfiles.push_back(p->id);
                        continue;
                    }
                }
                for (const auto& s : p->segments)
                {
                    auto candidate = Evaluate(*p, s, evidence);
                    if (candidate.match == ProfileCandidateMatch::Yes)
                    {
                        ++matches;
                        selected = &s;
                        selectedId = candidate.id;
                    }
                    else if (candidate.match == ProfileCandidateMatch::Unknown) ++unknown;
                    result.candidates.push_back(candidate);
                }
            }
            std::sort(result.candidates.begin(), result.candidates.end(),
                [](const auto& a, const auto& b) { return a.id < b.id; });
            std::sort(result.completedProfiles.begin(), result.completedProfiles.end());
            if (result.completedProfiles.size() == scope.size())
                return finish(LevelingSelectionStatus::Complete, "declared_completion_observed");
            // No implicit priorities: multiple definite matches always fail
            // closed. Any unresolved contender also blocks a unique match.
            if (matches > 1)
                return finish(LevelingSelectionStatus::Ambiguous, "multiple_matching_segments");
            if (unknown)
                return finish(LevelingSelectionStatus::EvidenceUnavailable, "candidate_evidence_unknown");
            if (!selected) return result;
            result.selected = selectedId;
            if (selected->nextSegment)
                result.nextSegment = ProfileSegmentId{selectedId->profileId, *selected->nextSegment};
            result.preparation = WorldPreparationPolicy::Evaluate(*selected, evidence);
            return result.preparation.Ready()
                ? finish(LevelingSelectionStatus::Selected, "unique_ready_segment")
                : finish(LevelingSelectionStatus::PreparationPending, "preparation_not_ready");
        }
    };
}
