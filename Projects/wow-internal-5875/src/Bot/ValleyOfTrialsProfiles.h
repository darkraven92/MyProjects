#pragma once

#include "QuestPlannerTypes.h"
#include "CrossroadsQuestProfiles.h"
#include "VanillaQuestDatabase.h"
#include "QuestProfileMergePolicy.h"
#include "QuestGraph.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Bot
{
    class ValleyOfTrialsProfiles
    {
    public:
        static const std::vector<QuestProfile>& HandAuthored()
        {
            /*
             * Phase 11A catalogue for an Orc Warrior path.
             *
             * The first three combat quests use runtime-verified
             * entries from the existing bot. The remaining profiles
             * establish the data model for the generic planner.
             *
             * "Sarkoth" appears twice in Vanilla. Quest 790 has one
             * objective leaderboard row (the claw); quest 804 is a
             * report-to-Gornek step with zero objective rows.
             */
            static const std::vector<QuestProfile> profiles =
            {
                {
                    4641,
                    "Your Place In The World",
                    10176,
                    3143,
                    1,
                    1000,
                    false,
                    "",
                    0,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::TalkToNpc,
                        3143,
                        0,
                        0,
                        1,
                        "Gornek"
                    }
                },
                {
                    2383,
                    "Simple Parchment",
                    3143,
                    3153,
                    1,
                    950,
                    true,
                    "WARRIOR",
                    0,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        VerifiedExistingSubsystem,
                    {
                        QuestObjectiveType::TalkToNpc,
                        3153,
                        0,
                        0,
                        1,
                        "Frang"
                    }
                },
                {
                    788,
                    "Cutting Teeth",
                    3143,
                    3143,
                    1,
                    900,
                    false,
                    "",
                    1,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        VerifiedExistingSubsystem,
                    {
                        QuestObjectiveType::KillMob,
                        3098,
                        0,
                        0,
                        10,
                        "Mottled Boar"
                    }
                },
                {
                    789,
                    "Sting of the Scorpid",
                    3143,
                    3143,
                    1,
                    850,
                    false,
                    "",
                    1,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        VerifiedExistingSubsystem,
                    {
                        QuestObjectiveType::
                            CollectItemFromMob,
                        3124,
                        4862,
                        0,
                        10,
                        "Scorpid Worker"
                    }
                },
                {
                    792,
                    "Vile Familiars",
                    3145,
                    3145,
                    1,
                    800,
                    false,
                    "",
                    1,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        VerifiedExistingSubsystem,
                    {
                        QuestObjectiveType::KillMob,
                        3101,
                        0,
                        0,
                        8,
                        "Vile Familiar"
                    }
                },
                {
                    794,
                    "Burning Blade Medallion",
                    3145,
                    3145,
                    1,
                    750,
                    false,
                    "",
                    1,
                    QuestRouteGroup::BurningBladeCoven,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::
                            CollectItemFromMob,
                        3183,
                        4859,
                        0,
                        1,
                        "Yarrog Baneshadow"
                    },
                    {
                        true,
                        1,
                        -58.1846f,
                        -4220.6400f,
                        62.3418f,
                        28.0f,
                        "Yarrog Baneshadow spawn"
                    }
                },
                {
                    805,
                    "Report to Sen'jin Village",
                    3145,
                    3188,
                    1,
                    700,
                    false,
                    "",
                    0,
                    QuestRouteGroup::SenjinRoad,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::TravelReport,
                        3188,
                        0,
                        0,
                        1,
                        "Master Gadrin"
                    }
                },
                {
                    6394,
                    "Thazz'ril's Pick",
                    11378,
                    11378,
                    1,
                    650,
                    true,
                    "",
                    1,
                    QuestRouteGroup::BurningBladeCoven,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::
                            InteractGameObject,
                        0,
                        16332,
                        178087,
                        1,
                        "Thazz'ril's Pick"
                    },
                    {
                        true,
                        1,
                        -87.74674f,
                        -4276.0050f,
                        65.3904f,
                        8.0f,
                        "Thazz'ril's Pick cave search"
                    }
                },
                {
                    4402,
                    "Galgar's Cactus Apple Surprise",
                    9796,
                    9796,
                    1,
                    500,
                    true,
                    "",
                    1,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::
                            CollectWorldItem,
                        0,
                        11583,
                        171938,
                        10,
                        "Cactus Apple"
                    },
                    {
                        true,
                        1,
                        -489.09f,
                        -4301.17f,
                        42.87f,
                        22.0f,
                        "Cactus Apple sweep start"
                    },
                    {
                        {true,1,-489.09f,-4301.17f,42.87f,22.0f,"Cactus Apple hotspot 1"},
                        {true,1,-406.27f,-4279.20f,46.38f,22.0f,"Cactus Apple hotspot 2"},
                        {true,1,-326.03f,-4395.06f,58.33f,22.0f,"Cactus Apple hotspot 3"},
                        {true,1,-556.42f,-4288.72f,37.44f,22.0f,"Cactus Apple hotspot 4"},
                        {true,1,-422.73f,-4377.85f,42.23f,22.0f,"Cactus Apple hotspot 5"},
                        {true,1,-444.98f,-4122.43f,51.09f,22.0f,"Cactus Apple hotspot 6"},
                        {true,1,-295.80f,-4337.23f,56.83f,22.0f,"Cactus Apple hotspot 7"},
                        {true,1,-465.10f,-4381.39f,50.60f,22.0f,"Cactus Apple hotspot 8"},
                        {true,1,-406.41f,-4460.83f,51.98f,22.0f,"Cactus Apple hotspot 9"},
                        {true,1,-317.60f,-4105.12f,54.33f,22.0f,"Cactus Apple hotspot 10"}
                    },
                    {
                        true,
                        1,
                        -561.63f,
                        -4221.80f,
                        41.67f,
                        5.0f,
                        "Galgar quest-giver audit seed"
                    }
                },
                {
                    5441,
                    "Lazy Peons",
                    11378,
                    11378,
                    1,
                    490,
                    true,
                    "",
                    1,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::UseItemOnUnit,
                        10556,
                        16114,
                        0,
                        5,
                        "Lazy Peon"
                    },
                    {
                        true,
                        1,
                        -225.61f,
                        -4284.88f,
                        65.10f,
                        35.0f,
                        "Lazy Peon search seed (runtime-observed)"
                    }
                },
                {
                    790,
                    "Sarkoth",
                    3287,
                    3287,
                    1,
                    480,
                    true,
                    "",
                    1,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::
                            CollectItemFromMob,
                        3281,
                        4905,
                        0,
                        1,
                        "Sarkoth"
                    }
                },
                {
                    804,
                    "Sarkoth",
                    3287,
                    3143,
                    1,
                    470,
                    true,
                    "",
                    0,
                    QuestRouteGroup::ValleyExterior,
                    QuestExecutionSupport::
                        ProfiledPlannerOnly,
                    {
                        QuestObjectiveType::TravelReport,
                        3143,
                        0,
                        0,
                        1,
                        "Gornek"
                    }
                }
            };

            return profiles;
        }

        static bool PreferHandAuthoredOverride(const QuestProfile& profile)
        {
            /*
             * These objectives need semantics not fully represented by the
             * generic Vanilla quest tables yet. Keep the proven executor data
             * as an override while every ordinary kill/loot/GO/report quest
             * comes from QuestDB.
             */
            return profile.questId == 5441 || profile.questId == 6394;
        }

        static const std::vector<QuestProfile>& All()
        {
            static const std::vector<QuestProfile> merged = []()
            {
                const auto& fallback = HandAuthored();
                auto& database = VanillaQuestDatabase::Instance();
                std::vector<QuestProfile> result = database.EnsureLoaded()
                    ? database.Profiles()
                    : fallback;

                const auto mergeProfile = [&](const QuestProfile& manual, bool explicitMechanic)
                {
                    auto it = std::find_if(
                        result.begin(),
                        result.end(),
                        [&](const QuestProfile& candidate)
                        {
                            return candidate.questId == manual.questId;
                        });

                    if (it == result.end())
                    {
                        const auto* source = database.SourceProfile(manual.questId);
                        result.push_back(source ? QuestProfileMergePolicy::Merge(manual, *source, true) : manual);
                        return;
                    }

                    *it = QuestProfileMergePolicy::Merge(manual, *it,
                        explicitMechanic || PreferHandAuthoredOverride(manual) ||
                        (manual.support == QuestExecutionSupport::VerifiedExistingSubsystem &&
                         manual.objective.type != QuestObjectiveType::TalkToNpc) ||
                        it->objective.type == QuestObjectiveType::Unknown);
                };

                for (const auto& manual : fallback)
                    mergeProfile(manual, true);
                for (const auto& crossroads : CrossroadsQuestProfiles::All())
                    mergeProfile(crossroads, true);

                return result;
            }();

            return merged;
        }

        static const QuestGraph& Graph()
        {
            static const QuestGraph graph(All());
            return graph;
        }

        static bool ClassCompatibleForLiveIdentity(
            const QuestProfile& profile, const std::string& classToken)
        {
            if (profile.classToken != nullptr && profile.classToken[0] != '\0' &&
                classToken != profile.classToken)
                return false;

            // Authored class tokens are not the only class restrictions. The
            // generated catalogue also carries source-backed class masks,
            // which can disambiguate live quests with identical titles/rows.
            const auto classMask = QuestEligibilityPolicy::ClassMask(classToken);
            return !(classMask && profile.requiredClassMask &&
                *profile.requiredClassMask != 0 &&
                (*profile.requiredClassMask & *classMask) == 0);
        }

        static const QuestProfile* Find(
            const PlannerQuestLogEntry& entry,
            const std::string& classToken,
            const std::set<int>* completedQuestIds = nullptr,
            std::uint32_t preferredGiverEntry = 0,
            const std::vector<QuestProfile>* catalogue = nullptr)
        {
            std::vector<const QuestProfile*> candidates;

            for (const auto& profile : catalogue ? *catalogue : All())
            {
                if (entry.title != profile.title)
                    continue;

                if (!ClassCompatibleForLiveIdentity(profile, classToken))
                    continue;

                candidates.push_back(&profile);
            }

            if (candidates.empty())
                return nullptr;

            // Phase 13C.2: title is presentation data, not quest identity.
            // During pickup the live giver entry is known and is stronger
            // evidence than a duplicate title.
            if (preferredGiverEntry != 0)
            {
                std::vector<const QuestProfile*> giverMatches;
                for (const auto* candidate : candidates)
                {
                    if (candidate->giverEntry == preferredGiverEntry)
                        giverMatches.push_back(candidate);
                }
                if (!giverMatches.empty())
                    candidates = std::move(giverMatches);
            }

            // Keep objective-count disambiguation for Vanilla duplicate names
            // such as the two Sarkoth quests.
            std::vector<const QuestProfile*> exactCountMatches;
            for (const auto* candidate : candidates)
            {
                if (candidate->expectedObjectiveCount == entry.objectiveCount)
                    exactCountMatches.push_back(candidate);
            }
            if (!exactCountMatches.empty())
            {
                candidates = std::move(exactCountMatches);
            }
            else
            {
                std::vector<const QuestProfile*> wildcardCountMatches;
                for (const auto* candidate : candidates)
                {
                    if (candidate->expectedObjectiveCount < 0)
                        wildcardCountMatches.push_back(candidate);
                }
                if (!wildcardCountMatches.empty())
                    candidates = std::move(wildcardCountMatches);
                else if (preferredGiverEntry == 0 && std::any_of(candidates.begin(), candidates.end(),
                    [](const QuestProfile* p) { return p->sourceMetadata.has_value(); }))
                    return nullptr; // enriched metadata conflicts with live rows
            }

            if (candidates.size() == 1)
                return candidates.front();

            /*
             * Duplicate-title, zero-objective chain steps cannot be safely
             * resolved from GetQuestLogTitle alone. Use persisted quest IDs
             * as chain context: completed candidates are removed and a
             * PreviousQuestId candidate becomes eligible only when its
             * prerequisite is known complete. If history is insufficient,
             * preserve ambiguity and let the runtime defer rather than guess.
             */
            if (completedQuestIds != nullptr)
            {
                std::vector<const QuestProfile*> notCompleted;
                for (const auto* candidate : candidates)
                {
                    if (completedQuestIds->find(candidate->questId) ==
                        completedQuestIds->end())
                    {
                        notCompleted.push_back(candidate);
                    }
                }
                if (!notCompleted.empty())
                    candidates = std::move(notCompleted);

                if (candidates.size() == 1)
                    return candidates.front();

                std::vector<const QuestProfile*> chainSatisfied;
                for (const auto* candidate : candidates)
                {
                    const int previous = candidate->previousQuestId < 0
                        ? -candidate->previousQuestId
                        : candidate->previousQuestId;
                    if (previous == 0 ||
                        completedQuestIds->find(previous) != completedQuestIds->end())
                    {
                        chainSatisfied.push_back(candidate);
                    }
                }

                if (chainSatisfied.size() == 1)
                    return chainSatisfied.front();

                if (!chainSatisfied.empty())
                    candidates = std::move(chainSatisfied);
            }

            return candidates.size() == 1 ? candidates.front() : nullptr;
        }

        static bool Validate(
            std::string& error)
        {
            const auto& profiles = All();

            if (profiles.empty())
            {
                error = "profile catalogue is empty.";
                return false;
            }

            std::set<int> questIds;
            for (const auto& profile : profiles)
            {
                if (profile.questId <= 0 ||
                    profile.title == nullptr ||
                    profile.title[0] == '\0')
                {
                    error = "profile has invalid identity.";
                    return false;
                }

                if (!questIds.insert(profile.questId).second)
                {
                    error = "duplicate quest ID " + std::to_string(profile.questId);
                    return false;
                }
            }

            // Duplicate quest titles are legal Vanilla data. They are not an
            // identity-validation failure in Phase 13C.2.
            error.clear();
            return true;
        }

        static int DuplicateTitleGroupCount()
        {
            std::set<std::string> seen;
            std::set<std::string> duplicates;

            for (const auto& profile : All())
            {
                const std::string title =
                    profile.title == nullptr ? std::string{} : std::string(profile.title);
                if (title.empty())
                    continue;
                if (!seen.insert(title).second)
                    duplicates.insert(title);
            }

            return static_cast<int>(duplicates.size());
        }
    };
}
