#include "../src/Bot/ObjectiveAnchorSelectionPolicy.h"
#include "../src/Bot/ValleyOfTrialsProfiles.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Bot;

int main()
{
    const auto& crossroads = CrossroadsQuestProfiles::All();
    assert(crossroads.size() == 3);
    assert(crossroads[1].questId == 881);
    assert(crossroads[2].questId == 905);
    const auto& profile = crossroads.front();
    assert(profile.questId == 903);
    assert(profile.searchDestinations.size() == 3);
    assert(profile.searchDestinations.size() <=
        ObjectiveAnchorSelectionPolicy::MaximumCandidates);

    // Runtime Crossroads origin: database/profile order puts southwestern
    // spawn 14257 first, but northern spawn 14255 is substantially closer.
    const auto local = ObjectiveAnchorSelectionPolicy::Select(
        profile, -370.968567f, -2570.201904f, 95.7876f);
    assert(local.valid && local.index == 1);
    assert(local.directDistance > 690.0f && local.directDistance < 710.0f);

    QuestProfile synthetic{};
    synthetic.destination = {true, 1, 0, 0, 0, 28, "map anchor"};
    synthetic.searchDestinations = {
        {true, 1, 1000, 0, 0, 28, "far"},
        {true, 1, 50, 0, 0, 28, "near"},
        {true, 0, 1, 0, 0, 28, "wrong map"}
    };
    auto selected = ObjectiveAnchorSelectionPolicy::Select(
        synthetic, 0, 0, 0);
    assert(selected.valid && selected.index == 1);
    assert(selected.directDistance == 50.0f);

    // Stable profile order wins exact ties, without depending on iteration
    // over world objects or a hash table.
    synthetic.searchDestinations[0] =
        {true, 1, -50, 0, 0, 28, "tie first"};
    selected = ObjectiveAnchorSelectionPolicy::Select(synthetic, 0, 0, 0);
    assert(selected.valid && selected.index == 0);
    selected = ObjectiveAnchorSelectionPolicy::Select(synthetic, 0, 0, 0);
    assert(selected.valid && selected.index == 0);

    synthetic.searchDestinations.resize(
        ObjectiveAnchorSelectionPolicy::MaximumCandidates + 1);
    selected = ObjectiveAnchorSelectionPolicy::Select(synthetic, 0, 0, 0);
    assert(!selected.valid);
    synthetic.searchDestinations.resize(3);
    selected = ObjectiveAnchorSelectionPolicy::Select(
        synthetic, std::nanf(""), 0, 0);
    assert(!selected.valid);

    // The new ordering is opt-in. Pre-existing Valley profiles retain their
    // original destination/step-selection behavior.
    bool sawValley = false;
    for (const auto& candidate : ValleyOfTrialsProfiles::All())
        if (candidate.questId == 788)
        {
            sawValley = true;
            assert(!candidate.preferNearestObjectiveAnchor);
        }
    assert(sawValley);

    // A failed source-backed seed may choose a different, same-map spawn.
    // The legacy nearest-anchor policy's eight-record input limit must not
    // discard a catalogue with many source-backed objective spawns.
    QuestProfile alternatives{};
    alternatives.destination = {true, 1, 100, 0, 0, 28, "primary"};
    alternatives.searchDestinations.push_back(alternatives.destination);
    alternatives.searchDestinations.push_back(
        {true, 1, 100, 0, 0, 28, "duplicate primary"});
    alternatives.searchDestinations.push_back(
        {true, 0, 1, 0, 0, 28, "wrong map"});
    alternatives.searchDestinations.push_back(
        {true, 1, 40, 0, 0, 28, "alternate"});
    for (int i = 0; i < 35; ++i)
        alternatives.searchDestinations.push_back(
            {true, 1, 200.0f + i, 0, 0, 28, "other source spawn"});

    const auto primary = ObjectiveAnchorSelectionPolicy::PrimarySearchIndex(
        alternatives);
    assert(primary.valid && primary.index == 0);
    auto alternate = ObjectiveAnchorSelectionPolicy::SelectAlternate(
        alternatives, 0, 0, 0, {primary.index});
    assert(alternate.valid && alternate.index == 3);
    alternate = ObjectiveAnchorSelectionPolicy::SelectAlternate(
        alternatives, 0, 0, 0, {primary.index, 3});
    assert(alternate.valid && alternate.index == 4);
    assert(!ObjectiveAnchorSelectionPolicy::SelectAlternate(
        alternatives, 0, 0, 0, {0, 3, 4, 5, 6, 7, 8, 9}).valid);

    // An authored destination outside the source-backed spawn list stays
    // authoritative; a failed route cannot silently replace that override.
    alternatives.destination.x = -100;
    assert(!ObjectiveAnchorSelectionPolicy::PrimarySearchIndex(
        alternatives).valid);
    assert(!ObjectiveAnchorSelectionPolicy::SelectAlternate(
        alternatives, 0, 0, 0, {0}).valid);

    // The game-thread executor cannot be instantiated by a host test, so
    // guard the two integration edges as well as the pure ranking policy.
    std::ifstream executorFile("src/Bot/CollectItemFromMobExecutor.h");
    assert(executorFile);
    const std::string executor{
        std::istreambuf_iterator<char>(executorFile), {}};
    assert(executor.find("PrimarySearchIndex(profile)") != std::string::npos);
    assert(executor.find("TryAlternateSourceAnchor(world.player, tick)") !=
        std::string::npos);
    assert(executor.find("if (navigator_->Failed())\n                {\n"
        "                    if (TryAlternateSourceAnchor(world.player, tick))") !=
        std::string::npos);
    assert(executor.find("std::make_unique<\n"
        "                    Navigation::GenericNavMeshPathFollower>()") !=
        std::string::npos);
}
