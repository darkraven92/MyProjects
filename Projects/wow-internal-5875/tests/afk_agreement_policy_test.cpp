#include "../src/Bot/AfkProductionPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using namespace Bot;
    AfkSafety idle; idle.healthyIdle=true;
    const AfkObservation clear{true,false,false,240000,0,300000};
    assert(AfkAgreementPolicy::Classify(clear)==AfkAgreementState::BothClear);
    assert(AfkAgreementPolicy::Classify({})==AfkAgreementState::Unknown);
    auto invalid=clear; invalid.thresholdMs=0;
    assert(AfkAgreementPolicy::Classify(invalid)==AfkAgreementState::Unknown);
    invalid=clear; invalid.lastInput=invalid.clientNow+1;
    assert(AfkAgreementPolicy::Classify(invalid)==AfkAgreementState::Unknown);
    assert(!AfkAgreementPolicy::Mixed(AfkAgreementState::Unknown));
    assert(!AfkAgreementPolicy::Mixed(AfkAgreementState::BothClear));
    assert(!AfkAgreementPolicy::Mixed(AfkAgreementState::BothActive));
    static_assert(AfkProductionPolicy::VerificationMs==3000); // unchanged budget, NOT measured convergence latency

    for (int bits=0; bits<4; ++bits)
    {
        auto o=clear; o.clientAfk=(bits&1)!=0; o.serverAfk=(bits&2)!=0;
        const auto expected=bits==0 ? AfkAgreementState::BothClear : bits==1 ?
            AfkAgreementState::ClientOnlyActive : bits==2 ? AfkAgreementState::ServerOnlyActive :
            AfkAgreementState::BothActive;
        assert(AfkAgreementPolicy::Classify(o)==expected);
        assert(AfkAgreementPolicy::Mixed(expected)==(bits==1 || bits==2));
        AfkProductionPolicy p;
        assert(p.Update(o,idle,100).action==AfkAction::InputPulse);
        p.Issued(AfkAction::InputPulse,o,100);
        assert(p.Update(o,idle,101).action==AfkAction::None); // input not yet delivered
        o.lastInput=o.clientNow;
        assert(p.Update(o,idle,102).result==AfkResult::Confirmed);
        const auto decision=p.Update(o,idle,103,false,false,AfkAutoClearSetting::Enabled);
        if (bits==0)
        {
            assert(decision.action==AfkAction::None);
            assert(p.Phase()==AfkProductionPhase::Recent); // prevention only
        }
        else if (bits==3)
        {
            assert(decision.action==AfkAction::NativeAutoClear);
            p.Issued(decision.action,o,103);
            for (std::uint64_t tick=104; tick<108; ++tick)
                assert(p.Update(o,idle,tick,false,false,AfkAutoClearSetting::Enabled).action==AfkAction::None);
            o.clientAfk=false; // server acknowledgment still outstanding
            assert(p.Update(o,idle,108).result==AfkResult::Pending);
            o.serverAfk=false;
            assert(p.Update(o,idle,109).result==AfkResult::Confirmed);
        }
        else
        {
            assert(decision.action==AfkAction::None && decision.result==AfkResult::Pending);
            assert(std::string(decision.reason)==AfkAgreementPolicy::ObservationReason(expected));
            for (std::uint64_t tick : {104u,1000u,3099u})
            {
                const auto waiting=p.Update(o,idle,tick,false,false,AfkAutoClearSetting::Enabled);
                assert(waiting.action==AfkAction::None && waiting.result==AfkResult::Pending);
            }
            const auto failure=p.Update(o,idle,3100,false,false,AfkAutoClearSetting::Enabled);
            assert(failure.result==AfkResult::Failed && failure.action==AfkAction::None);
            assert(std::string(failure.reason)==AfkAgreementPolicy::UnsupportedReason(expected));
            o.clientAfk=o.serverAfk=false;
            assert(p.Update(o,idle,4000).status==AfkStatus::Fault); // no automatic retry/toggle loop
        }
    }
    for (bool serverOnly : {false,true})
        for (bool convergesActive : {false,true})
        {
            AfkProductionPolicy p;
            auto o=clear; o.clientAfk=!serverOnly; o.serverAfk=serverOnly;
            p.Update(o,idle,0); p.Issued(AfkAction::InputPulse,o,0);
            o.lastInput=o.clientNow; p.Update(o,idle,1);
            assert(p.Update(o,idle,2,false,false,AfkAutoClearSetting::Enabled).action==AfkAction::None);
            // Simulated incoming authoritative evidence, not production writes.
            o.clientAfk=o.serverAfk=convergesActive;
            const auto result=p.Update(o,idle,3,false,false,AfkAutoClearSetting::Enabled);
            if (convergesActive)
            {
                assert(result.action==AfkAction::NativeAutoClear);
                p.Issued(result.action,o,3);
                assert(p.Update(o,idle,4).action==AfkAction::None);
                o.serverAfk=false;
                assert(p.Update(o,idle,5).result==AfkResult::Pending);
                o.clientAfk=false;
                assert(p.Update(o,idle,6).result==AfkResult::Confirmed);
            }
            else assert(result.result==AfkResult::Confirmed && result.action==AfkAction::None);
        }
    // A change from client-only to server-only does not earn another deadline.
    AfkProductionPolicy switched;
    auto o=clear; o.clientAfk=true;
    switched.Update(o,idle,0); switched.Issued(AfkAction::InputPulse,o,0);
    o.lastInput=o.clientNow; switched.Update(o,idle,1);
    o.clientAfk=false; o.serverAfk=true;
    assert(switched.Update(o,idle,2999).result==AfkResult::Pending);
    const auto failed=switched.Update(o,idle,3000);
    assert(std::string(failed.reason)=="server_only_afk_no_safe_reconciliation");
    AfkProductionPolicy lost;
    lost.Update(clear,idle,0); lost.Issued(AfkAction::InputPulse,clear,0);
    assert(lost.Update({},idle,1).result==AfkResult::Failed);
    assert(lost.Update({},idle,2).action==AfkAction::None);

    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    auto read=[](const auto& path) {
        std::ifstream file(path); assert(file);
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto adapter=read(root/"src/Bot/AfkClient5875.h");
    assert(adapter.find("Core::Memory::Write")==std::string::npos);
    assert(adapter.find("!result.before.clientAfk || !result.before.serverAfk")!=std::string::npos);
    assert(adapter.find("SendPacket")==std::string::npos);
    const auto controller=read(root/"src/Bot/SharedAfkController.h");
    for (const auto* event : {"AFK RECONCILIATION START", "AFK RECONCILIATION OBSERVE", "AFK RECONCILIATION RESULT"})
        assert(controller.find(event)!=std::string::npos);
    assert(controller.find("state!=previousAgreement_")!=std::string::npos);
    assert(controller.find("now>=nextMixedClockProbe_ && o.lastInput!=previous_.lastInput")!=std::string::npos);
}
