#include "../src/Bot/ServiceHubRegistryPolicy.h"

#include <cassert>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

int main()
{
    using namespace Bot;
    ServiceHubCandidate wuark{};
    wuark.entry = 3167;
    wuark.source = ServiceHubSource::Seeded;
    wuark.positionKnown = true;
    wuark.x = 357.1937f;
    wuark.y = -4708.1279f;
    wuark.z = 14.4788f;
    ServiceHubCandidate taiTasi{};
    taiTasi.entry = 3187;
    taiTasi.source = ServiceHubSource::Seeded;
    std::vector<ServiceHubCandidate> hubs{wuark, taiTasi};
    std::unordered_set<std::uint32_t> verified;

    // The current v2 row and an older v1 merchant can coexist with seeds.
    std::istringstream input(
        "V\t2\t3167\t358.128\t-4706.73\t14.394\t1\t0\n"
        "V\t1\t3187\t200\t-3200\t90\n"
        "V\t2\t5942\t210\t-3205\t90\t1\t2\n");
    ServiceHubRegistryPolicy::Load(input, hubs, verified);
    assert(hubs.size() == 3 && verified.size() == 3);
    assert(hubs[0].entry == 3167 && hubs[0].positionKnown);
    assert(hubs[0].source == ServiceHubSource::Persisted);
    assert(hubs[1].entry == 3187 && hubs[1].positionKnown);
    assert(hubs[1].sell == ServiceKnowledge::Available);
    assert(hubs[1].repair == ServiceKnowledge::Unknown);
    assert(hubs[2].entry == 5942);
    assert(hubs[2].repair == ServiceKnowledge::Unavailable);

    // Updating one merchant and saving must not delete the other two.
    hubs[1].repair = ServiceKnowledge::Available;
    std::ostringstream serialized;
    ServiceHubRegistryPolicy::Write(serialized, hubs, verified);
    std::vector<ServiceHubCandidate> reloaded;
    std::unordered_set<std::uint32_t> reloadedVerified;
    std::istringstream reload(serialized.str());
    ServiceHubRegistryPolicy::Load(reload, reloaded, reloadedVerified);
    assert(reloaded.size() == 3 && reloadedVerified.size() == 3);
    assert(reloaded[0].entry == 3167);
    assert(reloaded[1].entry == 3187 &&
           reloaded[1].repair == ServiceKnowledge::Available);
    assert(reloaded[2].entry == 5942 &&
           reloaded[2].repair == ServiceKnowledge::Unavailable);

    // An unverified, positionless seed is never persisted as proven.
    ServiceHubCandidate unverified{};
    unverified.entry = 9999;
    hubs.push_back(unverified);
    std::ostringstream withoutSeed;
    ServiceHubRegistryPolicy::Write(withoutSeed, hubs, verified);
    assert(withoutSeed.str().find("9999") == std::string::npos);
}
