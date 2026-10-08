#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Navigation
{
    struct DirectedPolyTransition
    {
        std::uint64_t from = 0;
        std::uint64_t to = 0;

        constexpr bool Valid() const { return from != 0 && to != 0 && from != to; }
        constexpr bool operator==(const DirectedPolyTransition&) const = default;
    };

    // Only the follower owns this object. It is cleared by Start(), not by an
    // internal replan. Legacy 13D.4 masks polygons globally within a query;
    // this policy first proves that the route uses the directed edge.
    class EpisodeBadTransitionPolicy
    {
    public:
        enum class AlternativeDecision
        {
            KeepOriginal,
            UseAlternative,
            NoAlternative
        };

        static constexpr std::size_t MaximumLearned = 8;
        static constexpr int MinimumBoundaryStalls = 2;

        void Reset()
        {
            learned_.clear();
            ClearObservation();
        }

        void ClearObservation()
        {
            observed_ = {};
            consecutiveIssued_ = 0;
            ClearStallEvidence();
        }

        void ClearStallEvidence()
        {
            activeIssued_ = {};
            lastStalled_ = {};
            stalledObservations_ = 0;
        }

        // A recovery attempt consumes the follower's safety budget even if
        // CTM rejects it, but only an accepted movement command may count as
        // evidence about the directed edge. A rejected dispatch invalidates
        // the current observation rather than preserving stale stalls.
        void ObserveDispatch(DirectedPolyTransition edge, bool accepted)
        {
            if (!accepted || !edge.Valid())
            {
                ClearObservation();
                return;
            }
            if (!(observed_ == edge))
            {
                observed_ = edge;
                consecutiveIssued_ = 0;
                stalledObservations_ = 0;
                lastStalled_ = {};
            }
            ++consecutiveIssued_;
            activeIssued_ = edge;
        }

        DirectedPolyTransition ObserveStalled()
        {
            const DirectedPolyTransition edge = activeIssued_;
            activeIssued_ = {};
            if (!edge.Valid() || !(edge == observed_))
                return {};
            ++stalledObservations_;
            lastStalled_ = edge;
            return edge;
        }

        DirectedPolyTransition BoundaryEvidence(
            bool fromVerticalStall, bool replanBudgetExhausted,
            bool boundaryAttemptAvailable) const
        {
            return fromVerticalStall && replanBudgetExhausted &&
                boundaryAttemptAvailable && observed_.Valid() &&
                lastStalled_ == observed_ &&
                stalledObservations_ >= MinimumBoundaryStalls
                    ? observed_ : DirectedPolyTransition{};
        }

        bool ExhaustionProvesEdge(
            DirectedPolyTransition edge, int attemptsUsed,
            int existingAttemptLimit) const
        {
            return edge.Valid() && observed_ == edge &&
                existingAttemptLimit > 0 &&
                attemptsUsed >= existingAttemptLimit &&
                consecutiveIssued_ >= existingAttemptLimit &&
                lastStalled_ == edge &&
                stalledObservations_ >= MinimumBoundaryStalls &&
                !activeIssued_.Valid();
        }

        bool Learn(DirectedPolyTransition edge)
        {
            if (!edge.Valid() || Learned(edge))
                return false;
            if (learned_.size() >= MaximumLearned)
                learned_.erase(learned_.begin());
            learned_.push_back(edge);
            return true;
        }

        bool Learned(DirectedPolyTransition edge) const
        {
            return std::find(learned_.begin(), learned_.end(), edge) !=
                learned_.end();
        }

        const std::vector<DirectedPolyTransition>& AllLearned() const
        { return learned_; }

        DirectedPolyTransition FirstMatch(
            const std::vector<std::uint64_t>& corridor) const
        {
            for (std::size_t i = 1; i < corridor.size(); ++i)
            {
                const DirectedPolyTransition edge{
                    corridor[i - 1], corridor[i]};
                if (Learned(edge))
                    return edge;
            }
            return {};
        }

        std::vector<std::uint64_t> MatchedTargetPolygons(
            const std::vector<std::uint64_t>& corridor) const
        {
            std::vector<std::uint64_t> targets;
            for (std::size_t i = 1; i < corridor.size(); ++i)
            {
                const DirectedPolyTransition edge{
                    corridor[i - 1], corridor[i]};
                if (Learned(edge) &&
                    std::find(targets.begin(), targets.end(), edge.to) ==
                        targets.end())
                    targets.push_back(edge.to);
            }
            return targets;
        }

        AlternativeDecision AssessAlternative(
            const std::vector<std::uint64_t>& original,
            bool alternativeFound,
            const std::vector<std::uint64_t>& alternative) const
        {
            if (!FirstMatch(original).Valid())
                return AlternativeDecision::KeepOriginal;
            return alternativeFound && !FirstMatch(alternative).Valid()
                ? AlternativeDecision::UseAlternative
                : AlternativeDecision::NoAlternative;
        }

        int ConsecutiveIssued() const { return consecutiveIssued_; }
        int StalledObservations() const { return stalledObservations_; }
        std::size_t Size() const { return learned_.size(); }
        DirectedPolyTransition LatestLearned() const
        {
            return learned_.empty() ? DirectedPolyTransition{} : learned_.back();
        }

    private:
        DirectedPolyTransition observed_{};
        DirectedPolyTransition activeIssued_{};
        DirectedPolyTransition lastStalled_{};
        int consecutiveIssued_ = 0;
        int stalledObservations_ = 0;
        std::vector<DirectedPolyTransition> learned_{};
    };
}
