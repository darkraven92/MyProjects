#include "../src/Bot/AfkDeadGhostPolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include "../src/Bot/AfkSafeInputScript.h"
#include "../src/Bot/AfkInputPulse.h"
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iterator>
using namespace Bot;
int main(int argc,char** argv)
{
    if (argc==2 && std::string(argv[1])=="--lua")
    { std::cout<<AfkInputGuardScript(true); return 0; }
    AfkDeadGhostPolicy p;
    AfkObservation o{true,false,false,240000,0,300000};
    for (auto life : {AfkLifeState::Dead,AfkLifeState::Ghost})
        for (auto gap : {AfkDeathGap::Idle,AfkDeathGap::RoutingToCorpse,
            AfkDeathGap::WaitingForReclaim,AfkDeathGap::Failed})
            assert(!p.Blocker(life,gap,o));
    assert(p.Blocker(AfkLifeState::Unknown,AfkDeathGap::Idle,o));
    assert(p.Blocker(AfkLifeState::Ghost,AfkDeathGap::CommandInFlight,o));
    assert(p.Blocker(AfkLifeState::Ghost,AfkDeathGap::Unknown,o));
    p.Issued(AfkLifeState::Ghost,o,10,true,true,true);
    assert(!p.Qualified(AfkLifeState::Ghost));
    assert(p.Verify(o,11)==AfkResult::Pending);
    o.lastInput=240001; o.clientNow=240002;
    assert(p.Verify(o,12)==AfkResult::Confirmed);
    assert(p.Qualified(AfkLifeState::Ghost));
    assert(!p.Qualified(AfkLifeState::Dead));
    for (int missing=0; missing<3; ++missing)
    {
        AfkDeadGhostPolicy bad;
        bad.Issued(AfkLifeState::Dead,o,0,missing!=0,missing!=1,missing!=2);
        assert(bad.Failed());
        assert(bad.Blocker(AfkLifeState::Dead,AfkDeathGap::Failed,o));
    }
    AfkDeadGhostPolicy timeout;
    timeout.Issued(AfkLifeState::Ghost,o,0,true,true,true);
    assert(timeout.Verify(o,3000)==AfkResult::Failed);
    for (int flags=1; flags<4; ++flags)
    {
        auto active=o; active.clientAfk=(flags&1)!=0; active.serverAfk=(flags&2)!=0;
        assert(p.Blocker(AfkLifeState::Ghost,AfkDeathGap::Failed,active));
    }
    o.clientNow=o.lastInput+300000;
    assert(p.Blocker(AfkLifeState::Ghost,AfkDeathGap::Failed,o));

    // Same production scheduler, not an independent ghost timer. CTM and
    // displacement provide no clock evidence, and cannot retire sticky due.
    o={true,false,false,240000,0,300000};
    AfkProductionPolicy scheduler;
    AfkSafety dead; dead.death=true;
    assert(scheduler.Update(o,dead,0,false,true).action==AfkAction::None);
    assert(scheduler.Due());
    o.clientNow=270000;
    assert(scheduler.Update(o,dead,1,false,true).action==AfkAction::None);
    assert(scheduler.Due());
    AfkDeadGhostPolicy trial;
    assert(!trial.Blocker(AfkLifeState::Ghost,AfkDeathGap::Failed,o));
    auto gap=dead; gap.death=false; gap.navigation=true;
    for (auto field : {&AfkSafety::combat,&AfkSafety::recovery,&AfkSafety::water,
        &AfkSafety::dialog,&AfkSafety::vendor,&AfkSafety::trainer,&AfkSafety::talents,
        &AfkSafety::equipment,&AfkSafety::loot,&AfkSafety::fault})
    {
        auto blocked=gap; blocked.*field=true;
        assert(scheduler.Update(o,blocked,2,false,true).action==AfkAction::None);
        assert(scheduler.Due());
    }
    assert(scheduler.Update(o,gap,3,false,true).action==AfkAction::InputPulse);
    scheduler.Issued(AfkAction::InputPulse,o,3);
    trial.Issued(AfkLifeState::Ghost,o,3,true,true,true);
    assert(trial.Pending());
    assert(scheduler.Update(o,gap,4,false,true).action==AfkAction::None);
    assert(trial.Verify(o,4)==AfkResult::Pending);
    o.lastInput=270001; o.clientNow=270002;
    assert(trial.Verify(o,5)==AfkResult::Confirmed);
    assert(scheduler.Update(o,gap,5,false,true).result==AfkResult::Confirmed);
    assert(!scheduler.Due());
    assert(scheduler.Update(o,gap,6,false,true).action==AfkAction::None);
    trial={}; assert(!trial.Qualified(AfkLifeState::Ghost)); // session reset
    trial.Issued(AfkLifeState::Ghost,o,7,true,true,true);
    assert(trial.Verify({},8)==AfkResult::Failed); // world loss cannot qualify
    for (bool press : {false,true})
    {
        struct Driver { bool press; int released=0;
            bool Down() { return press; } bool Up() { ++released; return true; } } driver{press};
        assert(AfkInputPulse(driver)==press);
        assert(driver.released==1);
    }
    AfkCandidateScene scene; scene.known=true;
    for (int field=0; field<4; ++field)
    {
        auto changed=scene;
        if (field==0) changed.x=1;
        if (field==1) changed.facing=1;
        if (field==2) changed.target=1;
        if (field==3) changed.movementFlags=1;
        assert(!AfkCandidateScene::Unchanged(scene,changed));
    }
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    auto read=[](const auto& path) {
        std::ifstream file(path); assert(file);
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto adapter=read(root/"src/Bot/AfkClient5875.h");
    assert(adapter.find("deadGhostPulse && (action!=AfkAction::InputPulse")!=std::string::npos);
    assert(adapter.find("Core::Memory::Write")==std::string::npos);
    assert(adapter.find("GetAsyncKeyState(key)")!=std::string::npos);
    const auto shared=read(root/"src/Bot/SharedAfkController.h");
    assert(shared.find("!qualifying && !observeOnly_ && AfkDeadGhostPolicy::DeadOrGhost")!=std::string::npos);
    assert(shared.find("recoveryUnchanged() && dispatch.lifeVerified")!=std::string::npos);
    assert(shared.find("const auto pulseLife=dispatch.lifeBefore")!=std::string::npos);
    assert(shared.find("deadGhost_.Issued(pulseLife,dispatch.before")!=std::string::npos);
    const auto monitor=read(root/"src/Bot/WorldMonitor.h");
    assert(monitor.find("case DeathRecoveryState::WaitingForAlive: afkDeathGap=AfkDeathGap::CommandInFlight")!=std::string::npos);
    assert(monitor.find("case DeathRecoveryState::WaitingForGhost:")!=std::string::npos);
    assert(monitor.find("deathRecovery.Update(")<monitor.find("const auto afkDeathState="));
    assert(AfkInputGuardScript(false)==AfkSafeInputScript);
}
