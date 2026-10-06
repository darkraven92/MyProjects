#include "../tools/NavmeshAuditGeometry.h"
#include <cassert>
#include <limits>

struct Point { float x, y, z; };

int main()
{
    using NavmeshAudit::ClipPortal;
    const Point a{0, 0, 10}, b{255, 510, 265};
    const auto internal = ClipPortal(a, b, 0xff, 0, 0);
    assert(internal && internal->first.x == 0 && internal->second.x == 255);
    const auto external = ClipPortal(a, b, 0, 64, 128);
    assert(external && external->first.x == 64 && external->second.x == 128);
    assert(external->first.y == 128 && external->second.y == 256);
    assert(external->first.z == 74 && external->second.z == 138);
    const auto full = ClipPortal(a, b, 2, 0, 255);
    assert(full && full->second.z == b.z);
    const auto point = ClipPortal(a, b, 4, 128, 128);
    assert(point && point->first.x == point->second.x);
    assert(!ClipPortal(a, b, 8, 0, 255));
    assert(!ClipPortal(a, b, 0, 128, 64));
    assert(!ClipPortal(a, b, 0, 0, 256));
    assert(!ClipPortal(Point{0, 0, std::numeric_limits<float>::quiet_NaN()},
                       b, 0xff, 0, 0));
}
