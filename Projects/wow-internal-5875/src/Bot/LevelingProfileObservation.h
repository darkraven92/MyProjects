#pragma once

#include "LevelingProfileSelector.h"

namespace Bot
{
    enum class LevelingProfileTransition { Initial, Unchanged, ContextChanged, SelectionChanged };

    struct LevelingProfileObservation
    {
        LevelingProfileTransition transition = LevelingProfileTransition::Initial;
        LevelingSelectionResult selection;
    };

    // Observation history only. Never retains a successful result in place of
    // missing evidence, follows nextSegment, or applies an execution transition.
    class LevelingProfileObservationTracker
    {
        struct Sample
        {
            ProfileEvidenceIdentity identity;
            std::optional<std::uint32_t> mapId;
            std::optional<std::string> scope;
            LevelingSelectionResult selection;
        };
        std::optional<Sample> previous_;

    public:
        void Reset() { previous_.reset(); }

        LevelingProfileObservation Observe(
            const std::vector<LevelingProfile>& profiles,
            const ProfileWorldEvidence& evidence,
            const std::optional<std::string>& profileId = std::nullopt)
        {
            Sample current{evidence.identity, evidence.mapId, profileId,
                           LevelingProfileSelector::Select(profiles, evidence, profileId)};
            LevelingProfileObservation result;
            result.selection = current.selection;
            if (previous_)
            {
                if (previous_->identity != current.identity || previous_->mapId != current.mapId ||
                    previous_->scope != current.scope)
                    result.transition = LevelingProfileTransition::ContextChanged;
                else if (previous_->selection != current.selection)
                    result.transition = LevelingProfileTransition::SelectionChanged;
                else
                    result.transition = LevelingProfileTransition::Unchanged;
            }
            previous_ = current;
            return result;
        }
    };
}
