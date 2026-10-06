#include "../src/Navigation/RouteCostProbePolicy.h"

#include <cassert>

using Navigation::NavigationPlanFailure;
using Navigation::RouteCostProbePolicy;
using Navigation::RouteCostProbeResult;
using Navigation::RouteCostProbeStatus;

int main()
{
    // Planning-only callers cannot dispatch either a hold or steering CTM.
    assert(!RouteCostProbePolicy::MayIssueMovement(true));
    assert(RouteCostProbePolicy::MayIssueMovement(false));

    const RouteCostProbeResult pending{};
    assert(pending.status == RouteCostProbeStatus::Pending);
    assert(pending.failure == NavigationPlanFailure::None);

    const auto ready = RouteCostProbePolicy::Ready(412.5f);
    assert(ready.status == RouteCostProbeStatus::Reachable);
    assert(ready.pathLength == 412.5f);
    assert(ready.failure == NavigationPlanFailure::None);

    const auto noPath = RouteCostProbePolicy::Failed(
        NavigationPlanFailure::NoPath);
    assert(noPath.status == RouteCostProbeStatus::Unreachable);
    assert(noPath.failure == NavigationPlanFailure::NoPath);

    const auto tooLong = RouteCostProbePolicy::Failed(
        NavigationPlanFailure::PathLengthExceeded);
    assert(tooLong.status == RouteCostProbeStatus::Unreachable);
    assert(tooLong.failure == NavigationPlanFailure::PathLengthExceeded);
}
