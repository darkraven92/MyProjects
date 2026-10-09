#include "../src/Bot/QuestDiscoveryDialogDiagnostics.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    const std::string script = Bot::QuestDiscoveryDialogDiagnostics::Script;
    if (argc == 2 && std::string(argv[1]) == "--lua")
    {
        std::cout << script;
        return 0;
    }
    for (const auto* action : {"AcceptQuest(", "SelectAvailableQuest(",
            "SelectGossipAvailableQuest(", "CloseQuest(", "CloseGossip(", "InteractUnit("})
        assert(script.find(action) == std::string::npos);
    for (const auto* field : {"readFinished=", "gossipOpen=", "questFrameOpen=",
            "detailOpen=", "greetingOpen=", "gossipAvailable=", "gossipActive=", "pcall("})
        assert(script.find(field) != std::string::npos);
    assert(script.find("math.min(n,8)") != std::string::npos);
    assert(script.find("math.min(n,16)") != std::string::npos);
    std::ifstream input("src/Bot/GenericQuestDiscoveryController.h");
    assert(input);
    const std::string source{std::istreambuf_iterator<char>(input), {}};
    // Observation only at a changed dialog result (reset by each interaction),
    // or the terminal read-failure path, never a new monitor-tick probe loop.
    assert(source.find("LogDialogObservation(snapshot, result);\n                    lastDialogResult_ = result;")
        != std::string::npos);
    assert(source.find("WOW_INTERNAL_DISCOVERY_READ_FINISHED=false;") != std::string::npos);
    assert(source.find("WOW_INTERNAL_DISCOVERY_READ_FINISHED=true") != std::string::npos);
    // Original dispatch, exact-title acceptance and live-log gate stay present.
    assert(source.find("selectedProfile_ = BestMatchingProfile(snapshot, offered);") != std::string::npos);
    assert(source.find("t==\" + title + \" then") != std::string::npos);
    assert(source.find("GenericQuestDiscoveryState::WaitingForQuestLog") != std::string::npos);
}
