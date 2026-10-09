#include "../src/Bot/MaintenanceCombatHandoffPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using P = Bot::MaintenanceCombatHandoffPolicy;
    assert(!P::RetireDelay(true,103,104,false,false));
    assert(P::RetireDelay(true,104,104,false,false));
    assert(P::RetireDelay(true,10000,104,false,false));
    assert(!P::RetireDelay(false,10000,104,false,false)); // real combat/recovery
    assert(!P::RetireDelay(true,10000,104,true,false));
    assert(!P::RetireDelay(true,10000,104,false,true)); // corpse owner
    std::ifstream input("src/Bot/CombatController.h"); assert(input);
    const std::string source{std::istreambuf_iterator<char>(input), {}};
    const auto begin=source.find("void RetireExpiredPostKillDelayForMaintenance");
    const auto method=source.substr(begin,source.find("        bool Start(",begin)-begin);
    assert(method.find("SetState(CombatState::AcquiringTarget)") != std::string::npos);
    assert(method.find("Update(") == std::string::npos);
    assert(method.find("BeginAcquire(") == std::string::npos);
    assert(method.find("ResetTargetState(") == std::string::npos);
    assert(method.find("MovementController") == std::string::npos);

    std::ifstream grindInput("src/Bot/GrindModeController.h"); assert(grindInput);
    const std::string grind{std::istreambuf_iterator<char>(grindInput), {}};
    const auto waitBegin=grind.find("            if (state_ == GrindModeState::WaitingForManualVendor)",
        grind.find("        void Update("));
    const auto waitHandoff=grind.find("combat.RetireExpiredPostKillDelayForMaintenance(tick);",waitBegin);
    assert(waitBegin!=std::string::npos && waitHandoff!=std::string::npos);
    const auto waiting=grind.substr(waitBegin,waitHandoff-waitBegin);
    for (const auto* guard : {"if (!world.valid", "if (const auto* aggressor = FindDirectAggressor(world))",
                             "if (combat.LockedGuid() != 0)"})
    {
        const auto at=waiting.find(guard); assert(at!=std::string::npos);
        assert(waiting.find("return;",at)!=std::string::npos);
    }
    const auto vendorBegin=grind.find("            if (state_ == GrindModeState::Vendoring)",waitHandoff);
    const auto vendorHandoff=grind.find("combat.RetireExpiredPostKillDelayForMaintenance(tick);",vendorBegin);
    assert(vendorBegin!=std::string::npos && vendorHandoff!=std::string::npos);
    const auto vendoring=grind.substr(vendorBegin,vendorHandoff-vendorBegin);
    assert(vendoring.find("FindDirectAggressor(world)")!=std::string::npos);
    const auto defend=vendoring.find("combat.AdoptExactTargetForDefense(world, aggressor->guid, tick)");
    assert(defend!=std::string::npos && vendoring.find("return;",defend)!=std::string::npos);
}
