#include "../src/Bot/AfkProductionPolicy.h"
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>

int main()
{
    using namespace Bot;
    AfkSafety idle; idle.healthyIdle=true;
    AfkSafety roaming; roaming.navigation=true;
    AfkObservation o{true,false,false,239999,0,300000};
    AfkProductionPolicy p;
    assert(p.Update(o,roaming,0,false,true).action==AfkAction::None);
    o.clientNow=240000;
    assert(AfkProductionPolicy::Band(o)==AfkProductionBand::Due);
    assert(p.Update(o,roaming,1,false,true).action==AfkAction::InputPulse);
    assert(!p.SessionDeliveryVerified() && AfkProductionPolicy::ImplementationQualified);
    auto combat=roaming; combat.combat=true;
    assert(p.Update(o,combat,2,false,true).action==AfkAction::None);
    assert(p.Due() && p.Phase()==AfkProductionPhase::Deferred);
    o.clientNow=270000;
    assert(AfkProductionPolicy::Band(o)==AfkProductionBand::Overdue);
    p.Update(o,combat,30000,false,true);
    assert(p.Due()); // CTM, navigation and combat never reset input age
    assert(p.Update(o,roaming,30001,false,true).action==AfkAction::InputPulse);
    p.Issued(AfkAction::InputPulse,o,30001);
    assert(p.Update(o,roaming,30002,false,true).result==AfkResult::Pending);
    o.lastInput=o.clientNow;
    assert(p.Update(o,roaming,30003,false,true).result==AfkResult::Confirmed);
    assert(p.SessionDeliveryVerified() && !p.Due());
    assert(p.Update(o,roaming,30004,false,true).action==AfkAction::None);
    for (auto field : {&AfkSafety::combat,&AfkSafety::death,&AfkSafety::recovery,
        &AfkSafety::water,&AfkSafety::dialog,&AfkSafety::vendor,&AfkSafety::trainer,
        &AfkSafety::talents,&AfkSafety::equipment,&AfkSafety::loot,&AfkSafety::fault})
    {
        AfkProductionPolicy blocked; auto s=roaming; s.*field=true;
        auto due=o; due.clientNow=due.lastInput+240000;
        assert(blocked.Update(due,s,1,false,true).status==AfkStatus::Blocked);
        assert(blocked.Due());
    }
    for (int flags=1; flags<=3; ++flags)
    {
        AfkProductionPolicy r;
        auto active=o; active.clientAfk=(flags&1)!=0; active.serverAfk=(flags&2)!=0;
        active.clientNow+=10;
        assert(r.Update(active,idle,0).action==AfkAction::InputPulse);
        r.Issued(AfkAction::InputPulse,active,0);
        active.lastInput=active.clientNow;
        assert(r.Update(active,idle,1).result==AfkResult::Confirmed);
        assert(r.Phase()==AfkProductionPhase::RecoveringAfk); // F12 != recovery
        auto result=r.Update(active,idle,2,false,false,AfkAutoClearSetting::Enabled);
        if (flags==3)
        {
            assert(result.action==AfkAction::NativeAutoClear);
            r.Issued(result.action,active,2);
            assert(r.Update(active,idle,3).action==AfkAction::None);
            active.clientAfk=false;
            assert(r.Update(active,idle,4).result==AfkResult::Pending);
            active.serverAfk=false;
            assert(r.Update(active,idle,5).result==AfkResult::Confirmed);
        }
        else
        {
            // Native clear is NOT safe on mixed flags. Do not turn server AFK
            // on or claim that a native early-return recovered server-only AFK.
            assert(result.action==AfkAction::None);
            assert(r.Update(active,idle,3000).result==AfkResult::Failed);
            assert(r.Update(active,idle,4000).status==AfkStatus::Fault);
        }
    }
    AfkProductionPolicy unknown;
    assert(unknown.Update({},idle,0).action==AfkAction::None);
    AfkProductionPolicy timeout;
    o.clientAfk=o.serverAfk=true; o.clientNow+=20;
    timeout.Update(o,idle,0); timeout.Issued(AfkAction::InputPulse,o,0);
    o.lastInput=o.clientNow; timeout.Update(o,idle,1);
    timeout.Issued(AfkAction::NativeAutoClear,o,2999);
    assert(timeout.Update(o,idle,3000).result==AfkResult::Failed); // original deadline
    AfkProductionPolicy noProof;
    noProof.Update(o,idle,0); noProof.Issued(AfkAction::InputPulse,o,0);
    assert(noProof.Update(o,idle,1).action==AfkAction::None);
    assert(noProof.Update(o,idle,3000).result==AfkResult::Failed);
    assert(!noProof.SessionDeliveryVerified());
    AfkProductionPolicy preempted;
    auto fresh=o; fresh.clientAfk=fresh.serverAfk=false; fresh.clientNow+=240000;
    preempted.Update(fresh,roaming,0,false,true);
    preempted.Issued(AfkAction::InputPulse,fresh,0);
    fresh.lastInput=fresh.clientNow;
    assert(preempted.Update(fresh,combat,1,false,true).result==AfkResult::Confirmed);
    assert(preempted.Update(fresh,combat,2,false,true).action==AfkAction::None);
    p.Reset(); assert(!p.SessionDeliveryVerified() && AfkProductionPolicy::ImplementationQualified);
    auto due=o; due.clientAfk=due.serverAfk=false; due.clientNow=due.lastInput+240000;
    assert(p.Update(due,roaming,1,false,false).action==AfkAction::None); // no owner-name whitelist
    assert(p.Update(due,idle,2,true).action==AfkAction::None); // observe never acts
    assert(AfkWorkloadSafetyPolicy::LandMovementAllowed(0x13f,true));
    assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(1,false)); // qualification still stationary
    for (auto bit : {0x40u,0x80u,0x2000u,0x4000u,0x00200000u,0x02000000u,0x80000000u})
        assert(!AfkWorkloadSafetyPolicy::LandMovementAllowed(bit,true));
    for (bool serverOnly : {true,false})
    {
        AfkProductionPolicy converging;
        auto mixed=o; mixed.clientAfk=!serverOnly; mixed.serverAfk=serverOnly; mixed.clientNow++;
        converging.Update(mixed,idle,0); converging.Issued(AfkAction::InputPulse,mixed,0);
        mixed.lastInput=mixed.clientNow; converging.Update(mixed,idle,1);
        mixed.clientAfk=mixed.serverAfk=true; // authoritative convergence, NOT a flag write by runtime
        assert(converging.Update(mixed,idle,2,false,false,AfkAutoClearSetting::Enabled).action==AfkAction::NativeAutoClear);
    }
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    auto read=[](const auto& path) {
        std::ifstream file(path); assert(file);
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto shared=read(root/"src/Bot/SharedAfkController.h");
    const auto monitor=read(root/"src/Bot/WorldMonitor.h");
    assert(shared.find("AfkProductionPolicy policy_")!=std::string::npos);
    assert(shared.find("benign_work_awaiting_runtime_verified_noop")==std::string::npos);
    assert(shared.find("runtimeVerified=no")==std::string::npos);
    assert(monitor.find("TemporaryGrindModeEnabled ? \"Grinding\" : \"Questing\"")!=std::string::npos);
    assert(monitor.find("grindMode.FirstAidActive()")!=std::string::npos);
    const auto adapter=read(root/"src/Bot/AfkClient5875.h");
    assert(adapter.find("const auto afterScene=ReadScene(player)")>adapter.find("result.issued=AfkInputPulse(driver)"));
    assert(adapter.find("Core::Memory::Write")==std::string::npos);
}
