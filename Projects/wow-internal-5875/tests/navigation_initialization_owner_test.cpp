#include "../src/Bot/RuntimeRobustnessSupervisor.h"

#include <cassert>
#include <cstdint>

int main()
{
    Bot::RuntimeRobustnessSample sample{};
    sample.active = true;
    sample.vendorActive = true;
    sample.navigationInitializationPending = true;
    sample.playerHealth = 100;
    sample.playerMaxHealth = 100;
    sample.level = 1;

    Bot::RuntimeRobustnessSupervisor supervisor;
    for (std::uint64_t tick = 0; tick <= 300; ++tick)
    {
        const auto event = supervisor.Update(sample, tick);
        assert(event.kind == Bot::RuntimeRobustnessEventKind::None);
    }

    // Completion gets the ordinary owner one fresh, bounded opportunity
    // to turn a plan into real physical progress.
    sample.navigationInitializationPending = false;
    assert(supervisor.Update(sample, 301).kind ==
           Bot::RuntimeRobustnessEventKind::None);
    assert(supervisor.Update(sample, 540).kind !=
           Bot::RuntimeRobustnessEventKind::Recovery);
    const auto expired = supervisor.Update(sample, 541);
    assert(expired.reason == Bot::RuntimeRobustnessReason::VendorOwnerTimeout);

    // With no pending plan, the pre-existing 60-second vendor guard applies.
    Bot::RuntimeRobustnessSupervisor ordinary;
    sample.navigationInitializationPending = false;
    assert(ordinary.Update(sample, 0).kind ==
           Bot::RuntimeRobustnessEventKind::None);
    const auto ordinaryExpired = ordinary.Update(sample, 240);
    assert(ordinaryExpired.reason ==
           Bot::RuntimeRobustnessReason::VendorOwnerTimeout);
}
