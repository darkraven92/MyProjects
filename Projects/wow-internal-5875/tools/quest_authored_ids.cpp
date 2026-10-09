#include "../src/Bot/ValleyOfTrialsProfiles.h"
#include <fstream>
#include <set>
// Build-time metadata manifest, derived from profile declarations, not a
// second handwritten quest-ID list. Does not load the runtime catalogue.
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::set<int> ids;
    for (const auto& p : Bot::ValleyOfTrialsProfiles::HandAuthored()) ids.insert(p.questId);
    for (const auto& p : Bot::CrossroadsQuestProfiles::All()) ids.insert(p.questId);
    std::ofstream out(argv[1]);
    for (int id : ids) out << id << '\n';
    return out ? 0 : 1;
}
