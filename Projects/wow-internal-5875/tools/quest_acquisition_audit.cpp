#include "../src/Bot/QuestAcquisitionPolicy.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
int main(int argc,char** argv)
{
    using namespace Bot;
    if(argc<4)
    { std::cerr<<"Usage: quest_acquisition_audit LEVEL RACE_TOKEN CLASS_TOKEN [log] [completion-ledger]\n"; return 2; }
    QuestEligibilityContext c;
    c.level=std::stoi(argv[1]); c.raceMask=QuestAcquisitionPolicy::RaceMask(argv[2]);
    c.classMask=QuestEligibilityPolicy::ClassMask(argv[3]);
    std::map<std::string,PlannerQuestLogEntry> observed;
    if(argc>4)
    {
        std::ifstream input(argv[4]);
        if(!input) return 2;
        std::regex pattern("Planner quest\\[[0-9]+\\]: title=\"([^\"]+)\" complete=(yes|no) objectiveRows=([0-9]+)");
        std::string line; std::smatch match;
        while(std::getline(input,line)) if(std::regex_search(line,match,pattern))
            observed[match[1]]={match[1],match[2]=="yes",std::stoi(match[3]),{}};
    }
    if(argc>5)
    {
        std::ifstream input(argv[5]); if(!input) return 2;
        std::string line;
        while(std::getline(input,line)) if(line.rfind("completed=",0)==0) c.completed.insert(std::stoi(line.substr(10)));
    }
    c.activeHistoryComplete=argc>4;
    for(const auto& [title,entry]:observed)
    {
        (void)title;
        const auto* profile=ValleyOfTrialsProfiles::Find(entry,argv[3]);
        if(profile) c.activeQuestIds.insert(profile->questId); else c.activeHistoryComplete=false;
    }
    int inactive=0,npc=0,go=0,spawn=0,executable=0,missingGiver=0,missingLocation=0,oldAudit=0;
    std::map<QuestAcquisitionResult,int> counts;
    std::map<QuestRuntimeSupport,int> support;
    for(const auto& p:ValleyOfTrialsProfiles::All())
    {
        const auto* node=ValleyOfTrialsProfiles::Graph().Find(p.questId);
        c.active=c.activeQuestIds.count(p.questId) || observed.count(p.title?p.title:"");
        if(c.active || c.completed.count(p.questId)) continue;
        ++inactive;
        npc+=p.giverEntry && !p.giverIsGameObject;
        go+=p.giverEntry && p.giverIsGameObject;
        spawn+=QuestAcquisitionPolicy::ValidDestination(p.giverDestination);
        executable+=node && node->classification.support==QuestRuntimeSupport::KnownExecutable;
        ++support[node->classification.support];
        missingGiver+=p.giverEntry==0;
        missingLocation+=!QuestAcquisitionPolicy::ValidDestination(p.giverDestination);
        ++counts[QuestAcquisitionPolicy::Evaluate(p,node,c)];
        auto oldContext=c; oldContext.raceMask.reset();
        const auto old=QuestEligibilityPolicy::Evaluate(p,node->classification,oldContext);
        oldAudit+=node->structurallyValid && node->classification.support==QuestRuntimeSupport::KnownExecutable &&
            (old==QuestEligibility::Eligible || old==QuestEligibility::PrerequisiteUnknown || old==QuestEligibility::UnknownRestrictions);
    }
    std::cout<<"catalogue="<<ValleyOfTrialsProfiles::All().size()<<" level="<<c.level
        <<" race="<<argv[2]<<" class="<<argv[3]<<" recordedActive="<<observed.size()
        <<" resolvedActive="<<c.activeQuestIds.size()<<" knownCompleted="<<c.completed.size()<<'\n';
    std::cout<<"inactiveUncompleted="<<inactive<<" npcGiver="<<npc<<" goGiver="<<go<<" withSpawn="<<spawn
        <<" executable="<<executable<<" missingGiver="<<missingGiver<<" missingLocation="<<missingLocation
        <<" priorAuditPermittedIncludingUnknown="<<oldAudit<<'\n';
    for(const auto& [reason,count]:counts) std::cout<<QuestAcquisitionPolicy::Name(reason)<<'='<<count<<'\n';
    for(const auto& [kind,count]:support) std::cout<<"inactiveSupport="<<QuestClassificationPolicy::SupportName(kind)<<" count="<<count<<'\n';
    std::cout<<"Static potential only: no fabricated live offers, reachability or complete history.\n";
}
