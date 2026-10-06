#include "../src/Bot/AfkQualificationHold.h"
#include "../src/Bot/AfkProtectionPolicy.h"
#include "../src/Bot/AfkInputPulse.h"
#include "../src/Bot/AfkDiagnosticTracker.h"
#include "../src/Bot/AfkClientFlag5875Policy.h"
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>

int main()
{
    using namespace Bot;
    static_assert(AfkClientFlag5875Policy::Known(0) && !AfkClientFlag5875Policy::Active(0));
    static_assert(AfkClientFlag5875Policy::Known(1) && AfkClientFlag5875Policy::Active(1));
    static_assert(AfkClientFlag5875Policy::Known(2) && AfkClientFlag5875Policy::Active(2));
    static_assert(!AfkClientFlag5875Policy::Known(3));
    static_assert(!AfkClientFlag5875Policy::Known(0xffffffffu));
    AfkQualificationHold normal(false);
    assert(!normal.Advance(nullptr));
    assert(!normal.InhibitsWorkloadAcquisition());
    for (const char* workload : {"Questing", "Grinding"})
    {
        (void)workload;
        AfkQualificationHold hold(true);
        assert(hold.State()==AfkQualificationState::Requested);
        assert(hold.Advance(nullptr));
        for (int tick=0; tick<5000; ++tick)
            assert(hold.Advance(nullptr) && hold.InhibitsWorkloadAcquisition());
        hold.Complete();
        assert(!hold.InhibitsWorkloadAcquisition());
        assert(!hold.Advance(nullptr));
    }
    for (const char* reason : {"combat", "death", "dialog_visible", "transaction",
        "world_unavailable", "navigation", "recovery", "water", "input_fault"})
    {
        AfkQualificationHold hold(true);
        assert(hold.Advance(nullptr));
        assert(!hold.Advance(reason));
        assert(hold.State()==AfkQualificationState::Aborted);
        assert(std::string(hold.Reason())==reason);
        assert(!hold.InhibitsWorkloadAcquisition());
        assert(!hold.Advance(nullptr)); // no silent restart
    }
    AfkQualificationHold hold(true);
    AfkProtectionPolicy protection;
    AfkSafety safe; safe.healthyIdle=true;
    AfkObservation live{true,false,false,300000,0,300000};
    assert(hold.Advance(nullptr));
    assert(protection.Update(live,safe,0,true).action==AfkAction::None);
    assert(live.lastInput==0 && !live.clientAfk && !live.serverAfk);
    protection.Issued(AfkAction::InputPulse,live,10);
    assert(protection.Update(live,safe,11).result==AfkResult::Pending);
    assert(protection.Update(live,safe,3010).result==AfkResult::Failed);
    struct Driver { int down=0,up=0; bool Down(){++down;return true;} bool Up(){++up;return true;} } driver;
    assert(AfkInputPulse(driver)); hold.Abort("combat");
    assert(driver.down==driver.up); // no cross-tick held input to abandon
    AfkDiagnosticTracker candidate;
    candidate.Observe(true,true,0);
    protection.Reset();
    assert(protection.Update({},safe,0).status==AfkStatus::Unknown);
    AfkSafety work; work.navigation=true;
    assert(AfkWorkloadSafetyPolicy::Classify(work,true)==AfkWorkloadSafety::BenignWork);
    assert(protection.Update(live,work,1).action==AfkAction::None); // no unproven relaxation
    assert(live.lastInput==0); // owner/CTM never manufacture qualifying activity
    work.combat=true;
    assert(AfkWorkloadSafetyPolicy::Classify(work,true)==AfkWorkloadSafety::Unsafe);
    AfkCandidateScene scene{true,1,2,3,4,10,0};
    assert(AfkCandidateScene::Unchanged(scene,scene));
    auto changed=scene; changed.x=2;
    assert(!AfkCandidateScene::Unchanged(scene,changed));
    changed=scene; changed.target=11;
    assert(!AfkCandidateScene::Unchanged(scene,changed));
    changed=scene; changed.facing=5;
    assert(!AfkCandidateScene::Unchanged(scene,changed));
    changed=scene; changed.movementFlags=1;
    assert(!AfkCandidateScene::Unchanged(scene,changed));
    assert(!AfkCandidateScene::Unchanged(scene,{}));
    // Integration contract: the actual monitor returns to observation before
    // either workload dispatch, and the GUI-created WoW inherits its environment.
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    auto read=[](const auto& path) {
        std::ifstream input(path); assert(input);
        return std::string(std::istreambuf_iterator<char>(input),{});
    };
    const auto monitor=read(root/"src/Bot/WorldMonitor.h");
    const auto gate=monitor.find("sharedAfk.AdvanceQualificationHold(");
    assert(gate!=std::string::npos);
    const auto resume=monitor.find("continue;",gate);
    assert(resume<monitor.find("Phase 11A data-driven quest planner probe",gate));
    assert(resume<monitor.find("grindMode.Update(",gate));
    assert(monitor.find("AFK FLAG CANDIDATE")!=std::string::npos);
    assert(monitor.find("source=player_flags_candidate signalVerified=no")!=std::string::npos);
    const auto gui=read(root/"src/gui.cpp");
    const auto launch=gui.find("const BOOL created = CreateProcessW(\n            wow.c_str()");
    assert(launch!=std::string::npos);
    assert(gui.substr(launch,400).find("FALSE,\n            0,\n            nullptr,\n            workingDirectory.c_str()")!=std::string::npos);
}
