#include "../src/Bot/WaterEvidencePolicy5875.h"
#include "../src/Bot/MirrorTimerProtocol5875.h"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static std::string Source(const char* relative)
{
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream file(root/relative);
    assert(file);
    return {std::istreambuf_iterator<char>{file},{}};
}

int main()
{
    using namespace Bot;
    WaterEvidenceTracker5875 tracker;
    WaterEvidenceSnapshot5875 evidence;
    assert(tracker.Update(evidence)==WaterObservationClass::Unknown);
    evidence.movementKnown=true;
    assert(tracker.Update(evidence)==WaterObservationClass::Unknown);
    assert(tracker.Update(evidence)==WaterObservationClass::DryOrNonSwimming);
    assert(!evidence.groundContactKnown); // !swimming cannot mean dry ground.
    evidence.movementFlags=WaterEvidenceTracker5875::SwimmingMask;
    assert(tracker.Update(evidence)==WaterObservationClass::Unknown);
    evidence.movementFlags=0;
    assert(tracker.Update(evidence)==WaterObservationClass::DryOrNonSwimming);
    evidence.movementFlags=WaterEvidenceTracker5875::SwimmingMask;
    assert(tracker.Update(evidence)==WaterObservationClass::Unknown);
    assert(tracker.Update(evidence)==WaterObservationClass::SwimmingStateUnknown);
    assert(!evidence.submergedKnown && !evidence.surfaceKnown);
    evidence.movementKnown=false;
    assert(tracker.Update(evidence)==WaterObservationClass::Unknown);
    assert(tracker.ShouldEmit(WaterObservationClass::Unknown,100));
    assert(!tracker.ShouldEmit(WaterObservationClass::Unknown,101));
    assert(tracker.ShouldEmit(WaterObservationClass::Unknown,30100));
    assert(tracker.ShouldEmit(WaterObservationClass::SwimmingStateUnknown,30101));

    // 5875 START type=BREATH,current=50000,max=60000,scale=-1,paused=0,spell=0.
    const std::array<std::uint8_t,21> draining{
        1,0,0,0, 0x50,0xc3,0,0, 0x60,0xea,0,0,
        0xff,0xff,0xff,0xff, 0, 0,0,0,0};
    auto timer=DecodeMirrorTimer5875(0x1d9,draining);
    assert(timer.valid && timer.kind==MirrorTimerKind5875::Breath);
    assert(timer.current==50000 && timer.maximum==60000 && timer.scale==-1);
    assert(timer.direction==MirrorTimerDirection5875::Draining);
    auto refilling=draining;
    refilling[12]=10; refilling[13]=refilling[14]=refilling[15]=0;
    timer=DecodeMirrorTimer5875(0x1d9,refilling);
    assert(timer.direction==MirrorTimerDirection5875::Refilling);
    // Even a known refilling BREATH event does not prove surface NOW.
    assert(!evidence.surfaceKnown);
    const std::array<std::uint8_t,5> pause{1,0,0,0,1};
    timer=DecodeMirrorTimer5875(0x1da,pause);
    assert(timer.valid && timer.event==MirrorTimerEvent5875::Pause);
    assert(timer.paused && timer.direction==MirrorTimerDirection5875::Paused);
    const std::array<std::uint8_t,4> stop{1,0,0,0};
    timer=DecodeMirrorTimer5875(0x1db,stop);
    assert(timer.valid && timer.event==MirrorTimerEvent5875::Stop);
    assert(timer.direction==MirrorTimerDirection5875::Unknown);
    assert(!DecodeMirrorTimer5875(0x1d9,stop).valid);
    assert(!DecodeMirrorTimer5875(0x1da,stop).valid);
    assert(!DecodeMirrorTimer5875(0x1db,pause).valid);

    const auto observe=Source("src/Bot/WaterEvidenceObserve5875.h");
    const auto adapter=Source("src/Bot/WaterEvidence5875.h");
    const auto monitor=Source("src/Bot/WorldMonitor.h");
    assert(adapter.find("0x20e0d4")!=std::string::npos);
    assert(adapter.find("player.movement)+0x40")!=std::string::npos);
    assert(observe.find("WATER EVIDENCE ")!=std::string::npos);
    assert(observe.find("breathKnown=no")!=std::string::npos);
    assert(observe.find("submergedKnown=no")!=std::string::npos);
    for (const char* forbidden : {"SendMessage", "SendInput", "ClickToMove",
        "TargetController", "FrameScript", "Ascend", "Jump", "WM_KEY"})
        assert(observe.find(forbidden)==std::string::npos);
    const auto branch=monitor.find("WaterEvidenceObserve5875::Run");
    const auto combatStart=monitor.find("combat.Start(0)");
    assert(branch!=std::string::npos && combatStart!=std::string::npos &&
           branch<combatStart);
}
