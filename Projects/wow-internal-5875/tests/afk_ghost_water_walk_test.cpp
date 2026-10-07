#include "../src/Bot/AfkQualificationHold.h"
#include "../src/Bot/AfkDeadGhostPolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using namespace Bot;
    using Movement=AfkWorkloadSafetyPolicy;
    constexpr std::uint32_t waterWalk=0x10000000u;
    for (auto life : {AfkLifeState::Alive,AfkLifeState::Dead,AfkLifeState::Unknown})
    {
        assert(Movement::AllowedMovementMask(life,true,true)==0x13fu);
        assert(!Movement::LandMovementAllowed(waterWalk,true,life,true));
    }
    // Ghost is not a global land exemption: only the guarded input-only pulse.
    assert(!Movement::LandMovementAllowed(waterWalk,true,AfkLifeState::Ghost,false));
    assert(!Movement::LandMovementAllowed(waterWalk,false,AfkLifeState::Ghost,false));
    assert(Movement::AllowedMovementMask(AfkLifeState::Ghost,true,true)==0x1000013fu);
    for (auto flags : {waterWalk,0x13fu,waterWalk|0x13fu})
        assert(Movement::LandMovementAllowed(flags,true,AfkLifeState::Ghost,true));
    for (auto bit : {0x40u,0x80u,0x200u,0x400u,0x800u,0x1000u,0x2000u,
        0x4000u,0x200000u,0x400000u,0x1000000u,0x2000000u,0x4000000u,
        0x8000000u,0x20000000u,0x40000000u,0x80000000u})
        assert(!Movement::LandMovementAllowed(waterWalk|bit,true,AfkLifeState::Ghost,true));
    const auto evidence=Movement::MovementEvidence(waterWalk,true,AfkLifeState::Ghost,true);
    assert(evidence.Fields()=="movementFlags=0x10000000 allowedMask=0x1000013f unsupportedBits=0x00000000");
    // Stationary rules still reject locomotion, even with the Ghost pulse scope.
    assert(!Movement::LandMovementAllowed(waterWalk|1u,false,AfkLifeState::Ghost,true));
    assert(Movement::AllowedMovementMask(AfkLifeState::Alive,false,false)==0x100u);

    AfkDeadGhostPolicy ghost;
    AfkObservation o{true,false,false,77000,0,300000};
    assert(!ghost.Blocker(AfkLifeState::Ghost,AfkDeathGap::RoutingToCorpse,o));
    assert(ghost.Blocker(AfkLifeState::Ghost,AfkDeathGap::CommandInFlight,o));
    AfkProductionPolicy scheduler;
    AfkSafety gap; gap.navigation=true;
    auto action=scheduler.Update(o,gap,0,false,true,AfkAutoClearSetting::Unknown,
        ghost.NeedsQualification(AfkLifeState::Ghost));
    assert(action.action==AfkAction::InputPulse && !scheduler.Due());
    scheduler.Issued(action.action,o,0);
    ghost.Issued(AfkLifeState::Ghost,o,0,true,true,true);
    assert(ghost.Verify(o,1,AfkLifeState::Ghost)==AfkResult::Pending);
    auto advanced=o; advanced.lastInput=77001; advanced.clientNow=77002;
    auto wrongLife=ghost;
    assert(wrongLife.Verify(advanced,2,AfkLifeState::Dead)==AfkResult::Failed);
    assert(ghost.Verify(advanced,2,AfkLifeState::Ghost)==AfkResult::Confirmed);
    assert(ghost.Qualified(AfkLifeState::Ghost) && !ghost.Qualified(AfkLifeState::Dead));
    assert(scheduler.Update(advanced,gap,2,false,true).result==AfkResult::Confirmed);
    for (int missing=0; missing<3; ++missing)
    {
        AfkDeadGhostPolicy bad;
        bad.Issued(AfkLifeState::Ghost,o,0,missing!=0,missing!=1,missing!=2);
        assert(bad.Failed()); // release / scene-UI / recovery proof mandatory
    }
    for (int flags=1; flags<4; ++flags)
    {
        auto active=o; active.clientAfk=(flags&1)!=0; active.serverAfk=(flags&2)!=0;
        assert(ghost.Blocker(AfkLifeState::Ghost,AfkDeathGap::RoutingToCorpse,active));
    }
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    auto read=[](const auto& path) {
        std::ifstream file(path); assert(file);
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto adapter=read(root/"src/Bot/AfkClient5875.h");
    assert(adapter.find("movement,ordinaryLandMovement,life,deadGhostPulse")!=std::string::npos);
    assert(adapter.find("deadGhostPulse && (action!=AfkAction::InputPulse")!=std::string::npos);
    assert(adapter.find("Core::Memory::Write")==std::string::npos);
    assert(adapter.find("const auto guardScript=AfkInputGuardScript(deadGhostPulse)")!=std::string::npos);
    const auto shared=read(root/"src/Bot/SharedAfkController.h");
    assert(shared.find("AFK DEAD/GHOST MOVEMENT ELIGIBLE")!=std::string::npos);
    assert(shared.find("dispatch.lifeVerified && pulseLife==life")!=std::string::npos);
}
