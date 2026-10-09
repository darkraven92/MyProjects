#pragma once

#include "QuestPlannerTypes.h"

#include <vector>

namespace Bot
{
    // Focused Crossroads quest data. Quest 903 was already active; quest 881
    // adds pickup and a source-item summon objective. IDs/anchors come from
    // local WDB names, QuestDB patch-10 and its retained source SQL.
    // These are profile data, never generic navigation/quest-controller rules.
    struct CrossroadsQuestProfiles
    {
        static const std::vector<QuestProfile>& All()
        {
            static const std::vector<QuestProfile> profiles = []
            {
                QuestProfile profile{};
                profile.questId = 903;
                profile.title = "Prowlers of the Barrens";
                profile.giverEntry = 3338;
                profile.turnInEntry = 3338;
                profile.minimumLevel = 10;
                profile.expectedObjectiveCount = 1;
                profile.support = QuestExecutionSupport::VerifiedExistingSubsystem;
                profile.objective = {
                    QuestObjectiveType::CollectItemFromMob,
                    3425, 5096, 0, 7, "Savannah Prowler"
                };

                // Three bounded, verified QuestDB creature spawns from the
                // northern and southwestern clusters near the Crossroads;
                // live target positions supersede static search seeds.
                profile.searchDestinations = {
                    {true, 1, -851.938f, -3356.920f, 91.789f, 28.0f,
                        "QuestDB Savannah Prowler spawn 14257"},
                    {true, 1, -539.658f, -1890.550f, 93.039f, 28.0f,
                        "QuestDB Savannah Prowler spawn 14255"},
                    {true, 1, -712.652f, -3424.570f, 91.789f, 28.0f,
                        "QuestDB Savannah Prowler spawn 14248"}
                };
                profile.destination = profile.searchDestinations.front();
                profile.preferNearestObjectiveAnchor = true;
                profile.giverDestination = {true, 1, -482.475f, -2670.190f,
                    97.522f, 5.0f, "QuestDB Sergra Darkthorn spawn 13167"};
                profile.turnInDestination = profile.giverDestination;

                QuestObjectiveStep step{};
                step.slot = 0;
                step.leaderboardIndex = 0;
                step.objective = profile.objective;
                step.destination = profile.destination;
                step.searchDestinations = profile.searchDestinations;
                profile.objectives.push_back(step);
                QuestProfile summoned{};
                summoned.questId = 881;
                summoned.title = "Echeyakee";
                summoned.giverEntry = 3338;
                summoned.turnInEntry = 3338;
                summoned.minimumLevel = 10;
                summoned.expectedObjectiveCount = 1;
                summoned.previousQuestId = 903;
                summoned.questUseItemId = 10327; // Horn of Echeyakee
                // The local patch-0 item_template specifies a 30,000 ms
                // spell cooldown. WorldMonitor polls every 250 ms; 128 ticks
                // leaves a small margin before a bounded retry.
                summoned.questUseMinimumIntervalTicks = 128;
                summoned.support = QuestExecutionSupport::VerifiedExistingSubsystem;
                summoned.objective = {
                    QuestObjectiveType::UseQuestItemAtLocation,
                    3475, 5100, 0, 1, "Echeyakee"
                };
                // QuestDB patch-10 gameobject_spawn 164651 (Echeyakee's
                // Lair). Both records describe the same small use area.
                summoned.searchDestinations = {
                    {true, 1, 459.800f, -3036.000f, 91.300f, 5.0f,
                        "QuestDB Echeyakee's Lair spawn 50383"},
                    {true, 1, 458.110f, -3035.910f, 91.684f, 5.0f,
                        "QuestDB Echeyakee's Lair spawn 261399"}
                };
                summoned.destination = summoned.searchDestinations.front();
                summoned.preferNearestObjectiveAnchor = true;
                summoned.giverDestination = profile.giverDestination;
                summoned.turnInDestination = profile.turnInDestination;
                QuestObjectiveStep summonedStep{};
                summonedStep.slot = 1;
                summonedStep.leaderboardIndex = 0;
                summonedStep.objective = summoned.objective;
                summonedStep.destination = summoned.destination;
                summonedStep.searchDestinations = summoned.searchDestinations;
                summoned.objectives.push_back(summonedStep);

                // QuestDB patch-0 quest 905 has three spell-cast-at-GO rows.
                // The GO anchors and loot-source spawns below are QuestDB
                // data, never generic routing instructions.
                QuestProfile nests{};
                nests.questId = 905;
                nests.title = "The Angry Scytheclaws";
                nests.giverEntry = 3338;
                nests.turnInEntry = 3338;
                nests.minimumLevel = 10;
                nests.previousQuestId = 881;
                nests.expectedObjectiveCount = 3;
                nests.support = QuestExecutionSupport::VerifiedExistingSubsystem;
                nests.questUseItemId = 5165; // Sunscale Feather, spell 5316.
                // Local item_template records a 10-second item-spell cooldown.
                nests.questUseMinimumIntervalTicks = 48;
                nests.giverDestination = profile.giverDestination;
                nests.turnInDestination = profile.turnInDestination;
                nests.questResourceSources = {
                    {3256, "Sunscale Scytheclaw",
                        {true, 1, -1537.100f, -2688.210f, 91.380f, 28.0f,
                            "QuestDB Sunscale Scytheclaw spawn 20073"}},
                    {3255, "Sunscale Screecher",
                        {true, 1, -1525.840f, -2634.480f, 93.120f, 28.0f,
                            "QuestDB Sunscale Screecher spawn 19958"}},
                    {3254, "Sunscale Lashtail",
                        {true, 1, -619.677f, -2740.680f, 92.483f, 28.0f,
                            "QuestDB Sunscale Lashtail spawn 19816"}}
                };
                nests.objectives = {
                    {2, 0,
                        {QuestObjectiveType::UseItemAtGameObject,
                            0, 0, 6907, 1, "Blue Raptor Nest"},
                        {true, 1, -1502.470f, -2707.260f, 92.806f, 4.25f,
                            "QuestDB Blue Raptor Nest spawn 15750"}},
                    {3, 1,
                        {QuestObjectiveType::UseItemAtGameObject,
                            0, 0, 6908, 1, "Yellow Raptor Nest"},
                        {true, 1, -1527.430f, -2648.480f, 92.106f, 4.25f,
                            "QuestDB Yellow Raptor Nest spawn 15751"}},
                    {4, 2,
                        {QuestObjectiveType::UseItemAtGameObject,
                            0, 0, 6906, 1, "Red Raptor Nest"},
                        {true, 1, -1533.440f, -2692.780f, 91.289f, 4.25f,
                            "QuestDB Red Raptor Nest spawn 15749"}}
                };
                nests.objective = nests.objectives.front().objective;
                nests.destination = nests.objectives.front().destination;

                return std::vector<QuestProfile>{profile, summoned, nests};
            }();
            return profiles;
        }
    };
}
