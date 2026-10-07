#include "../src/Navigation/LivingWaterTraversalPolicy.h"
#include "../src/Navigation/NavigationInitTelemetryPolicy.h"
#include "../src/Bot/LivingWaterBlockPolicy.h"

#include <cassert>
#include <string_view>

int main()
{
    using namespace Navigation;
    constexpr auto living = WaterTraversalMode::AvoidUntilQualified;
    constexpr auto ghost = WaterTraversalMode::GhostDeathRecovery;
    static_assert(LivingWaterTraversalPolicy::Include(living) == 0x01);
    static_assert(LivingWaterTraversalPolicy::Exclude(living) == 0x08);
    static_assert(LivingWaterTraversalPolicy::Exclude(living, 0x10) == 0x18);
    static_assert(LivingWaterTraversalPolicy::Traversable(0x01, living));
    static_assert(!LivingWaterTraversalPolicy::Traversable(0x08, living));
    static_assert(!LivingWaterTraversalPolicy::Traversable(0x09, living));
    static_assert(!LivingWaterTraversalPolicy::Traversable(0x19, living));
    static_assert(LivingWaterTraversalPolicy::Traversable(0x08, ghost));
    static_assert(LivingWaterTraversalPolicy::Traversable(0x09, ghost));
    static_assert(NavigationInitTelemetryPolicy::QueryFailure(
        "Living-player water traversal is disabled.") ==
        NavigationPlanFailure::WaterTraversalDisabled);
    assert(std::string_view(NavigationInitTelemetryPolicy::ReasonName(
        NavigationPlanFailure::WaterTraversalDisabled)) ==
        "water_traversal_disabled");

    Bot::LivingWaterBlockPolicy block;
    using Event = Bot::LivingWaterBlockEvent;
    assert(block.Observe(false, true, true) == Event::Entered);
    assert(block.Blocked());
    assert(block.Observe(false, false, false) == Event::None);
    assert(block.Blocked());
    assert(block.Observe(false, true, false) == Event::None);
    assert(block.Observe(false, true, false) == Event::None);
    assert(block.Blocked());
    assert(block.Observe(false, true, false) == Event::ExitedNonSwimming);
    assert(!block.Blocked());
    assert(block.Observe(false, true, true) == Event::Entered);
    assert(block.Observe(true, true, true) == Event::ExitedDeathOwnership);
    assert(!block.Blocked());
}
