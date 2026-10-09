#include "../src/Bot/VanillaQuestDatabase.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
int main()
{
    char path[]="/tmp/wow-metadata-scope-XXXXXX";
    const int descriptor=mkstemp(path); assert(descriptor>=0); close(descriptor);
    std::ofstream out(path);
    for(int id : {100001,100002})
    {
        out << "Q\t" << id << "\tFixture\t1\t0\t17\t18\t0\tTravelReport\t18\t0\t0\t1\tTarget\tNone\t600\t-1\t0\n";
        out << "M\t" << id << "\t2\t1\t0\t14\t7\t0\n";
        if(id==100002) out << "H\t" << id << "\tenrichment_only\n";
    }
    out.close(); assert(out);
    assert(setenv("WOW_INTERNAL_QUESTDB_CATALOG",path,1)==0);
    auto& database=Bot::VanillaQuestDatabase::Instance();
    assert(database.EnsureLoaded());
    assert(database.Profiles().size()==1);
    assert(database.Profiles().front().questId==100001);
    const auto* metadata=database.SourceProfile(100002);
    assert(metadata && metadata->requiredCondition==7 && metadata->requiredRaceMask==2);
    assert(metadata->sourceObjectiveCount==0);
    assert(std::string(metadata->title)=="Fixture");
}
