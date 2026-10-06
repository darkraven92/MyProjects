#include "../src/Bot/AfkQualificationPolicy.h"
#include "../src/Bot/AfkQualificationHold.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <limits>

int main()
{
    using namespace Bot;
    using Phase=AfkQualificationPhase;
    const AfkObservation active{true,true,true,400000,0,300000};
    auto clear=active; clear.clientAfk=clear.serverAfk=false;
    AfkQualificationPolicy normal;
    assert(normal.Update(clear,true,true,0).action==AfkAction::None);
    assert(normal.Phase()==Phase::Baseline);
    assert(normal.Update(active,true,true,1).action==AfkAction::InputPulse);
    assert(normal.Phase()==Phase::CandidateReady);
    assert(normal.Update(clear,true,true,2).action==AfkAction::None);
    assert(normal.Phase()==Phase::Baseline); // natural clear isn't candidate proof
    AfkQualificationPolicy dispatchRace;
    dispatchRace.Update(active,true,true,0);
    dispatchRace.Issued(clear,1); // adapter's fresher pre-command observation
    assert(dispatchRace.Update(clear,true,true,2).result==AfkResult::Failed);
    assert(!dispatchRace.ClearVerified());
    AfkQualificationPolicy unknown;
    assert(unknown.Update({},true,true,0).result==AfkResult::Failed);
    auto recent=active; recent.lastInput=recent.clientNow-354;
    for (bool naturallyClears : {false,true})
    {
        AfkQualificationPolicy sync;
        assert(sync.Update(recent,true,true,0).action==AfkAction::None);
        assert(sync.Phase()==Phase::Synchronizing);
        assert(sync.Update(recent,true,true,999).action==AfkAction::None);
        auto next=recent;
        if (naturallyClears) next.clientAfk=next.serverAfk=false;
        const auto decision=sync.Update(next,true,true,1000);
        assert(sync.Phase()==(naturallyClears ? Phase::Baseline : Phase::CandidateReady));
        assert(decision.action==(naturallyClears ? AfkAction::None : AfkAction::InputPulse));
        assert(next.lastInput==recent.lastInput); // never manufacture clock/AFK evidence
        assert(!sync.DeliveryVerified() && !sync.ClearVerified());
    }
    AfkQualificationPolicy p;
    AfkQualificationHold hold(true);
    assert(hold.Advance(nullptr));
    assert(p.Update(active,true,true,0).action==AfkAction::InputPulse);
    p.Issued(active,10);
    assert(p.Update(active,true,true,11).result==AfkResult::Pending);
    auto delivered=active; delivered.lastInput=delivered.clientNow;
    assert(p.Update(delivered,true,true,20).result==AfkResult::Confirmed);
    assert(p.DeliveryVerified() && !p.ClearVerified() && p.Windows()==0);
    assert(p.Update(delivered,true,true,21).result==AfkResult::Pending);
    auto halfClear=delivered; halfClear.clientAfk=false;
    assert(p.Update(halfClear,true,true,22).result==AfkResult::Pending);
    assert(!p.ClearVerified());
    clear=halfClear; clear.serverAfk=false;
    assert(p.Update(clear,true,true,1000).result==AfkResult::Confirmed);
    assert(p.ClearVerified() && p.Windows()==0 && p.Phase()==Phase::Prevention);
    // Candidate clear is a fresh baseline, NOT either prevention window.
    for (unsigned index=1; index<=2; ++index)
    {
        const auto now=std::uint64_t(index)*240000;
        clear.clientNow=clear.lastInput+239999;
        assert(p.Update(clear,true,true,now-1).action==AfkAction::None);
        clear.clientNow++;
        assert(p.Update(clear,true,true,now).action==AfkAction::InputPulse);
        p.Issued(clear,now);
        clear.lastInput=clear.clientNow;
        assert(p.Update(clear,true,true,now+1).result==AfkResult::Confirmed);
        assert(p.Windows()==index-1 && !p.ClearVerified());
        assert(p.Update(clear,true,true,now+2).result==AfkResult::Confirmed);
        assert(p.Windows()==index);
        assert(hold.InhibitsWorkloadAcquisition());
    }
    assert(p.Phase()==Phase::Complete);
    hold.Complete(); assert(!hold.InhibitsWorkloadAcquisition());
    for (bool advances : {false,true})
    {
        AfkQualificationPolicy timeout;
        timeout.Update(active,true,true,0); timeout.Issued(active,10);
        timeout.Update(advances ? delivered : active,true,true,11);
        const auto result=timeout.Update(advances ? delivered : active,true,true,3010);
        assert(result.result==AfkResult::Failed);
        assert(std::string(result.reason)==(advances ? "candidate_afk_clear_timeout" : "candidate_delivery_timeout"));
        assert(timeout.Update(clear,true,true,3011).result==AfkResult::Failed);
    }
    for (bool safe : {false,true})
    {
        AfkQualificationPolicy failed;
        failed.Update(active,true,true,0); failed.Issued(active,10);
        assert(failed.Update(delivered,safe,false,11).result==AfkResult::Failed);
        assert(failed.Windows()==0);
    }
    // Scene/UI failures must fail even after input delivery, while AFK propagates.
    AfkQualificationPolicy sideEffect;
    sideEffect.Update(active,true,true,0); sideEffect.Issued(active,10);
    sideEffect.Update(delivered,true,true,11);
    assert(sideEffect.Update(clear,true,false,12).result==AfkResult::Failed);
    AfkCandidateScene scene{true,1,2,3,0,10,0};
    for (int field=0; field<6; ++field)
    {
        auto changed=scene;
        switch (field)
        {
        case 0: changed.x+=0.02f; break;
        case 1: changed.facing+=0.01f; break;
        case 2: ++changed.target; break;
        case 3: ++changed.movementFlags; break;
        case 4: changed.known=false; break;
        case 5: changed.x=std::numeric_limits<float>::quiet_NaN(); break;
        }
        assert(!AfkCandidateScene::Unchanged(scene,changed));
        AfkQualificationPolicy badScene;
        badScene.Update(active,true,true,0); badScene.Issued(active,10);
        assert(badScene.Update(delivered,true,AfkCandidateScene::Unchanged(scene,changed),11).result==AfkResult::Failed);
    }
    auto rounding=scene; rounding.x+=0.001f; rounding.facing=6.283185307179586f;
    assert(AfkCandidateScene::Unchanged(scene,rounding));
    // A prevention interval containing AFK cannot be rescued and counted as PASS.
    AfkQualificationPolicy interrupted;
    interrupted.Update(active,true,true,0); interrupted.Issued(active,10);
    interrupted.Update(delivered,true,true,11);
    interrupted.Update(clear,true,true,12);
    assert(interrupted.Update(active,true,true,13).result==AfkResult::Failed);
    assert(interrupted.Windows()==0);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream file(root/"src/Bot/SharedAfkController.h"); assert(file);
    const std::string source(std::istreambuf_iterator<char>(file),{});
    assert(source.find("initial_afk_state_not_clear")==std::string::npos);
    assert(source.find("qualificationFlow_.Update(")!=std::string::npos);
}
