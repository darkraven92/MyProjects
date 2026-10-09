#include "../src/Navigation/TerrainTransitionPolicy.h"
#include "../src/Navigation/EpisodeBadTransitionPolicy.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
    struct Point { float x, y, z; };
}

int main()
{
    using Navigation::TerrainTransitionPolicy;
    using Navigation::EpisodeBadTransitionPolicy;
    using Navigation::DirectedPolyTransition;

    static_assert(TerrainTransitionPolicy::GroundFlag == 0x01);
    static_assert(TerrainTransitionPolicy::WaterFlag == 0x08);
    static_assert(TerrainTransitionPolicy::SteepFlag == 0x10);
    static_assert(TerrainTransitionPolicy::MinimumVerticalDelta == 6.0f);
    static_assert(TerrainTransitionPolicy::MaximumHorizontal == 14.0f);
    static_assert(TerrainTransitionPolicy::MinimumRiseRun == 0.75f);

    // Query completion, not geometric route length, gives the preferred
    // non-steep corridor priority over a shorter steep-enabled corridor.
    assert(TerrainTransitionPolicy::Complete(true, true, false));
    assert(!TerrainTransitionPolicy::Complete(true, true, true));
    assert(!TerrainTransitionPolicy::Complete(false, true, false));
    assert(!TerrainTransitionPolicy::Complete(true, false, false));
    const float preferredLength = 780.657f;
    const float steepLength = 624.604f;
    assert(preferredLength > steepLength);
    assert(TerrainTransitionPolicy::Complete(true, true, false));
    assert(!TerrainTransitionPolicy::UsesSteep(
        TerrainTransitionPolicy::GroundFlag));
    assert(TerrainTransitionPolicy::UsesSteep(
        TerrainTransitionPolicy::GroundFlag |
        TerrainTransitionPolicy::SteepFlag));
    assert(TerrainTransitionPolicy::EnteredRef(0, true, 202) == 202);
    assert(TerrainTransitionPolicy::EnteredRef(101, true, 202) == 101);
    assert(TerrainTransitionPolicy::EnteredRef(0, false, 202) == 0);
    assert(TerrainTransitionPolicy::SameEndpointPolygons(
        101, 202, 101, 202));
    assert(!TerrainTransitionPolicy::SameEndpointPolygons(
        101, 202, 101, 303));
    assert(!TerrainTransitionPolicy::SameEndpointPolygons(
        0, 202, 0, 202));

    // The same pure rule is called by planning and the live 12B.10 guard.
    const Point origin{0.0f, 0.0f, 0.0f};
    const auto flat = TerrainTransitionPolicy::Assess(
        origin, Point{12.0f, 0.0f, 0.0f});
    assert(!flat.rejected);
    const auto mildSlope = TerrainTransitionPolicy::Assess(
        origin, Point{12.0f, 0.0f, 5.0f});
    assert(!mildSlope.rejected);
    const auto longRamp = TerrainTransitionPolicy::Assess(
        origin, Point{40.0f, 0.0f, 10.0f});
    assert(!longRamp.rejected);
    const auto mountainPortal = TerrainTransitionPolicy::Assess(
        origin, Point{12.3f, 0.0f, 17.6f});
    assert(mountainPortal.rejected);
    assert(mountainPortal.riseRun > 1.4f);
    const auto observedBarrensPortal = TerrainTransitionPolicy::Assess(
        Point{-483.8f, -2109.3f, 96.9f},
        Point{-484.267f, -2121.6f, 114.461f});
    assert(observedBarrensPortal.rejected);
    const auto safeSteepFallbackLeg = TerrainTransitionPolicy::Assess(
        origin, Point{3.0f, 0.0f, 2.0f});
    assert(!safeSteepFallbackLeg.rejected);

    // A failed directed portal may use the existing episode-local memory;
    // reverse travel and an alternate corridor remain independent.
    EpisodeBadTransitionPolicy episode;
    const DirectedPolyTransition bad{101, 202};
    assert(episode.Learn(bad));
    assert(episode.Learned(bad));
    assert(!episode.Learned({202, 101}));
    assert(episode.FirstMatch({100, 101, 202, 300}) == bad);
    assert(!episode.FirstMatch({202, 101}).Valid());
    assert(episode.AssessAlternative({101, 202, 300}, true,
        {101, 404, 300}) ==
        EpisodeBadTransitionPolicy::AlternativeDecision::UseAlternative);
    assert(episode.AssessAlternative({101, 202, 300}, false,
        {}) == EpisodeBadTransitionPolicy::AlternativeDecision::NoAlternative);
    assert(episode.AssessAlternative({101, 202, 300}, true,
        {101, 202, 300}) ==
        EpisodeBadTransitionPolicy::AlternativeDecision::NoAlternative);
    episode.Reset();
    assert(!episode.Learned(bad));

    // No numeric movement, long-stage, or recovery budget is defined here.
    // Unknown/non-finite geometry is left for existing path validation.
    const auto invalid = TerrainTransitionPolicy::Assess(
        origin, Point{std::numeric_limits<float>::quiet_NaN(), 0.0f, 7.0f});
    assert(!std::isfinite(invalid.horizontal));
    assert(!invalid.rejected);
}
