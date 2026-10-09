#include "../src/Bot/ClassTrainerPolicy.h"
#include "../src/Bot/ClassTrainerScript.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace Bot;
int main(int argc,char** argv)
{
    ClassTrainerCatalogue fixture;
    fixture.spells[50001]={50001,50001,1,"Test Ability","Rank 1"};
    fixture.spells[50002]={50002,50001,2,"Test Ability","Rank 2"};
    fixture.spells[50003]={50003,50003,0,"Second Ability",""};
    fixture.services.push_back({60001,8,70001,50001,50001,1,0,0,1,10,"Test Ability","Rank 1"});
    fixture.services.push_back({60001,8,70002,50002,50001,2,50001,0,2,20,"Test Ability","Rank 2"});
    fixture.services.push_back({60001,8,70003,50003,50003,0,0,0,1,10,"Second Ability",""});
    if(argc>1 && std::string(argv[1])=="--lua")
    {
        // Fixture replaces only parameter constants, not production decision logic.
        std::cout<<TrainerScript(fixture,60001,8,"MAGE","Test Trainer",50,0,true); return 0;
    }
    TrainerNeedPolicy need;
    assert(!need.Pending(0)); assert(need.ObserveLevel(1)); assert(!need.ObserveLevel(1));
    assert(need.Pending(1)); need.Complete(); assert(!need.Pending(50000));
    assert(need.ObserveLevel(2)); assert(need.Pending(2));
    need.Defer(10); assert(!need.Pending(2409)); assert(need.Pending(2410));
    assert(need.ObserveLevel(3)); assert(need.Pending(20));
    TrainerAbilityEvidence e{true,true,true,true,true,false,true,100,40,50,false};
    assert(ClassTrainerPolicy::Evaluate(e)==TrainerDecision::Learn);
    auto bad=e; bad.classMatches=false; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::WrongTrainer);
    bad=e; bad.classTrainer=false; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::WrongTrainer);
    bad=e; bad.knownSameOrHigher=true; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::KnownRank);
    bad=e; bad.frameOpen=false; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::FrameMissing);
    bad=e; bad.prerequisitesKnown=false; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::PrerequisiteMissing);
    bad=e; bad.liveAvailable=false; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::Unavailable);
    bad=e; bad.sourceBacked=false; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::MissingSource);
    bad=e; bad.cost=51; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::InsufficientFunds);
    bad=e; bad.maintenancePending=true; assert(ClassTrainerPolicy::Evaluate(bad)==TrainerDecision::MaintenancePending);
    assert(ClassTrainerPolicy::Reserve(100,80,30)==110);
    assert(ClassTrainerPolicy::Reserve(1000,0,0)==500);
    ClassTrainerCatalogue db; std::ifstream in("data/questdb/runtime/class_trainers.tsv");
    assert(db.Load(in)); assert(db.services.size()>100); assert(db.actors.size()>1);
    for(const auto& s:db.services) { assert(db.spells.contains(s.spell)); assert(db.actors.contains(s.entry)); }
    std::istringstream invalid("# wrong\n"); assert(!db.Load(invalid)); assert(db.services.empty());
    std::ifstream controller("src/Bot/ClassTrainerController.h");
    const std::string source{std::istreambuf_iterator<char>(controller),{}};
    assert(source.find("nav_->Start")!=std::string::npos);
    assert(source.find("defense_.Update")!=std::string::npos);
    assert(source.find("spellbook_confirmation_timeout")!=std::string::npos);
    assert(source.find("unconfirmed_.insert(spell)")<source.find("reserve_,0,true,spell)"));
    assert(source.find("ClassTrainerPolicy::Evaluate(evidence)")<source.find("reserve_,0,true,spell)"));
    assert(source.find("ClassTrainerPolicy::EpisodeTicks")!=std::string::npos);
    std::ifstream runtime("src/Bot/QuestPlannerRuntimeController.h");
    const std::string owner{std::istreambuf_iterator<char>(runtime),{}};
    assert(owner.find("maintenance_.Active() || trainer_.Active(),")!=std::string::npos);
    assert(owner.find("trainer_.Update")<owner.find("trainer_.TryStart"));
}
