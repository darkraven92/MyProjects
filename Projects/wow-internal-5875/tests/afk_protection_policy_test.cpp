#include "../src/Bot/AfkProtectionPolicy.h"
#include "../src/Bot/AfkInputPulse.h"
#include "../src/Bot/AfkPreventionWindow.h"
#include "../src/Bot/AfkSafeInputScript.h"
#include <cassert>
#include <stdexcept>
#include <initializer_list>
#include <iostream>
#include <string>
using namespace Bot;
int main(int argc,char** argv)
{
    if (argc==2 && std::string(argv[1])=="--lua")
    { std::cout<<AfkSafeInputScript; return 0; }
    AfkProtectionPolicy p;
    AfkSafety safe; safe.healthyIdle=true;
    AfkObservation o{true,false,false,239999,0,300000};
    assert(p.Update({},safe,0).status==AfkStatus::Unknown);
    assert(p.Update(o,safe,0).action==AfkAction::None);
    o.clientNow=240000;
    assert(p.Update(o,safe,1).action==AfkAction::InputPulse);
    for (auto field : {&AfkSafety::combat,&AfkSafety::navigation,&AfkSafety::death,
        &AfkSafety::recovery,&AfkSafety::water,&AfkSafety::dialog,&AfkSafety::vendor,
        &AfkSafety::trainer,&AfkSafety::talents,&AfkSafety::equipment,&AfkSafety::loot,&AfkSafety::fault})
    {
        auto blocked=safe; blocked.*field=true;
        assert(p.Update(o,blocked,1).status==AfkStatus::Blocked);
        assert(p.Update(o,blocked,1).action==AfkAction::None);
    }
    assert(p.Update(o,{},1).action==AfkAction::None);
    assert(p.Update(o,safe,1,true).action==AfkAction::None);
    p.Issued(AfkAction::InputPulse,o,10);
    assert(p.Update(o,safe,11).result==AfkResult::Pending);
    assert(p.Update(o,safe,11).action==AfkAction::None);
    o.clientNow=240100; o.lastInput=240050;
    assert(p.Update(o,safe,100).result==AfkResult::Confirmed);
    assert(p.Update(o,safe,101).action==AfkAction::None);
    o.serverAfk=o.clientAfk=true;
    assert(p.Update(o,safe,102).action==AfkAction::ClearFlag);
    p.Issued(AfkAction::ClearFlag,o,102);
    o.clientAfk=false;
    assert(p.Update(o,safe,103).result==AfkResult::Pending);
    o.serverAfk=false;
    assert(p.Update(o,safe,104).result==AfkResult::Confirmed);
    o.clientNow=480050;
    assert(p.Update(o,safe,105).action==AfkAction::InputPulse);
    p.Issued(AfkAction::InputPulse,o,105);
    assert(p.Update({},safe,3105).result==AfkResult::Failed);
    assert(p.Update(o,safe,500000).status==AfkStatus::Fault);
    p.Reset(); // stop/restart cannot restore pending input or a prior clock
    assert(p.Update({},safe,0).status==AfkStatus::Unknown);
    o={true,false,false,0x100u,0xfffffff0u,300000};
    assert(p.Update(o,safe,0).status==AfkStatus::Active); // clock wrap
    struct Driver
    {
        int downs=0,ups=0; bool fail=false,throws=false;
        bool Down() { ++downs; if(throws) throw std::runtime_error("input"); return !fail; }
        bool Up() { ++ups; return true; }
    };
    Driver d; assert(AfkInputPulse(d)); assert(d.downs==1 && d.ups==1);
    d.fail=true; assert(!AfkInputPulse(d)); assert(d.ups==2);
    d.throws=true; assert(!AfkInputPulse(d)); assert(d.ups==3);
    AfkPreventionWindow window;
    o={true,false,false,240000,0,300000};
    window.Issued(o);
    o.lastInput=o.clientNow;
    assert(!window.Confirmed(AfkAction::InputPulse,o)); // initial pulse is not a window
    o.clientNow+=240000;
    window.Observe(o,true); window.Issued(o);
    o.lastInput=o.clientNow;
    assert(window.Confirmed(AfkAction::InputPulse,o));
    o.clientNow+=240000; o.serverAfk=true;
    window.Observe(o,true); o.serverAfk=false; // cannot erase an intervening AFK event
    window.Issued(o); o.lastInput=o.clientNow;
    assert(!window.Confirmed(AfkAction::InputPulse,o));
    window.Observe({},true); // missing observation invalidates a prevention interval
    o.clientNow+=240000; window.Issued(o); o.lastInput=o.clientNow;
    assert(!window.Confirmed(AfkAction::InputPulse,o));
    window.Observe(o,false); // ownership preemption cannot count as controlled safe idle
    o.clientNow+=240000; window.Issued(o); o.lastInput=o.clientNow;
    assert(!window.Confirmed(AfkAction::InputPulse,o));
    window.Issued(o); // recent input is not a full prevention interval
    assert(!window.Confirmed(AfkAction::InputPulse,o));
    p.Reset(); o.clientAfk=o.serverAfk=true;
    assert(p.Update(o,safe,1).action==AfkAction::InputPulse);
    p.Issued(AfkAction::InputPulse,o,1); p.Reset();
    assert(p.PendingAction()==AfkAction::None);
    p.DispatchFailed();
    assert(p.Update(o,safe,2).status==AfkStatus::Fault);
}
