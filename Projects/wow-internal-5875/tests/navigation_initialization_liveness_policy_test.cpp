#include "../src/Bot/NavigationInitializationLivenessPolicy.h"

using Bot::NavigationInitializationLivenessPolicy;

static_assert(NavigationInitializationLivenessPolicy::DeferOwnerRecovery(true, 0));
static_assert(NavigationInitializationLivenessPolicy::DeferOwnerRecovery(
    true, 60 * 1000));
static_assert(!NavigationInitializationLivenessPolicy::DeferOwnerRecovery(
    true, NavigationInitializationLivenessPolicy::MaximumPendingMs));
static_assert(!NavigationInitializationLivenessPolicy::DeferOwnerRecovery(
    false, 0));
static_assert(!NavigationInitializationLivenessPolicy::DeferOwnerRecovery(
    true, -1));

int main() {}
