#include "../src/Bot/QuestGiverOfferCycle.h"
#include "../src/Navigation/LocalRecoveryExhaustionPolicy.h"
#include "../src/Bot/QuestAcquisitionPolicy.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
static std::string Read(const char* path)
{
    std::ifstream file(path); assert(file);
    return {(std::istreambuf_iterator<char>(file)),{}};
}
int main()
{
    Bot::QuestGiverOfferCycle cycle;
    assert(cycle.TakeReaudit()==0);
    assert(cycle.Accepted(50001,123)); assert(cycle.TakeReaudit()==123);
    assert(cycle.TakeReaudit()==0); // one reread per verified acceptance
    assert(cycle.Accepted(50002,123)); assert(cycle.TakeReaudit()==123);
    assert(!cycle.Accepted(50002,123)); // cannot reopen on same confirmation forever
    assert(!cycle.Accepted(0,123)); assert(!cycle.Accepted(50003,0));
    for(int i=3;i<=20;++i) assert(cycle.Accepted(50000+i,123));
    assert(!cycle.Accepted(50021,123));
    cycle={}; assert(cycle.Accepted(50001,456)); // new bounded sweep
    using Navigation::LocalRecoveryExhaustionPolicy;
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(100,100,4));
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(110,100,4)); // backtrack
    assert(!LocalRecoveryExhaustionPolicy::EarnedProgressReset(97,100,4)); // short oscillation
    assert(LocalRecoveryExhaustionPolicy::EarnedProgressReset(95,100,4));
    assert(LocalRecoveryExhaustionPolicy::Assess(4,4,false,0,1)==
           Navigation::LocalRecoveryExhaustionDecision::FailExhausted);
    auto source=Read("src/Bot/GenericQuestDiscoveryController.h");
    const auto reread=source.find("const auto reauditGuid=offerCycle_.TakeReaudit()");
    assert(reread!=std::string::npos);
    assert(reread<source.find("const auto* auditProfile = FindNextGiverAuditProfile"));
    assert(source.find("ResolveActivatedProfile(snapshot)) MarkKnownQuestActive")!=std::string::npos);
    assert(source.find("return confirmedCheckedGiverEntries_;")!=std::string::npos);
    const auto confirmed=source.find("confirmedCheckedGiverEntries_.insert(giverEntry_)");
    assert(confirmed>source.find("selectedProfile_ = BestMatchingProfile(snapshot, offered)"));
    assert(source.find("confirmedCheckedGiverEntries_.insert",confirmed+1)==std::string::npos);
    source=Read("src/gui.cpp");
    const auto vendorCase=source.find("case IdVendorAutomation:");
    assert(source.find("RequestVendorAutomation",vendorCase)<source.find("return 0;",vendorCase));
    source=Read("src/Bot/QuestMaintenanceController.h");
    const auto enabled=source.find("if (enabled && !enabled_)");
    assert(enabled!=std::string::npos && source.find("nextTrip_=0;",enabled)!=std::string::npos);
    source=Read("src/Navigation/GenericNavMeshPathFollower.h");
    const auto reset=source.find("void ResetEscalatingRecovery(");
    assert(source.find("EarnedProgressReset",reset)<source.find("replans_ = 0",reset));
    assert(source.find("MaximumPathLength =\n            2000.0f")!=std::string::npos);
}
