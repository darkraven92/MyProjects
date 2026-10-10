#include "../src/Bot/CombatAttackReadback.h"
#include "../src/Bot/CombatInputProbePolicy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc==2 && std::string_view(argv[1])=="--lua")
    { std::cout<<Bot::CombatAttackReadbackScript; return 0; }
    Bot::CombatAttackStatus s{};
    for (int slot=1; slot<=120; ++slot)
        for (int active=0; active<2; ++active)
        {
            assert(Bot::ParseCombatAttackReadback(std::to_string(slot)+"|"+std::to_string(active),s));
            assert(s.valid && s.actionSlotFound && s.actionSlot==slot && s.active==bool(active));
        }
    assert(Bot::ParseCombatAttackReadback("0|-1",s) && s.valid && !s.actionSlotFound && !s.active);
    for (const auto bad:{"", "0|0", "0|1", "1|-1", "121|0", "-1|0", "1|2", "1|1tail",
            "1|1|0", " |1", "1|", "|1", "999999999999999999|1", "unknown_script_error"})
    {
        s={true,true,true,1};
        assert(!Bot::ParseCombatAttackReadback(bad,s));
        assert(!s.valid && !s.actionSlotFound && !s.active && !s.actionSlot);
    }
    for (const auto reason:{"unknown_frame_cycle","unknown_frame_iteration_limit"})
        assert(Bot::CombatInputProbePolicy::Classify(reason)==Bot::CombatInputProbeState::Unknown);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream f(root/"src/Bot/AutoAttackController.h"); assert(f);
    const std::string source(std::istreambuf_iterator<char>(f),{});
    const auto fallback=source.find("else Probe(e.attack,&e.attackReason);");
    assert(fallback!=std::string::npos);
    // Readback is attached only to attack evidence; permissions are determined
    // before it, exclusively from the full guarded probe.
    assert(source.substr(fallback,source.find("return e;",fallback)-fallback).find("e.inputSafe")==std::string::npos);
}
