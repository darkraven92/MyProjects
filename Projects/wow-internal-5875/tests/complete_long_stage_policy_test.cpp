#include "../src/Navigation/CompleteLongStagePolicy.h"

#include <cassert>
#include <vector>

using Navigation::CompleteLongStagePoint;
using Navigation::CompleteLongStagePolicy;
using Navigation::CompleteLongStageResult;
using Navigation::CompleteLongStageArrivalDecision;
using Navigation::CompleteLongPlanTarget;

static std::vector<CompleteLongStagePoint> LinearPath(int end)
{
    std::vector<CompleteLongStagePoint> points;
    for (int x = 0; x < end; x += 100)
        points.push_back({static_cast<double>(x), 0.0, 0.0});
    points.push_back({static_cast<double>(end), 0.0, 0.0});
    return points;
}

static Navigation::CompleteLongStageSelection Select(
    const std::vector<CompleteLongStagePoint>& points,
    bool complete = true, bool capacity = true)
{
    return CompleteLongStagePolicy::Select(
        points, complete, capacity, 2000.0, 1700.0, 300.0, 30.0, 4.0, 4.0);
}

int main()
{
    assert(Select(LinearPath(500)).result == CompleteLongStageResult::NotNeeded);
    assert(Select(LinearPath(1999)).result == CompleteLongStageResult::NotNeeded);
    assert(Select(LinearPath(2000)).result == CompleteLongStageResult::NotNeeded);
    for (const int length : {2001, 2919, 3015})
    {
        const auto selected = Select(LinearPath(length));
        assert(selected.result == CompleteLongStageResult::Selected);
        assert(selected.prefixLength <= 1700.0);
        assert(selected.prefixLength >= 4.0);
        assert(selected.pointIndex > 0);
        assert(selected.pointIndex + 1 < LinearPath(length).size());
        assert(selected.totalLength == static_cast<double>(length));
    }

    const auto longPath = LinearPath(2919);
    assert(Select(longPath, false).result == CompleteLongStageResult::Ineligible);
    assert(Select(longPath, true, false).result == CompleteLongStageResult::Ineligible);

    auto unsafe = longPath;
    unsafe[24].x = unsafe[23].x + 301.0;
    assert(Select(unsafe).result == CompleteLongStageResult::UnsafeSegment);
    unsafe = longPath;
    unsafe[24].z = 31.0;
    assert(Select(unsafe).result == CompleteLongStageResult::UnsafeSegment);

    // An unstageable prefix stays rejected rather than bypassing the limit.
    assert(Select({{0, 0, 0}, {2000, 0, 0}, {2500, 0, 0}}).result ==
           CompleteLongStageResult::UnsafeSegment);
    assert(!CompleteLongStagePolicy::RepeatedWithoutProgress(1.0, 4.0, 4.0));
    assert(CompleteLongStagePolicy::RepeatedWithoutProgress(1.0, 0.0, 4.0));
    assert(CompleteLongStagePolicy::ArrivalMadeProgress(1700.0, 1000.0, 4.0));
    assert(!CompleteLongStagePolicy::ArrivalMadeProgress(0.0, 1000.0, 4.0));
    assert(!CompleteLongStagePolicy::ArrivalMadeProgress(1700.0, -1.0, 4.0));
    assert(CompleteLongStagePolicy::OnArrival(1700.0, 1000.0, 4.0) ==
           CompleteLongStageArrivalDecision::ReplanToFinal);
    assert(CompleteLongStagePolicy::OnArrival(0.0, 1000.0, 4.0) ==
           CompleteLongStageArrivalDecision::Fail);
    assert(!CompleteLongStagePolicy::FinalArrivalAllowed(true, false));
    assert(!CompleteLongStagePolicy::FinalArrivalAllowed(false, true));
    assert(CompleteLongStagePolicy::FinalArrivalAllowed(false, false));
    assert(CompleteLongStagePolicy::CanStartStage(7, 8));
    assert(!CompleteLongStagePolicy::CanStartStage(8, 8));

    // A corridor refresh during stage 2 keeps the same target and index,
    // even if recovery has moved only a fraction of the progress threshold.
    assert(CompleteLongStagePolicy::PlanTarget(true, false) ==
           CompleteLongPlanTarget::ActiveStage);
    assert(CompleteLongStagePolicy::StageIndexAfterAcceptedPlan(2, false) == 2);
    assert(!CompleteLongStagePolicy::RejectRepeatedNewStage(
        false, 0.0, 0.02, 4.0));

    // Completion, not an ordinary replan, returns planning to the caller's
    // final destination and makes a genuinely new stage increment the index.
    assert(CompleteLongStagePolicy::PlanTarget(false, true) ==
           CompleteLongPlanTarget::FinalDestination);
    assert(CompleteLongStagePolicy::OnArrival(1700.0, 500.0, 4.0) ==
           CompleteLongStageArrivalDecision::ReplanToFinal);
    assert(CompleteLongStagePolicy::StageIndexAfterAcceptedPlan(2, true) == 3);
    assert(CompleteLongStagePolicy::RejectRepeatedNewStage(
        true, 0.5, 0.02, 4.0));
    assert(!CompleteLongStagePolicy::RejectRepeatedNewStage(
        true, 100.0, 0.02, 4.0));
    assert(!CompleteLongStagePolicy::RejectRepeatedNewStage(
        true, 0.5, 4.0, 4.0));
    assert(!CompleteLongStagePolicy::FinalArrivalAllowed(false, true));
    assert(CompleteLongStagePolicy::FinalArrivalAllowed(false, false));

    // The follower keeps its caller-owned final destination and replans from
    // the live stage endpoint; the pure selection never rewrites that target.
    const auto first = Select(LinearPath(3015));
    assert(first.result == CompleteLongStageResult::Selected);
    assert(Select(LinearPath(1315)).result == CompleteLongStageResult::NotNeeded);
}
