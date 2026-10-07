#include "../src/Bot/AfkRuntimeFaultPolicy.h"
#include "../src/Bot/AfkProductionPolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    using namespace Bot;
    AfkObservation due{true,false,false,240000,0,300000};
    for (int debt : {0,1,22})
    {
        const auto fault=AfkRuntimeFaultPolicy::Assess({false,false,debt});
        assert(!fault.terminal && fault.recoveryDebt==debt);
        AfkSafety roaming; roaming.navigation=true; roaming.fault=fault.terminal;
        AfkProductionPolicy policy;
        assert(policy.Update(due,roaming,1,false,true).action==AfkAction::InputPulse);
        assert(policy.Due());
        assert(policy.Update(due,roaming,2,false,true).action==AfkAction::InputPulse);
    }
    const auto combat=AfkRuntimeFaultPolicy::Assess({true,false,22});
    const auto grind=AfkRuntimeFaultPolicy::Assess({false,true,22});
    assert(combat.terminal && combat.reason==AfkRuntimeFaultReason::TerminalCombat);
    assert(grind.terminal && grind.reason==AfkRuntimeFaultReason::TerminalGrind);
    for (const auto fault : {combat,grind})
    {
        AfkSafety unsafe; unsafe.fault=fault.terminal; unsafe.faultReason=fault.reason;
        AfkProductionPolicy policy;
        const auto result=policy.Update(due,unsafe,1,false,true);
        assert(result.status==AfkStatus::Blocked && result.action==AfkAction::None);
        assert(std::string(result.reason)==AfkRuntimeFaultPolicy::ReasonName(fault.reason));
    }
    AfkSafety roaming; roaming.navigation=true;
    for (auto field : {&AfkSafety::combat,&AfkSafety::recovery,&AfkSafety::dialog,
        &AfkSafety::vendor,&AfkSafety::equipment,&AfkSafety::water})
    {
        auto unsafe=roaming; unsafe.*field=true;
        AfkProductionPolicy policy;
        assert(policy.Update(due,unsafe,1,false,true).status==AfkStatus::Blocked);
    }
    assert(AfkProductionPolicy::Band(due)==AfkProductionBand::Due);
    due.clientNow=270000;
    assert(AfkProductionPolicy::Band(due)==AfkProductionBand::Overdue);
    due.clientNow=300000;
    assert(AfkProductionPolicy::Band(due)==AfkProductionBand::ThresholdCrossed);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream file(root/"src/Bot/WorldMonitor.h"); assert(file);
    const std::string monitor(std::istreambuf_iterator<char>{file},{});
    assert(monitor.find("afkSafety.fault=afkFault.terminal")!=std::string::npos);
    assert(monitor.find("runtimeRobustness.RecoveriesWithoutProgress()>0") == std::string::npos);
}
