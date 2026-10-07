#include "../src/Bot/AfkQualificationPolicy.h"
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>

int main()
{
    using namespace Bot;
    const AfkObservation active{true,true,true,400000,0,300000};
    auto delivered=active; delivered.lastInput=400000;
    auto clear=delivered; clear.clientAfk=clear.serverAfk=false;
    auto start=[&](AfkQualificationPolicy& p)
    {
        p.Update(active,true,true,0);
        assert(p.Update(active,true,true,1500).action==AfkAction::InputPulse);
        p.Issued(active,1500);
        assert(p.Update(delivered,true,true,1501).result==AfkResult::Confirmed);
        assert(p.DeliveryVerified() && !p.ClearVerified() && !p.PreventionStarted());
    };
    // Reproduce the live rejected input-only candidate. Not a prevention failure.
    AfkQualificationPolicy rejected;
    start(rejected);
    assert(rejected.Update(delivered,true,true,4500).result==AfkResult::Failed);
    assert(!rejected.PreventionStarted() && rejected.Windows()==0);
    for (auto setting : {AfkAutoClearSetting::Unknown,AfkAutoClearSetting::Disabled})
    {
        AfkQualificationPolicy p(true); start(p);
        assert(p.Update(delivered,true,true,1502,setting).result==AfkResult::Failed);
        assert(!p.ClearVerified() && !p.PreventionStarted());
    }
    for (int partial=0; partial<3; ++partial)
    {
        AfkQualificationPolicy p(true); start(p);
        assert(p.Update(delivered,true,true,1502,AfkAutoClearSetting::Enabled).action==AfkAction::NativeAutoClear);
        p.NativeClearIssued();
        auto observation=delivered;
        if (partial==1) observation.clientAfk=false;
        if (partial==2) observation.serverAfk=false;
        for (std::uint64_t now : {1503,2000,4499})
        {
            const auto result=p.Update(observation,true,true,now,AfkAutoClearSetting::Enabled);
            assert(result.action==AfkAction::None && result.result==AfkResult::Pending);
            assert(!p.ClearVerified() && !p.PreventionStarted());
        }
        assert(p.Update(observation,true,true,4500).result==AfkResult::Failed);
        assert(!p.PreventionStarted()); // no new deadline or fabricated window
    }
    AfkQualificationPolicy p(true); start(p);
    assert(p.Update(delivered,true,true,1502,AfkAutoClearSetting::Enabled).action==AfkAction::NativeAutoClear);
    p.NativeClearIssued();
    assert(p.Update(clear,true,true,2000).result==AfkResult::Confirmed);
    assert(p.ClearVerified() && p.PreventionStarted() && p.Windows()==0);
    assert(p.Phase()==AfkQualificationPhase::Prevention); // still TWO windows required
    for (unsigned index=1; index<=2; ++index)
    {
        const auto now=std::uint64_t(index)*240000+2000;
        clear.clientNow=clear.lastInput+240000;
        assert(p.Update(clear,true,true,now).action==AfkAction::InputPulse);
        p.Issued(clear,now);
        clear.lastInput=clear.clientNow;
        p.Update(clear,true,true,now+1);
        assert(p.Windows()==index-1);
        assert(p.Update(clear,true,true,now+2).result==AfkResult::Confirmed);
        assert(p.Windows()==index);
    }
    assert(p.Phase()==AfkQualificationPhase::Complete);
    AfkQualificationPolicy scene(true); start(scene);
    assert(scene.Update(delivered,true,false,1502,AfkAutoClearSetting::Enabled).result==AfkResult::Failed);
    AfkQualificationPolicy mismatch(true); start(mismatch);
    auto half=delivered; half.serverAfk=false;
    assert(mismatch.Update(half,true,true,1502,AfkAutoClearSetting::Enabled).action==AfkAction::None);
    AfkQualificationPolicy noDelivery(true);
    noDelivery.Update(active,true,true,0);
    noDelivery.Update(active,true,true,1500); noDelivery.Issued(active,1500);
    assert(noDelivery.Update(active,true,true,1501,AfkAutoClearSetting::Enabled).action==AfkAction::None);
    assert(!noDelivery.DeliveryVerified());
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream adapter(root/"src/Bot/AfkClient5875.h"); assert(adapter);
    const std::string code(std::istreambuf_iterator<char>(adapter),{});
    assert(code.find("Core::Memory::Write")==std::string::npos);
    assert(code.find("SetCVar")==std::string::npos);
    const auto sceneRead=code.find("result.sceneBefore=ReadScene(player)");
    const auto freshRead=code.find("result.before=Read(player)",sceneRead);
    assert(sceneRead<freshRead && freshRead<code.find("result.issued=AfkInputPulse(driver)",freshRead));
    assert(code.find("reinterpret_cast<void*>(player.address),0)")!=std::string::npos);
}
