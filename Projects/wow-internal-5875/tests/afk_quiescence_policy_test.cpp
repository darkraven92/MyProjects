#include "../src/Bot/AfkQualificationPolicy.h"
#include "../src/Bot/AfkQualificationHold.h"
#include "../src/Bot/AfkInputPulse.h"
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>

int main()
{
    using namespace Bot;
    using Phase=AfkQualificationPhase;
    AfkObservation live{true,false,false,57169770,57169769,300000};
    AfkQualificationHold hold(true);
    AfkQualificationPolicy p;
    assert(hold.Advance(nullptr));
    assert(p.Update(live,true,true,0).action==AfkAction::None);
    assert(p.Phase()==Phase::AwaitingQuiescence);
    assert(!p.BaselineCaptured());
    live.lastInput+=24; live.clientNow+=24;
    assert(p.Update(live,true,true,24).result!=AfkResult::Failed);
    assert(p.Phase()==Phase::AwaitingQuiescence);
    assert(!p.DeliveryVerified() && !p.ClearVerified() && p.Windows()==0);
    assert(p.Update(live,true,true,AfkQualificationPolicy::QuietIntervalMs).action==AfkAction::None);
    assert(!p.BaselineCaptured()); // interval restarts at the actual clock change
    assert(p.Update(live,true,true,24+AfkQualificationPolicy::QuietIntervalMs).action==AfkAction::None);
    assert(p.BaselineCaptured() && p.Phase()==Phase::Baseline);
    assert(p.BaselineObservation().lastInput==57169793);
    assert(!p.BaselineObservation().clientAfk && !p.BaselineObservation().serverAfk);
    assert(hold.InhibitsWorkloadAcquisition()); // same gate for both workloads
    ++live.lastInput; ++live.clientNow;
    assert(p.Update(live,true,true,2000).result==AfkResult::Failed);
    assert(std::string(p.Update(live,true,true,2001).reason)=="unattributed_input_during_qualification");
    hold.Abort("unattributed_input_during_qualification");
    assert(!hold.InhibitsWorkloadAcquisition());

    AfkQualificationPolicy noisy;
    noisy.Update(live,true,true,0);
    for (std::uint64_t now=100; now<AfkQualificationPolicy::MaximumQuiescenceMs; now+=100)
    {
        ++live.lastInput; ++live.clientNow;
        assert(noisy.Update(live,true,true,now).action==AfkAction::None);
        assert(!noisy.BaselineCaptured() && !noisy.DeliveryVerified());
    }
    assert(std::string(noisy.Update(live,true,true,AfkQualificationPolicy::MaximumQuiescenceMs).reason)==
        "quiescence_not_reached");
    assert(noisy.Phase()==Phase::Failed);
    assert(!noisy.BaselineCaptured());

    for (const char* reason : {"combat", "hostile_threat", "dialog", "death", "loading", "transaction", "water"})
    {
        AfkQualificationHold unsafeHold(true); unsafeHold.Advance(nullptr);
        AfkQualificationPolicy unsafe; unsafe.Update(live,true,true,0);
        assert(!unsafeHold.Advance(reason));
        assert(unsafe.Update(live,false,true,1).result==AfkResult::Failed);
        assert(!unsafeHold.InhibitsWorkloadAcquisition());
        assert(unsafe.PendingAction()==AfkAction::None); // no key dispatch in quiescence
    }
    AfkQualificationPolicy unknown; unknown.Update(live,true,true,0);
    assert(unknown.Update({},true,true,1).result==AfkResult::Failed);
    AfkQualificationPolicy reversed; reversed.Update(live,true,true,10);
    assert(reversed.Update(live,true,true,9).result==AfkResult::Failed);
    // Synchronization is AFTER the evidence boundary, not another noise grace.
    AfkQualificationPolicy syncInput;
    auto afk=live; afk.clientAfk=afk.serverAfk=true;
    syncInput.Update(afk,true,true,0);
    syncInput.Update(afk,true,true,AfkQualificationPolicy::QuietIntervalMs);
    ++afk.lastInput; ++afk.clientNow;
    assert(syncInput.Update(afk,true,true,AfkQualificationPolicy::QuietIntervalMs+1).result==AfkResult::Failed);

    AfkQualificationPolicy dispatchRace;
    afk.lastInput=0;
    dispatchRace.Update(afk,true,true,0);
    assert(dispatchRace.Update(afk,true,true,AfkQualificationPolicy::QuietIntervalMs).action==AfkAction::InputPulse);
    ++afk.lastInput;
    dispatchRace.Issued(afk,AfkQualificationPolicy::QuietIntervalMs+1);
    assert(dispatchRace.Phase()==Phase::Failed && !dispatchRace.DeliveryVerified());

    AfkQualificationPolicy active;
    live.clientAfk=live.serverAfk=true;
    active.Update(live,true,true,0);
    assert(active.Update(live,true,true,AfkQualificationPolicy::QuietIntervalMs).action==AfkAction::None);
    assert(active.Phase()==Phase::Synchronizing);
    const auto due=AfkQualificationPolicy::QuietIntervalMs+AfkQualificationPolicy::SynchronizationMs;
    assert(active.Update(live,true,true,due).action==AfkAction::InputPulse);
    const auto baselineClock=active.BaselineObservation().lastInput;
    active.Issued(live,due);
    assert(active.Update(live,true,true,due+1).result==AfkResult::Pending);
    live.lastInput+=100; live.clientNow+=100;
    assert(active.Update(live,true,true,due+2).result==AfkResult::Confirmed);
    assert(active.DeliveryVerified() && !active.ClearVerified());
    assert(active.BaselineObservation().lastInput==baselineClock);
    live.clientAfk=live.serverAfk=false;
    assert(active.Update(live,true,true,due+3).result==AfkResult::Confirmed);
    assert(active.ClearVerified() && active.Windows()==0);
    // Paired dispatch is unchanged; no held state survives an abort.
    struct Driver { int down=0,up=0; bool Down(){++down;return true;} bool Up(){++up;return true;} } driver;
    assert(AfkInputPulse(driver)); hold.Abort("combat");
    assert(driver.down==driver.up);
    AfkQualificationHold normal(false); assert(!normal.Advance(nullptr));
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream file(root/"src/Bot/SharedAfkController.h"); assert(file);
    const std::string source(std::istreambuf_iterator<char>(file),{});
    // The runtime must not abort before the policy can classify startup input.
    assert(source.find("o.lastInput!=previous_.lastInput &&")==std::string::npos);
    assert(source.find("baselineScene_=sceneNow")!=std::string::npos);
    assert(source.find("AFK QUIESCENCE COMPLETE")!=std::string::npos);
}
