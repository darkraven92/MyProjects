#pragma once

#include "EpisodeBadTransitionPolicy.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Navigation
{
    // One death/corpse-route episode. Raw poly refs are valid only in the
    // NavMesh generation that supplied them; this is never world-persistent.
    class DeathRouteTransitionMemory
    {
    public:
        static constexpr std::size_t MaximumTransitions =
            EpisodeBadTransitionPolicy::MaximumLearned;
        static constexpr std::size_t MaximumCorridorFingerprints = 9;

        void Reset()
        {
            generation_ = 0;
            transitions_.clear();
            corridorFingerprints_.clear();
        }

        // Returns true when incompatible or unknown topology invalidated all
        // previous refs. A zero generation never authorizes reuse.
        bool ObserveGeneration(std::uint64_t generation)
        {
            if (generation_ == generation && generation != 0)
                return false;
            const bool hadEvidence = generation_ != 0 ||
                !transitions_.empty() || !corridorFingerprints_.empty();
            Reset();
            generation_ = generation;
            return hadEvidence;
        }

        bool Learn(DirectedPolyTransition edge)
        {
            if (generation_ == 0 || !edge.Valid() || Contains(edge))
                return false;
            if (transitions_.size() >= MaximumTransitions)
                return false;
            transitions_.push_back(edge);
            return true;
        }

        bool Contains(DirectedPolyTransition edge) const
        {
            return std::find(transitions_.begin(), transitions_.end(), edge) !=
                transitions_.end();
        }

        std::size_t SharedTransitions(
            const std::vector<DirectedPolyTransition>& edges) const
        {
            std::size_t count = 0;
            for (const auto edge : edges)
                if (Contains(edge)) ++count;
            return count;
        }

        bool RememberCorridor(std::uint64_t fingerprint)
        {
            if (fingerprint == 0 || generation_ == 0 ||
                std::find(corridorFingerprints_.begin(),
                    corridorFingerprints_.end(), fingerprint) !=
                    corridorFingerprints_.end())
                return false;
            if (corridorFingerprints_.size() >= MaximumCorridorFingerprints)
                corridorFingerprints_.erase(corridorFingerprints_.begin());
            corridorFingerprints_.push_back(fingerprint);
            return true;
        }

        std::uint64_t Generation() const { return generation_; }
        std::size_t Size() const { return transitions_.size(); }
        const std::vector<DirectedPolyTransition>& Transitions() const
        { return transitions_; }

    private:
        std::uint64_t generation_ = 0;
        std::vector<DirectedPolyTransition> transitions_{};
        std::vector<std::uint64_t> corridorFingerprints_{};
    };
}
