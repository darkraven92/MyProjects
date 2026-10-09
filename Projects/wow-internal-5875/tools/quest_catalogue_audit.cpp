#include "../src/Bot/QuestPlanner.h"
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
int main(int argc, char** argv)
{
    using namespace Bot;
    const auto& profiles=ValleyOfTrialsProfiles::All();
    std::map<QuestRuntimeSupport,int> totals;
    for(const auto& p:profiles) ++totals[QuestClassificationPolicy::Classify(p).support];
    std::cout << "catalogue=" << VanillaQuestDatabase::Instance().LoadedPath() << " total=" << profiles.size();
    for(auto support:{QuestRuntimeSupport::KnownExecutable,QuestRuntimeSupport::KnownSemanticButUnsupported,
        QuestRuntimeSupport::Ambiguous,QuestRuntimeSupport::MissingData})
        std::cout << ' ' << QuestClassificationPolicy::SupportName(support) << '=' << totals[support];
    const auto& graph=ValleyOfTrialsProfiles::Graph();
    int invalid=0;
    for(const auto& [id,node]:graph.Nodes()) { (void)id; invalid+=!node.structurallyValid; }
    std::cout << " nodes=" << graph.Nodes().size() << " edges=" << graph.Edges().size()
        << " validationIssues=" << graph.Issues().size() << " invalidNodes=" << invalid << '\n';
    std::map<QuestGraphIssueKind,int> issues;
    for(const auto& issue:graph.Issues()) ++issues[issue.kind];
    for(const auto& [kind,count]:issues) std::cout << "graphIssueKind=" << static_cast<int>(kind) << " count=" << count << '\n';
    if(argc<2) return 0;
    std::ifstream log(argv[1]);
    std::regex pattern("Planner quest\\[[0-9]+\\]: title=\"([^\"]+)\" complete=(yes|no) objectiveRows=([0-9]+)");
    std::map<std::string,PlannerQuestLogEntry> observed;
    std::string line; std::smatch match;
    while(std::getline(log,line)) if(std::regex_search(line,match,pattern))
        observed[match[1]]={match[1],match[2]=="yes",std::stoi(match[3]),{}};
    int resolved=0;
    for(const auto& [title,entry]:observed)
    {
        const auto* p=ValleyOfTrialsProfiles::Find(entry,"WARRIOR");
        resolved+=p!=nullptr;
        std::cout << "identity title=\"" << title << "\" quest=" << (p?p->questId:0)
            << " result=" << (p?"resolved":"missing_duplicate_or_row_conflict") << '\n';
    }
    std::cout << "recordedActive=" << observed.size() << " resolved=" << resolved
        << " unresolved=" << observed.size()-resolved << '\n';
}
