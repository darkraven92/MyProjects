#include "../src/Navigation/LongPathDiagnosticPolicy.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

struct Point
{
    float x;
    float y;
    float z;
};

using Navigation::LongPathDiagnosticPolicy;

int main()
{
    constexpr float limit = 2000.0f;
    static_assert(!LongPathDiagnosticPolicy::ExceedsSafetyLength(1999.0f, limit));
    static_assert(!LongPathDiagnosticPolicy::ExceedsSafetyLength(limit, limit));
    static_assert(LongPathDiagnosticPolicy::ExceedsSafetyLength(2000.125f, limit));
    static_assert(!LongPathDiagnosticPolicy::UsesPartialStaging(false));
    static_assert(LongPathDiagnosticPolicy::UsesPartialStaging(true));

    // A complete long corridor is length-rejected, not partial-staged.
    static_assert(LongPathDiagnosticPolicy::ExceedsSafetyLength(2500.0f, limit) &&
                  !LongPathDiagnosticPolicy::UsesPartialStaging(false));

    // Existing node-pool-limited partial staging still requires a useful
    // connected prefix; a complete route cannot enter this branch.
    static_assert(LongPathDiagnosticPolicy::CapacityLimitedPartialStage(
        true, true, 4.0f, 2.0f, 3.0f, 1.0f));
    static_assert(!LongPathDiagnosticPolicy::CapacityLimitedPartialStage(
        false, true, 4.0f, 2.0f, 3.0f, 1.0f));
    static_assert(!LongPathDiagnosticPolicy::CapacityLimitedPartialStage(
        true, true, 2.0f, 2.0f, 3.0f, 1.0f));

    static_assert(!LongPathDiagnosticPolicy::PolygonBufferFilled(361, 512));
    static_assert(LongPathDiagnosticPolicy::PolygonBufferFilled(512, 512));
    static_assert(!LongPathDiagnosticPolicy::PolygonBufferFilled(512, 0));

    const std::vector<Point> shortPath{{0, 0, 0}, {1000, 0, 0}};
    const auto shortMetric = LongPathDiagnosticPolicy::MeasureStraightPath(shortPath);
    assert(shortMetric.known && shortMetric.straightPathLength == 1000.0);

    const std::vector<Point> fractionalLongPath{
        {0, 0, 0}, {1000.125f, 0, 0}, {2000.25f, 0, 0}};
    const auto longMetric =
        LongPathDiagnosticPolicy::MeasureStraightPath(fractionalLongPath);
    assert(longMetric.known && longMetric.straightPathLength == 2000.25);
    assert(longMetric.straightPathLength > limit); // no integer truncation

    const std::vector<Point> empty{};
    assert(!LongPathDiagnosticPolicy::MeasureStraightPath(empty).known);
    const std::vector<Point> unavailable{
        {0, 0, 0}, {std::numeric_limits<float>::quiet_NaN(), 0, 0}};
    assert(!LongPathDiagnosticPolicy::MeasureStraightPath(unavailable).known);
}
