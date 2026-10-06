#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace Navigation
{
    struct CompleteLongStagePoint
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    enum class CompleteLongStageResult
    {
        NotNeeded,
        Selected,
        Ineligible,
        UnsafeSegment,
        NoForwardStage
    };

    enum class CompleteLongStageArrivalDecision
    {
        ReplanToFinal,
        Fail
    };

    enum class CompleteLongPlanTarget
    {
        ActiveStage,
        FinalDestination
    };

    struct CompleteLongStageSelection
    {
        CompleteLongStageResult result = CompleteLongStageResult::Ineligible;
        std::size_t pointIndex = 0;
        double prefixLength = 0.0;
        double totalLength = 0.0;
    };

    // Pure geometry/eligibility policy. The follower still validates the
    // selected prefix with its unchanged generic 2000-unit safety guard.
    struct CompleteLongStagePolicy
    {
        static CompleteLongPlanTarget PlanTarget(bool stageActive,
                                                 bool needsFinalPlan)
        {
            return stageActive && !needsFinalPlan
                ? CompleteLongPlanTarget::ActiveStage
                : CompleteLongPlanTarget::FinalDestination;
        }

        static int StageIndexAfterAcceptedPlan(int currentIndex,
                                               bool selectedNewStage)
        {
            return selectedNewStage ? currentIndex + 1 : currentIndex;
        }

        static bool CanStartStage(int completedOrStarted, int maximum)
        {
            return completedOrStarted >= 0 && completedOrStarted < maximum;
        }

        static bool RepeatedWithoutProgress(double priorStageDistance,
                                            double physicalProgress,
                                            double meaningfulProgress)
        {
            return std::isfinite(priorStageDistance) &&
                std::isfinite(physicalProgress) &&
                priorStageDistance < meaningfulProgress &&
                physicalProgress < meaningfulProgress;
        }

        static bool RejectRepeatedNewStage(bool priorStageCompleted,
                                           double priorStageDistance,
                                           double progressSinceCompletion,
                                           double meaningfulProgress)
        {
            return priorStageCompleted &&
                RepeatedWithoutProgress(priorStageDistance,
                                        progressSinceCompletion,
                                        meaningfulProgress);
        }

        static bool ArrivalMadeProgress(double physicalProgress,
                                        double destinationImprovement,
                                        double meaningfulProgress)
        {
            return std::isfinite(physicalProgress) &&
                std::isfinite(destinationImprovement) &&
                physicalProgress >= meaningfulProgress &&
                destinationImprovement >= meaningfulProgress;
        }

        static CompleteLongStageArrivalDecision OnArrival(
            double physicalProgress, double destinationImprovement,
            double meaningfulProgress)
        {
            return ArrivalMadeProgress(physicalProgress,
                                       destinationImprovement,
                                       meaningfulProgress)
                ? CompleteLongStageArrivalDecision::ReplanToFinal
                : CompleteLongStageArrivalDecision::Fail;
        }

        static bool FinalArrivalAllowed(bool stageActive,
                                        bool needsFinalPlan)
        {
            return !stageActive && !needsFinalPlan;
        }

        static CompleteLongStageSelection Select(
            const std::vector<CompleteLongStagePoint>& points,
            bool complete, bool capacityAvailable,
            double safetyLimit, double stageLimit,
            double maximumSegment, double maximumVerticalSegment,
            double minimumProgress, double minimumDestinationImprovement)
        {
            CompleteLongStageSelection selected{};
            if (!complete || !capacityAvailable || points.size() < 2 ||
                !std::isfinite(safetyLimit) || !std::isfinite(stageLimit) ||
                stageLimit <= 0.0 || stageLimit >= safetyLimit)
                return selected;

            const auto distance = [](const CompleteLongStagePoint& a,
                                     const CompleteLongStagePoint& b)
            {
                return std::hypot(std::hypot(a.x - b.x, a.y - b.y), a.z - b.z);
            };
            double running = 0.0;
            const double initialFinalDistance = distance(points.front(), points.back());
            selected.result = CompleteLongStageResult::NoForwardStage;
            for (std::size_t index = 1; index < points.size(); ++index)
            {
                const double segment = distance(points[index - 1], points[index]);
                const double rise = std::fabs(points[index].z - points[index - 1].z);
                if (!std::isfinite(segment) || !std::isfinite(rise) ||
                    segment > maximumSegment || rise > maximumVerticalSegment)
                {
                    selected.result = CompleteLongStageResult::UnsafeSegment;
                    return selected;
                }
                running += segment;
                if (!std::isfinite(running))
                {
                    selected.result = CompleteLongStageResult::UnsafeSegment;
                    return selected;
                }
                if (running <= stageLimit && index + 1 < points.size() &&
                    distance(points.front(), points[index]) >= minimumProgress &&
                    initialFinalDistance - distance(points[index], points.back()) >=
                        minimumDestinationImprovement)
                {
                    selected.pointIndex = index;
                    selected.prefixLength = running;
                }
            }
            selected.totalLength = running;
            if (running <= safetyLimit)
                selected.result = CompleteLongStageResult::NotNeeded;
            else if (selected.pointIndex != 0)
                selected.result = CompleteLongStageResult::Selected;
            return selected;
        }
    };
}
