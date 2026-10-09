#include "../src/Bot/ServiceHubCatalogue.h"
#include "../src/Bot/ServiceHubRegistryPolicy.h"
#include "../src/Bot/ServiceHubBackoffPolicy.h"
#include "../src/Bot/MaintenanceOutcomePolicy.h"
#include "../src/Navigation/RouteCostProbePolicy.h"
#include "../src/Navigation/TerrainTransitionPolicy.h"
#include <cassert>
#include <iterator>

static std::string Read(const char* file)
{
    std::ifstream input(file); assert(input);
    return {std::istreambuf_iterator<char>(input), {}};
}
int main()
{
    using namespace Bot;
    using Navigation::RouteCostProbePolicy;
    assert(!RouteCostProbePolicy::MayIssueMovement(true));
    assert(RouteCostProbePolicy::MayIssueMovement(false));
    assert(RouteCostProbePolicy::HazardFingerprint({1,2})==RouteCostProbePolicy::HazardFingerprint({2,1,1}));
    assert(RouteCostProbePolicy::HazardFingerprint({1,2})!=RouteCostProbePolicy::HazardFingerprint({1,3}));
    struct Point { float x,y,z; };
    assert(Navigation::TerrainTransitionPolicy::Assess(Point{0,0,0},Point{5,0,8}).rejected);
    assert(!Navigation::TerrainTransitionPolicy::Assess(Point{0,0,0},Point{10,0,1}).rejected);
    // Both modes use the same query/validation function; only hazard readback
    // differs (loaded runtime state vs read-only file snapshot before startup).
    const auto follower=Read("src/Navigation/GenericNavMeshPathFollower.h");
    assert(follower.find("HardCellCentersForProbe(mapId_)")!=std::string::npos);
    assert(follower.find("persistentHazardPolygons =\n                ResolvePersistentHazardPolygons(player);")!=std::string::npos);
    assert(follower.find("validation=14O.1 revalidation=every_plan")!=std::string::npos);
    assert(follower.find("MaximumPathLength =\n            2000.0f")!=std::string::npos);
    const auto hazard=Read("src/Navigation/NavigationHazardMemory.h");
    const auto probe=hazard.substr(hazard.find("std::vector<NavPoint> HardCellCentersForProbe"));
    const auto function=probe.substr(0,probe.find("int HardCellCount"));
    assert(function.find("snapshot.LoadLocked()")!=std::string::npos);
    assert(function.find("SaveLocked") == std::string::npos);
    assert(function.find("RecordFailure") == std::string::npos);

    ServiceHubCatalogue catalogue;
    std::istringstream input("# SERVICE_HUBS_V1\tbuild=5875\tpatch=10\n"
        "N\t60001\t35\t4\nN\t60002\t35\t16388\nN\t60003\t36\t4\n"
        "F\t35\t35\t1\t1\t0\t0\t0\t0\t0\nF\t36\t36\t2\t2\t1\t35\t0\t0\t0\n"
        "P\t60001\t1\t1\t10\t0\t0\nP\t60001\t2\t1\t20\t0\t0\n"
        "P\t60002\t3\t1\t30\t0\t0\nP\t60003\t4\t1\t1\t0\t0\n"
        "P\t60002\t5\t0\t1\t0\t0\n");
    assert(catalogue.Load(input));
    auto candidates=catalogue.Candidates(1,35,{0,0,0},false);
    assert(candidates.size()==2 && candidates[0].entry==60001 && candidates[0].x==10);
    assert(candidates[0].source==ServiceHubSource::SourceBacked);
    assert(candidates[0].sell==ServiceKnowledge::Unknown && candidates[0].repair==ServiceKnowledge::Unknown);
    assert(catalogue.Candidates(1,0,{},false).empty());
    assert(catalogue.Candidates(1,35,{},true).size()==1);
    auto shortlist=ServiceHubSelectionPolicy::Shortlist(candidates,true,false,4);
    assert(shortlist[0]==0);
    ServiceHubBackoffPolicy backoff;
    backoff.Reject(candidates[0].entry,100,2400);
    assert(backoff.Blocked(candidates[0].entry,2499));
    assert(!backoff.Blocked(candidates[1].entry,101));
    candidates[0].failed=backoff.Blocked(candidates[0].entry,101);
    shortlist=ServiceHubSelectionPolicy::Shortlist(candidates,true,false,4);
    assert(shortlist.size()==1 && shortlist[0]==1);
    assert(!backoff.Blocked(candidates[0].entry,2500));
    backoff.Prune(2500);

    std::unordered_set<std::uint32_t> verified;
    std::ostringstream unverified;
    ServiceHubRegistryPolicy::Write(unverified,candidates,verified);
    assert(unverified.str().find("60001")==std::string::npos);
    verified.insert(candidates[0].entry);
    candidates[0].source=ServiceHubSource::MerchantFrameVerified;
    candidates[0].sell=ServiceKnowledge::Available;
    std::ostringstream saved;
    ServiceHubRegistryPolicy::Write(saved,candidates,verified);
    assert(saved.str().find("60001")!=std::string::npos);
    assert(saved.str().find("60002")==std::string::npos);

    assert(MaintenanceOutcomePolicy::Unmet(false,true,false,false,true,false,true,false,true));
    assert(MaintenanceOutcomePolicy::Unmet(false,false,true,true,false,false,true,false,true));
    assert(!MaintenanceOutcomePolicy::Unmet(false,true,true,true,true,false,true,false,true));
    assert(MaintenanceOutcomePolicy::Unmet(true,true,true,true,true,false,true,false,true));
    const auto vendor=Read("src/Bot/VendorController.h");
    assert(vendor.find("if (!MerchantOpen() || !vendor.valid")!=std::string::npos);
    assert(vendor.find("VENDOR ROUTE REVALIDATE")!=std::string::npos);
    assert(vendor.find("bagPressureSatisfied_ = true; // authoritative")!=std::string::npos);
    assert(vendor.find("maintenanceUnmet_ = MaintenanceOutcomePolicy::Unmet")!=std::string::npos);
    assert(vendor.find("MaximumServiceCandidates = 10")!=std::string::npos);
    const auto quest=Read("src/Bot/QuestMaintenanceController.h");
    assert(quest.find("SetCandidateBackoff(&candidateBackoff_)")!=std::string::npos);
    assert(quest.find("outcome_=vendor_->Outcome()")<quest.find("Cancel(); nextTrip_=tick+QuestMaintenancePolicy::RetryTicks;",quest.find("outcome_=vendor_->Outcome()")));
    assert(quest.find("maintenance_temporarily_unavailable")==std::string::npos); // typed policy name, not a duplicated string branch
    ServiceHubCatalogue real; std::ifstream data("data/questdb/runtime/service_hubs.tsv"); assert(real.Load(data));
    assert(real.Candidates(1,35,{},false).size()>1);
    std::istringstream invalid("# SERVICE_HUBS_V1\tbuild=5875\tpatch=10\nN\t1\t35\t16\n");
    assert(!real.Load(invalid));
}
