#include "../src/Bot/ConsumableClassificationPolicy.h"

#include <cassert>

int main()
{
    using Bot::ConsumableClassificationPolicy;
    static_assert(ConsumableClassificationPolicy::IsFood(
        "restores health over time while eating"));
    static_assert(ConsumableClassificationPolicy::IsDrink(
        "restores mana over time while drinking"));
    static_assert(!ConsumableClassificationPolicy::IsFood("unknown item"));
    static_assert(!ConsumableClassificationPolicy::IsDrink("unknown item"));
    static_assert(ConsumableClassificationPolicy::StackContribution(true, 4) == 4);
    static_assert(ConsumableClassificationPolicy::StackContribution(false, 4) == 0);
    assert(ConsumableClassificationPolicy::LuaDefinition() != nullptr);
}
